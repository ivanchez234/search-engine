#include "database.h"

#include <QAtomicInt>
#include <QDateTime>
#include <QFileInfo>
#include <QPair>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>
#include <QVariant>

namespace {

constexpr int kChunkSize = 500;
constexpr int kWordCacheLimit = 200000;

#ifdef Q_OS_WIN
constexpr Qt::CaseSensitivity kPathCaseSensitivity = Qt::CaseInsensitive;
#else
constexpr Qt::CaseSensitivity kPathCaseSensitivity = Qt::CaseSensitive;
#endif

void setError(QString *target, const QString &message)
{
    if (target)
        *target = message;
}

QString placeholders(int count)
{
    QStringList parts;
    parts.reserve(count);
    for (int i = 0; i < count; ++i)
        parts.append(QStringLiteral("?"));
    return parts.join(QStringLiteral(", "));
}

QString placeholderRows(int rowCount, int columnCount)
{
    const QString row = QLatin1Char('(') + placeholders(columnCount) + QLatin1Char(')');
    QStringList rows;
    rows.reserve(rowCount);
    for (int i = 0; i < rowCount; ++i)
        rows.append(row);
    return rows.join(QStringLiteral(", "));
}

QString makeConnectionName()
{
    static QAtomicInt counter(0);
    return QStringLiteral("se_%1_%2")
        .arg(reinterpret_cast<quintptr>(QThread::currentThread()), 0, 16)
        .arg(counter.fetchAndAddRelaxed(1));
}

}

Database::Database(const DatabaseConfig &config)
    : m_config(config)
    , m_connectionName(makeConnectionName())
{
}

Database::~Database()
{
    close();
}

QSqlDatabase Database::handle() const
{
    return QSqlDatabase::database(m_connectionName, false);
}

bool Database::isOpen() const
{
    const QSqlDatabase db = handle();
    return db.isValid() && db.isOpen();
}

bool Database::open(QString *error)
{
    if (isOpen())
        return true;

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QPSQL"))) {
        setError(error,
                 QStringLiteral("Драйвер QPSQL недоступен. Установленные драйверы: %1")
                     .arg(QSqlDatabase::drivers().join(QStringLiteral(", "))));
        return false;
    }

    bool opened = false;
    QString message;
    {
        QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QPSQL"), m_connectionName);
        db.setHostName(m_config.host);
        db.setPort(m_config.port);
        db.setDatabaseName(m_config.name);
        db.setUserName(m_config.user);
        db.setPassword(m_config.password);
        db.setConnectOptions(QStringLiteral("connect_timeout=%1").arg(m_config.connectTimeoutSec));

        opened = db.open();
        if (!opened)
            message = db.lastError().text().trimmed();
    }

    if (!opened) {
        QSqlDatabase::removeDatabase(m_connectionName);
        setError(error,
                 QStringLiteral("Не удалось подключиться к базе данных %1@%2:%3 — %4")
                     .arg(m_config.name, m_config.host)
                     .arg(m_config.port)
                     .arg(message));
        return false;
    }
    return true;
}

void Database::close()
{
    if (m_connectionName.isEmpty())
        return;

    m_wordIds.clear();
    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName, false);
        if (db.isValid() && db.isOpen())
            db.close();
    }
    QSqlDatabase::removeDatabase(m_connectionName);
    m_connectionName.clear();
}

bool Database::ensureSchema(QString *error)
{
    static const QStringList statements{
        QStringLiteral("CREATE TABLE IF NOT EXISTS documents ("
                       "  id           BIGSERIAL   PRIMARY KEY,"
                       "  path         TEXT        NOT NULL UNIQUE,"
                       "  file_name    TEXT        NOT NULL,"
                       "  file_size    BIGINT      NOT NULL DEFAULT 0,"
                       "  modified_at  TIMESTAMPTZ,"
                       "  indexed_at   TIMESTAMPTZ NOT NULL DEFAULT now(),"
                       "  total_words  INTEGER     NOT NULL DEFAULT 0,"
                       "  unique_words INTEGER     NOT NULL DEFAULT 0)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS words ("
                       "  id   BIGSERIAL   PRIMARY KEY,"
                       "  word VARCHAR(32) NOT NULL UNIQUE)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS document_words ("
                       "  document_id BIGINT  NOT NULL REFERENCES documents (id) ON DELETE CASCADE,"
                       "  word_id     BIGINT  NOT NULL REFERENCES words (id)     ON DELETE CASCADE,"
                       "  frequency   INTEGER NOT NULL CHECK (frequency > 0),"
                       "  CONSTRAINT pk_document_words PRIMARY KEY (document_id, word_id))"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_document_words_word_id"
                       "  ON document_words (word_id)")};

    QSqlDatabase db = handle();
    if (!db.isOpen()) {
        setError(error, QStringLiteral("Нет соединения с базой данных."));
        return false;
    }

    if (!db.transaction()) {
        setError(error,
                 QStringLiteral("Не удалось начать транзакцию: %1").arg(db.lastError().text().trimmed()));
        return false;
    }

    for (const QString &statement : statements) {
        QSqlQuery query(db);
        if (!query.exec(statement)) {
            const QString message = query.lastError().text().trimmed();
            db.rollback();
            setError(error, QStringLiteral("Не удалось создать структуру базы данных: %1").arg(message));
            return false;
        }
    }

    if (!db.commit()) {
        const QString message = db.lastError().text().trimmed();
        db.rollback();
        setError(error, QStringLiteral("Не удалось сохранить структуру базы данных: %1").arg(message));
        return false;
    }
    return true;
}

