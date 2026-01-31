#pragma once
#include "../domain/author_fwd.h"
#include "../domain/book.h"
#include "use_cases.h"

namespace app {

class UseCasesImpl : public UseCases {
public:
    UseCasesImpl(domain::AuthorRepository& authors, domain::BookRepository& books)
        : authors_{authors}
        , books_{books} {
    }

    void AddAuthor(const std::string& name) override;
    void AddBook(int publication_year, const std::string& title,
                 const domain::AuthorId& author_id,
                 const std::vector<std::string>& tags) override;
    bool AddBookWithAuthorName(int publication_year, const std::string& title,
                               const std::string& author_name, bool create_author,
                               const std::vector<std::string>& tags) override;
    std::vector<domain::Author> GetAuthors() override;
    std::optional<domain::Author> FindAuthorByName(const std::string& name) override;
    std::vector<domain::BookRepository::BookInfo> GetBooks() override;
    std::vector<domain::BookRepository::BookInfo> GetBooksByTitle(
        const std::string& title) override;
    std::optional<domain::BookRepository::BookDetails> GetBookDetails(
        const domain::BookId& id) override;
    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) override;
    bool DeleteBook(const domain::BookId& id) override;
    bool EditBook(const domain::BookId& id, const std::string& title, int publication_year,
                  const std::vector<std::string>& tags) override;
    bool DeleteAuthor(const domain::AuthorId& id) override;
    bool EditAuthor(const domain::AuthorId& id, const std::string& name) override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app
