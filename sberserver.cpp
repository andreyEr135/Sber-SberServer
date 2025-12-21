#include "sberserver.h"
#include <QMessageBox>
#include <QDir>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QFont>
#include <QFrame>
#include <QFile>
#include <QDebug>
#include <QApplication>
#include <QCoreApplication>

SberServer::SberServer(QWidget *parent)
    : QMainWindow(parent),
      currentMessageDialog(nullptr),
      m_lastConnectedSerialNumber("") // НОВОЕ: Инициализируем пустым
{
    setWindowTitle("СберСервер");
    resize(600, 300);

    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QApplication::setQuitOnLastWindowClosed(false);

    QVBoxLayout *mainLayout = new QVBoxLayout(centralWidget);

    QLabel *title = new QLabel(QString("СберСервер\nВерсия %1.%2.%3").arg(VERSION_MAJOR).arg(VERSION_MINOR).arg(VERSION_BUILD), this);
    QFont fontTitle = title->font();
    fontTitle.setPointSize(20);
    fontTitle.setBold(true);
    title->setFont(fontTitle);
    title->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(title);

    QLabel *setupTitle = new QLabel("Настройки", this);
    QFont fontSetupTitle = setupTitle->font();
    fontSetupTitle.setPointSize(16);
    setupTitle->setFont(fontSetupTitle);
    mainLayout->addWidget(setupTitle);

    QLabel *localPathLog = new QLabel("Локальная директория логов:", this);
    QFont fontLocalPathLog = localPathLog->font();
    fontLocalPathLog.setPointSize(12);
    localPathLog->setFont(fontLocalPathLog);

    mainLayout->addWidget(localPathLog);
    QHBoxLayout *localPathLayout = new QHBoxLayout();
    localLogsPathEdit = new QLineEdit("received_files", this);
    QFont fontLocalLogsPathEdit = localLogsPathEdit->font();
    fontLocalLogsPathEdit.setPointSize(12);
    localLogsPathEdit->setFont(fontLocalLogsPathEdit);
    localPathLayout->addWidget(localLogsPathEdit);
    localBrowseButton = new QPushButton("Обзор...", this);
    QFont fontLocalBrowseButton = localBrowseButton->font();
    fontLocalBrowseButton.setPointSize(12);
    localBrowseButton->setFont(fontLocalBrowseButton);
    connect(localBrowseButton, &QPushButton::clicked, this, &SberServer::browseLocalLogsDirectory);
    localPathLayout->addWidget(localBrowseButton);
    mainLayout->addLayout(localPathLayout);

    QLabel *networkPathLog = new QLabel("Сетевая директория логов:", this);
    QFont fontNetworkPathLog = networkPathLog->font();
    fontNetworkPathLog.setPointSize(12);
    networkPathLog->setFont(fontNetworkPathLog);
    mainLayout->addWidget(networkPathLog);
    QHBoxLayout *networkPathLayout = new QHBoxLayout();
    networkLogsPathEdit = new QLineEdit("C:/NetworkLogs", this);
    QFont fontNetworkLogsPathEdit = networkLogsPathEdit->font();
    fontNetworkLogsPathEdit.setPointSize(12);
    networkLogsPathEdit->setFont(fontNetworkLogsPathEdit);
    networkPathLayout->addWidget(networkLogsPathEdit);
    networkBrowseButton = new QPushButton("Обзор...", this);
    QFont fontNetworkBrowseButton = networkBrowseButton->font();
    fontNetworkBrowseButton.setPointSize(12);
    networkBrowseButton->setFont(fontNetworkBrowseButton);
    connect(networkBrowseButton, &QPushButton::clicked, this, &SberServer::browseNetworkLogsDirectory);
    networkPathLayout->addWidget(networkBrowseButton);
    mainLayout->addLayout(networkPathLayout);

    startStopButton = new QPushButton("Запустить", this);
    QFont fontStartStopButton = startStopButton->font();
    fontStartStopButton.setPointSize(12);
    startStopButton->setFont(fontNetworkBrowseButton);
    connect(startStopButton, &QPushButton::clicked, this, &SberServer::startStopServer);
    mainLayout->addWidget(startStopButton);

    QFrame *line = new QFrame(this);
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Sunken);
    mainLayout->addWidget(line);
    QLabel *statusTitle = new QLabel("Статус", this);
    QFont fontStatusTitle = statusTitle->font();
    fontStatusTitle.setPointSize(16);
    statusTitle->setFont(fontStatusTitle);
    mainLayout->addWidget(statusTitle);

    statusLabel = new QLabel("Сервер остановлен.", this);
    QFont fontStatusLabel = statusLabel->font();
    fontStatusLabel.setPointSize(12);
    statusLabel->setFont(fontStatusLabel);
    mainLayout->addWidget(statusLabel);

    mainLayout->addStretch();

    m_configFile = new ConfigFile(this);
    if (!m_configFile->loadConfig()) {
        qDebug() << "Ошибка при загрузке конфигурации. Возможно, используется дефолтная.";
    }
    localLogsPathEdit->setText(m_configFile->localLogsDirectory());
    networkLogsPathEdit->setText(m_configFile->networkLogsDirectory());

    connect(localLogsPathEdit, &QLineEdit::textChanged, this, &SberServer::onLocalPathEdited);
    connect(networkLogsPathEdit, &QLineEdit::textChanged, this, &SberServer::onNetworkPathEdited);

    m_tcpServer = new TcpServer(this);
    m_tcpServer->setLocalLogsDirectory(m_configFile->localLogsDirectory());
    m_tcpServer->setNetworkLogsDirectory(m_configFile->networkLogsDirectory());

    connect(m_tcpServer, &TcpServer::serverStatusChanged, this, &SberServer::handleServerStatusChanged);
    connect(m_tcpServer, &TcpServer::clientConnected, this, &SberServer::handleClientConnected);
    connect(m_tcpServer, &TcpServer::clientDisconnected, this, &SberServer::handleClientDisconnected);
    connect(m_tcpServer, &TcpServer::fileTransferStarted, this, &SberServer::handleFileTransferStarted);
    connect(m_tcpServer, &TcpServer::fileTransferFinished, this, &SberServer::handleFileTransferFinished);
    connect(m_tcpServer, &TcpServer::testStarted, this, &SberServer::handleTestStarted);
    connect(m_tcpServer, &TcpServer::serialNumberReceived, this, &SberServer::handleSerialNumberReceived);
    connect(m_tcpServer, &TcpServer::errorMessage, this, &SberServer::handleErrorMessage);
    connect(m_tcpServer, &TcpServer::warningMessage, this, &SberServer::handleWarningMessage);
    connect(m_tcpServer, &TcpServer::infoMessage, this, &SberServer::handleInfoMessage);

    m_trayIcon = new QSystemTrayIcon(this);
    m_trayIcon->setIcon(QIcon(":/Icons/main.png"));
    m_trayIcon->setToolTip("СберСервер");

    // Устанавливаем начальную подсказку трея
    updateTrayToolTip(statusLabel->text());

    m_trayMenu = new QMenu(this);

    m_startStopAction = new QAction("Запустить", this);
    connect(m_startStopAction, &QAction::triggered, this, &SberServer::trayStartStopServer);
    m_trayMenu->addAction(m_startStopAction);

    QAction *settingsAction = new QAction("Настройки", this);
    connect(settingsAction, &QAction::triggered, this, &SberServer::showSettings);
    m_trayMenu->addAction(settingsAction);

    m_trayMenu->addSeparator();

    QAction *quitAction = new QAction("Выход", this);
    connect(quitAction, &QAction::triggered, this, &SberServer::quitApplication);
    m_trayMenu->addAction(quitAction);

    m_trayIcon->setContextMenu(m_trayMenu);
    connect(m_trayIcon, &QSystemTrayIcon::activated, this, &SberServer::iconActivated);
    m_trayIcon->show();
}

