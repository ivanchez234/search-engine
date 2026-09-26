#include "fileindexer.h"

#include "database.h"
#include "textprocessor.h"

#include <QByteArray>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QSet>
#include <QTextStream>
#include <QThread>
#include <QThreadPool>
#include <QtConcurrent>

#include <exception>
#include <utility>

namespace {

constexpr int kBinaryProbeSize = 4096;

void setError(QString *target, const QString &message)
{
    if (target)
        *target = message;
}

void printLine(const QString &text)
{
    QTextStream out(stdout);
    out << text << '\n';
    out.flush();
}

} // namespace

FileIndexer::FileIndexer(Database &database, IndexerConfig config)
    : m_database(database)
    , m_config(std::move(config))
{
}

QStringList FileIndexer::absoluteRoots() const
{
    QStringList roots;
    for (const QString &directory : m_config.directories) {
        const QDir dir(directory);
        if (dir.exists())
            roots.append(dir.absolutePath());
    }
    return roots;
}

QStringList FileIndexer::collectFiles() const
{
    const QSet<QString> extensions(m_config.extensions.cbegin(), m_config.extensions.cend());

    QSet<QString> visited;
    QStringList files;

    for (const QString &directory : m_config.directories) {
        const QDir root(directory);
        if (!root.exists()) {
            printLine(QStringLiteral("Внимание: каталог не найден, пропущен — %1")
                          .arg(QDir::toNativeSeparators(directory)));
            continue;
        }

        QDirIterator::IteratorFlags flags = QDirIterator::Subdirectories;
        if (m_config.followSymlinks)
            flags |= QDirIterator::FollowSymlinks;

        QDirIterator iterator(root.absolutePath(), QDir::Files | QDir::Readable, flags);
        while (iterator.hasNext()) {
            iterator.next();
            const QFileInfo info = iterator.fileInfo();

            if (!extensions.contains(info.suffix().toLower()))
                continue;

            const QString absolutePath = info.absoluteFilePath();
            if (visited.contains(absolutePath))
                continue;

            visited.insert(absolutePath);
            files.append(absolutePath);
        }
    }

    files.sort();
    return files;
}

ParsedDocument FileIndexer::parseFile(const ParseTask &task)
{
    ParsedDocument document;
    document.path = task.path;

    try {
        const QFileInfo info(task.path);
        document.size = info.size();
        document.modified = info.lastModified();

        if (task.maxFileSize > 0 && document.size > task.maxFileSize) {
            document.error = QStringLiteral("размер превышает допустимый");
            return document;
        }

        QFile file(task.path);
        if (!file.open(QIODevice::ReadOnly)) {
            document.error = file.errorString();
            return document;
        }

        const QByteArray raw = file.readAll();
        if (file.error() != QFileDevice::NoError) {
            document.error = file.errorString();
            return document;
        }
        file.close();

        if (raw.left(kBinaryProbeSize).contains('\0')) {
            document.error = QStringLiteral("файл не является текстовым");
            return document;
        }

        const QString text = QString::fromUtf8(raw);
        document.frequencies = TextProcessor::frequencies(text, &document.totalWords);
        document.ok = true;
    } catch (const std::exception &exception) {
        document.ok = false;
        document.error = QString::fromUtf8(exception.what());
    } catch (...) {
        document.ok = false;
        document.error = QStringLiteral("неизвестная ошибка при разборе файла");
    }

    return document;
}

bool FileIndexer::storeBatch(const QVector<ParsedDocument> &documents, QString *error)
{
    if (documents.isEmpty())
        return true;

    QString message;
    if (!m_database.beginTransaction(&message)) {
        setError(error, message);
        return false;
    }

    bool ok = true;
    for (const ParsedDocument &document : documents) {
        if (!m_database.saveDocument(document, &message)) {
            ok = false;
            break;
        }
    }

    if (ok && m_database.commitTransaction(&message)) {
        m_stats.filesIndexed += static_cast<int>(documents.size());
        for (const ParsedDocument &document : documents)
            m_stats.wordsIndexed += document.totalWords;
        return true;
    }

    // postgres валит всю транзакцию на первой ошибке, поэтому откатываем
    // пачку и прогоняем её по одному — иначе из-за одного кривого файла
    // потеряются все остальные
    m_database.rollbackTransaction();
    printLine(QStringLiteral("Ошибка при записи порции (%1), повтор по одному файлу.").arg(message));

    for (const ParsedDocument &document : documents) {
        QString singleError;
        if (!m_database.beginTransaction(&singleError)) {
            setError(error, singleError);
            return false;
        }

        if (m_database.saveDocument(document, &singleError)
            && m_database.commitTransaction(&singleError)) {
            ++m_stats.filesIndexed;
            m_stats.wordsIndexed += document.totalWords;
        } else {
            m_database.rollbackTransaction();
            ++m_stats.filesFailed;
            printLine(QStringLiteral("  не сохранён: %1 (%2)")
                          .arg(QDir::toNativeSeparators(document.path), singleError));
        }
    }
    return true;
}

bool FileIndexer::run(QString *error)
{
    QElapsedTimer timer;
    timer.start();
    m_stats = Statistics();

    printLine(QStringLiteral("Каталоги: %1").arg(m_config.directories.join(QStringLiteral(", "))));
    printLine(QStringLiteral("Расширения: %1").arg(m_config.extensions.join(QStringLiteral(", "))));

    const QStringList files = collectFiles();
    m_stats.filesFound = static_cast<int>(files.size());

    if (files.isEmpty()) {
        printLine(QStringLiteral("Подходящих файлов не найдено."));
        m_stats.elapsedMs = timer.elapsed();
        return true;
    }

    const int threadCount =
        qMax(1, m_config.threadCount > 0 ? m_config.threadCount : QThread::idealThreadCount());
    QThreadPool::globalInstance()->setMaxThreadCount(threadCount);

    printLine(QStringLiteral("Найдено файлов: %1, рабочих потоков: %2").arg(m_stats.filesFound).arg(threadCount));

    const int total = m_stats.filesFound;
    for (int offset = 0; offset < total; offset += m_config.batchSize) {
        const int count = qMin(m_config.batchSize, total - offset);

        QVector<ParseTask> tasks;
        tasks.reserve(count);
        for (int i = 0; i < count; ++i)
            tasks.append(ParseTask{files.at(offset + i), m_config.maxFileSizeBytes});

        const QVector<ParsedDocument> parsed =
            QtConcurrent::blockingMapped<QVector<ParsedDocument>>(tasks, &FileIndexer::parseFile);

        QVector<ParsedDocument> ready;
        ready.reserve(parsed.size());
        for (const ParsedDocument &document : parsed) {
            if (document.ok) {
                ready.append(document);
            } else {
                ++m_stats.filesSkipped;
                printLine(QStringLiteral("  пропущен: %1 (%2)")
                              .arg(QDir::toNativeSeparators(document.path), document.error));
            }
        }

        QString message;
        if (!storeBatch(ready, &message)) {
            setError(error, message);
            m_stats.elapsedMs = timer.elapsed();
            return false;
        }

        printLine(QStringLiteral("[%1/%2] обработано").arg(offset + count).arg(total));
    }

    if (m_config.pruneMissing) {
        int removed = 0;
        QString message;
        if (m_database.pruneMissingDocuments(absoluteRoots(), &removed, &message))
            m_stats.documentsRemoved = removed;
        else
            printLine(QStringLiteral("Внимание: не удалось удалить устаревшие записи (%1).").arg(message));
    }

    m_stats.elapsedMs = timer.elapsed();
    return true;
}
