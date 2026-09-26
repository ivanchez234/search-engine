#include "textprocessor.h"

namespace {

template <typename Handler>
void forEachWord(const QString &text, Handler handler)
{
    QString token;
    token.reserve(TextProcessor::MaxWordLength + 2);

    const int length = static_cast<int>(text.size());
    for (int i = 0; i <= length; ++i) {
        const QChar symbol = (i < length) ? text.at(i) : QLatin1Char(' ');

        if (symbol.isLetterOrNumber()) {
            if (token.size() <= TextProcessor::MaxWordLength)
                token.append(symbol);
            continue;
        }

        if (token.isEmpty())
            continue;

        if (token.size() >= TextProcessor::MinWordLength && token.size() <= TextProcessor::MaxWordLength) {
            const QString word = token.toLower();
            if (word.size() >= TextProcessor::MinWordLength && word.size() <= TextProcessor::MaxWordLength)
                handler(word);
        }
        token.clear();
    }
}

}

QStringList TextProcessor::tokenize(const QString &text)
{
    QStringList words;
    forEachWord(text, [&words](const QString &word) { words.append(word); });
    return words;
}

QHash<QString, int> TextProcessor::frequencies(const QString &text, int *totalWords)
{
    QHash<QString, int> result;
    int total = 0;

    forEachWord(text, [&result, &total](const QString &word) {
        ++result[word];
        ++total;
    });

    if (totalWords)
        *totalWords = total;
    return result;
}

QStringList TextProcessor::normalizeQuery(const QString &query, int maxWords)
{
    QStringList words;
    forEachWord(query, [&words, maxWords](const QString &word) {
        if (maxWords > 0 && words.size() >= maxWords)
            return;
        if (!words.contains(word))
            words.append(word);
    });
    return words;
}
