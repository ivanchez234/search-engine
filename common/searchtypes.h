#ifndef SEARCHTYPES_H
#define SEARCHTYPES_H

#include <QDateTime>
#include <QHash>
#include <QMetaType>
#include <QString>
#include <QVector>

struct ParsedDocument
{
    QString path;
    qint64 size = 0;
    QDateTime modified;
    int totalWords = 0;
    QHash<QString, int> frequencies;
    bool ok = false;
    QString error;
};

struct DocumentHit
{
    QString path;
    QString fileName;
    qint64 relevance = 0;
    int matchedWords = 0;
};

struct WordStat
{
    QString word;
    qint64 totalFrequency = 0;
    int documentCount = 0;
};

Q_DECLARE_METATYPE(DocumentHit)
Q_DECLARE_METATYPE(WordStat)

#endif // SEARCHTYPES_H
