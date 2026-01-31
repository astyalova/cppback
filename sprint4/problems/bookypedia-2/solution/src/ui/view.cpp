#include "view.h"

#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <cctype>
#include <iostream>
#include <sstream>
#include <unordered_set>

#include "../app/use_cases.h"
#include "../domain/author.h"
#include "../domain/book.h"
#include "../menu/menu.h"

using namespace std::literals;
namespace ph = std::placeholders;

namespace {

std::string NormalizeSpaces(const std::string& input) {
    std::string value = input;
    boost::algorithm::trim(value);
    std::istringstream iss{value};
    std::string word;
    std::string result;
    while (iss >> word) {
        if (!result.empty()) {
            result.push_back(' ');
        }
        result += word;
    }
    return result;
}

std::vector<std::string> NormalizeTags(const std::string& input) {
    std::vector<std::string> result;
    std::unordered_set<std::string> seen;
    std::string part;
    std::stringstream ss{input};
    while (std::getline(ss, part, ',')) {
        auto tag = NormalizeSpaces(part);
        if (tag.empty()) {
            continue;
        }
        if (seen.insert(tag).second) {
            result.emplace_back(std::move(tag));
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::string JoinTags(const std::vector<std::string>& tags) {
    std::string result;
    for (size_t i = 0; i < tags.size(); ++i) {
        if (i != 0) {
            result += ", ";
        }
        result += tags[i];
    }
    return result;
}

}  // namespace

namespace ui {
namespace detail {

std::ostream& operator<<(std::ostream& out, const AuthorInfo& author) {
    out << author.name;
    return out;
}

std::ostream& operator<<(std::ostream& out, const BookInfo& book) {
    out << book.title << " by " << book.author_name << ", " << book.publication_year;
    return out;
}

std::ostream& operator<<(std::ostream& out, const AuthorBookInfo& book) {
    out << book.title << ", " << book.publication_year;
    return out;
}

}  // namespace detail

template <typename T>
void PrintVector(std::ostream& out, const std::vector<T>& vector) {
    int i = 1;
    for (auto& value : vector) {
        out << i++ << " " << value << std::endl;
    }
}

void PrintAuthors(std::ostream& out, const std::vector<detail::AuthorInfo>& authors) {
    int i = 1;
    for (const auto& author : authors) {
        out << i++ << " " << author << std::endl;
    }
}

View::View(menu::Menu& menu, app::UseCases& use_cases, std::istream& input, std::ostream& output)
    : menu_{menu}
    , use_cases_{use_cases}
    , input_{input}
    , output_{output} {
    menu_.AddAction(  //
        "AddAuthor"s, "name"s, "Adds author"s, std::bind(&View::AddAuthor, this, ph::_1)
        // либо
        // [this](auto& cmd_input) { return AddAuthor(cmd_input); }
    );
    menu_.AddAction("AddBook"s, "<pub year> <title>"s, "Adds book"s,
                    std::bind(&View::AddBook, this, ph::_1));
    menu_.AddAction("DeleteAuthor"s, "[name]"s, "Deletes author"s,
                    std::bind(&View::DeleteAuthor, this, ph::_1));
    menu_.AddAction("EditAuthor"s, "[name]"s, "Edits author"s,
                    std::bind(&View::EditAuthor, this, ph::_1));
    menu_.AddAction("ShowAuthors"s, {}, "Show authors"s, std::bind(&View::ShowAuthors, this));
    menu_.AddAction("ShowBooks"s, {}, "Show books"s, std::bind(&View::ShowBooks, this));
    menu_.AddAction("ShowBook"s, "[title]"s, "Show book"s,
                    std::bind(&View::ShowBook, this, ph::_1));
    menu_.AddAction("ShowAuthorBooks"s, {}, "Show author books"s,
                    std::bind(&View::ShowAuthorBooks, this));
    menu_.AddAction("DeleteBook"s, "[title]"s, "Delete book"s,
                    std::bind(&View::DeleteBook, this, ph::_1));
    menu_.AddAction("EditBook"s, "[title]"s, "Edit book"s,
                    std::bind(&View::EditBook, this, ph::_1));
}

bool View::AddAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);
        use_cases_.AddAuthor(std::move(name));
    } catch (const std::exception&) {
        output_ << "Failed to add author"sv << std::endl;
    }
    return true;
}

bool View::AddBook(std::istream& cmd_input) const {
    try {
        auto params = GetBookParams(cmd_input);
        if (!params) {
            return true;
        }

        output_ << "Enter author name or empty line to select from list:" << std::endl;
        std::string author_name;
        if (!std::getline(input_, author_name)) {
            return true;
        }
        boost::algorithm::trim(author_name);

        if (author_name.empty()) {
            auto author_id = SelectAuthor();
            if (!author_id) {
                return true;
            }
            auto tags = ReadTags();
            use_cases_.AddBook(params->publication_year, params->title,
                               domain::AuthorId::FromString(*author_id), tags);
            return true;
        }

        auto author = use_cases_.FindAuthorByName(author_name);
        if (!author.has_value()) {
            output_ << "No author found. Do you want to add " << author_name << " (y/n)?"
                    << std::endl;
            std::string answer;
            if (!std::getline(input_, answer)) {
                return true;
            }
            if (answer != "y"s && answer != "Y"s) {
                output_ << "Failed to add book"sv << std::endl;
                return true;
            }
            auto tags = ReadTags();
            if (!use_cases_.AddBookWithAuthorName(params->publication_year, params->title,
                                                  author_name, true, tags)) {
                output_ << "Failed to add book"sv << std::endl;
            }
            return true;
        }

        auto tags = ReadTags();
        use_cases_.AddBook(params->publication_year, params->title, author->GetId(), tags);
    } catch (const std::exception&) {
        output_ << "Failed to add book"sv << std::endl;
    }
    return true;
}

bool View::DeleteAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::optional<std::string> author_id;
        if (name.empty()) {
            author_id = SelectAuthor();
            if (!author_id) {
                return true;
            }
        } else {
            auto author = use_cases_.FindAuthorByName(name);
            if (!author) {
                output_ << "Failed to delete author"sv << std::endl;
                return true;
            }
            author_id = author->GetId().ToString();
        }