bool Database::tablesExist(bool *exists, QString *error)
{
    if (exists)
        *exists = false;

    QSqlDatabase db = handle();
    if (!db.isOpen()) {
        setError(error, QStringLiteral("Нет соединения с базой данных."));
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM information_schema.tables"
                                   " WHERE table_schema = current_schema()"
                                   "   AND table_name IN ('documents', 'words', 'document_words')"))
        || !query.next()) {
        setError(error,
                 QStringLiteral("Не удалось проверить структуру базы данных: %1")
                     .arg(query.lastError().text().trimmed()));
        return false;
    }

    if (exists)
        *exists = (query.value(0).toInt() == 3);
    return true;
}

bool Database::beginTransaction(QString *error)
{
    QSqlDatabase db = handle();
    if (!db.isOpen()) {
        setError(error, QStringLiteral("Нет соединения с базой данных."));
        return false;
    }
    if (!db.transaction()) {
        setError(error,
                 QStringLiteral("Не удалось начать транзакцию: %1").arg(db.lastError().text().trimmed()));
        return false;
    }
    return true;
}

bool Database::commitTransaction(QString *error)
{
    QSqlDatabase db = handle();
    if (!db.isOpen()) {
        setError(error, QStringLiteral("Нет соединения с базой данных."));
        return false;
    }
    if (!db.commit()) {
        setError(error,
                 QStringLiteral("Не удалось завершить транзакцию: %1").arg(db.lastError().text().trimmed()));
        return false;
    }
    return true;
}

void Database::rollbackTransaction()
{
    QSqlDatabase db = handle();
    if (db.isValid() && db.isOpen())
        db.rollback();

    // слова, добавленные в откаченной транзакции, из базы исчезли.
    // не сбросим кэш — следующие вставки сошлются на несуществующие id
    m_wordIds.clear();
}

bool Database::resolveWordIds(const QStringList &words, QString *error)
{
    if (m_wordIds.size() > kWordCacheLimit)
        m_wordIds.clear();

    QStringList missing;
    missing.reserve(words.size());
    for (const QString &word : words) {
        if (!m_wordIds.contains(word))
            missing.append(word);
    }
    if (missing.isEmpty())
        return true;

    QSqlDatabase db = handle();
    const int total = static_cast<int>(missing.size());

    for (int offset = 0; offset < total; offset += kChunkSize) {
        const int count = qMin(kChunkSize, total - offset);

        {
            QSqlQuery query(db);
            query.prepare(QStringLiteral("INSERT INTO words (word) VALUES %1 ON CONFLICT (word) DO NOTHING")
                              .arg(placeholderRows(count, 1)));
            for (int i = 0; i < count; ++i)
                query.addBindValue(missing.at(offset + i));

            if (!query.exec()) {
                setError(error,
                         QStringLiteral("Не удалось добавить слова в словарь: %1")
                             .arg(query.lastError().text().trimmed()));
                return false;
            }
        }

        {
            QSqlQuery query(db);
            query.prepare(QStringLiteral("SELECT id, word FROM words WHERE word IN (%1)")
                              .arg(placeholders(count)));
            for (int i = 0; i < count; ++i)
                query.addBindValue(missing.at(offset + i));

            if (!query.exec()) {
                setError(error,
                         QStringLiteral("Не удалось получить идентификаторы слов: %1")
                             .arg(query.lastError().text().trimmed()));
                return false;
            }
            while (query.next())
                m_wordIds.insert(query.value(1).toString(), query.value(0).toLongLong());
        }
    }
    return true;
}

