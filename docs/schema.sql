-- Структура базы данных поисковой системы.
--
-- Этот файл носит справочный характер: те же самые запросы выполняет
-- программа «Индексатор» при первом запуске (Database::ensureSchema).
-- Выполнять его вручную не требуется.

-- Проиндексированные документы.
CREATE TABLE IF NOT EXISTS documents (
    id           BIGSERIAL   PRIMARY KEY,
    path         TEXT        NOT NULL UNIQUE,
    file_name    TEXT        NOT NULL,
    file_size    BIGINT      NOT NULL DEFAULT 0,
    modified_at  TIMESTAMPTZ,
    indexed_at   TIMESTAMPTZ NOT NULL DEFAULT now(),
    total_words  INTEGER     NOT NULL DEFAULT 0,
    unique_words INTEGER     NOT NULL DEFAULT 0
);

-- Словарь: каждое слово хранится ровно один раз.
CREATE TABLE IF NOT EXISTS words (
    id   BIGSERIAL   PRIMARY KEY,
    word VARCHAR(32) NOT NULL UNIQUE
);

-- Промежуточная таблица связи «многие-ко-многим» + частота слова в документе.
CREATE TABLE IF NOT EXISTS document_words (
    document_id BIGINT  NOT NULL REFERENCES documents (id) ON DELETE CASCADE,
    word_id     BIGINT  NOT NULL REFERENCES words (id)     ON DELETE CASCADE,
    frequency   INTEGER NOT NULL CHECK (frequency > 0),
    CONSTRAINT pk_document_words PRIMARY KEY (document_id, word_id)
);

-- Поиск идёт «от слова к документам», поэтому нужен индекс по word_id.
-- (по document_id индекс уже создан первичным ключом)
CREATE INDEX IF NOT EXISTS idx_document_words_word_id ON document_words (word_id);


-- ---------------------------------------------------------------------------
-- Запрос поиска (пример для двух слов: «привет мир»).
-- Берутся документы, в которых встречаются ВСЕ слова запроса,
-- релевантность = суммарное число упоминаний этих слов в документе.
-- ---------------------------------------------------------------------------
--
-- SELECT d.path,
--        d.file_name,
--        SUM(dw.frequency) AS relevance,
--        COUNT(DISTINCT w.id) AS matched_words
--   FROM document_words dw
--   JOIN words     w ON w.id = dw.word_id
--   JOIN documents d ON d.id = dw.document_id
--  WHERE w.word IN ('привет', 'мир')
--  GROUP BY d.id, d.path, d.file_name
-- HAVING COUNT(DISTINCT w.id) = 2
--  ORDER BY relevance DESC, d.file_name ASC
--  LIMIT 10;
