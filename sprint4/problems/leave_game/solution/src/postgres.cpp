#include "postgres.h"

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

#include <cstdint>

namespace db {

namespace {
constexpr const char* kInsertRecord = "insert_retired_player";
constexpr const char* kSelectRecords = "select_retired_players";
}  // namespace

PostgresRecordsRepository::PostgresRecordsRepository(std::string db_url, size_t pool_size)
    : pool_{pool_size, [url = std::move(db_url)] {
        auto conn = std::make_shared<pqxx::connection>(url);
        conn->prepare(kInsertRecord,
            "INSERT INTO retired_players (id, name, score, play_time_ms) "
            "VALUES ($1, $2, $3, $4)");
        conn->prepare(kSelectRecords,
            "SELECT name, score, play_time_ms FROM retired_players "
            "ORDER BY score DESC, play_time_ms ASC, name ASC "
            "OFFSET $1 LIMIT $2");
        return conn;
    }} {
    EnsureSchema();
}

void PostgresRecordsRepository::EnsureSchema() {
    auto conn = pool_.GetConnection();
    pqxx::work work{*conn};
    work.exec(
        "CREATE TABLE IF NOT EXISTS retired_players ("
        "id UUID PRIMARY KEY,"
        "name TEXT NOT NULL,"
        "score INTEGER NOT NULL,"
        "play_time_ms BIGINT NOT NULL"
        ")");
    work.exec(
        "CREATE INDEX IF NOT EXISTS retired_players_score_time_name_idx "
        "ON retired_players (score DESC, play_time_ms ASC, name ASC)");
    work.commit();
}

void PostgresRecordsRepository::AddRecord(std::string_view name, int score, std::chrono::milliseconds play_time) {
    auto conn = pool_.GetConnection();
    pqxx::work work{*conn};
    boost::uuids::random_generator gen;
    auto id = boost::uuids::to_string(gen());
    work.exec_prepared(kInsertRecord, id, name, score, play_time.count());
    work.commit();
}

std::vector<RetiredPlayerRecord> PostgresRecordsRepository::GetRecords(size_t start, size_t max_items) {
    auto conn = pool_.GetConnection();
    pqxx::read_transaction read_tx{*conn};
    pqxx::result rows = read_tx.exec_prepared(kSelectRecords, start, max_items);
    std::vector<RetiredPlayerRecord> records;
    records.reserve(rows.size());
    for (const auto& row : rows) {
        RetiredPlayerRecord rec;
        rec.name = row["name"].c_str();
        rec.score = row["score"].as<int>();
        rec.play_time = std::chrono::milliseconds{row["play_time_ms"].as<std::int64_t>()};
        records.push_back(std::move(rec));
    }
    return records;
}

}  // namespace db
