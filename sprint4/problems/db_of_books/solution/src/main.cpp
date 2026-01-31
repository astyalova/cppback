#include <boost/json.hpp>
#include <pqxx/pqxx>

#include <cstdlib>
#include <iostream>
#include <string>

namespace json = boost::json;

void EnsureTable(pqxx::connection& connection) {
    pqxx::work work{connection};
    work.exec(
        "CREATE TABLE IF NOT EXISTS books ("
        "id SERIAL PRIMARY KEY,"
        "title varchar(100) NOT NULL,"
        "author varchar(100) NOT NULL,"
        "year integer NOT NULL,"
        "\"ISBN\" char(13) UNIQUE NULL"
        ")");
    work.commit();
}

json::object MakeResult(bool ok) {
    json::object response;
    response["result"] = ok;
    return response;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        return 1;
    }

    pqxx::connection connection{argv[1]};
    EnsureTable(connection);

    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) {
            continue;
        }

        json::value root = json::parse(line);
        json::object& obj = root.as_object();
        std::string action = json::value_to<std::string>(obj.at("action"));

        if (action == "exit") {
            break;
        }

        if (action == "add_book") {
            bool ok = true;
            try {
                json::object& payload = obj.at("payload").as_object();
                std::string title = json::value_to<std::string>(payload.at("title"));
                std::string author = json::value_to<std::string>(payload.at("author"));
                int year = payload.at("year").as_int64();
                json::value isbn_value = payload.at("ISBN");

                pqxx::work work{connection};
                if (isbn_value.is_null()) {
                    work.exec_params(
                        "INSERT INTO books (title, author, year, \"ISBN\") VALUES ($1, $2, $3, $4)",
                        title,
                        author,
                        year,
                        pqxx::null());
                } else {
                    std::string isbn = json::value_to<std::string>(isbn_value);
                    work.exec_params(
                        "INSERT INTO books (title, author, year, \"ISBN\") VALUES ($1, $2, $3, $4)",
                        title,
                        author,
                        year,
                        isbn);
                }
                work.commit();
            } catch (const pqxx::sql_error&) {
                ok = false;
            }

            std::cout << json::serialize(MakeResult(ok)) << '\n';
            continue;
        }

        if (action == "all_books") {
            pqxx::read_transaction read_tx{connection};
            pqxx::result rows = read_tx.exec(
                "SELECT id, title, author, year, \"ISBN\" FROM books "
                "ORDER BY year DESC, title ASC, author ASC, \"ISBN\" ASC NULLS LAST");

            json::array out;
            out.reserve(rows.size());
            for (const auto& row : rows) {
                json::object book;
                book["id"] = row["id"].as<int>();
                book["title"] = row["title"].c_str();
                book["author"] = row["author"].c_str();
                book["year"] = row["year"].as<int>();
                if (row["ISBN"].is_null()) {
                    book["ISBN"] = nullptr;
                } else {
                    book["ISBN"] = row["ISBN"].c_str();
                }
                out.emplace_back(std::move(book));
            }

            std::cout << json::serialize(out) << '\n';
            continue;
        }
    }

    return 0;
}
