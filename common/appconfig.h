#ifndef APPCONFIG_H
#define APPCONFIG_H

#include <QString>
#include <QStringList>

class QSettings;

struct DatabaseConfig
{
    QString host = QStringLiteral("localhost");
    int port = 5432;
    QString name = QStringLiteral("search_engine");
    QString user = QStringLiteral("postgres");
    QString password;
    int connectTimeoutSec = 10;
};

struct IndexerConfig
{
    QStringList directories;
    QStringList extensions;
    int threadCount = 0;
    int batchSize = 64;
    qint64 maxFileSizeBytes = 32LL * 1024 * 1024;
    bool pruneMissing = true;
    bool followSymlinks = false;
};

struct SearchConfig
{
    int maxQueryWords = 4;
    int maxResults = 10;
};

class AppConfig
{
public:
    explicit AppConfig(QString filePath);

    bool load(QString *errorMessage = nullptr);

    QString filePath() const { return m_filePath; }
    const DatabaseConfig &database() const { return m_database; }
    const IndexerConfig &indexer() const { return m_indexer; }
    const SearchConfig &search() const { return m_search; }

    static QString locate(const QString &fileName, const QString &explicitPath = QString());

private:
    bool readDatabase(QSettings &settings, QString *errorMessage);
    void readIndexer(QSettings &settings);
    void readSearch(QSettings &settings);

    QString m_filePath;
    DatabaseConfig m_database;
    IndexerConfig m_indexer;
    SearchConfig m_search;
};

#endif // APPCONFIG_H
