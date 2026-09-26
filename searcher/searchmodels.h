#ifndef SEARCHMODELS_H
#define SEARCHMODELS_H

#include "searchtypes.h"

#include <QAbstractTableModel>
#include <QVector>

class WordTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { WordColumn = 0, FrequencyColumn, DocumentsColumn, ColumnCount };

    explicit WordTableModel(QObject *parent = nullptr);

    void setWords(const QVector<WordStat> &words);
    void clear();

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    QVector<WordStat> m_words;
};

class DocumentTableModel : public QAbstractTableModel
{
    Q_OBJECT

public:
    enum Column { NumberColumn = 0, FileNameColumn, RelevanceColumn, PathColumn, ColumnCount };

    explicit DocumentTableModel(QObject *parent = nullptr);

    void setDocuments(const QVector<DocumentHit> &documents);
    void clear();

    QString pathAt(int row) const;

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;

private:
    QVector<DocumentHit> m_documents;
};

#endif // SEARCHMODELS_H
