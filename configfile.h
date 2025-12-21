#ifndef CONFIGFILE_H
#define CONFIGFILE_H

#include <QString>
#include <QObject> // QObject для возможности использования сигналов/слотов, хотя здесь пока не обязательно.
                  // Но это хорошая практика для классов, которые взаимодействуют с данными.
#include <QDebug> // Для qDebug в реализации

class ConfigFile : public QObject
{
    Q_OBJECT // Не забываем макрос Q_OBJECT для классов, наследующих QObject

public:
    explicit ConfigFile(QObject *parent = nullptr);

    // Методы для загрузки и сохранения конфигурации
    bool loadConfig();
    bool saveConfig(const QString &localPath, const QString &networkPath);

    // Методы доступа к данным конфигурации
    QString localLogsDirectory() const { return m_localLogsDirectory; }
    QString networkLogsDirectory() const { return m_networkLogsDirectory; }

private:
    QString m_localLogsDirectory;
    QString m_networkLogsDirectory;

    const QString m_configFileName = "config.json"; // Имя файла конфигурации
};

#endif // CONFIGFILE_H
