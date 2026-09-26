#include "mainwindow.h"

#include "searchmodels.h"
#include "searchworker.h"
#include "textprocessor.h"

#include <QApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QSortFilterProxyModel>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableView>
#include <QTextBrowser>
#include <QUrl>
#include <QVBoxLayout>

namespace {

QString documentsWord(int count)
{
    const int hundreds = count % 100;
    if (hundreds >= 11 && hundreds <= 14)
        return QStringLiteral("документов");

    switch (count % 10) {
    case 1:
        return QStringLiteral("документ");
    case 2:
    case 3:
    case 4:
        return QStringLiteral("документа");
    default:
        return QStringLiteral("документов");
    }
}

} // namespace

MainWindow::MainWindow(const AppConfig &config, QWidget *parent)
    : QMainWindow(parent)
    , m_searchConfig(config.search())
{
    buildUi();
    setupWorker(config.database());
}

MainWindow::~MainWindow()
{
    m_thread.quit();
    m_thread.wait();
}

void MainWindow::buildUi()
{
    setWindowTitle(QStringLiteral("Поисковая система"));
    resize(900, 620);

    m_tabs = new QTabWidget(this);
    m_tabs->addTab(createSearchTab(), QStringLiteral("Поиск"));
    m_tabs->addTab(createDictionaryTab(), QStringLiteral("Словарь"));
    setCentralWidget(m_tabs);

    statusBar()->showMessage(QStringLiteral("Подключение к базе данных..."));
    setSearchEnabled(false);
}

QWidget *MainWindow::createSearchTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);

    auto *queryLayout = new QHBoxLayout;

    m_queryEdit = new QLineEdit(tab);
    m_queryEdit->setPlaceholderText(QStringLiteral("Введите слова через пробел, например: привет мир"));
    m_queryEdit->setClearButtonEnabled(true);
    queryLayout->addWidget(m_queryEdit, 1);

    m_searchButton = new QPushButton(QStringLiteral("Искать"), tab);
    m_searchButton->setDefault(true);
    queryLayout->addWidget(m_searchButton);

    layout->addLayout(queryLayout);

    m_hintLabel = new QLabel(tab);
    m_hintLabel->setText(QStringLiteral("Запрос: не более %1 слов, каждое слово — от %2 символов. "
                                        "Выводится не более %3 результатов.")
                             .arg(m_searchConfig.maxQueryWords)
                             .arg(TextProcessor::MinWordLength)
                             .arg(m_searchConfig.maxResults));
    m_hintLabel->setWordWrap(true);
    layout->addWidget(m_hintLabel);

    m_resultsView = new QTextBrowser(tab);
    m_resultsView->setOpenLinks(false);
    m_resultsView->setOpenExternalLinks(false);
    m_resultsView->setPlaceholderText(QStringLiteral("Здесь появятся результаты поиска."));
    layout->addWidget(m_resultsView, 1);

    connect(m_searchButton, &QPushButton::clicked, this, &MainWindow::onSearchRequested);
    connect(m_queryEdit, &QLineEdit::returnPressed, this, &MainWindow::onSearchRequested);
    connect(m_resultsView, &QTextBrowser::anchorClicked, this, &MainWindow::onResultLinkClicked);

    return tab;
}

