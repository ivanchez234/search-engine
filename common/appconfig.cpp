#include "appconfig.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSettings>
#include <QVariant>
#include <QtGlobal>

#include <utility>

namespace {

void setError(QString *target, const QString &message)
{
    if (target)
        *target = message;
}

QStringList splitList(const QVariant &value)
{
    static const QRegularExpression separator(QStringLiteral("[,;]"));

    QStringList result;
    const QStringList raw = value.toStringList();
    for (const QString &chunk : raw) {
        const QStringList parts = chunk.split(separator, Qt::SkipEmptyParts);
        for (const QString &part : parts) {
            const QString trimmed = part.trimmed();
            if (!trimmed.isEmpty())
                result.append(trimmed);
        }
    }
    return result;
}

}

AppConfig::AppConfig(QString filePath)
    : m_filePath(std::move(filePath))
{
}

bool AppConfig::load(QString *errorMessage)
{
    if (m_filePath.isEmpty()) {
        setError(errorMessage, QStringLiteral("Не задан путь к файлу конфигурации."));
        return false;
    }

    if (!QFileInfo::exists(m_filePath)) {
        setError(errorMessage,
                 QStringLiteral("Файл конфигурации не найден: %1").arg(QDir::toNativeSeparators(m_filePath)));
        return false;
    }

    QSettings settings(m_filePath, QSettings::IniFormat);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    settings.setIniCodec("UTF-8");
#endif
    if (settings.status() != QSettings::NoError) {
        setError(errorMessage,
                 QStringLiteral("Не удалось прочитать файл конфигурации: %1")
                     .arg(QDir::toNativeSeparators(m_filePath)));
        return false;
    }

    if (!readDatabase(settings, errorMessage))
        return false;

    readIndexer(settings);
    readSearch(settings);
    return true;
}

bool AppConfig::readDatabase(QSettings &settings, QString *errorMessage)
{
    settings.beginGroup(QStringLiteral("Database"));
    m_database.host = settings.value(QStringLiteral("host"), m_database.host).toString().trimmed();
    m_database.port = settings.value(QStringLiteral("port"), m_database.port).toInt();
    m_database.name = settings.value(QStringLiteral("database"),
                                     settings.value(QStringLiteral("name"), m_database.name))
                          .toString()
                          .trimmed();
    m_database.user = settings.value(QStringLiteral("user"), m_database.user).toString().trimmed();
    m_database.password = settings.value(QStringLiteral("password")).toString();
    m_database.connectTimeoutSec =
        settings.value(QStringLiteral("connectTimeout"), m_database.connectTimeoutSec).toInt();
    settings.endGroup();

    if (m_database.host.isEmpty()) {
        setError(errorMessage, QStringLiteral("В секции [Database] не задан параметр host."));
        return false;
    }
    if (m_database.port < 1 || m_database.port > 65535) {
        setError(errorMessage,
                 QStringLiteral("В секции [Database] указан недопустимый порт: %1").arg(m_database.port));
        return false;
    }
    if (m_database.name.isEmpty()) {
        setError(errorMessage, QStringLiteral("В секции [Database] не задано имя базы данных (database)."));
        return false;
    }
    if (m_database.user.isEmpty()) {
        setError(errorMessage, QStringLiteral("В секции [Database] не задано имя пользователя (user)."));
        return false;
    }
    m_database.connectTimeoutSec = qBound(1, m_database.connectTimeoutSec, 600);
    return true;
}

void AppConfig::readIndexer(QSettings &settings)
{
    settings.beginGroup(QStringLiteral("Indexer"));

    m_indexer.directories.clear();
    const QStringList directories = splitList(settings.value(QStringLiteral("directories")));
    for (const QString &directory : directories) {
        const QString normalized = QDir::fromNativeSeparators(directory);
        if (!normalized.isEmpty())
            m_indexer.directories.append(QDir::cleanPath(normalized));
    }

    m_indexer.extensions.clear();
    QStringList extensions = splitList(settings.value(QStringLiteral("extensions")));
    if (extensions.isEmpty())
        extensions = QStringList{QStringLiteral("txt"), QStringLiteral("md"),
                                 QStringLiteral("ini"), QStringLiteral("csv")};
    for (QString extension : extensions) {
        while (extension.startsWith(QLatin1Char('.')) || extension.startsWith(QLatin1Char('*')))
            extension.remove(0, 1);
        extension = extension.trimmed().toLower();
        if (!extension.isEmpty() && !m_indexer.extensions.contains(extension))
            m_indexer.extensions.append(extension);
    }

    m_indexer.threadCount = qBound(0, settings.value(QStringLiteral("threads"), 0).toInt(), 256);
    m_indexer.batchSize = qBound(1, settings.value(QStringLiteral("batchSize"), 64).toInt(), 10000);

    const int maxFileSizeMb = qBound(1, settings.value(QStringLiteral("maxFileSizeMB"), 32).toInt(), 4096);
    m_indexer.maxFileSizeBytes = static_cast<qint64>(maxFileSizeMb) * 1024 * 1024;

    m_indexer.pruneMissing = settings.value(QStringLiteral("pruneMissing"), true).toBool();
    m_indexer.followSymlinks = settings.value(QStringLiteral("followSymlinks"), false).toBool();

    settings.endGroup();
}

void AppConfig::readSearch(QSettings &settings)
{
    settings.beginGroup(QStringLiteral("Search"));
    m_search.maxQueryWords = qBound(1, settings.value(QStringLiteral("maxQueryWords"), 4).toInt(), 16);
    m_search.maxResults = qBound(1, settings.value(QStringLiteral("maxResults"), 10).toInt(), 1000);
    settings.endGroup();
}

QString AppConfig::locate(const QString &fileName, const QString &explicitPath)
{
    if (!explicitPath.isEmpty())
        return QDir::cleanPath(QDir::fromNativeSeparators(explicitPath));

    const QStringList candidates{QCoreApplication::applicationDirPath() + QLatin1Char('/') + fileName,
                                 QDir::current().absoluteFilePath(fileName)};

    for (const QString &candidate : candidates) {
        if (QFileInfo::exists(candidate))
            return QDir::cleanPath(candidate);
    }
    return QDir::cleanPath(candidates.first());
}
