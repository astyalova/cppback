#include "postgres.h"
#include <pqxx/result>
#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

void AuthorRepositoryImpl::Save(const domain::Author& author) {
    // Пока каждое обращение к репозиторию выполняется внутри отдельной транзакции
    // В будущих уроках вы узнаете про паттерн Unit of Work, при помощи которого сможете несколько
    // запросов выполнить в рамках одной транзакции.
    // Вы также может самостоятельно почитать информацию про этот паттерн и применить его здесь.
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO authors (id, name) VALUES ($1, $2)
ON CONFLICT (id) DO UPDATE SET name=$2;
)"_zv,
        author.GetId().ToString(), author.GetName());
    work.commit();
}

std::vector<domain::Author> AuthorRepositoryImpl::GetAll() {
    pqxx::work work{connection_};
    std::vector<domain::Author> result;
    auto rows = work.exec(
        R"(
SELECT id, name
FROM authors
ORDER BY name;
)"_zv);
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.emplace_back(domain::AuthorId::FromString(row.at("id").as<std::string>()),
                            row.at("name").as<std::string>());
    }
    return result;
}

std::optional<domain::Author> AuthorRepositoryImpl::FindByName(const std::string& name) {
    pqxx::work work{connection_};
    auto row = work.exec_params(
        R"(
SELECT id, name
FROM authors
WHERE name = $1;
)"_zv,
        name);
    if (row.empty()) {
        return std::nullopt;
    }
    return domain::Author{domain::AuthorId::FromString(row[0].at("id").as<std::string>()),
                          row[0].at("name").as<std::string>()};
}

bool AuthorRepositoryImpl::UpdateName(const domain::AuthorId& id, const std::string& name) {
    pqxx::work work{connection_};
    auto res = work.exec_params(
        R"(
UPDATE authors
SET name = $2
WHERE id = $1;
)"_zv,
        id.ToString(), name);
    if (res.affected_rows() == 0) {
        return false;
    }
    work.commit();
    return true;
}

bool AuthorRepositoryImpl::DeleteByIdCascade(const domain::AuthorId& id) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
DELETE FROM book_tags
WHERE book_id IN (
    SELECT id FROM books WHERE author_id = $1
);
)"_zv,
        id.ToString());
    work.exec_params(
        R"(
DELETE FROM books
WHERE author_id = $1;
)"_zv,
        id.ToString());
    auto res = work.exec_params(
        R"(
DELETE FROM authors
WHERE id = $1;
)"_zv,
        id.ToString());
    if (res.affected_rows() == 0) {
        return false;
    }
    work.commit();
    return true;
}

void BookRepositoryImpl::Save(const domain::Book& book, const std::vector<std::string>& tags) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year)
VALUES ($1, $2, $3, $4);
)"_zv,
        book.GetId().ToString(), book.GetAuthorId().ToString(), book.GetTitle(),
        book.GetPublicationYear());
    for (const auto& tag : tags) {
        work.exec_params(
            R"(
INSERT INTO book_tags (book_id, tag)
VALUES ($1, $2);
)"_zv,
            book.GetId().ToString(), tag);
    }
    work.commit();
}

bool BookRepositoryImpl::AddBookWithAuthorName(int publication_year, const std::string& title,
                                               const std::string& author_name,
                                               bool create_author,
                                               const std::vector<std::string>& tags) {
    pqxx::work work{connection_};
    auto rows = work.exec_params(
        R"(
SELECT id
FROM authors
WHERE name = $1;
)"_zv,
        author_name);
    domain::AuthorId author_id;
    if (rows.empty()) {
        if (!create_author) {
            return false;
        }
        author_id = domain::AuthorId::New();
        work.exec_params(
            R"(
INSERT INTO authors (id, name) VALUES ($1, $2);
)"_zv,
            author_id.ToString(), author_name);
    } else {
        author_id = domain::AuthorId::FromString(rows[0].at("id").as<std::string>());
    }

    auto book_id = domain::BookId::New();
    work.exec_params(
        R"(
INSERT INTO books (id, author_id, title, publication_year)
VALUES ($1, $2, $3, $4);
)"_zv,
        book_id.ToString(), author_id.ToString(), title, publication_year);
    for (const auto& tag : tags) {
        work.exec_params(
            R"(
INSERT INTO book_tags (book_id, tag)
VALUES ($1, $2);
)"_zv,
            book_id.ToString(), tag);
    }
    work.commit();
    return true;
}

std::vector<domain::BookRepository::BookInfo> BookRepositoryImpl::GetAllDetailed() {
    pqxx::work work{connection_};
    std::vector<domain::BookRepository::BookInfo> result;
    auto rows = work.exec(
        R"(
SELECT books.id,
       books.author_id,
       books.title,
       books.publication_year,
       authors.name AS author_name
FROM books
JOIN authors ON authors.id = books.author_id
ORDER BY books.title, authors.name, books.publication_year;
)"_zv);
    result.reserve(rows.size());
    for (const auto& row : rows) {
        domain::BookRepository::BookInfo info;
        info.id = domain::BookId::FromString(row.at("id").as<std::string>());
        info.author_id = domain::AuthorId::FromString(row.at("author_id").as<std::string>());
        info.title = row.at("title").as<std::string>();
        info.author_name = row.at("author_name").as<std::string>();
        info.publication_year = row.at("publication_year").as<int>();
        result.emplace_back(std::move(info));
    }
    return result;
}