QWidget *MainWindow::createDictionaryTab()
{
    auto *tab = new QWidget(this);
    auto *layout = new QVBoxLayout(tab);

    auto *splitter = new QSplitter(Qt::Vertical, tab);

    auto *wordsBox = new QGroupBox(QStringLiteral("Все слова в базе данных"), splitter);
    auto *wordsLayout = new QVBoxLayout(wordsBox);

    auto *filterLayout = new QHBoxLayout;
    filterLayout->addWidget(new QLabel(QStringLiteral("Фильтр:"), wordsBox));

    m_wordFilterEdit = new QLineEdit(wordsBox);
    m_wordFilterEdit->setPlaceholderText(QStringLiteral("часть слова"));
    m_wordFilterEdit->setClearButtonEnabled(true);
    filterLayout->addWidget(m_wordFilterEdit, 1);

    m_refreshWordsButton = new QPushButton(QStringLiteral("Обновить"), wordsBox);
    m_refreshWordsButton->setToolTip(
        QStringLiteral("Перечитать словарь из базы данных (например, после новой индексации)"));
    filterLayout->addWidget(m_refreshWordsButton);

    wordsLayout->addLayout(filterLayout);

    m_wordModel = new WordTableModel(this);
    m_wordProxy = new QSortFilterProxyModel(this);
    m_wordProxy->setSourceModel(m_wordModel);
    m_wordProxy->setFilterKeyColumn(WordTableModel::WordColumn);
    m_wordProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_wordProxy->setSortRole(Qt::EditRole);

    m_wordsView = new QTableView(wordsBox);
    m_wordsView->setModel(m_wordProxy);
    m_wordsView->setSortingEnabled(true);
    m_wordsView->sortByColumn(WordTableModel::FrequencyColumn, Qt::DescendingOrder);
    m_wordsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_wordsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_wordsView->setAlternatingRowColors(true);
    m_wordsView->verticalHeader()->setVisible(false);
    m_wordsView->horizontalHeader()->setStretchLastSection(false);
    m_wordsView->horizontalHeader()->setSectionResizeMode(WordTableModel::WordColumn,
                                                          QHeaderView::Stretch);
    m_wordsView->horizontalHeader()->setSectionResizeMode(WordTableModel::FrequencyColumn,
                                                          QHeaderView::ResizeToContents);
    m_wordsView->horizontalHeader()->setSectionResizeMode(WordTableModel::DocumentsColumn,
                                                          QHeaderView::ResizeToContents);
    wordsLayout->addWidget(m_wordsView, 1);

    splitter->addWidget(wordsBox);

    auto *documentsBox = new QGroupBox(QStringLiteral("Найденные документы"), splitter);
    auto *documentsLayout = new QVBoxLayout(documentsBox);

    auto *buttonLayout = new QHBoxLayout;

    m_showDocumentsButton = new QPushButton(QStringLiteral("Показать документы"), documentsBox);
    m_showDocumentsButton->setEnabled(false);
    m_showDocumentsButton->setToolTip(
        QStringLiteral("Станет доступна после выполнения поискового запроса"));
    buttonLayout->addWidget(m_showDocumentsButton);

    m_documentsLabel = new QLabel(QStringLiteral("Запрос ещё не выполнялся."), documentsBox);
    m_documentsLabel->setWordWrap(true);
    buttonLayout->addWidget(m_documentsLabel, 1);

    documentsLayout->addLayout(buttonLayout);

    m_documentModel = new DocumentTableModel(this);

    m_documentsView = new QTableView(documentsBox);
    m_documentsView->setModel(m_documentModel);
    m_documentsView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_documentsView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_documentsView->setAlternatingRowColors(true);
    m_documentsView->verticalHeader()->setVisible(false);
    m_documentsView->horizontalHeader()->setSectionResizeMode(DocumentTableModel::NumberColumn,
                                                              QHeaderView::ResizeToContents);
    m_documentsView->horizontalHeader()->setSectionResizeMode(DocumentTableModel::FileNameColumn,
                                                              QHeaderView::ResizeToContents);
    m_documentsView->horizontalHeader()->setSectionResizeMode(DocumentTableModel::RelevanceColumn,
                                                              QHeaderView::ResizeToContents);
    m_documentsView->horizontalHeader()->setSectionResizeMode(DocumentTableModel::PathColumn,
                                                              QHeaderView::Stretch);
    documentsLayout->addWidget(m_documentsView, 1);

    splitter->addWidget(documentsBox);
    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    layout->addWidget(splitter);

    connect(m_wordFilterEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_wordProxy->setFilterFixedString(text.trimmed());
    });
    connect(m_refreshWordsButton, &QPushButton::clicked, this, [this] {
        if (m_connected)
            emit requestWordStatistics();
    });
    connect(m_showDocumentsButton, &QPushButton::clicked, this,
            &MainWindow::onShowDocumentsRequested);
    connect(m_documentsView, &QTableView::doubleClicked, this, &MainWindow::onDocumentActivated);

    return tab;
}

