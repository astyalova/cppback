#include "model.h"

#include <stdexcept>

namespace model {
using namespace std::literals;

#include <cassert>

CollectionResult TryCollectPoint(Position a, Position b, Position c) {
    // Проверим, что перемещение ненулевое.
    // Тут приходится использовать строгое равенство, а не приближённое,
    // пскольку при сборе заказов придётся учитывать перемещение даже на небольшое
    // расстояние.
    assert(b.x != a.x || b.y != a.y);
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

// В задании на разработку тестов реализовывать следующую функцию не нужно -
// она будет линковаться извне.

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> res;

    for(int i = 0; i < provider.GatherersCount(); ++i) {
        for(int j = 0; j < provider.ItemsCount(); ++j) {
            auto gatherer = provider.GetGatherer(i);
            auto item = provider.GetItem(j);
            auto collect = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);

            if(collect.IsCollected(item.width + gatherer.width)) {
                res.push_back(GatheringEvent(j, i, collect.sq_distance, collect.proj_ratio));
            }
        }
    }

    std::sort(res.begin(), res.end(), [](GatheringEvent& lhs, GatheringEvent& rhs) {
        return lhs.time < rhs.time;
    });
    return res;
}

Dog* GameSession::RestoreDog(const std::string& name,
                             std::uint64_t token,
                             Dog::Coordinate coord,
                             Dog::Speed speed,
                             Direction dir,
                             int bag_capacity,
                             const std::vector<LostObject>& bag,
                             const Position& prev_position,
                             int score) {
    if (bag_capacity < 0) {
        throw std::runtime_error("Invalid bag capacity");
    }
    if (bag.size() > static_cast<size_t>(bag_capacity)) {
        throw std::runtime_error("Bag content exceeds capacity");
    }
    auto dog = dogs_.emplace_back(std::make_unique<Dog>(token, name, coord, speed)).get();
    dogs_id_[dog->GetToken()] = dog;
    dog->SetDir(dir);
    dog->SetBagCapacity(bag_capacity);
    dog->ClearBag();
    for (const auto& item : bag) {
        dog->AddToBag(item);
    }
    dog->SetPrevPosition(prev_position);
    dog->ResetScore();
    dog->AddScore(score);
    return dog;
}

Dog* GameSession::FindDogByToken(std::uint64_t token) const noexcept {
    if (auto it = dogs_id_.find(token); it != dogs_id_.end()) {
        return it->second;
    }
    return nullptr;
}

void GameSession::RestoreLostObjects(std::unordered_map<int, LostObject> loots, int next_loot_id) {
    loots_ = std::move(loots);
    next_loot_id_ = next_loot_id;
}

void GameSession::ClearState() {
    dogs_.clear();
    dogs_id_.clear();
    loots_.clear();
    next_loot_id_ = 0;
}

void Map::AddOffice(Office office) {
    if (warehouse_id_to_index_.contains(office.GetId())) {
        throw std::invalid_argument("Duplicate warehouse");
    }

    const size_t index = offices_.size();
    Office& o = offices_.emplace_back(std::move(office));
    try {
        warehouse_id_to_index_.emplace(o.GetId(), index);
    } catch (...) {
        // Удаляем офис из вектора, если не удалось вставить в unordered_map
        offices_.pop_back();
        throw;
    }
}

void Game::AddMap(Map map) {
    const size_t index = maps_.size();
    if (auto [it, inserted] = map_id_to_index_.emplace(map.GetId(), index); !inserted) {
        throw std::invalid_argument("Map with id "s + *map.GetId() + " already exists"s);
    } else {
        try {
            maps_.emplace_back(std::move(map));
        } catch (...) {
            map_id_to_index_.erase(it);
            throw;
        }
    }
}

std::string GetDirAsStr(Direction dir) noexcept {
    static const auto info = std::unordered_map<Direction, std::string>{
        {Direction::NORTH, "U"},
        {Direction::SOUTH, "D"},
        {Direction::WEST, "L"},
        {Direction::EAST, "R"}
    };
    return info.at(dir);
    }

Direction GetDirFromStr(const std::string& dir) noexcept {
    static const auto info = std::unordered_map<std::string, Direction>{
        {"U", Direction::NORTH},
        {"D", Direction::SOUTH},
        {"L", Direction::WEST},
        {"R", Direction::EAST}
    };

    return info.at(dir);
}

}  // namespace model