bool Database::saveDocument(const ParsedDocument &document, QString *error)
{
    QSqlDatabase db = handle();
    if (!db.isOpen()) {
        setError(error, QStringLiteral("Нет соединения с базой данных."));
        return false;
    }

    qint64 documentId = -1;
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "INSERT INTO documents (path, file_name, file_size, modified_at, total_words, unique_words, indexed_at)"
            " VALUES (?, ?, ?, ?, ?, ?, now())"
            " ON CONFLICT (path) DO UPDATE SET"
            "   file_name    = EXCLUDED.file_name,"
            "   file_size    = EXCLUDED.file_size,"
            "   modified_at  = EXCLUDED.modified_at,"
            "   total_words  = EXCLUDED.total_words,"
            "   unique_words = EXCLUDED.unique_words,"
            "   indexed_at   = now()"
            " RETURNING id"));
        query.addBindValue(document.path);
        query.addBindValue(QFileInfo(document.path).fileName());
        query.addBindValue(document.size);
        query.addBindValue(document.modified);
        query.addBindValue(document.totalWords);
        query.addBindValue(static_cast<int>(document.frequencies.size()));

        if (!query.exec() || !query.next()) {
            setError(error,
                     QStringLiteral("Не удалось сохранить документ «%1»: %2")
                         .arg(document.path, query.lastError().text().trimmed()));
            return false;
        }
        documentId = query.value(0).toLongLong();
    }

    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral("DELETE FROM document_words WHERE document_id = ?"));
        query.addBindValue(documentId);
        if (!query.exec()) {
            setError(error,
                     QStringLiteral("Не удалось очистить прежний индекс документа «%1»: %2")
                         .arg(document.path, query.lastError().text().trimmed()));
            return false;
        }
    }

    if (document.frequencies.isEmpty())
        return true;

    const QStringList words = document.frequencies.keys();
    if (!resolveWordIds(words, error))
        return false;

    QVector<QPair<qint64, int>> links;
    links.reserve(static_cast<int>(document.frequencies.size()));
    for (auto it = document.frequencies.constBegin(); it != document.frequencies.constEnd(); ++it) {
        const qint64 wordId = m_wordIds.value(it.key(), -1);
        if (wordId < 0) {
            setError(error, QStringLiteral("Не удалось определить идентификатор слова «%1».").arg(it.key()));
            return false;
        }
        links.append(qMakePair(wordId, it.value()));
    }

    const int total = static_cast<int>(links.size());
    for (int offset = 0; offset < total; offset += kChunkSize) {
        const int count = qMin(kChunkSize, total - offset);

        QSqlQuery query(db);
        query.prepare(QStringLiteral("INSERT INTO document_words (document_id, word_id, frequency)"
                                     " VALUES %1"
                                     " ON CONFLICT (document_id, word_id)"
                                     " DO UPDATE SET frequency = EXCLUDED.frequency")
                          .arg(placeholderRows(count, 3)));
        for (int i = 0; i < count; ++i) {
            query.addBindValue(documentId);
            query.addBindValue(links.at(offset + i).first);
            query.addBindValue(links.at(offset + i).second);
        }

        if (!query.exec()) {
            setError(error,
                     QStringLiteral("Не удалось сохранить частоты слов документа «%1»: %2")
                         .arg(document.path, query.lastError().text().trimmed()));
            return false;
        }
    }
    return true;
}

bool Database::pruneMissingDocuments(const QStringList &roots, int *removedCount, QString *error)
{
    if (removedCount)
        *removedCount = 0;

    QSqlDatabase db = handle();
    if (!db.isOpen()) {
        setError(error, QStringLiteral("Нет соединения с базой данных."));
        return false;
    }

    QVector<qint64> stale;
    {
        QSqlQuery query(db);
        if (!query.exec(QStringLiteral("SELECT id, path FROM documents"))) {
            setError(error,
                     QStringLiteral("Не удалось получить список документов: %1")
                         .arg(query.lastError().text().trimmed()));
            return false;
        }
        while (query.next()) {
            const qint64 id = query.value(0).toLongLong();
            const QString path = query.value(1).toString();

            if (!roots.isEmpty()) {
                bool insideRoot = false;
                for (const QString &root : roots) {
                    if (path.startsWith(root, kPathCaseSensitivity)) {
                        insideRoot = true;
                        break;
                    }
                }
                if (!insideRoot)
                    continue;
            }

            if (!QFileInfo::exists(path))
                stale.append(id);
        }
    }

    if (stale.isEmpty())
        return true;

    const int total = static_cast<int>(stale.size());
    for (int offset = 0; offset < total; offset += kChunkSize) {
        const int count = qMin(kChunkSize, total - offset);

        QSqlQuery query(db);
        query.prepare(
            QStringLiteral("DELETE FROM documents WHERE id IN (%1)").arg(placeholders(count)));
        for (int i = 0; i < count; ++i)
            query.addBindValue(stale.at(offset + i));

        if (!query.exec()) {
            setError(error,
                     QStringLiteral("Не удалось удалить устаревшие документы: %1")
                         .arg(query.lastError().text().trimmed()));
            return false;
        }
    }

    if (removedCount)
        *removedCount = total;
    return true;
}