void MainWindow::setupWorker(const DatabaseConfig &config)
{
    m_worker = new SearchWorker(config);
    m_worker->moveToThread(&m_thread);

    connect(&m_thread, &QThread::started, m_worker, &SearchWorker::initialize);
    connect(&m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &SearchWorker::initialized, this, &MainWindow::onInitialized);
    connect(m_worker, &SearchWorker::searchFinished, this, &MainWindow::onSearchFinished);
    connect(m_worker, &SearchWorker::wordStatisticsReady, this,
            &MainWindow::onWordStatisticsReady);
    connect(m_worker, &SearchWorker::countersReady, this, &MainWindow::onCountersReady);
    connect(m_worker, &SearchWorker::failed, this, &MainWindow::onWorkerFailed);

    connect(this, &MainWindow::requestSearch, m_worker, &SearchWorker::search);
    connect(this, &MainWindow::requestWordStatistics, m_worker, &SearchWorker::loadWordStatistics);

    m_thread.start();
}

void MainWindow::setSearchEnabled(bool enabled)
{
    m_queryEdit->setEnabled(enabled);
    m_searchButton->setEnabled(enabled);
    m_refreshWordsButton->setEnabled(enabled);
}

void MainWindow::setBusy(bool busy)
{
    if (m_busy != busy) {
        m_busy = busy;
        if (busy)
            QApplication::setOverrideCursor(Qt::WaitCursor);
        else
            QApplication::restoreOverrideCursor();
    }

    setSearchEnabled(!m_busy && m_connected);
    m_showDocumentsButton->setEnabled(!m_busy && m_searchPerformed);
}

void MainWindow::onInitialized(bool success, const QString &error)
{
    m_connected = success;

    if (!success) {
        statusBar()->showMessage(QStringLiteral("Нет подключения к базе данных"));
        setSearchEnabled(false);
        QMessageBox::critical(this, QStringLiteral("Ошибка подключения"), error);
        return;
    }

    statusBar()->showMessage(QStringLiteral("Подключение установлено"));
    setSearchEnabled(true);
    m_queryEdit->setFocus();

    emit requestWordStatistics();
}

void MainWindow::onSearchRequested()
{
    if (m_busy || !m_connected)
        return;

    const QString rawQuery = m_queryEdit->text();
    const QStringList allWords = TextProcessor::normalizeQuery(rawQuery, 0);
    const QStringList words = allWords.mid(0, m_searchConfig.maxQueryWords);
    m_droppedWords = static_cast<int>(allWords.size() - words.size());

    if (words.isEmpty()) {
        QMessageBox::information(
            this, QStringLiteral("Пустой запрос"),
            QStringLiteral("Введите хотя бы одно слово длиной от %1 символов.\n"
                           "Знаки препинания и слишком короткие слова не учитываются.")
                .arg(TextProcessor::MinWordLength));
        m_queryEdit->setFocus();
        return;
    }

    setBusy(true);
    statusBar()->showMessage(QStringLiteral("Идёт поиск..."));
    m_resultsView->setHtml(QStringLiteral("<p>Идёт поиск...</p>"));

    emit requestSearch(words, m_searchConfig.maxResults);
}

void MainWindow::onSearchFinished(const QVector<DocumentHit> &hits, const QStringList &words)
{
    m_lastHits = hits;
    m_lastWords = words;
    m_searchPerformed = true;

    setBusy(false);
    showResults(hits, words);

    m_showDocumentsButton->setEnabled(true);
    m_showDocumentsButton->setToolTip(
        QStringLiteral("Показать документы последнего запроса и их релевантность"));
    m_documentsLabel->setText(
        QStringLiteral("Готов результат запроса «%1». Нажмите «Показать документы».")
            .arg(words.join(QLatin1Char(' '))));

    statusBar()->showMessage(hits.isEmpty()
                                 ? QStringLiteral("Ничего не найдено")
                                 : QStringLiteral("Найдено %1 %2")
                                       .arg(hits.size())
                                       .arg(documentsWord(static_cast<int>(hits.size()))));
}

