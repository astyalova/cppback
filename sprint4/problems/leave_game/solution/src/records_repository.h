#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace db {

struct RetiredPlayerRecord {
    std::string name;
    int score = 0;
    double play_time = 0.0;
};

class RecordsRepository {
public:
    virtual ~RecordsRepository() = default;

    virtual void AddRecord(std::string_view name, int score, double play_time) = 0;
    virtual std::vector<RetiredPlayerRecord> GetRecords(size_t start, size_t max_items) = 0;
};

using RecordsRepositoryPtr = std::shared_ptr<RecordsRepository>;

}  // namespace db
