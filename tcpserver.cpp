#include "tcpserver.h"

#include <QHostAddress>
#include <QDateTime>
#include <QTextStream>
#include <QDebug>
#include <QCoreApplication> // Для QCoreApplication::applicationDirPath()

TcpServer::TcpServer(QObject *parent)
    : QObject(parent),
      m_tcpServer(new QTcpServer(this)),
      m_clientSocket(nullptr),
      m_currentFile(nullptr)
{
    connect(m_tcpServer, &QTcpServer::newConnection, this, &TcpServer::newConnection);
}

TcpServer::~TcpServer()
{
    stopServer(); // Убедимся, что сервер остановлен
    if (m_clientSocket && m_clientSocket->isOpen()) {
        m_clientSocket->close();
    }
    if (m_currentFile) {
        if (m_currentFile->isOpen()) m_currentFile->close();
        delete m_currentFile;
    }
}

void TcpServer::startServer(int port)
{
    if (m_tcpServer->isListening()) {
        emit warningMessage("Предупреждение", "Сервер уже запущен.");
        return;
    }

    // Убедимся, что локальная директория существует
    QDir localDir(m_localLogsDirectory);
    if (!localDir.exists()) {
        if (!localDir.mkpath(".")) {
            emit errorMessage("Ошибка", "Не удалось создать локальную директорию: " + m_localLogsDirectory);
            emit serverStatusChanged("Ошибка запуска сервера.");
            return;
        }
    }

    if (!m_tcpServer->listen(QHostAddress::Any, port)) {
        emit errorMessage("Ошибка", "Не удалось запустить сервер: " + m_tcpServer->errorString());
        emit serverStatusChanged("Ошибка запуска сервера.");
    } else {
        emit serverStatusChanged("Сервер запущен на порту " + QString::number(port));
        emit infoMessage("Сервер запущен", "Сервер запущен на порту " + QString::number(port));
    }
}

void TcpServer::stopServer()
{
    if (m_tcpServer->isListening()) {
        m_isShuttingDown = true;
        m_tcpServer->close();
        if (m_clientSocket && m_clientSocket->isOpen()) {
            m_clientSocket->close(); // Отключаем текущего клиента
        }
        emit serverStatusChanged("Сервер остановлен.");
        emit infoMessage("Сервер остановлен", "Сервер остановлен.");
        m_isShuttingDown = false;
    }
}

void TcpServer::setLocalLogsDirectory(const QString &path)
{
    m_localLogsDirectory = path;
}

void TcpServer::setNetworkLogsDirectory(const QString &path)
{
    m_networkLogsDirectory = path;
}

void TcpServer::newConnection()
{
    // Если уже есть подключенный клиент, отклоняем новое подключение
    if (m_clientSocket && m_clientSocket->isOpen()) {
        QTcpSocket *newClient = m_tcpServer->nextPendingConnection();
        newClient->disconnectFromHost();
        newClient->deleteLater();
        emit warningMessage("Подключение", "Новый клиент попытался подключиться, но сервер занят.");
        return;
    }

    m_clientSocket = m_tcpServer->nextPendingConnection();
    if (m_clientSocket) {
        connect(m_clientSocket, &QTcpSocket::readyRead, this, &TcpServer::readClientData);
        connect(m_clientSocket, &QTcpSocket::disconnected, this, &TcpServer::clientDisconnectedSlot);

        emit clientConnected(m_clientSocket->peerAddress().toString());
        //emit serverStatusChanged("Клиент подключен: " + m_clientSocket->peerAddress().toString());
        emit infoMessage("Клиент подключен", "Подключен клиент: " + m_clientSocket->peerAddress().toString());

        resetFileTransferState(); // Сбрасываем состояние для нового клиента
    }
}

