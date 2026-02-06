#pragma once

#include "model.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <random>
#include <sstream>
#include <stdexcept>
#include <unordered_map>
#include <vector>
#include <optional>
#include <unordered_set>

namespace player {
    using namespace model;
    using Token = std::string;

    class Player {
    public:
        static constexpr double HALF_WIDTH = 0.4;

        Player(GameSession* game, Dog* dog, Token token)
            : game_(game)
            , dog_(dog)
            , token_(std::move(token)) {}

        std::uint64_t GetDogId() const {
            return dog_->GetToken();
        }

        const Token& GetToken() const {
            return token_;
        }

        GameSession* GetSession() const {
            return game_;
        }

        Dog* GetDog() const {
            return dog_;
        }

        void ChangeDir(std::optional<model::Direction> dir) {
            model::Dog::Speed dog_speed{0.0, 0.0};
            if (!dir) {
                dog_speed = model::Dog::Speed{0.0, 0.0};
            } else {
                double speed = game_->GetMap()->GetSpeed(); 
                switch (*dir) {
                    case model::Direction::NORTH:
                        dog_speed = model::Dog::Speed{0.0, -speed};
                        break;
                    case model::Direction::SOUTH:
                        dog_speed = model::Dog::Speed{0.0, speed};
                        break;
                    case model::Direction::WEST:
                        dog_speed = model::Dog::Speed{-speed, 0.0};
                        break;
                    case model::Direction::EAST:
                        dog_speed = model::Dog::Speed{speed, 0.0};
                        break;
                }
                dog_->SetDir(*dir);
            }
            dog_->SetSpeed(dog_speed);
        }


        void SetSpeed(Dog::Speed speed) {
            dog_->SetSpeed(speed);
        }

        void Tick(std::chrono::milliseconds time) {
            dog_->AddPlayTime(time);
            auto speed = dog_->GetSpeed();
            if (speed.x == 0.0 && speed.y == 0.0) {
                dog_->AddIdleTime(time);
            } else {
                dog_->ResetIdleTime();
            }
            Move(time);
        }

        void Move(std::chrono::milliseconds time) {
            auto speed = dog_->GetSpeed();
            if (speed.x == 0.0 && speed.y == 0.0) {
                return;
            }

            dog_->SetPrevPosition(Position{static_cast<double>(dog_->GetCoord().x),static_cast<double>(dog_->GetCoord().y)});

            auto time_s_d = std::chrono::duration<double>(time).count();
            auto current_pos = dog_->GetCoord();
            auto next_pos = model::Dog::Coordinate{current_pos.x + (speed.x * time_s_d), current_pos.y + (speed.y * time_s_d)};

            const auto& roads = game_->GetMap()->GetRoads();
            
            auto road_it = std::find_if(roads.begin(), roads.end(), [&next_pos](const model::Road& road){
                model::Dog::Coordinate min_road_pos{std::min(road.GetStart().x, road.GetEnd().x) - HALF_WIDTH, 
                                        std::min(road.GetStart().y, road.GetEnd().y) - HALF_WIDTH};
                model::Dog::Coordinate max_road_pos{std::max(road.GetStart().x, road.GetEnd().x) + HALF_WIDTH, 
                                        std::max(road.GetStart().y, road.GetEnd().y) + HALF_WIDTH};
                return (next_pos.x >= min_road_pos.x && next_pos.x <= max_road_pos.x
                        && next_pos.y >= min_road_pos.y && next_pos.y <= max_road_pos.y);
            });
            if (road_it != roads.end()) {
                dog_->SetCoord(next_pos);
                return;
            }

            next_pos = current_pos;
            bool next_pos_on_road = true;
            std::unordered_set<size_t> viewed_road;
            while (next_pos_on_road) {
                int64_t roadIndex = FindRoadIndex(next_pos, viewed_road);
                if (roadIndex == -1) {
                    break;
                }

                const auto& road = roads.at(roadIndex);
                switch (dog_->GetDir())
                {
                case model::Direction::NORTH: {
                    next_pos.y = std::min(road.GetStart().y, road.GetEnd().y);
                    next_pos.y -= 0.4;
                    break;
                }
                case model::Direction::SOUTH: {
                    next_pos.y = std::max(road.GetStart().y, road.GetEnd().y);
                    next_pos.y += 0.4;
                    break;
                }
                case model::Direction::WEST: {
                    next_pos.x = std::min(road.GetStart().x, road.GetEnd().x);
                    next_pos.x -= 0.4;
                    break;
                }
                case model::Direction::EAST: {
                    next_pos.x = std::max(road.GetStart().x, road.GetEnd().x);
                    next_pos.x += 0.4;
                    break;
                }
                }
            }
            dog_->SetSpeed(model::Dog::Speed{0.0, 0.0});
            dog_->SetCoord(next_pos);
        }

