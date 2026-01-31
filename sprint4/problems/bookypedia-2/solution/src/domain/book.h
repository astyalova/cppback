#pragma once
#include <optional>
#include <string>
#include <vector>

#include "../util/tagged_uuid.h"
#include "author.h"

namespace domain {

namespace detail {
struct BookTag {};
}  // namespace detail

using BookId = util::TaggedUUID<detail::BookTag>;

class Book {
public:
    Book(BookId id, AuthorId author_id, std::string title, int publication_year)
        : id_{std::move(id)}
        , author_id_{std::move(author_id)}
        , title_{std::move(title)}
        , publication_year_{publication_year} {
    }

    const BookId& GetId() const noexcept {
        return id_;
    }

    const AuthorId& GetAuthorId() const noexcept {
        return author_id_;
    }

    const std::string& GetTitle() const noexcept {
        return title_;
    }

    int GetPublicationYear() const noexcept {
        return publication_year_;
    }

private:
    BookId id_;
    AuthorId author_id_;
    std::string title_;
    int publication_year_;
};

class BookRepository {
public:
    struct BookInfo {
        BookId id;
        AuthorId author_id;
        std::string title;
        std::string author_name;
        int publication_year = 0;
    };

    struct BookDetails {
        BookInfo info;
        std::vector<std::string> tags;
    };

    virtual void Save(const Book& book, const std::vector<std::string>& tags) = 0;
    virtual bool AddBookWithAuthorName(int publication_year, const std::string& title,
                                       const std::string& author_name, bool create_author,
                                       const std::vector<std::string>& tags) = 0;
    virtual std::vector<BookInfo> GetAllDetailed() = 0;
    virtual std::vector<BookInfo> GetByTitleDetailed(const std::string& title) = 0;
    virtual std::optional<BookDetails> GetDetails(const BookId& id) = 0;
    virtual std::vector<Book> GetByAuthor(const AuthorId& author_id) = 0;
    virtual bool DeleteById(const BookId& id) = 0;
    virtual bool Update(const BookId& id, const std::string& title, int publication_year,
                        const std::vector<std::string>& tags) = 0;

protected:
    ~BookRepository() = default;
};

}  // namespace domain