void MainWindow::showResults(const QVector<DocumentHit> &hits, const QStringList &words)
{
    const QString query = words.join(QLatin1Char(' ')).toHtmlEscaped();

    QString note;
    if (m_droppedWords > 0) {
        note = QStringLiteral("<p><i>Запрос ограничен %1 словами, поэтому учтены только "
                              "первые из них; остальные (%2) отброшены.</i></p>")
                   .arg(m_searchConfig.maxQueryWords)
                   .arg(m_droppedWords);
    }

    if (hits.isEmpty()) {
        m_resultsView->setHtml(
            QStringLiteral("<h3>По запросу «%1» ничего не найдено</h3>%2"
                           "<p>Документ попадает в выдачу, только если содержит "
                           "<b>все</b> слова запроса. Попробуйте оставить меньше слов "
                           "или проверьте, проиндексирован ли нужный каталог.</p>")
                .arg(query, note));
        return;
    }

    QString html = QStringLiteral("<h3>По запросу «%1» найдено: %2 %3</h3>")
                       .arg(query)
                       .arg(hits.size())
                       .arg(documentsWord(static_cast<int>(hits.size())))
                   + note + QStringLiteral("<ol>");

    for (const DocumentHit &hit : hits) {
        html += QStringLiteral("<li style=\"margin-bottom:8px;\">"
                               "<b>%1</b> — релевантность <b>%2</b><br>"
                               "<a href=\"%3\">%4</a></li>")
                    .arg(hit.fileName.toHtmlEscaped(),
                         QLocale().toString(hit.relevance),
                         QUrl::fromLocalFile(hit.path).toString(QUrl::FullyEncoded),
                         hit.path.toHtmlEscaped());
    }
    html += QStringLiteral("</ol><p><i>Релевантность — суммарное количество упоминаний "
                           "слов запроса в документе. Щёлкните по пути, чтобы открыть файл.</i></p>");

    m_resultsView->setHtml(html);
}

void MainWindow::onWordStatisticsReady(const QVector<WordStat> &stats)
{
    m_wordModel->setWords(stats);
    m_wordsView->sortByColumn(WordTableModel::FrequencyColumn, Qt::DescendingOrder);
}

void MainWindow::onCountersReady(qint64 documents, qint64 words)
{
    statusBar()->showMessage(QStringLiteral("В базе данных: документов — %1, уникальных слов — %2")
                                 .arg(QLocale().toString(documents), QLocale().toString(words)));
}

void MainWindow::onWorkerFailed(const QString &message)
{
    const bool duringSearch = m_busy;

    setBusy(false);
    statusBar()->showMessage(QStringLiteral("Ошибка при обращении к базе данных"));
    if (duringSearch)
        m_resultsView->setHtml(QStringLiteral("<p>Запрос не выполнен.</p>"));

    QMessageBox::critical(this, QStringLiteral("Внутренняя ошибка"), message);
}

void MainWindow::onShowDocumentsRequested()
{
    m_documentModel->setDocuments(m_lastHits);

    if (m_lastHits.isEmpty()) {
        m_documentsLabel->setText(
            QStringLiteral("По запросу «%1» документов не найдено.")
                .arg(m_lastWords.join(QLatin1Char(' '))));
        return;
    }

    m_documentsLabel->setText(QStringLiteral("Запрос «%1»: %2 %3. Двойной щелчок открывает файл.")
                                  .arg(m_lastWords.join(QLatin1Char(' ')))
                                  .arg(m_lastHits.size())
                                  .arg(documentsWord(static_cast<int>(m_lastHits.size()))));
}

void MainWindow::onDocumentActivated(const QModelIndex &index)
{
    if (index.isValid())
        openDocument(m_documentModel->pathAt(index.row()));
}

void MainWindow::onResultLinkClicked(const QUrl &url)
{
    if (url.isLocalFile())
        openDocument(url.toLocalFile());
}

void MainWindow::openDocument(const QString &path)
{
    if (path.isEmpty())
        return;

    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, QStringLiteral("Файл недоступен"),
                             QStringLiteral("Файл не найден на диске:\n%1\n\n"
                                            "Возможно, он был перемещён или удалён после индексации.")
                                 .arg(path));
        return;
    }

    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) {
        QMessageBox::warning(this, QStringLiteral("Не удалось открыть файл"),
                             QStringLiteral("Система не смогла открыть файл:\n%1").arg(path));
    }
}