std::vector<domain::BookRepository::BookInfo> BookRepositoryImpl::GetByTitleDetailed(
    const std::string& title) {
    pqxx::work work{connection_};
    std::vector<domain::BookRepository::BookInfo> result;
    auto rows = work.exec_params(
        R"(
SELECT books.id,
       books.author_id,
       books.title,
       books.publication_year,
       authors.name AS author_name
FROM books
JOIN authors ON authors.id = books.author_id
WHERE books.title = $1
ORDER BY authors.name, books.publication_year;
)"_zv,
        title);
    result.reserve(rows.size());
    for (const auto& row : rows) {
        domain::BookRepository::BookInfo info;
        info.id = domain::BookId::FromString(row.at("id").as<std::string>());
        info.author_id = domain::AuthorId::FromString(row.at("author_id").as<std::string>());
        info.title = row.at("title").as<std::string>();
        info.author_name = row.at("author_name").as<std::string>();
        info.publication_year = row.at("publication_year").as<int>();
        result.emplace_back(std::move(info));
    }
    return result;
}

std::optional<domain::BookRepository::BookDetails> BookRepositoryImpl::GetDetails(
    const domain::BookId& id) {
    pqxx::work work{connection_};
    auto book_rows = work.exec_params(
        R"(
SELECT books.id,
       books.author_id,
       books.title,
       books.publication_year,
       authors.name AS author_name
FROM books
JOIN authors ON authors.id = books.author_id
WHERE books.id = $1;
)"_zv,
        id.ToString());
    if (book_rows.empty()) {
        return std::nullopt;
    }
    domain::BookRepository::BookDetails details;
    const auto& row = book_rows[0];
    details.info.id = domain::BookId::FromString(row.at("id").as<std::string>());
    details.info.author_id = domain::AuthorId::FromString(row.at("author_id").as<std::string>());
    details.info.title = row.at("title").as<std::string>();
    details.info.author_name = row.at("author_name").as<std::string>();
    details.info.publication_year = row.at("publication_year").as<int>();

    auto tags_rows = work.exec_params(
        R"(
SELECT tag
FROM book_tags
WHERE book_id = $1
ORDER BY tag;
)"_zv,
        id.ToString());
    details.tags.reserve(tags_rows.size());
    for (const auto& tag_row : tags_rows) {
        details.tags.emplace_back(tag_row.at("tag").as<std::string>());
    }
    return details;
}

std::vector<domain::Book> BookRepositoryImpl::GetByAuthor(const domain::AuthorId& author_id) {
    pqxx::work work{connection_};
    std::vector<domain::Book> result;
    auto rows = work.exec_params(
        R"(
SELECT id, author_id, title, publication_year
FROM books
WHERE author_id = $1
ORDER BY publication_year, title;
)"_zv,
        author_id.ToString());
    result.reserve(rows.size());
    for (const auto& row : rows) {
        result.emplace_back(domain::BookId::FromString(row.at("id").as<std::string>()),
                            domain::AuthorId::FromString(row.at("author_id").as<std::string>()),
                            row.at("title").as<std::string>(),
                            row.at("publication_year").as<int>());
    }
    return result;
}

bool BookRepositoryImpl::DeleteById(const domain::BookId& id) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
DELETE FROM book_tags
WHERE book_id = $1;
)"_zv,
        id.ToString());
    auto res = work.exec_params(
        R"(
DELETE FROM books
WHERE id = $1;
)"_zv,
        id.ToString());
    if (res.affected_rows() == 0) {
        return false;
    }
    work.commit();
    return true;
}

bool BookRepositoryImpl::Update(const domain::BookId& id, const std::string& title,
                                int publication_year, const std::vector<std::string>& tags) {
    pqxx::work work{connection_};
    auto res = work.exec_params(
        R"(
UPDATE books
SET title = $2,
    publication_year = $3
WHERE id = $1;
)"_zv,
        id.ToString(), title, publication_year);
    if (res.affected_rows() == 0) {
        return false;
    }
    work.exec_params(
        R"(
DELETE FROM book_tags
WHERE book_id = $1;
)"_zv,
        id.ToString());
    for (const auto& tag : tags) {
        work.exec_params(
            R"(
INSERT INTO book_tags (book_id, tag)
VALUES ($1, $2);
)"_zv,
            id.ToString(), tag);
    }
    work.commit();
    return true;
}

Database::Database(pqxx::connection connection)
    : connection_{std::move(connection)} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS authors (
    id UUID CONSTRAINT author_id_constraint PRIMARY KEY,
    name varchar(100) UNIQUE NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS books (
    id UUID CONSTRAINT book_id_constraint PRIMARY KEY,
    author_id UUID NOT NULL REFERENCES authors(id) ON DELETE CASCADE,
    title varchar(100) NOT NULL,
    publication_year INTEGER
);
)"_zv);
    work.exec(R"(
CREATE TABLE IF NOT EXISTS book_tags (
    book_id UUID NOT NULL REFERENCES books(id) ON DELETE CASCADE,
    tag varchar(30) NOT NULL,
    CONSTRAINT book_tag_pk PRIMARY KEY (book_id, tag)
);
)"_zv);

    // коммитим изменения
    work.commit();
}

}  // namespace postgres
