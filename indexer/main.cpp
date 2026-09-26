#include "appconfig.h"
#include "database.h"
#include "fileindexer.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QTextStream>

#include <exception>

namespace {

enum ExitCode {
    ExitSuccess = 0,
    ExitConfigError = 2,
    ExitDatabaseError = 3,
    ExitIndexingError = 4,
    ExitUnexpectedError = 5
};

void printError(const QString &message)
{
    QTextStream stream(stderr);
    stream << QStringLiteral("Ошибка: ") << message << '\n';
    stream.flush();
}

void printLine(const QString &message)
{
    QTextStream stream(stdout);
    stream << message << '\n';
    stream.flush();
}

void printStatistics(const FileIndexer::Statistics &stats)
{
    printLine(QStringLiteral("Индексация завершена."));
    printLine(QStringLiteral("  найдено файлов:    %1").arg(stats.filesFound, 10));
    printLine(QStringLiteral("  проиндексировано:  %1").arg(stats.filesIndexed, 10));
    printLine(QStringLiteral("  пропущено:         %1").arg(stats.filesSkipped, 10));
    printLine(QStringLiteral("  ошибок записи:     %1").arg(stats.filesFailed, 10));
    printLine(QStringLiteral("  слов записано:     %1").arg(stats.wordsIndexed, 10));
    printLine(QStringLiteral("  удалено записей:   %1").arg(stats.documentsRemoved, 10));
    printLine(QStringLiteral("  затрачено времени: %1 с")
                  .arg(stats.elapsedMs / 1000.0, 10, 'f', 1));
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("indexer"));
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0"));

    QCommandLineParser parser;
    parser.setApplicationDescription(
        QStringLiteral("Индексатор текстовых файлов для поисковой системы."));
    parser.addHelpOption();
    parser.addVersionOption();

    const QCommandLineOption configOption(
        QStringList{QStringLiteral("c"), QStringLiteral("config")},
        QStringLiteral("Путь к ini-файлу конфигурации (по умолчанию indexer.ini рядом с программой)."),
        QStringLiteral("файл"));
    parser.addOption(configOption);
    parser.addPositionalArgument(
        QStringLiteral("каталог"),
        QStringLiteral("Каталоги для индексации. Если заданы, заменяют список из ini-файла."),
        QStringLiteral("[каталог...]"));
    parser.process(application);

    try {
        const QString configPath =
            AppConfig::locate(QStringLiteral("indexer.ini"), parser.value(configOption));
        AppConfig config(configPath);

        QString error;
        if (!config.load(&error)) {
            printError(error);
            return ExitConfigError;
        }
        printLine(QStringLiteral("Конфигурация: %1").arg(QDir::toNativeSeparators(config.filePath())));

        IndexerConfig indexerConfig = config.indexer();
        const QStringList arguments = parser.positionalArguments();
        if (!arguments.isEmpty()) {
            indexerConfig.directories.clear();
            for (const QString &argument : arguments)
                indexerConfig.directories.append(QDir::cleanPath(QDir::fromNativeSeparators(argument)));
        }
        if (indexerConfig.directories.isEmpty()) {
            printError(QStringLiteral("Не задан ни один каталог для индексации: укажите параметр "
                                      "directories в секции [Indexer] или передайте путь аргументом."));
            return ExitConfigError;
        }

        Database database(config.database());
        if (!database.open(&error)) {
            printError(error);
            return ExitDatabaseError;
        }
        if (!database.ensureSchema(&error)) {
            printError(error);
            return ExitDatabaseError;
        }

        FileIndexer indexer(database, indexerConfig);
        if (!indexer.run(&error)) {
            printError(error);
            return ExitIndexingError;
        }
        printStatistics(indexer.statistics());

        qint64 documents = 0;
        qint64 words = 0;
        if (database.counters(&documents, &words, &error))
            printLine(QStringLiteral("В базе данных: документов — %1, уникальных слов — %2")
                          .arg(documents)
                          .arg(words));

        return ExitSuccess;
    } catch (const std::exception &exception) {
        printError(QString::fromUtf8(exception.what()));
        return ExitUnexpectedError;
    } catch (...) {
        printError(QStringLiteral("Непредвиденная ошибка при работе программы."));
        return ExitUnexpectedError;
    }
}
