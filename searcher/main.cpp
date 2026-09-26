#include "appconfig.h"
#include "mainwindow.h"
#include "searchtypes.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QDir>
#include <QMessageBox>
#include <QMetaType>
#include <QVector>

#include <exception>

namespace {

enum ExitCode { ExitSuccess = 0, ExitConfigError = 2, ExitUnexpectedError = 5 };

}

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("searcher"));
    QApplication::setApplicationVersion(QStringLiteral("1.0"));
    QApplication::setApplicationDisplayName(QStringLiteral("Поисковая система"));

    try {
        qRegisterMetaType<QVector<DocumentHit>>();
        qRegisterMetaType<QVector<WordStat>>();

        QCommandLineParser parser;
        parser.setApplicationDescription(
            QStringLiteral("Поиск по индексу, построенному программой «Индексатор»."));
        parser.addHelpOption();
        parser.addVersionOption();

        const QCommandLineOption configOption(
            QStringList{QStringLiteral("c"), QStringLiteral("config")},
            QStringLiteral("Путь к ini-файлу конфигурации "
                           "(по умолчанию searcher.ini рядом с программой)."),
            QStringLiteral("файл"));
        parser.addOption(configOption);
        parser.process(application);

        const QString configPath =
            AppConfig::locate(QStringLiteral("searcher.ini"), parser.value(configOption));
        AppConfig config(configPath);

        QString error;
        if (!config.load(&error)) {
            QMessageBox::critical(nullptr, QStringLiteral("Ошибка конфигурации"),
                                  QStringLiteral("%1\n\nОжидаемый файл: %2")
                                      .arg(error, QDir::toNativeSeparators(configPath)));
            return ExitConfigError;
        }

        MainWindow window(config);
        window.show();

        return QApplication::exec();
    } catch (const std::exception &exception) {
        QMessageBox::critical(nullptr, QStringLiteral("Внутренняя ошибка"),
                              QString::fromUtf8(exception.what()));
        return ExitUnexpectedError;
    } catch (...) {
        QMessageBox::critical(nullptr, QStringLiteral("Внутренняя ошибка"),
                              QStringLiteral("Непредвиденная ошибка при работе программы."));
        return ExitUnexpectedError;
    }
}