        if (!use_cases_.DeleteAuthor(domain::AuthorId::FromString(*author_id))) {
            output_ << "Failed to delete author"sv << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Failed to delete author"sv << std::endl;
    }
    return true;
}

bool View::EditAuthor(std::istream& cmd_input) const {
    try {
        std::string name;
        std::getline(cmd_input, name);
        boost::algorithm::trim(name);

        std::optional<std::string> author_id;
        if (name.empty()) {
            author_id = SelectAuthor();
            if (!author_id) {
                return true;
            }
        } else {
            auto author = use_cases_.FindAuthorByName(name);
            if (!author) {
                output_ << "Failed to edit author"sv << std::endl;
                return true;
            }
            author_id = author->GetId().ToString();
        }

        output_ << "Enter new name:" << std::endl;
        std::string new_name;
        if (!std::getline(input_, new_name)) {
            return true;
        }
        boost::algorithm::trim(new_name);
        if (!use_cases_.EditAuthor(domain::AuthorId::FromString(*author_id), new_name)) {
            output_ << "Failed to edit author"sv << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Failed to edit author"sv << std::endl;
    }
    return true;
}

bool View::ShowAuthors() const {
    PrintAuthors(output_, GetAuthors());
    return true;
}

bool View::ShowBooks() const {
    PrintVector(output_, GetBooks());
    return true;
}

bool View::ShowBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::optional<detail::BookInfo> book;
        if (!title.empty()) {
            auto matches = GetBooksByTitle(title);
            if (matches.empty()) {
                return true;
            }
            if (matches.size() == 1) {
                book = matches.front();
            } else {
                book = SelectBook(matches, "Enter the book # or empty line to cancel:");
            }
        } else {
            auto all_books = GetBooks();
            if (all_books.empty()) {
                return true;
            }
            book = SelectBook(all_books, "Enter the book # or empty line to cancel:");
        }

        if (!book) {
            return true;
        }

        auto details = GetBookDetails(book->id);
        if (!details) {
            return true;
        }

        output_ << "Title: " << details->title << std::endl;
        output_ << "Author: " << details->author_name << std::endl;
        output_ << "Publication year: " << details->publication_year << std::endl;
        if (!details->tags.empty()) {
            output_ << "Tags: " << JoinTags(details->tags) << std::endl;
        }
    } catch (const std::exception&) {
        return true;
    }
    return true;
}

