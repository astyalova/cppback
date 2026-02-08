#pragma once

#include <pqxx/connection>
#include <pqxx/transaction>
#include <string>
#include <vector>

#include "tagged_uuid.h"

namespace postgres {

namespace detail {
struct PlayerTag {};
}  // namespace detail

using PlayerId = util::TaggedUUID<detail::PlayerTag>;

struct PlayerRecord {
    std::string name;
    int score;
    double play_time;
};

class Database {
public:
    explicit Database(const std::string& db_url);

    void SaveRecord(const std::string& name, int score, double play_time);
    std::vector<PlayerRecord> GetRecords(size_t start, size_t limit);

private:
    pqxx::connection connection_;
};

}  // namespace postgres
