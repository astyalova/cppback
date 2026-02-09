#pragma once

#include "model.h"

#include <algorithm>
#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>
#include <optional>
#include <unordered_set>

namespace player {
    using namespace model;

    class Player {
    public:
        static constexpr double HALF_WIDTH = 0.4;

        Player(GameSession* game, Dog* dog) : game_(game), dog_(dog) {}

        std::uint64_t GetDogId() const {
            return dog_->GetToken();
        }

        GameSession* GetSession() const {
            return game_;
        }

        Dog* GetDog() const {
            return dog_;
        }

        void ChangeDir(std::optional<model::Direction> dir);
        void SetSpeed(Dog::Speed speed);
        void Move(std::chrono::milliseconds time);

    private:
        GameSession* game_;
        Dog* dog_;

        int64_t FindRoadIndex(model::Dog::Coordinate pos, std::unordered_set<size_t>& viewed_road);
    };

    class Players {
    public:
        using Token = std::string;

        Players() = default;
        
        Players(const Players&) = delete;
        Players& operator=(const Players&) = delete;

        std::pair<Player*, Token> Add(Dog* dog, GameSession* session);
        Player* FindByDogIdAndMapId(uint64_t dog_id, Map::Id map_id);
        Player* FindByToken(const Token& token);
        void MovePlayers(std::chrono::milliseconds time);
        std::vector<Player*> GetAllPlayers() const;
        void RemovePlayer(Player* player);

    private:
        std::vector<std::unique_ptr<Player>> players_;
        std::unordered_map<Token, Player*> player_token_;

        std::random_device random_device_;
        std::mt19937_64 generator1_{[this] {
            std::uniform_int_distribution<std::mt19937_64::result_type> dist;
            return dist(random_device_);
        }()};

        std::mt19937_64 generator2_{[this] {
            std::uniform_int_distribution<std::mt19937_64::result_type> dist;
            return dist(random_device_);
        }()};


        Token GeneratePlayerToken();
    };
}; // namespace player