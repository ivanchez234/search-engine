#ifndef DATABASE_H
#define DATABASE_H

#include "appconfig.h"
#include "searchtypes.h"

#include <QHash>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

class QSqlDatabase;

// одно соединение = один поток, так требует QtSql.
// нужен доступ из другого потока — заводи там свой экземпляр
class Database
{
    Q_DISABLE_COPY_MOVE(Database)

public:
    explicit Database(const DatabaseConfig &config);
    ~Database();

    bool open(QString *error = nullptr);
    void close();
    bool isOpen() const;

    bool ensureSchema(QString *error = nullptr);

    bool tablesExist(bool *exists, QString *error = nullptr);

    bool beginTransaction(QString *error = nullptr);
    bool commitTransaction(QString *error = nullptr);
    void rollbackTransaction();

    bool saveDocument(const ParsedDocument &document, QString *error = nullptr);

    bool pruneMissingDocuments(const QStringList &roots, int *removedCount = nullptr,
                               QString *error = nullptr);

    bool searchDocuments(const QStringList &words, int limit, QVector<DocumentHit> *hits,
                         QString *error = nullptr);

    bool wordStatistics(QVector<WordStat> *stats, QString *error = nullptr);

    bool counters(qint64 *documents, qint64 *words, QString *error = nullptr);

private:
    QSqlDatabase handle() const;
    bool resolveWordIds(const QStringList &words, QString *error);

    DatabaseConfig m_config;
    QString m_connectionName;
    QHash<QString, qint64> m_wordIds;
};

#endif // DATABASE_H