    private:
        GameSession* game_;
        Dog* dog_;
        Token token_;

        int64_t FindRoadIndex(model::Dog::Coordinate pos, std::unordered_set<size_t>& viewed_road) {
            const auto& roads = game_->GetMap()->GetRoads();
            for (size_t i = 0; i < roads.size(); ++i) {
                if (viewed_road.count(i)) {
                    continue;
                }

                const auto& road = roads.at(i);
                model::Dog::Coordinate min_road_pos{std::min(road.GetStart().x, road.GetEnd().x) - 0.4, 
                                        std::min(road.GetStart().y, road.GetEnd().y) - 0.4};
                model::Dog::Coordinate max_road_pos{std::max(road.GetStart().x, road.GetEnd().x) + 0.4, 
                                        std::max(road.GetStart().y, road.GetEnd().y) + 0.4};
                if (pos.x >= min_road_pos.x && pos.x <= max_road_pos.x
                    && pos.y >= min_road_pos.y && pos.y <= max_road_pos.y) {
                    viewed_road.insert(i);
                    return i;
                }
            }
            
            return -1;
        }
    };

    class Players {
    public:
        using Token = player::Token;

        struct SavedPlayer {
            Token token;
            std::string map_id;
            std::uint64_t dog_id;
        };

        Players() = default;
        
        Players(const Players&) = delete;
        Players& operator=(const Players&) = delete;

        std::pair<Player*, Token> Add(Dog* dog, GameSession* session) {
            Token token = GeneratePlayerToken();
            auto player_ptr = std::make_unique<Player>(session, dog, token);
            Player* player = player_ptr.get();

            players_.emplace_back(std::move(player_ptr));
            player_token_[token] = player;
            
            return {player, token};
        }

        Player* AddWithToken(Dog* dog, GameSession* session, Token token) {
            if (player_token_.contains(token)) {
                throw std::runtime_error("Duplicate player token");
            }
            auto player_ptr = std::make_unique<Player>(session, dog, token);
            Player* player = player_ptr.get();
            players_.emplace_back(std::move(player_ptr));
            player_token_[std::move(token)] = player;
            return player;
        }

        std::vector<SavedPlayer> GetSavedPlayers() const {
            std::vector<SavedPlayer> result;
            result.reserve(player_token_.size());
            for (const auto& [token, player] : player_token_) {
                if (!player || !player->GetSession() || !player->GetSession()->GetMap()) {
                    continue;
                }
                result.push_back(SavedPlayer{
                    token,
                    *player->GetSession()->GetMap()->GetId(),
                    player->GetDogId()
                });
            }
            return result;
        }

        void Clear() {
            players_.clear();
            player_token_.clear();
        }

        Player* FindByDogIdAndMapId(uint64_t dog_id, Map::Id map_id) {
            auto it = std::find_if(players_.begin(), players_.end(), 
                [dog_id, map_id](const auto& player) {
                    return player->GetDogId() == dog_id && 
                        player->GetSession()->GetMap()->GetId() == map_id;
                });
            
            return (it != players_.end()) ? it->get() : nullptr;
        }

        Player* FindByToken(const Token& token) {
            if(player_token_.find(token) != player_token_.end()) {
                return player_token_[token];
            }
            return nullptr;
        }

        struct RetiredPlayerInfo {
            std::string name;
            int score = 0;
            double play_time = 0.0;
        };

        std::vector<RetiredPlayerInfo> RetireIdlePlayers(std::chrono::milliseconds retirement_time) {
            std::vector<RetiredPlayerInfo> retired;
            for (auto it = players_.begin(); it != players_.end(); ) {
                Player& player = **it;
                Dog* dog = player.GetDog();
                if (dog && dog->GetIdleTime() >= retirement_time) {
                    retired.push_back(RetiredPlayerInfo{
                        dog->GetNickname(),
                        dog->GetScore(),
                        std::chrono::duration_cast<std::chrono::duration<double>>(dog->GetPlayTime()).count()
                    });
                    if (auto* session = player.GetSession()) {
                        session->RemoveDogByToken(player.GetDogId());
                    }
                    player_token_.erase(player.GetToken());
                    it = players_.erase(it);
                    continue;
                }
                ++it;
            }
            return retired;
        }

        void MovePlayers(std::chrono::milliseconds time) {
            for (const auto& player : players_) {
                player->Tick(time);
            }
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
