#include "postgres.h"

#include <pqxx/pqxx>
#include <pqxx/zview.hxx>

namespace postgres {

using namespace std::literals;
using pqxx::operator"" _zv;

Database::Database(const std::string& db_url)
    : connection_{db_url} {
    pqxx::work work{connection_};
    work.exec(R"(
CREATE TABLE IF NOT EXISTS retired_players (
    id UUID CONSTRAINT retired_players_pkey PRIMARY KEY,
    name varchar(100) NOT NULL,
    score INTEGER NOT NULL,
    play_time DOUBLE PRECISION NOT NULL
);
)"_zv);
    work.exec(R"(
CREATE INDEX IF NOT EXISTS idx_retired_players_score 
ON retired_players (score DESC, play_time, name);
)"_zv);
    work.commit();
}

void Database::SaveRecord(const std::string& name, int score, double play_time) {
    pqxx::work work{connection_};
    work.exec_params(
        R"(
INSERT INTO retired_players (id, name, score, play_time) VALUES ($1, $2, $3, $4);
)"_zv,
        PlayerId::New().ToString(), name, score, play_time);
    work.commit();
}

std::vector<PlayerRecord> Database::GetRecords(size_t start, size_t limit) {
    pqxx::read_transaction r(connection_);
    auto query_text = "SELECT name, score, play_time FROM retired_players "
                      "ORDER BY score DESC, play_time, name "
                      "OFFSET " + std::to_string(start) + " "
                      "LIMIT " + std::to_string(limit) + ";";

    std::vector<PlayerRecord> res;
    for (auto [name, score, play_time] : r.query<std::string, int, double>(query_text)) {
        res.emplace_back(PlayerRecord{name, score, play_time});
    }
    return res;
}

}  // namespace postgres