SberServer::~SberServer()
{
    if (m_trayIcon) {
        m_trayIcon->hide();
    }
    if (currentMessageDialog) {
        currentMessageDialog->close();
    }
}

void SberServer::closeEvent(QCloseEvent *event)
{
    if (startStopButton->text() == "Запустить")
    {
        exit(0);
    }
    if (m_trayIcon->isVisible()) {
        hide();
        event->ignore();
        m_trayIcon->showMessage(
                    "СберСервер",
                    "Приложение свернуто в трей. Для выхода выберите 'Выход' в меню трея.",
                    QSystemTrayIcon::Information,
                    2000
                    );
    } else {
        event->accept();
    }
}

void SberServer::showNonBlockingMessage(QMessageBox::Icon icon, const QString &title, const QString &text)
{
    if (currentMessageDialog) {
        currentMessageDialog->close();
    }

    QMessageBox *msgBox = new QMessageBox(icon, title, text, QMessageBox::Ok, this);
    msgBox->setAttribute(Qt::WA_DeleteOnClose);

    connect(msgBox, &QMessageBox::finished, this, [this, msgBox](int result){
        Q_UNUSED(result);
        if (this->currentMessageDialog == msgBox) {
            this->currentMessageDialog = nullptr;
        }
    });

    msgBox->show();
    currentMessageDialog = msgBox;
}

