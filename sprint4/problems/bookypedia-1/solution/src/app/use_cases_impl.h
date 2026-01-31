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
                 const domain::AuthorId& author_id) override;
    std::vector<domain::Author> GetAuthors() override;
    std::vector<domain::Book> GetBooks() override;
    std::vector<domain::Book> GetAuthorBooks(const domain::AuthorId& author_id) override;

private:
    domain::AuthorRepository& authors_;
    domain::BookRepository& books_;
};

}  // namespace app
