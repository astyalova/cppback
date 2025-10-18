#pragma once

#include "model.h"

#include <iomanip>
#include <random>
#include <sstream>
#include <unordered_map>
#include <vector>

namespace player {
    using namespace model;

    class Player {
    public:
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

    private:
        GameSession* game_;
        Dog* dog_;
    };

    class Players {
    public:
        using Token = std::string;

        Players() = default;
        
        Players(const Players&) = delete;
        Players& operator=(const Players&) = delete;

        std::pair<Player*, Token> Add(Dog* dog, GameSession* session) {
            Token token = GeneratePlayerToken();
            auto player_ptr = std::make_unique<Player>(session, dog);
            Player* player = player_ptr.get();

            players_.emplace_back(std::move(player_ptr));
            player_token_[token] = player;
            
            return {player, token};
        }

        Player* FindByDogIdAndMapId(uint64_t dog_id, Map::Id map_id) {
            for (auto& p : players_) {
                if (p->GetDogId() == dog_id && p->GetSession()->GetMap()->GetId() == map_id) {
                    return p.get();
                }
            }
            return nullptr;
        }

        Player* FindByToken(const Token& token) {
            if(player_token_.find(token) != player_token_.end()) {
                return player_token_[token];
            }
            return nullptr;
        }

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


        Token GeneratePlayerToken() {
            static constexpr auto num_size = sizeof(std::mt19937_64::result_type)*2UL;

            static std::stringstream stream;
            stream << std::setfill('0') << std::setw(num_size) << std::hex << generator1_()
                << std::setfill('0') << std::setw(num_size) << std::hex << generator2_();
            auto token = stream.str();
            
            stream.str(std::string());
            stream.clear();

            return token;
        }
    };
}; // namespace player