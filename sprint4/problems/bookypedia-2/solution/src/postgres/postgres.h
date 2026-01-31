#pragma once
#include <optional>
#include <pqxx/connection>
#include <pqxx/transaction>

#include "../domain/author.h"
#include "../domain/book.h"

namespace postgres {

class AuthorRepositoryImpl : public domain::AuthorRepository {
public:
    explicit AuthorRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Author& author) override;
    std::vector<domain::Author> GetAll() override;
    std::optional<domain::Author> FindByName(const std::string& name) override;
    bool UpdateName(const domain::AuthorId& id, const std::string& name) override;
    bool DeleteByIdCascade(const domain::AuthorId& id) override;

private:
    pqxx::connection& connection_;
};

class BookRepositoryImpl : public domain::BookRepository {
public:
    explicit BookRepositoryImpl(pqxx::connection& connection)
        : connection_{connection} {
    }

    void Save(const domain::Book& book, const std::vector<std::string>& tags) override;
    bool AddBookWithAuthorName(int publication_year, const std::string& title,
                               const std::string& author_name, bool create_author,
                               const std::vector<std::string>& tags) override;
    std::vector<domain::BookRepository::BookInfo> GetAllDetailed() override;
    std::vector<domain::BookRepository::BookInfo> GetByTitleDetailed(
        const std::string& title) override;
    std::optional<domain::BookRepository::BookDetails> GetDetails(
        const domain::BookId& id) override;
    std::vector<domain::Book> GetByAuthor(const domain::AuthorId& author_id) override;
    bool DeleteById(const domain::BookId& id) override;
    bool Update(const domain::BookId& id, const std::string& title, int publication_year,
                const std::vector<std::string>& tags) override;

private:
    pqxx::connection& connection_;
};

class Database {
public:
    explicit Database(pqxx::connection connection);

    AuthorRepositoryImpl& GetAuthors() & {
        return authors_;
    }

    BookRepositoryImpl& GetBooks() & {
        return books_;
    }

private:
    pqxx::connection connection_;
    AuthorRepositoryImpl authors_{connection_};
    BookRepositoryImpl books_{connection_};
};

}  // namespace postgres
