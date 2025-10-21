#pragma once

#include "json_loader.h"
#include "model.h"
#include "player.h"

#include <boost/json.hpp>
#include <stdexcept>

using namespace model;
using namespace players;
namespace json = boost::json;
using namespace std::literals;

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
    Application(Game&& game, bool randomize_spawn_points = false, bool auto_tick_enabled = false)
        : game_(std::move(game))
        , randomize_spawn_points_(randomize_spawn_points)
        , auto_tick_enabled_(auto_tick_enabled) {}

    Application(const Application&) = delete;
    Application& operator=(const Application&) = delete;

    [[nodiscard]] bool GetAutoTick() const noexcept { return auto_tick_enabled_; }

    [[nodiscard]] json::value GetMapsShortInfo() const noexcept {
        return json_parser::MapsToShortJson(game_.GetMaps());
    }

    [[nodiscard]] json::value GetMapInfo(const std::string& map_id) const {
        auto map = game_.FindMap(Map::Id{map_id});
        if (!map) {
            throw AppErrorException("Map not found"s, AppErrorException::Category::InvalidMapId);
        }
        return json_parser::MapToJson(map);
    }

    [[nodiscard]] json::value GetPlayers(const Players::Token& token) const {
        auto player = players_.FindByToken(token);
        if (!player) {
            throw AppErrorException("No player with such token"s, AppErrorException::Category::NoPlayerWithToken);
        }

        json::object result;
        for (const auto& dog : player->GetSession()->GetDogs()) {
            result[std::to_string(dog->GetId())] = json::object{{"name"sv, dog->GetName()}};
        }
        return result;
    }

    [[nodiscard]] json::value JoinGame(const std::string& user_name, const std::string& map_id) {
        if (user_name.empty()) {
            throw AppErrorException("Empty player name"s, AppErrorException::Category::EmptyPlayerName);
        }

        auto map = game_.FindMap(Map::Id{map_id});
        if (!map) {
            throw AppErrorException("Map not found"s, AppErrorException::Category::InvalidMapId);
        }

        auto session = game_.FindSession(map);
        if (!session) {
            session = game_.CreateSession(map);
        }

        auto dog = session->CreateDog(user_name, randomize_spawn_points_);
        auto player_info = players_.Add(dog, session);

        return json::object{
            {"authToken"sv, player_info.token},
            {"playerId"sv, player_info.player->GetId()}
        };
    }

    [[nodiscard]] json::value GetGameState(const Players::Token& token) const {
        auto player = players_.FindByToken(token);
        if (!player) {
            throw AppErrorException("No player with such token"s, AppErrorException::Category::NoPlayerWithToken);
        }

        json::object players_by_id;
        for (const auto& dog : player->GetSession()->GetDogs()) {
            players_by_id[std::to_string(dog->GetId())] = json::object{
                {"pos"sv, json::array{dog->GetPosition().x, dog->GetPosition().y}},
                {"speed"sv, json::array{dog->GetSpeed().x, dog->GetSpeed().y}},
                {"dir"sv, GetDirAsChar(dog->GetDir())}
            };
        }

        return json::object{{"players"sv, players_by_id}};
    }

    void ActionPlayer(const Players::Token& token, const std::string& direction_str) {
        std::optional<Direction> dir;
        if (!direction_str.empty()) {
            try {
                dir = GetDirFromChar()(direction_str);
            } catch (const DirectionConvertException&) {
                throw AppErrorException("Invalid direction"s, AppErrorException::Category::InvalidDirection);
            }
        }

        auto player = players_.FindByToken(token);
        if (!player) {
            throw AppErrorException("No player with such token"s, AppErrorException::Category::NoPlayerWithToken);
        }

        if (dir) {
            player->ChangeDir(*dir);
        } else {
            player->SetSpeed(Dog::Speed{0.0, 0.0});
        }
    }

    void Tick(std::chrono::milliseconds delta) {
        if (delta < 0ms) {
            throw AppErrorException("Negative time delta"s, AppErrorException::Category::InvalidTime);
        }
        players_.MovePlayers(delta);
    }

private:
    Game game_;
    Players players_;
    bool randomize_spawn_points_;
    bool auto_tick_enabled_;
};
