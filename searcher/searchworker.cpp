#include "searchworker.h"

#include "database.h"

#include <exception>
#include <utility>

SearchWorker::SearchWorker(DatabaseConfig config, QObject *parent)
    : QObject(parent)
    , m_config(std::move(config))
{
}

SearchWorker::~SearchWorker() = default;

bool SearchWorker::ensureReady()
{
    if (m_database && m_database->isOpen())
        return true;

    emit failed(QStringLiteral("Нет соединения с базой данных. "
                               "Проверьте настройки и перезапустите программу."));
    return false;
}

void SearchWorker::initialize()
{
    try {
        auto database = std::make_unique<Database>(m_config);

        QString error;
        if (!database->open(&error)) {
            emit initialized(false, error);
            return;
        }

        bool schemaReady = false;
        if (!database->tablesExist(&schemaReady, &error)) {
            emit initialized(false, error);
            return;
        }
        if (!schemaReady) {
            emit initialized(false,
                             QStringLiteral("В базе данных «%1» нет таблиц поискового индекса.\n"
                                            "Сначала запустите программу «Индексатор».")
                                 .arg(m_config.name));
            return;
        }

        m_database = std::move(database);
        emit initialized(true, QString());

        qint64 documents = 0;
        qint64 words = 0;
        if (m_database->counters(&documents, &words, &error))
            emit countersReady(documents, words);
    } catch (const std::exception &exception) {
        emit initialized(false, QString::fromUtf8(exception.what()));
    } catch (...) {
        emit initialized(false, QStringLiteral("Непредвиденная ошибка при подключении к базе данных."));
    }
}

void SearchWorker::search(const QStringList &words, int limit)
{
    if (!ensureReady())
        return;

    try {
        QVector<DocumentHit> hits;
        QString error;
        if (!m_database->searchDocuments(words, limit, &hits, &error)) {
            emit failed(error);
            return;
        }
        emit searchFinished(hits, words);
    } catch (const std::exception &exception) {
        emit failed(QString::fromUtf8(exception.what()));
    } catch (...) {
        emit failed(QStringLiteral("Непредвиденная ошибка при выполнении поиска."));
    }
}

void SearchWorker::loadWordStatistics()
{
    if (!ensureReady())
        return;

    try {
        QVector<WordStat> stats;
        QString error;
        if (!m_database->wordStatistics(&stats, &error)) {
            emit failed(error);
            return;
        }
        emit wordStatisticsReady(stats);

        qint64 documents = 0;
        qint64 words = 0;
        if (m_database->counters(&documents, &words, &error))
            emit countersReady(documents, words);
    } catch (const std::exception &exception) {
        emit failed(QString::fromUtf8(exception.what()));
    } catch (...) {
        emit failed(QStringLiteral("Непредвиденная ошибка при чтении списка слов."));
    }
}
