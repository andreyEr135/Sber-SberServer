#include "configfile.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>

ConfigFile::ConfigFile(QObject *parent)
    : QObject(parent),
      m_localLogsDirectory("received_files"), // Значения по умолчанию
      m_networkLogsDirectory("C:/NetworkLogs") // Значения по умолчанию
{
    // Конфигурация будет загружена в SberServer::SberServer
}

bool ConfigFile::loadConfig()
{
    QFile configFile(m_configFileName);
    if (!configFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "ConfigFile: config.json не найден или не может быть открыт для чтения. Используем значения по умолчанию и сохраняем их.";
        // Если файл не существует, сохраняем значения по умолчанию
        return saveConfig(m_localLogsDirectory, m_networkLogsDirectory);
    }

    QByteArray jsonData = configFile.readAll();
    configFile.close();

    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (doc.isNull()) {
        qDebug() << "ConfigFile: Ошибка парсинга config.json. Используем значения по умолчанию и перезаписываем файл.";
        return saveConfig(m_localLogsDirectory, m_networkLogsDirectory);
    }

    QJsonObject obj = doc.object();

    if (obj.contains("localLogsDirectory") && obj["localLogsDirectory"].isString()) {
        m_localLogsDirectory = obj["localLogsDirectory"].toString();
    } else {
        qDebug() << "ConfigFile: Ключ 'localLogsDirectory' не найден или не является строкой в config.json. Используем значение по умолчанию.";
    }

    if (obj.contains("networkLogsDirectory") && obj["networkLogsDirectory"].isString()) {
        m_networkLogsDirectory = obj["networkLogsDirectory"].toString();
    } else {
        qDebug() << "ConfigFile: Ключ 'networkLogsDirectory' не найден или не является строкой в config.json. Используем значение по умолчанию.";
    }
    qDebug() << "ConfigFile: Конфигурация успешно загружена.";
    return true;
}

bool ConfigFile::saveConfig(const QString &localPath, const QString &networkPath)
{
    QJsonObject config;
    config["localLogsDirectory"] = localPath;
    config["networkLogsDirectory"] = networkPath;

    QJsonDocument doc(config);
    QFile configFile(m_configFileName);
    if (!configFile.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        qDebug() << "ConfigFile: Не удалось открыть config.json для записи: " << configFile.errorString();
        return false;
    }

    configFile.write(doc.toJson(QJsonDocument::Indented));
    configFile.close();

    m_localLogsDirectory = localPath; // Обновляем внутренние переменные при успешном сохранении
    m_networkLogsDirectory = networkPath;

    qDebug() << "ConfigFile: Конфигурация сохранена в config.json.";
    return true;
}
