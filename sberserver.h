#ifndef SBERSERVER_H
#define SBERSERVER_H

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QMessageBox> // Добавляем для QMessageBox
#include <QSystemTrayIcon> // Новый заголовочный файл
#include <QMenu>           // Новый заголовочный файл
#include <QCloseEvent>     // Для переопределения closeEvent

#include "configfile.h"
#include "tcpserver.h"


class SberServer : public QMainWindow
{
    Q_OBJECT

public:
    explicit SberServer(QWidget *parent = nullptr);
    ~SberServer();

protected:
    // Переопределяем метод closeEvent для обработки закрытия окна
    void closeEvent(QCloseEvent *event) override;

private slots:
    void startStopServer();
    void browseLocalLogsDirectory();
    void browseNetworkLogsDirectory();
    void onLocalPathEdited(const QString &text);
    void onNetworkPathEdited(const QString &text);

    // Слоты для обработки сигналов от TcpFileServer
    void handleServerStatusChanged(const QString &message);
    void handleClientConnected(const QString &peerAddress);
    void handleClientDisconnected(const QString &peerAddress);
    void handleFileTransferStarted(const QString &fileName);
    void handleFileTransferFinished(const QString &fileName, bool success, const QString &message);
    void handleTestStarted();
    void handleSerialNumberReceived(const QString &serialNumber);
    void handleErrorMessage(const QString &title, const QString &text);
    void handleWarningMessage(const QString &title, const QString &text);
    void handleInfoMessage(const QString &title, const QString &text);

    void showNonBlockingMessage(QMessageBox::Icon icon, const QString &title, const QString &text); // Новый слот/метод

    // Новые слоты для системного трея
    void iconActivated(QSystemTrayIcon::ActivationReason reason); // Обработка кликов по иконке трея
    void trayStartStopServer(); // Вызов startStopServer из меню трея
    void showSettings();        // Показать основное окно из меню трея ("Настройки")
    void quitApplication();     // Корректное завершение приложения из меню трея ("Выход")

    // НОВЫЙ СЛОТ ДЛЯ ОБНОВЛЕНИЯ ПОДСКАЗКИ ТРЕЯ
    void updateTrayToolTip(const QString &tooltipText);

private:
    QLabel *statusLabel;
    QPushButton *startStopButton;
    QLineEdit *localLogsPathEdit;
    QPushButton *localBrowseButton;
    QLineEdit *networkLogsPathEdit;
    QPushButton *networkBrowseButton;

    int serverPort = 5643; // Порт сервера
    QMessageBox *currentMessageDialog = nullptr; // Для отслеживания активного неблокирующего диалога

    ConfigFile *m_configFile;
    TcpServer *m_tcpServer;

    QSystemTrayIcon *m_trayIcon; // Иконка в системном трее
    QMenu *m_trayMenu;           // Контекстное меню для иконки трея
    QAction *m_startStopAction;  // Действие "Запустить/Остановить" в меню трея
    QString m_lastConnectedSerialNumber;
};

#endif // SBERSERVER_H