void TcpServer::readClientData()
{
    if (!m_clientSocket || !m_clientSocket->bytesAvailable()) {
        return;
    }

    m_receiveBuffer.append(m_clientSocket->readAll());

    while (true) {
        if (m_currentState == FileState::None) {
            // Отбрасываем любые ведущие символы, не являющиеся '#'
            while (!m_receiveBuffer.isEmpty() && m_receiveBuffer.at(0) != '#') {
                m_receiveBuffer.remove(0, 1);
            }

            if (m_receiveBuffer.isEmpty()) {
                break;
            }

            // --- Обработка команды #START# ---
            if (m_receiveBuffer.startsWith("#START#")) {
                emit testStarted();
                emit infoMessage("Начало тестирования", "Устройство начало тестирование...");
                m_receiveBuffer.remove(0, QString("#START#").length());
                continue;
            }

            // --- Обработка команды #sn# ---
            if (m_receiveBuffer.length() >= QString("#sn##").length() && m_receiveBuffer.startsWith("#sn#")) {
                int endHash = m_receiveBuffer.indexOf('#', QString("#sn#").length());
                if (endHash != -1) {
                    QString fullCommand = QString::fromUtf8(m_receiveBuffer.mid(0, endHash + 1));
                    QString serialNumber = fullCommand.mid(QString("#sn#").length(), endHash - QString("#sn#").length());

                    emit serialNumberReceived(serialNumber);
                    emit infoMessage("Подключено устройство", "Устройство подключено с серийным номером: " + serialNumber);

                    m_receiveBuffer.remove(0, fullCommand.length());
                    continue;
                }
            }

            // Если не #START# и не #sn#, тогда пытаемся разобрать как заголовок файла
            int firstHash = m_receiveBuffer.indexOf('#', 1);
            int secondHash = m_receiveBuffer.indexOf('#', firstHash + 1);
            int thirdHash = m_receiveBuffer.indexOf('#', secondHash + 1);

            // Проверка на корректность заголовка
            if (firstHash == -1 || secondHash == -1 || thirdHash == -1 ||
                !(firstHash > 0 && secondHash > firstHash && thirdHash > secondHash))
            {
                // Некорректный заголовок, пытаемся найти следующий '#' и отбросить текущую часть
                int nextHash = m_receiveBuffer.indexOf('#', 1);
                if (nextHash != -1) {
                    m_receiveBuffer.remove(0, nextHash);
                } else {
                    m_receiveBuffer.clear(); // Ничего не нашли, очищаем буфер
                }
                continue;
            }

            QString header = QString::fromUtf8(m_receiveBuffer.mid(0, thirdHash + 1));
            QString type = header.mid(1, firstHash - 1);
            QString filename = header.mid(firstHash + 1, secondHash - (firstHash + 1));
            QString lengthStr = header.mid(secondHash + 1, thirdHash - (secondHash + 1));

            bool ok;
            m_expectedFileSize = lengthStr.toLongLong(&ok);

            if (!ok || m_expectedFileSize < 0 || (type != "TEST_OK" && type != "TEST_ERR")) {
                emit warningMessage("Ошибка заголовка", "Получен некорректный заголовок файла или тип теста.");
                m_receiveBuffer.remove(0, thirdHash + 1); // Отбрасываем некорректный заголовок
                continue;
            }

            m_currentFileName = filename;
            m_bytesReceived = 0;

            QDir dir(m_localLogsDirectory);
            if (!dir.exists()) {
                if (!dir.mkpath(".")) {
                    emit errorMessage("Ошибка сохранения", "Не удалось создать локальную директорию: " + m_localLogsDirectory);
                    resetFileTransferState(); // Сброс состояния при ошибке
                    m_receiveBuffer.remove(0, thirdHash + 1);
                    continue;
                }
            }

            QString fullLocalPathForWrite = m_localLogsDirectory + "/" + m_currentFileName;
            if (m_currentFile) { // Если вдруг файл не был закрыт ранее
                if (m_currentFile->isOpen()) m_currentFile->close();
                delete m_currentFile;
                m_currentFile = nullptr;
            }
            m_currentFile = new QFile(fullLocalPathForWrite);
            if (!m_currentFile->open(QIODevice::WriteOnly)) {
                emit errorMessage("Ошибка сохранения", "Не удалось создать локальный файл: " + fullLocalPathForWrite + ". " + m_currentFile->errorString());
                resetFileTransferState(); // Сброс состояния при ошибке
                m_receiveBuffer.remove(0, thirdHash + 1);
                continue;
            }

            m_receiveBuffer.remove(0, thirdHash + 1);
            m_currentState = FileState::ReceivingFile;
            emit fileTransferStarted(m_currentFileName); // Сообщаем о начале приема файла
        } else if (m_currentState == FileState::ReceivingFile) {
            qint64 remainingBytesToReceive = m_expectedFileSize - m_bytesReceived;

            if (remainingBytesToReceive <= 0) { // Файл полностью принят
                QString fullLocalPath = m_localLogsDirectory + "/" + m_currentFileName;
                QString fullNetworkPath = m_networkLogsDirectory + "/" + m_currentFileName;
                bool copiedToNetwork = false;

                if (m_currentFile) {
                    if (m_currentFile->isOpen()) m_currentFile->close();
                    delete m_currentFile;
                    m_currentFile = nullptr;
                }

                QDir networkDir(m_networkLogsDirectory);
                if (networkDir.exists()) {
                    // Проверяем, существует ли файл назначения
                    if (QFile::exists(fullNetworkPath)) {
                        // Если существует, пытаемся удалить его
                        if (!QFile::remove(fullNetworkPath)) {
                            qDebug() << "TcpServer: Не удалось удалить существующий файл в сетевой директории: " << fullNetworkPath;
                            // Возможно, здесь нужно выдать ошибку или предупреждение
                            emit errorMessage("Ошибка копирования", "Не удалось удалить старый файл для перезаписи: " + fullNetworkPath);
                            return; // Прекращаем выполнение, так как не можем удалить старый файл
                        }
                    }
                    if (QFile::copy(fullLocalPath, fullNetworkPath)) {
                        copiedToNetwork = true;
                    } else {
                        qDebug() << "TcpServer: Не удалось скопировать файл в сетевую директорию: " << fullNetworkPath;
                    }
                } else {
                     emit warningMessage("Внимание",
                                          "Сетевая директория '" + m_networkLogsDirectory + "' не существует. Лог-файл не будет скопирован на сетевой ресурс.");
                }

                bool isSuccess = m_currentFileName.contains("ok", Qt::CaseInsensitive);
                QString message;
                if (isSuccess) {
                    message = "Тестирование успешно завершено.\n";
                    // statusLabel обновляется в SberServer по сигналу fileTransferFinished, а не здесь напрямую
                } else {
                    message = "Тестирование завершено с ошибками.\n";
                    // statusLabel обновляется в SberServer по сигналу fileTransferFinished, а не здесь напрямую
                }

                if (copiedToNetwork) {
                    message += "Лог файл сохранен на сервере по пути: " + fullNetworkPath;
                } else {
                    message += "Лог файл сохранен локально по пути: " + fullLocalPath + "\n"
                               "Не удалось сохранить на сетевой ресурс.";
                }

                emit fileTransferFinished(m_currentFileName, isSuccess, message);
                resetFileTransferState();
                continue;
            }

            qint64 bytesToWrite = qMin(remainingBytesToReceive, (qint64)m_receiveBuffer.length());

            if (bytesToWrite > 0) {
                if (m_currentFile && m_currentFile->isOpen()) {
                    m_currentFile->write(m_receiveBuffer.mid(0, bytesToWrite));
                    m_bytesReceived += bytesToWrite;
                    m_receiveBuffer.remove(0, bytesToWrite);
                    emit fileTransferProgress(m_bytesReceived, m_expectedFileSize);
                } else {
                    emit errorMessage("Ошибка файла", "Файл не открыт для записи. Сброс состояния.");
                    resetFileTransferState();
                    break;
                }
            } else {
                break; // Нет данных для записи или буфер пуст
            }
        }
    }
}

