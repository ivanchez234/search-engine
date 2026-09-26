#ifndef SEARCHWORKER_H
#define SEARCHWORKER_H

#include "appconfig.h"
#include "searchtypes.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

class Database;

class SearchWorker : public QObject
{
    Q_OBJECT

public:
    explicit SearchWorker(DatabaseConfig config, QObject *parent = nullptr);
    ~SearchWorker() override;

public slots:
    void initialize();

    void search(const QStringList &words, int limit);
    void loadWordStatistics();

signals:
    void initialized(bool success, const QString &error);

    void searchFinished(const QVector<DocumentHit> &hits, const QStringList &words);

    void wordStatisticsReady(const QVector<WordStat> &stats);
    void countersReady(qint64 documents, qint64 words);
    void failed(const QString &message);

private:
    bool ensureReady();

    DatabaseConfig m_config;
    std::unique_ptr<Database> m_database;
};

#endif // SEARCHWORKER_H