bool View::ShowAuthorBooks() const {
    try {
        if (auto author_id = SelectAuthor()) {
            PrintVector(output_, GetAuthorBooks(*author_id));
        }
    } catch (const std::exception&) {
        throw std::runtime_error("Failed to Show Books");
    }
    return true;
}

bool View::DeleteBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::optional<detail::BookInfo> book;
        if (!title.empty()) {
            auto matches = GetBooksByTitle(title);
            if (matches.empty()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            if (matches.size() == 1) {
                book = matches.front();
            } else {
                book = SelectBook(matches, "Enter the book # or empty line to cancel:");
            }
        } else {
            auto all_books = GetBooks();
            if (all_books.empty()) {
                return true;
            }
            book = SelectBook(all_books, "Enter the book # or empty line to cancel:");
        }

        if (!book) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }
        if (!use_cases_.DeleteBook(domain::BookId::FromString(book->id))) {
            output_ << "Failed to delete book"sv << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Failed to delete book"sv << std::endl;
    }
    return true;
}

bool View::EditBook(std::istream& cmd_input) const {
    try {
        std::string title;
        std::getline(cmd_input, title);
        boost::algorithm::trim(title);

        std::optional<detail::BookInfo> book;
        if (!title.empty()) {
            auto matches = GetBooksByTitle(title);
            if (matches.empty()) {
                output_ << "Book not found"sv << std::endl;
                return true;
            }
            if (matches.size() == 1) {
                book = matches.front();
            } else {
                book = SelectBook(matches, "Enter the book # or empty line to cancel:");
            }
        } else {
            auto all_books = GetBooks();
            if (all_books.empty()) {
                return true;
            }
            book = SelectBook(all_books, "Enter the book # or empty line to cancel:");
        }

        if (!book) {
            return true;
        }

        auto details = GetBookDetails(book->id);
        if (!details) {
            output_ << "Book not found"sv << std::endl;
            return true;
        }

        output_ << "Enter new title or empty line to use the current one (" << details->title
                << "):" << std::endl;
        std::string new_title;
        if (!std::getline(input_, new_title)) {
            return true;
        }
        boost::algorithm::trim(new_title);
        if (new_title.empty()) {
            new_title = details->title;
        }

        output_ << "Enter publication year or empty line to use the current one ("
                << details->publication_year << "):" << std::endl;
        std::string year_text;
        if (!std::getline(input_, year_text)) {
            return true;
        }
        boost::algorithm::trim(year_text);
        int new_year = details->publication_year;
        if (!year_text.empty()) {
            new_year = std::stoi(year_text);
        }

        auto tags = ReadTags(details->tags);
        if (!use_cases_.EditBook(domain::BookId::FromString(details->id), new_title, new_year,
                                 tags)) {
            output_ << "Book not found"sv << std::endl;
        }
    } catch (const std::exception&) {
        output_ << "Book not found"sv << std::endl;
    }
    return true;
}

std::optional<detail::AddBookParams> View::GetBookParams(std::istream& cmd_input) const {
    detail::AddBookParams params;

    if (!(cmd_input >> params.publication_year)) {
        throw std::runtime_error("Invalid publication year");
    }
    std::getline(cmd_input, params.title);
    boost::algorithm::trim(params.title);
    if (params.title.empty()) {
        throw std::runtime_error("Invalid title");
    }

    return params;
}

