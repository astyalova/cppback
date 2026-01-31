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
                           const AuthorId& author_id, const std::vector<std::string>& tags) {
    if (title.empty()) {
        throw std::invalid_argument("Book title is empty");
    }
    books_.Save({BookId::New(), author_id, title, publication_year}, tags);
}

bool UseCasesImpl::AddBookWithAuthorName(int publication_year, const std::string& title,
                                         const std::string& author_name, bool create_author,
                                         const std::vector<std::string>& tags) {
    if (title.empty()) {
        throw std::invalid_argument("Book title is empty");
    }
    if (author_name.empty()) {
        throw std::invalid_argument("Author name is empty");
    }
    return books_.AddBookWithAuthorName(publication_year, title, author_name, create_author,
                                        tags);
}

std::vector<Author> UseCasesImpl::GetAuthors() {
    return authors_.GetAll();
}

std::optional<Author> UseCasesImpl::FindAuthorByName(const std::string& name) {
    return authors_.FindByName(name);
}

std::vector<BookRepository::BookInfo> UseCasesImpl::GetBooks() {
    return books_.GetAllDetailed();
}

std::vector<BookRepository::BookInfo> UseCasesImpl::GetBooksByTitle(const std::string& title) {
    return books_.GetByTitleDetailed(title);
}

std::optional<BookRepository::BookDetails> UseCasesImpl::GetBookDetails(const BookId& id) {
    return books_.GetDetails(id);
}

std::vector<Book> UseCasesImpl::GetAuthorBooks(const AuthorId& author_id) {
    return books_.GetByAuthor(author_id);
}

bool UseCasesImpl::DeleteBook(const BookId& id) {
    return books_.DeleteById(id);
}

bool UseCasesImpl::EditBook(const BookId& id, const std::string& title, int publication_year,
                            const std::vector<std::string>& tags) {
    if (title.empty()) {
        throw std::invalid_argument("Book title is empty");
    }
    return books_.Update(id, title, publication_year, tags);
}

bool UseCasesImpl::DeleteAuthor(const AuthorId& id) {
    return authors_.DeleteByIdCascade(id);
}

bool UseCasesImpl::EditAuthor(const AuthorId& id, const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("Author name is empty");
    }
    return authors_.UpdateName(id, name);
}

}  // namespace app
