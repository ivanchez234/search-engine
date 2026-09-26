#ifndef FILEINDEXER_H
#define FILEINDEXER_H

#include "appconfig.h"
#include "searchtypes.h"

#include <QStringList>
#include <QVector>

class Database;

class FileIndexer
{
public:
    struct Statistics
    {
        int filesFound = 0;
        int filesIndexed = 0;
        int filesSkipped = 0;
        int filesFailed = 0;
        qint64 wordsIndexed = 0;
        int documentsRemoved = 0;
        qint64 elapsedMs = 0;
    };

    FileIndexer(Database &database, IndexerConfig config);

    bool run(QString *error = nullptr);

    const Statistics &statistics() const { return m_stats; }

private:
    struct ParseTask
    {
        QString path;
        qint64 maxFileSize = 0;
    };

    QStringList collectFiles() const;
    QStringList absoluteRoots() const;
    bool storeBatch(const QVector<ParsedDocument> &documents, QString *error);

    static ParsedDocument parseFile(const ParseTask &task);

    Database &m_database;
    IndexerConfig m_config;
    Statistics m_stats;
};

#endif // FILEINDEXER_H