void TcpServer::clientDisconnectedSlot()
{
    if (m_clientSocket) {
        QString clientInfo = m_clientSocket->peerAddress().toString();
        if (!m_isShuttingDown) {
            emit clientDisconnected(clientInfo);
            emit infoMessage("Устройство отключено", "Устройство отключено.");
        }

        if (m_currentState == FileState::ReceivingFile && m_currentFile && m_currentFile->isOpen()) {
            m_currentFile->close();
            if (m_bytesReceived < m_expectedFileSize) {
                emit warningMessage("Прием файла прерван",
                                    "Прием файла '" + m_currentFileName + "' прерван! Получено " +
                                    QString::number(m_bytesReceived) + " из " + QString::number(m_expectedFileSize) + " байт.");
                // Можно удалить неполный файл, если нужно
            }
        } else if (m_currentFile && m_currentFile->isOpen()){
            m_currentFile->close();
        }
        resetFileTransferState(); // Сброс состояния
        m_clientSocket->deleteLater();
        m_clientSocket = nullptr;
    }
    // После отключения клиента, если сервер все еще слушает, устанавливаем статус сервера
    if (m_tcpServer->isListening()) {
        emit serverStatusChanged("Сервер запущен на порту " + QString::number(m_tcpServer->serverPort()));
    } else {
         emit serverStatusChanged("Сервер остановлен.");
    }
}

void TcpServer::resetFileTransferState()
{
    m_currentState = FileState::None;
    m_receiveBuffer.clear();
    m_currentFileName.clear();
    m_expectedFileSize = 0;
    m_bytesReceived = 0;
    if (m_currentFile) {
        if (m_currentFile->isOpen()) m_currentFile->close();
        delete m_currentFile;
        m_currentFile = nullptr;
    }
}