std::optional<std::string> View::SelectAuthor() const {
    output_ << "Select author:" << std::endl;
    auto authors = GetAuthors();
    PrintVector(output_, authors);
    output_ << "Enter author # or empty line to cancel" << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int author_idx;
    try {
        author_idx = std::stoi(str);
    } catch (std::exception const&) {
        throw std::runtime_error("Invalid author num");
    }

    --author_idx;
    if (author_idx < 0 || author_idx >= static_cast<int>(authors.size())) {
        throw std::runtime_error("Invalid author num");
    }

    return authors[author_idx].id;
}

std::optional<detail::BookInfo> View::SelectBook(const std::vector<detail::BookInfo>& books,
                                                 const std::string& prompt) const {
    PrintVector(output_, books);
    output_ << prompt << std::endl;

    std::string str;
    if (!std::getline(input_, str) || str.empty()) {
        return std::nullopt;
    }

    int book_idx;
    try {
        book_idx = std::stoi(str);
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid book num");
    }

    --book_idx;
    if (book_idx < 0 || book_idx >= static_cast<int>(books.size())) {
        throw std::runtime_error("Invalid book num");
    }

    return books[book_idx];
}

std::vector<detail::AuthorInfo> View::GetAuthors() const {
    std::vector<detail::AuthorInfo> dst_autors;
    auto authors = use_cases_.GetAuthors();
    dst_autors.reserve(authors.size());
    for (const auto& author : authors) {
        dst_autors.push_back({author.GetId().ToString(), author.GetName()});
    }
    return dst_autors;
}

std::vector<detail::BookInfo> View::GetBooks() const {
    std::vector<detail::BookInfo> books;
    auto all_books = use_cases_.GetBooks();
    books.reserve(all_books.size());
    for (const auto& book : all_books) {
        books.push_back({book.id.ToString(), book.title, book.author_name,
                         book.publication_year});
    }
    return books;
}

std::vector<detail::BookInfo> View::GetBooksByTitle(const std::string& title) const {
    std::vector<detail::BookInfo> books;
    auto matches = use_cases_.GetBooksByTitle(title);
    books.reserve(matches.size());
    for (const auto& book : matches) {
        books.push_back({book.id.ToString(), book.title, book.author_name,
                         book.publication_year});
    }
    return books;
}

std::optional<detail::BookDetails> View::GetBookDetails(const std::string& book_id) const {
    auto details = use_cases_.GetBookDetails(domain::BookId::FromString(book_id));
    if (!details) {
        return std::nullopt;
    }

    detail::BookDetails result;
    result.id = details->info.id.ToString();
    result.title = details->info.title;
    result.author_name = details->info.author_name;
    result.publication_year = details->info.publication_year;
    result.tags = details->tags;
    return result;
}

std::vector<detail::AuthorBookInfo> View::GetAuthorBooks(const std::string& author_id) const {
    std::vector<detail::AuthorBookInfo> books;
    auto author_books = use_cases_.GetAuthorBooks(domain::AuthorId::FromString(author_id));
    books.reserve(author_books.size());
    for (const auto& book : author_books) {
        books.push_back({book.GetTitle(), book.GetPublicationYear()});
    }
    return books;
}

std::vector<std::string> View::ReadTags(const std::vector<std::string>& current_tags) const {
    output_ << "Enter tags (current tags: " << JoinTags(current_tags) << "):" << std::endl;
    std::string line;
    if (!std::getline(input_, line)) {
        return {};
    }
    return NormalizeTags(line);
}

std::vector<std::string> View::ReadTags() const {
    output_ << "Enter tags (comma separated):" << std::endl;
    std::string line;
    if (!std::getline(input_, line)) {
        return {};
    }
    return NormalizeTags(line);
}

}  // namespace ui
