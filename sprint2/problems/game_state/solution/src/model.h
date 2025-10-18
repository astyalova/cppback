#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>

#include "tagged.h"

namespace model {

using Dimension = int;
using Coord = Dimension;

struct Point {
    Coord x, y;
};

struct Size {
    Dimension width, height;
};

struct Rectangle {
    Point position;
    Size size;
};

struct Offset {
    Dimension dx, dy;
};

class Road {
    struct HorizontalTag {
        explicit HorizontalTag() = default;
    };

    struct VerticalTag {
        explicit VerticalTag() = default;
    };

public:
    constexpr static HorizontalTag HORIZONTAL{};
    constexpr static VerticalTag VERTICAL{};

    Road(HorizontalTag, Point start, Coord end_x) noexcept
        : start_{start}
        , end_{end_x, start.y} {
    }

    Road(VerticalTag, Point start, Coord end_y) noexcept
        : start_{start}
        , end_{start.x, end_y} {
    }

    bool IsHorizontal() const noexcept {
        return start_.y == end_.y;
    }

    bool IsVertical() const noexcept {
        return start_.x == end_.x;
    }

    Point GetStart() const noexcept {
        return start_;
    }

    Point GetEnd() const noexcept {
        return end_;
    }

private:
    Point start_;
    Point end_;
};

class Building {
public:
    explicit Building(Rectangle bounds) noexcept
        : bounds_{bounds} {
    }

    const Rectangle& GetBounds() const noexcept {
        return bounds_;
    }

private:
    Rectangle bounds_;
};

class Office {
public:
    using Id = util::Tagged<std::string, Office>;

    Office(Id id, Point position, Offset offset) noexcept
        : id_{std::move(id)}
        , position_{position}
        , offset_{offset} {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    Point GetPosition() const noexcept {
        return position_;
    }

    Offset GetOffset() const noexcept {
        return offset_;
    }

private:
    Id id_;
    Point position_;
    Offset offset_;
};

class Map {
public:
    using Id = util::Tagged<std::string, Map>;
    using Roads = std::vector<Road>;
    using Buildings = std::vector<Building>;
    using Offices = std::vector<Office>;

    Map(Id id, std::string name) noexcept
        : id_(std::move(id))
        , name_(std::move(name)) {
    }

    const Id& GetId() const noexcept {
        return id_;
    }

    const std::string& GetName() const noexcept {
        return name_;
    }

    const Buildings& GetBuildings() const noexcept {
        return buildings_;
    }

    const Roads& GetRoads() const noexcept {
        return roads_;
    }

    const Offices& GetOffices() const noexcept {
        return offices_;
    }

    void AddRoad(const Road& road) {
        roads_.emplace_back(road);
    }

    void AddBuilding(const Building& building) {
        buildings_.emplace_back(building);
    }

    void AddOffice(Office office);

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
};

class Dog {
public:

    enum class Direction {
        NORTH,
        SOUTH,
        WEST,
        EAST
    };

    struct Coordinate {
        double x;
        double y;
    };

    struct Speed {
        double x;
        double y;
    };

    Dog(std::uint64_t token, std::string nickname, Coordinate coord = {0.0, 0.0})
        : token_(token), nickname_(std::move(nickname)), coord_(coord) {}

    std::uint64_t GetToken() const noexcept {
        return token_;
    }

    const std::string& GetNickname() const noexcept {
        return nickname_;
    }

    Direction GetDir() const noexcept {
        return dir_;
    }

    char GetDirAsChar() const noexcept {
        if(GetDir() == Direction::EAST) {
            return 'R';
        } else if(GetDir() == Direction::WEST) {
            return 'L';
        } else if(GetDir() == Direction::SOUTH) {
            return 'D';
        } 
        return 'U';
    }

    Coordinate GetCoord() const noexcept {
        return coord_;
    }

    Speed GetSpeed() const noexcept {
        return speed_;
    }

private:
    std::uint64_t token_;
    std::string nickname_;
    Coordinate coord_ = {0.0, 0.0};
    Direction dir_ = Direction::NORTH;
    Speed speed_ = {0.0, 0.0};
};

class GameSession {
public:
    explicit GameSession(const Map* map) : map_(map) {}

    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    Dog* CreateDog(const std::string& name) {
        auto dog = dogs_.emplace_back(std::make_unique<Dog>(dogs_.size(), name)).get();
        dogs_id_[dog->GetToken()] = dog;
        return dog;
    }

    std::vector<Dog*> GetDogs() const {
        std::vector<Dog*> result;
        result.reserve(dogs_.size());
        for (const auto& dog : dogs_) {
            result.push_back(dog.get());
        }
        return result;
    }

    const Map* GetMap() const {
        return map_;
    }

private:
    std::vector<std::unique_ptr<Dog>> dogs_;
    std::unordered_map<std::uint64_t, Dog*> dogs_id_;
    const Map* map_;
};

class Game {
public:
    using Maps = std::vector<Map>;

    void AddMap(Map map);

    const Maps& GetMaps() const noexcept {
        return maps_;
    }

    const Map* FindMap(const Map::Id& id) const noexcept {
        if (auto it = map_id_to_index_.find(id); it != map_id_to_index_.end()) {
            return &maps_.at(it->second);
        }
        return nullptr;
    }

    GameSession* CreateSession(const Map* map) {
        return sessions_.emplace_back(std::make_unique<GameSession>(map)).get();
    }

    GameSession* FindSession(const Map* map) const {
        auto it = std::find_if(
            sessions_.begin(), sessions_.end(),
            [map](const auto& session) { return session->GetMap() == map; }
        );
        if (it != sessions_.end()) {
            return it->get();
        }
        return nullptr;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    using Sessions = std::vector<std::unique_ptr<GameSession>>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    Sessions sessions_;
};


} // namespace model