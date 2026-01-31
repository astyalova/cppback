#include <catch2/catch_test_macros.hpp>

#include "../src/app/use_cases_impl.h"
#include "../src/domain/author.h"
#include "../src/domain/book.h"

namespace {

struct MockAuthorRepository : domain::AuthorRepository {
    std::vector<domain::Author> saved_authors;

    void Save(const domain::Author& author) override {
        saved_authors.emplace_back(author);
    }

    std::vector<domain::Author> GetAll() override {
        return saved_authors;
    }

    std::optional<domain::Author> FindByName(const std::string&) override {
        return std::nullopt;
    }

    bool UpdateName(const domain::AuthorId&, const std::string&) override {
        return false;
    }

    bool DeleteByIdCascade(const domain::AuthorId&) override {
        return false;
    }
};

struct MockBookRepository : domain::BookRepository {
    std::vector<domain::Book> saved_books;

    void Save(const domain::Book& book, const std::vector<std::string>&) override {
        saved_books.emplace_back(book);
    }

    bool AddBookWithAuthorName(int, const std::string&, const std::string&, bool,
                               const std::vector<std::string>&) override {
        return false;
    }

    std::vector<domain::BookRepository::BookInfo> GetAllDetailed() override {
        return {};
    }

    std::vector<domain::BookRepository::BookInfo> GetByTitleDetailed(
        const std::string&) override {
        return {};
    }

    std::optional<domain::BookRepository::BookDetails> GetDetails(
        const domain::BookId&) override {
        return std::nullopt;
    }

    std::vector<domain::Book> GetByAuthor(const domain::AuthorId& author_id) override {
        std::vector<domain::Book> result;
        for (const auto& book : saved_books) {
            if (book.GetAuthorId() == author_id) {
                result.emplace_back(book);
            }
        }
        return result;
    }

    bool DeleteById(const domain::BookId&) override {
        return false;
    }

    bool Update(const domain::BookId&, const std::string&, int,
                const std::vector<std::string>&) override {
        return false;
    }
};

struct Fixture {
    MockAuthorRepository authors;
    MockBookRepository books;
};

}  // namespace

SCENARIO_METHOD(Fixture, "Book Adding") {
    GIVEN("Use cases") {
        app::UseCasesImpl use_cases{authors, books};

        WHEN("Adding an author") {
            const auto author_name = "Joanne Rowling";
            use_cases.AddAuthor(author_name);

            THEN("author with the specified name is saved to repository") {
                REQUIRE(authors.saved_authors.size() == 1);
                CHECK(authors.saved_authors.at(0).GetName() == author_name);
                CHECK(authors.saved_authors.at(0).GetId() != domain::AuthorId{});
            }
        }
    }
}