void SberServer::onLocalPathEdited(const QString &text)
{
    if (!m_configFile->saveConfig(text, networkLogsPathEdit->text())) {
        showNonBlockingMessage(QMessageBox::Critical, "Ошибка сохранения", "Не удалось сохранить файл конфигурации config.json.");
    }
}

void SberServer::onNetworkPathEdited(const QString &text)
{
    if (!m_configFile->saveConfig(localLogsPathEdit->text(), text)) {
        showNonBlockingMessage(QMessageBox::Critical, "Ошибка сохранения", "Не удалось сохранить файл конфигурации config.json.");
    }
}

void SberServer::startStopServer()
{
    if (startStopButton->text() == "Запустить") { // Сервер остановлен, нужно запустить
        m_tcpServer->startServer(serverPort);
        startStopButton->setText("Остановить");
        m_startStopAction->setText("Остановить");
        localLogsPathEdit->setEnabled(false);
        localBrowseButton->setEnabled(false);
        networkLogsPathEdit->setEnabled(false);
        networkBrowseButton->setEnabled(false);
        hide();
        m_trayIcon->showMessage(
                    "СберСервер запущен",
                    "Приложение работает в фоновом режиме. Для доступа к настройкам используйте меню трея.",
                    QSystemTrayIcon::Information,
                    3000
                    );
    } else { // Сервер запущен, нужно остановить
        m_tcpServer->stopServer();
        startStopButton->setText("Запустить");
        m_startStopAction->setText("Запустить");
        localLogsPathEdit->setEnabled(true);
        localBrowseButton->setEnabled(true);
        networkLogsPathEdit->setEnabled(true);
        networkBrowseButton->setEnabled(true);
        m_lastConnectedSerialNumber = ""; // НОВОЕ: Сброс серийного номера при остановке сервера
    }
}

void SberServer::browseLocalLogsDirectory()
{
    QString directory = QFileDialog::getExistingDirectory(this, "Выберите локальную директорию для логов",
                                                           localLogsPathEdit->text());
    if (!directory.isEmpty()) {
        localLogsPathEdit->setText(directory);
    }
}

void SberServer::browseNetworkLogsDirectory()
{
    QString directory = QFileDialog::getExistingDirectory(this, "Выберите сетевую директорию для логов",
                                                           networkLogsPathEdit->text());
    if (!directory.isEmpty()) {
        networkLogsPathEdit->setText(directory);
    }
}

void SberServer::handleServerStatusChanged(const QString &message)
{
    statusLabel->setText(message);
    updateTrayToolTip(message);
    if (message.contains("запущен", Qt::CaseInsensitive)) {
        startStopButton->setText("Остановить");
        localLogsPathEdit->setEnabled(false);
        localBrowseButton->setEnabled(false);
        networkLogsPathEdit->setEnabled(false);
        networkBrowseButton->setEnabled(false);
    } else {
        startStopButton->setText("Запустить");
        localLogsPathEdit->setEnabled(true);
        localBrowseButton->setEnabled(true);
        networkLogsPathEdit->setEnabled(true);
        networkBrowseButton->setEnabled(true);
    }
}

