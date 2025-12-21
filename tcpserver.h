#ifndef TCPSERVER_H
#define TCPSERVER_H


#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QFile>
#include <QByteArray>
#include <QString>
#include <QDir> // Для работы с директориями

class TcpServer : public QObject
{
    Q_OBJECT

public:
    explicit TcpServer(QObject *parent = nullptr);
    ~TcpServer();

    // Методы для управления сервером
    void startServer(int port);
    void stopServer();

    // Методы для установки путей логов (UI будет их обновлять)
    void setLocalLogsDirectory(const QString &path);
    void setNetworkLogsDirectory(const QString &path);

signals:
    // Сигналы для SberServer (UI)
    void serverStatusChanged(const QString &message);
    void clientConnected(const QString &peerAddress);
    void clientDisconnected(const QString &peerAddress);
    void fileTransferStarted(const QString &fileName);
    void fileTransferProgress(qint64 bytesReceived, qint64 totalBytes);
    void fileTransferFinished(const QString &fileName, bool success, const QString &message);
    void testStarted();
    void serialNumberReceived(const QString &serialNumber);
    void errorMessage(const QString &title, const QString &text);
    void warningMessage(const QString &title, const QString &text);
    void infoMessage(const QString &title, const QString &text);

private slots:
    void newConnection();
    void readClientData();
    void clientDisconnectedSlot(); // Переименован, чтобы избежать конфликта с сигналом

private:
    QTcpServer *m_tcpServer;
    QTcpSocket *m_clientSocket; // Храним только один активный клиент

    // Состояние приема файла
    enum class FileState {
        None,
        ReceivingHeader,
        ReceivingFile
    };
    FileState m_currentState = FileState::None;

    QByteArray m_receiveBuffer;
    QString m_currentFileName;
    qint64 m_expectedFileSize = 0;
    qint64 m_bytesReceived = 0;
    QFile *m_currentFile = nullptr;

    QString m_localLogsDirectory;
    QString m_networkLogsDirectory;
    bool m_isShuttingDown = false;

    void resetFileTransferState();
};


#endif // TCPSERVER_H
