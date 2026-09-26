#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "appconfig.h"
#include "searchtypes.h"

#include <QMainWindow>
#include <QModelIndex>
#include <QStringList>
#include <QThread>
#include <QUrl>
#include <QVector>

class DocumentTableModel;
class SearchWorker;
class WordTableModel;

class QLabel;
class QLineEdit;
class QPushButton;
class QSortFilterProxyModel;
class QTableView;
class QTabWidget;
class QTextBrowser;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(const AppConfig &config, QWidget *parent = nullptr);
    ~MainWindow() override;

signals:
    void requestSearch(const QStringList &words, int limit);
    void requestWordStatistics();

private slots:
    void onInitialized(bool success, const QString &error);
    void onSearchRequested();
    void onSearchFinished(const QVector<DocumentHit> &hits, const QStringList &words);
    void onWordStatisticsReady(const QVector<WordStat> &stats);
    void onCountersReady(qint64 documents, qint64 words);
    void onWorkerFailed(const QString &message);
    void onShowDocumentsRequested();
    void onDocumentActivated(const QModelIndex &index);
    void onResultLinkClicked(const QUrl &url);

private:
    void buildUi();
    QWidget *createSearchTab();
    QWidget *createDictionaryTab();
    void setupWorker(const DatabaseConfig &config);

    void setBusy(bool busy);
    void setSearchEnabled(bool enabled);
    void showResults(const QVector<DocumentHit> &hits, const QStringList &words);
    void openDocument(const QString &path);

    SearchConfig m_searchConfig;

    QThread m_thread;
    SearchWorker *m_worker = nullptr;

    QTabWidget *m_tabs = nullptr;

    QLineEdit *m_queryEdit = nullptr;
    QPushButton *m_searchButton = nullptr;
    QLabel *m_hintLabel = nullptr;
    QTextBrowser *m_resultsView = nullptr;

    QLineEdit *m_wordFilterEdit = nullptr;
    QPushButton *m_refreshWordsButton = nullptr;
    QPushButton *m_showDocumentsButton = nullptr;
    QTableView *m_wordsView = nullptr;
    QTableView *m_documentsView = nullptr;
    QLabel *m_documentsLabel = nullptr;

    WordTableModel *m_wordModel = nullptr;
    QSortFilterProxyModel *m_wordProxy = nullptr;
    DocumentTableModel *m_documentModel = nullptr;

    QVector<DocumentHit> m_lastHits;
    QStringList m_lastWords;
    int m_droppedWords = 0;

    bool m_busy = false;
    bool m_connected = false;
    bool m_searchPerformed = false;
};

#endif // MAINWINDOW_H