void SberServer::handleClientConnected(const QString &peerAddress)
{
    // Пока ничего не делаем, ждем серийный номер
}

void SberServer::handleClientDisconnected(const QString &peerAddress)
{
    // НОВОЕ: Очищаем серийный номер при отключении клиента
    m_lastConnectedSerialNumber = "";
    // Если сервер все еще запущен, возвращаемся к общему статусу "Сервер запущен."
    if (startStopButton->text() == "Остановить") {
        statusLabel->setText("Сервер запущен.");
        updateTrayToolTip("Сервер запущен.");
    } else {
        statusLabel->setText("Сервер остановлен.");
        updateTrayToolTip("Сервер остановлен.");
    }
}

void SberServer::handleFileTransferStarted(const QString &fileName)
{
    qDebug() << "Начало приема файла: " << fileName;
}

void SberServer::handleFileTransferFinished(const QString &fileName, bool success, const QString &message)
{
    if (success) {
        showNonBlockingMessage(QMessageBox::Information, "Тестирование завершено", message);
    } else {
        showNonBlockingMessage(QMessageBox::Critical, "Тестирование завершено с ошибками", message);
    }

    // НОВОЕ: После завершения тестирования восстанавливаем статус
    if (startStopButton->text() == "Остановить") { // Если сервер все еще работает
        if (!m_lastConnectedSerialNumber.isEmpty()) {
            QString statusTxt = "Устройство с серийным номером " + m_lastConnectedSerialNumber + " подключено";
            statusLabel->setText(statusTxt);
            updateTrayToolTip(statusTxt);
        } else {
            // Если серийный номер не был получен (или устройство отключилось),
            // возвращаемся к общему статусу "Сервер запущен."
            statusLabel->setText("Сервер запущен.");
            updateTrayToolTip("Сервер запущен.");
        }
    }
}

void SberServer::handleTestStarted()
{
    showNonBlockingMessage(QMessageBox::Information, "Начало тестирования", "Устройство начало тестирование...");
    statusLabel->setText("Устройство начало тестирование");
    updateTrayToolTip("Идет процесс тестирования устройства");
}

void SberServer::handleSerialNumberReceived(const QString &serialNumber)
{
    QString msg = "Устройство подключено с серийным номером: " + serialNumber;
    showNonBlockingMessage(QMessageBox::Information, "Подключено устройство", msg);
    QString statusTxt = "Устройство с серийным номером " + serialNumber + " подключено";
    statusLabel->setText(statusTxt);
    updateTrayToolTip(statusTxt);
    m_lastConnectedSerialNumber = serialNumber; // НОВОЕ: Сохраняем серийный номер
}

void SberServer::handleErrorMessage(const QString &title, const QString &text)
{
    showNonBlockingMessage(QMessageBox::Critical, title, text);
}

void SberServer::handleWarningMessage(const QString &title, const QString &text)
{
    showNonBlockingMessage(QMessageBox::Warning, title, text);
}

void SberServer::handleInfoMessage(const QString &title, const QString &text)
{
    showNonBlockingMessage(QMessageBox::Information, title, text);
}

void SberServer::iconActivated(QSystemTrayIcon::ActivationReason reason)
{
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        if (isVisible()) {
            hide();
        } else {
            showNormal();
            activateWindow();
        }
        break;
    default:
        break;
    }
}

void SberServer::trayStartStopServer()
{
    startStopServer();
}

void SberServer::showSettings()
{
    showNormal();
    activateWindow();
}

void SberServer::quitApplication()
{
    m_tcpServer->stopServer();
    QCoreApplication::quit();
}

void SberServer::updateTrayToolTip(const QString &tooltipText)
{
    if (m_trayIcon) {
        m_trayIcon->setToolTip(tooltipText);
    }
}
