#ifndef TEXTPROCESSOR_H
#define TEXTPROCESSOR_H

#include <QHash>
#include <QString>
#include <QStringList>

namespace TextProcessor {

constexpr int MinWordLength = 3;
constexpr int MaxWordLength = 32;

QStringList tokenize(const QString &text);

QHash<QString, int> frequencies(const QString &text, int *totalWords = nullptr);

QStringList normalizeQuery(const QString &query, int maxWords);

}
#endif // TEXTPROCESSOR_H
