#include "use_cases_impl.h"

#include <stdexcept>

#include "../domain/author.h"
#include "../domain/book.h"

namespace app {
using namespace domain;

void UseCasesImpl::AddAuthor(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Author name is empty");
    }
    authors_.Save({AuthorId::New(), name});
}

void UseCasesImpl::AddBook(int publication_year, const std::string& title,
                           const AuthorId& author_id) {
    if (title.empty()) {
        throw std::invalid_argument("Book title is empty");
    }
    books_.Save({BookId::New(), author_id, title, publication_year});
}

std::vector<Author> UseCasesImpl::GetAuthors() {
    return authors_.GetAll();
}

std::vector<Book> UseCasesImpl::GetBooks() {
    return books_.GetAll();
}

std::vector<Book> UseCasesImpl::GetAuthorBooks(const AuthorId& author_id) {
    return books_.GetByAuthor(author_id);
}

}  // namespace app
