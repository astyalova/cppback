#pragma once


#include "json_loader.h"
#include "json_serializer.h"
#include "model.h"
#include "player.h"
#include "extra_data.h"

#include <boost/json.hpp>
#include <stdexcept>
#include <string>
#include <chrono>

class AppErrorException : public std::invalid_argument {
public:
    enum class Category {
        EmptyPlayerName,
        NoPlayerWithToken,
        InvalidMapId,
        InvalidDirection,
        InvalidTime
    };

    AppErrorException(std::string msg, Category category)
        : std::invalid_argument(std::move(msg))
        , category_(category) {}

    Category GetCategory() const noexcept { return category_; }

private:
    Category category_;
};

class Application {
public:
    Application(model::Game&& game, bool spawn = false, bool auto_tick_enabled = false)
        : game_(std::move(game))
        , spawn_(spawn)
        , auto_tick_enabled_(auto_tick_enabled) {}

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] bool GetAutoTick() const noexcept { return auto_tick_enabled_; }

    [[nodiscard]] std::string GetMapsShortInfo() const noexcept {
        return json_serializer::SerializeMaps(game_.GetMaps());
    }

[[nodiscard]] std::string GetMapInfo(const std::string& map_id) const {
    auto map = game_.FindMap(model::Map::Id{map_id});
    if (!map) {
        throw AppErrorException("Map not found", AppErrorException::Category::InvalidMapId);
    }

    boost::json::object map_json;
    map_json["id"] = *map->GetId();
    map_json["name"] = map->GetName();

    json_serializer::SerializeBuildings(*map, map_json);
    json_serializer::SerializeRoads(*map, map_json);
    json_serializer::SerializeOffices(*map, map_json);

    if (const auto *loot_types_ptr = extra_data::ExtraDataRepository::GetInstance().GetLootTypes(map->GetId())) {
        map_json["lootTypes"] = *loot_types_ptr;
    } else {
        map_json["lootTypes"] = boost::json::array{};
    }

    return boost::json::serialize(map_json);
}



    [[nodiscard]] boost::json::value GetPlayers(const std::string& token) {
        auto player = players_.FindByToken(token);
        if (!player) {
            throw AppErrorException("No player with such token", AppErrorException::Category::NoPlayerWithToken);
        }

        boost::json::object result;
        for (const auto& dog : player->GetSession()->GetDogs()) {
            result[std::to_string(dog->GetToken())] = boost::json::object{
                {"name", dog->GetNickname()}
            };
        }
        return result;
    }

    [[nodiscard]] boost::json::value JoinGame(const std::string& user_name, const std::string& map_id) {
        if (user_name.empty()) {
            throw AppErrorException("Empty player name", AppErrorException::Category::EmptyPlayerName);
        }

        auto map = game_.FindMap(model::Map::Id{map_id});
        if (!map) {
            throw AppErrorException("Map not found", AppErrorException::Category::InvalidMapId);
        }

        auto session = game_.FindSession(map);
        if (!session) {
            session = game_.CreateSession(map);
        }

        auto dog = session->CreateDog(user_name, spawn_);
        std::pair<player::Player*, std::string> player_info = players_.Add(dog, session);

        return boost::json::object{
            {"authToken", player_info.second},
            {"playerId", player_info.first->GetDogId()}
        };
    }

    [[nodiscard]] boost::json::value GetGameState(const player::Players::Token& token) {
        auto player = players_.FindByToken(token);
        if (!player) {
            throw AppErrorException("No player with such token", AppErrorException::Category::NoPlayerWithToken);
        }

        boost::json::object players_by_id;
        if (auto session = player->GetSession()) {
            for (const auto& dog : session->GetDogs()) {
                boost::json::array bag_array;
                for (const auto& item : dog->GetBag()) {
                    bag_array.push_back(boost::json::object{
                        {"id", item.id},
                        {"type", item.type}
                    });
                }
                players_by_id[std::to_string(dog->GetToken())] = boost::json::object {
                    {"pos", boost::json::array{ static_cast<double>(dog->GetCoord().x),
                                            static_cast<double>(dog->GetCoord().y) }},
                    {"speed", boost::json::array{ static_cast<double>(dog->GetSpeed().x),
                                                static_cast<double>(dog->GetSpeed().y) }},
                    {"dir", player::GetDirAsStr(dog->GetDir())},
                    {"bag", std::move(bag_array)},
                    {"score", dog->GetScore()}
                };
            }
        }

        boost::json::array lost_objects_json;
        if (auto session = player->GetSession()) {
            auto lost_objects = session->GetLostObjects();
            for (const auto& [id, obj] : lost_objects) {
            lost_objects_json.push_back(boost::json::object{
                {"id", id},
                {"type", obj.type},
                {"pos", boost::json::array{obj.position.x, obj.position.y }}
            });
            }
        }

        return boost::json::object{
            {"players", std::move(players_by_id)},
            {"lostObjects", std::move(lost_objects_json)}
        };
    }


    void ActionPlayer(const player::Players::Token& token, const std::string& direction_str) {
        std::optional<model::Direction> dir;
        if (!direction_str.empty()) {
            try {
                dir = player::GetDirFromStr(direction_str);
            } catch (const std::exception& e) {
                throw AppErrorException("Invalid direction", AppErrorException::Category::InvalidDirection);
            }
        }

        auto player = players_.FindByToken(token);
        if (!player) {
            throw AppErrorException("No player with such token", AppErrorException::Category::NoPlayerWithToken);
        }

        if (dir) {
            player->ChangeDir(*dir);
        } else {
            player->SetSpeed(model::Dog::Speed{0.0, 0.0});
        }
    }

    void Tick(std::chrono::milliseconds delta) {
        if (delta < static_cast<std::chrono::milliseconds>(0)) {
            throw AppErrorException("Negative time delta", AppErrorException::Category::InvalidTime);
        }
        players_.MovePlayers(delta);

        for (auto& session : game_.GetSessions()) {
            session->AddRandomLoot(delta);
            session->HandleCollisions(delta);
        }
    }

    [[nodiscard]] std::unordered_map<int, model::LostObject> GetLostObjects(const player::Players::Token& token) {
        auto player = players_.FindByToken(token);
        if (!player) {
            throw AppErrorException("No player with such token", AppErrorException::Category::NoPlayerWithToken);
        }

        auto session = player->GetSession();
        if (!session) return {};

        return session->GetLostObjects();
    }

    struct MapLostObjectsInfo {
        int loot_type_count;
    };

    MapLostObjectsInfo GetMapLostObjectsInfo(const std::string& map_id) {
        auto map = game_.FindMap(model::Map::Id{map_id});
        if (!map) {
            throw AppErrorException("Map not found", AppErrorException::Category::InvalidMapId);
        }
        return MapLostObjectsInfo{map->GetLootTypeCount()};
    }

private:
    model::Game game_;
    player::Players players_;
    bool spawn_;
    bool auto_tick_enabled_;
};
