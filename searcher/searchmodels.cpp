#include "searchmodels.h"

#include <QLocale>

namespace {

QString formatNumber(qint64 value)
{
    return QLocale().toString(value);
}

}

WordTableModel::WordTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void WordTableModel::setWords(const QVector<WordStat> &words)
{
    beginResetModel();
    m_words = words;
    endResetModel();
}

void WordTableModel::clear()
{
    beginResetModel();
    m_words.clear();
    endResetModel();
}

int WordTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_words.size());
}

int WordTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant WordTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_words.size())
        return QVariant();

    const WordStat &stat = m_words.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case WordColumn:
            return stat.word;
        case FrequencyColumn:
            return formatNumber(stat.totalFrequency);
        case DocumentsColumn:
            return formatNumber(stat.documentCount);
        default:
            return QVariant();
        }

    // по этой роли сортирует прокси: отдаём сырые числа,
    // иначе строкой "10" окажется меньше "9"
    case Qt::EditRole:
        switch (index.column()) {
        case WordColumn:
            return stat.word;
        case FrequencyColumn:
            return stat.totalFrequency;
        case DocumentsColumn:
            return stat.documentCount;
        default:
            return QVariant();
        }

    case Qt::TextAlignmentRole:
        if (index.column() == WordColumn)
            return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);
        return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);

    default:
        return QVariant();
    }
}

QVariant WordTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return QVariant();

    if (orientation == Qt::Vertical)
        return section + 1;

    switch (section) {
    case WordColumn:
        return QStringLiteral("Слово");
    case FrequencyColumn:
        return QStringLiteral("Всего упоминаний");
    case DocumentsColumn:
        return QStringLiteral("В документах");
    default:
        return QVariant();
    }
}

DocumentTableModel::DocumentTableModel(QObject *parent)
    : QAbstractTableModel(parent)
{
}

void DocumentTableModel::setDocuments(const QVector<DocumentHit> &documents)
{
    beginResetModel();
    m_documents = documents;
    endResetModel();
}

void DocumentTableModel::clear()
{
    beginResetModel();
    m_documents.clear();
    endResetModel();
}

QString DocumentTableModel::pathAt(int row) const
{
    if (row < 0 || row >= m_documents.size())
        return QString();
    return m_documents.at(row).path;
}

int DocumentTableModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_documents.size());
}

int DocumentTableModel::columnCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant DocumentTableModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_documents.size())
        return QVariant();

    const DocumentHit &hit = m_documents.at(index.row());

    switch (role) {
    case Qt::DisplayRole:
        switch (index.column()) {
        case NumberColumn:
            return index.row() + 1;
        case FileNameColumn:
            return hit.fileName;
        case RelevanceColumn:
            return formatNumber(hit.relevance);
        case PathColumn:
            return hit.path;
        default:
            return QVariant();
        }

    case Qt::ToolTipRole:
        return hit.path;

    case Qt::TextAlignmentRole:
        if (index.column() == NumberColumn || index.column() == RelevanceColumn)
            return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
        return static_cast<int>(Qt::AlignLeft | Qt::AlignVCenter);

    default:
        return QVariant();
    }
}

QVariant DocumentTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole || orientation != Qt::Horizontal)
        return QVariant();

    switch (section) {
    case NumberColumn:
        return QStringLiteral("№");
    case FileNameColumn:
        return QStringLiteral("Файл");
    case RelevanceColumn:
        return QStringLiteral("Релевантность");
    case PathColumn:
        return QStringLiteral("Полный путь");
    default:
        return QVariant();
    }
}
