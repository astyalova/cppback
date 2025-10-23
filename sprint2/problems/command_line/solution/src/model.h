#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <random>
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

    Map(Id id, std::string name, const double speed) noexcept
        : id_(std::move(id))
        , name_(std::move(name))
        , speed_(speed) {}

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

    double GetSpeed() const noexcept {
        return speed_;
    }

private:
    using OfficeIdToIndex = std::unordered_map<Office::Id, size_t, util::TaggedHasher<Office::Id>>;

    Id id_;
    std::string name_;
    Roads roads_;
    Buildings buildings_;

    OfficeIdToIndex warehouse_id_to_index_;
    Offices offices_;
    double speed_;
};

enum class Direction {
    NORTH,
    SOUTH,
    WEST,
    EAST
};


std::string GetDirAsStr(Direction dir) noexcept;
Direction GetDirFromStr(const std::string& dir) noexcept;

class Dog {
public:


    struct Coordinate {
        double x;
        double y;
    };

    struct Speed {
        double x;
        double y;
    };

    static constexpr Coordinate DEFAULT_POSITION = Coordinate{0.0, 0.0};
    static constexpr Speed DEFAULT_SPEED = Speed{0.0, 0.0};

    Dog(std::uint64_t token, std::string nickname, Coordinate coord = DEFAULT_POSITION, Speed speed = DEFAULT_SPEED)
        : token_(token), nickname_(std::move(nickname)), coord_(coord), speed_(speed) {}

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

    Direction GetDirFromChar() const noexcept {
        if(GetDirAsChar() == 'R') {
            return Direction::EAST;
        } else if(GetDirAsChar() == 'L') {
            return Direction::WEST;
        } else if(GetDirAsChar() == 'D') {
            return Direction::SOUTH;
        } 
        return Direction::NORTH;
    }

    Coordinate GetCoord() const noexcept {
        return coord_;
    }

    Speed GetSpeed() const noexcept {
        return speed_;
    }

    void SetSpeed(Speed speed) {
        speed_ = speed;
    }

    void SetDir(Direction dir) {
        dir_ = dir;
    }

    void SetCoord(Coordinate coord) {
        coord_ = coord;
    }

private:
    std::uint64_t token_;
    std::string nickname_;
    Coordinate coord_ {0.0, 0.0};
    Direction dir_ = Direction::NORTH;
    Speed speed_ {0.0, 0.0};
};

class GameSession {
public:
    explicit GameSession(const Map* map) : map_(map) {}

    GameSession(const GameSession&) = delete;
    GameSession& operator=(const GameSession&) = delete;

    Dog* CreateDog(const std::string& name, bool spawn = false) {
        auto dog = dogs_.emplace_back(
            std::make_unique<Dog>(
                dogs_.size(),
                name,
                GenerateNewPosition(spawn) 
            )
        ).get();

        dogs_id_[dog->GetToken()] = dog;
        return dog;
    }


    Dog::Coordinate GenerateNewPosition(bool randomize_spawn_point = false) const noexcept {
        if (!randomize_spawn_point) {
            return Dog::Coordinate {double(map_->GetRoads().at(0).GetStart().x), 
                        double(map_->GetRoads().at(0).GetStart().y)};
        }

        std::random_device rand_device; 
        std::mt19937_64 rand_engine(rand_device());

        std::uniform_int_distribution<std::mt19937_64::result_type> unif(0, map_->GetRoads().size() - 1);

        const auto& road = map_->GetRoads().at(unif(rand_engine));
        auto r_start = road.GetStart();
        auto r_end = road.GetEnd();
        
        Dog::Coordinate pos{0.0, 0.0}; 
        if (std::abs(r_start.x - r_end.x) > std::abs(r_start.y - r_end.y)) {
            std::uniform_real_distribution<double> unif_d(std::min(r_start.x, r_end.x), std::max(r_start.x, r_end.x));
            pos.x = unif_d(rand_engine);
            pos.y = (((pos.x - double(r_start.x)) * double(r_end.y - r_start.y)) / double(r_end.x - r_start.x)) + double(r_start.y);
        } else {
            std::uniform_real_distribution<double> unif_d(std::min(r_start.y, r_end.y), std::max(r_start.y, r_end.y));
            pos.y = unif_d(rand_engine);
            pos.x = (((pos.y - double(r_start.y)) * double(r_end.x - r_start.x)) / double(r_end.y - r_start.y)) + double(r_start.x);
        }
        return pos;
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

    Game(double speed = {1.0}) : speed_{speed}
    {}

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

    double GetSpeed() const noexcept {
        return speed_;
    }

private:
    using MapIdHasher = util::TaggedHasher<Map::Id>;
    using MapIdToIndex = std::unordered_map<Map::Id, size_t, MapIdHasher>;
    using Sessions = std::vector<std::unique_ptr<GameSession>>;

    std::vector<Map> maps_;
    MapIdToIndex map_id_to_index_;
    Sessions sessions_;
    double speed_;
};


} // namespace model