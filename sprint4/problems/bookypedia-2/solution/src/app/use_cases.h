#pragma once

#include <optional>
#include <string>
#include <vector>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {

class UseCases {
public:
    virtual void AddAuthor(const std::string& name) = 0;
    virtual void AddBook(int publication_year, const std::string& title,
                         const domain::AuthorId& author_id,
                         const std::vector<std::string>& tags) = 0;
    virtual bool AddBookWithAuthorName(int publication_year, const std::string& title,
                                       const std::string& author_name, bool create_author,
                                       const std::vector<std::string>& tags) = 0;
    virtual std::vector<domain::Author> GetAuthors() = 0;
    virtual std::optional<domain::Author> FindAuthorByName(const std::string& name) = 0;
    virtual std::vector<domain::BookRepository::BookInfo> GetBooks() = 0;
    virtual std::vector<domain::BookRepository::BookInfo> GetBooksByTitle(
        const std::string& title) = 0;
    virtual std::optional<domain::BookRepository::BookDetails> GetBookDetails(
        const domain::BookId& id) = 0;
    virtual std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) = 0;
    virtual bool DeleteBook(const domain::BookId& id) = 0;
    virtual bool EditBook(const domain::BookId& id, const std::string& title,
                          int publication_year, const std::vector<std::string>& tags) = 0;
    virtual bool DeleteAuthor(const domain::AuthorId& id) = 0;
    virtual bool EditAuthor(const domain::AuthorId& id, const std::string& name) = 0;

protected:
    ~UseCases() = default;
};

}  // namespace app
