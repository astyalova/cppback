#pragma once


#include "json_loader.h"
#include "json_serializer.h"
#include "model.h"
#include "player.h"

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

        map_json["id"] = map->GetId().Get();
        map_json["name"] = map->GetName();

        json_serializer::SerializeBuildings(*map, map_json);
        json_serializer::SerializeRoads(*map, map_json);
        json_serializer::SerializeOffices(*map, map_json);

        boost::json::array loot_types_json;
        for (int i = 0; i < map->GetLootTypeCount(); ++i) {
            loot_types_json.push_back(boost::json::object{{"type", i}});
        }
        map_json["lootTypes"] = loot_types_json;

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
        for (const auto& dog : player->GetSession()->GetDogs()) {
            players_by_id[std::to_string(dog->GetToken())] = boost::json::object {
                {"pos", boost::json::array{dog->GetCoord().x, dog->GetCoord().y}},
                {"speed", boost::json::array{dog->GetSpeed().x, dog->GetSpeed().y}},
                {"dir", player::GetDirAsStr(dog->GetDir())}
            };
        }

        boost::json::array lost_objects_json;
        auto session = player->GetSession();
        if (session) {
            auto lost_objects = session->GetLostObjects();
            for (const auto& [id, obj] : lost_objects) {
                lost_objects_json.push_back(boost::json::object{
                    {"type", obj.type}
                });
            }
        }
        return boost::json::object{{"players", players_by_id}, {"lostObjects", lost_objects_json}};
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
    }

    [[nodiscard]] std::unordered_map<int, model::GameSession::Loot> GetLostObjects(const player::Players::Token& token) {
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