bool Database::searchDocuments(const QStringList &words, int limit, QVector<DocumentHit> *hits,
                               QString *error)
{
    if (!hits) {
        setError(error, QStringLiteral("Не задан приёмник результатов поиска."));
        return false;
    }
    hits->clear();

    if (words.isEmpty())
        return true;

    QSqlDatabase db = handle();
    if (!db.isOpen()) {
        setError(error, QStringLiteral("Нет соединения с базой данных."));
        return false;
    }

    const int wordCount = static_cast<int>(words.size());

    QString sql = QStringLiteral("SELECT d.path,"
                                 "       d.file_name,"
                                 "       SUM(dw.frequency) AS relevance,"
                                 "       COUNT(DISTINCT w.id) AS matched_words"
                                 "  FROM document_words dw"
                                 "  JOIN words     w ON w.id = dw.word_id"
                                 "  JOIN documents d ON d.id = dw.document_id"
                                 " WHERE w.word IN (%1)"
                                 " GROUP BY d.id, d.path, d.file_name"
                                 " HAVING COUNT(DISTINCT w.id) = %2"
                                 " ORDER BY relevance DESC, d.file_name ASC")
                      .arg(placeholders(wordCount))
                      .arg(wordCount);
    if (limit > 0)
        sql += QStringLiteral(" LIMIT %1").arg(limit);

    QSqlQuery query(db);
    query.prepare(sql);
    for (const QString &word : words)
        query.addBindValue(word);

    if (!query.exec()) {
        setError(error,
                 QStringLiteral("Не удалось выполнить поисковый запрос: %1")
                     .arg(query.lastError().text().trimmed()));
        return false;
    }

    while (query.next()) {
        DocumentHit hit;
        hit.path = query.value(0).toString();
        hit.fileName = query.value(1).toString();
        hit.relevance = query.value(2).toLongLong();
        hit.matchedWords = query.value(3).toInt();
        hits->append(hit);
    }
    return true;
}

bool Database::wordStatistics(QVector<WordStat> *stats, QString *error)
{
    if (!stats) {
        setError(error, QStringLiteral("Не задан приёмник статистики."));
        return false;
    }
    stats->clear();

    QSqlDatabase db = handle();
    if (!db.isOpen()) {
        setError(error, QStringLiteral("Нет соединения с базой данных."));
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT w.word,"
                                   "       SUM(dw.frequency) AS total,"
                                   "       COUNT(dw.document_id) AS documents"
                                   "  FROM words w"
                                   "  JOIN document_words dw ON dw.word_id = w.id"
                                   " GROUP BY w.id, w.word"
                                   " ORDER BY total DESC, w.word ASC"))) {
        setError(error,
                 QStringLiteral("Не удалось получить список слов: %1")
                     .arg(query.lastError().text().trimmed()));
        return false;
    }

    while (query.next()) {
        WordStat stat;
        stat.word = query.value(0).toString();
        stat.totalFrequency = query.value(1).toLongLong();
        stat.documentCount = query.value(2).toInt();
        stats->append(stat);
    }
    return true;
}

bool Database::counters(qint64 *documents, qint64 *words, QString *error)
{
    QSqlDatabase db = handle();
    if (!db.isOpen()) {
        setError(error, QStringLiteral("Нет соединения с базой данных."));
        return false;
    }

    QSqlQuery query(db);
    if (!query.exec(QStringLiteral("SELECT (SELECT COUNT(*) FROM documents),"
                                   "       (SELECT COUNT(*) FROM words)"))
        || !query.next()) {
        setError(error,
                 QStringLiteral("Не удалось получить статистику базы данных: %1")
                     .arg(query.lastError().text().trimmed()));
        return false;
    }

    if (documents)
        *documents = query.value(0).toLongLong();
    if (words)
        *words = query.value(1).toLongLong();
    return true;
}
