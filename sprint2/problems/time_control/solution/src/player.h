#pragma once

#include "model.h"

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

        void ChangeDir(std::optional<model::Dog::Direction> dir) {
            auto dog_speed = game_->GetMap()->GetSpeed();;
            if(!dir) {
                dog_speed = model::Dog::Speed{0.0, 0.0};
            } else {
                double speed(game_->GetMap()->GetSpeed());
                if(dir == model::Dog::Direction::NORTH) {
                    dog_speed = model::Dog::Speed{0.0, -speed};
                } else if(dir == model::Dog::Direction::SOUTH) {
                    dog_speed = model::Dog::Speed{0.0, speed};
                } else if(dir == model::Dog::Direction::WEST) {
                    dog_speed = model::Dog::Speed{-speed, 0.0};
                } else {
                    dog_speed = model::Dog::Speed{speed, 0.0};
                }
                dog_->SetDir(*dir);
            }
            dog_->SetSpeed(dog_speed);
        }

        void Move(int time) {
            auto speed = dog_->GetSpeed();
            if (speed.x == 0.0 && speed.y == 0.0) {
                return;
            }

            auto time_s_d = double(time) * 0.001;
            auto current_pos = dog_->GetCoord();
            auto next_pos = model::Dog::Coordinate{current_pos.x + (speed.x * time_s_d), current_pos.y + (speed.y * time_s_d)};

            const auto& roads = game_->GetMap()->GetRoads();
            
            auto road_it = std::find_if(roads.begin(), roads.end(), [&next_pos](const model::Road& road){
                model::Dog::Coordinate min_road_pos{std::min(road.GetStart().x, road.GetEnd().x) - 0.4, 
                                        std::min(road.GetStart().y, road.GetEnd().y) - 0.4};
                model::Dog::Coordinate max_road_pos{std::max(road.GetStart().x, road.GetEnd().x) + 0.4, 
                                        std::max(road.GetStart().y, road.GetEnd().y) + 0.4};
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
                case model::Dog::Direction::NORTH: {
                    next_pos.y = std::min(road.GetStart().y, road.GetEnd().y);
                    next_pos.y -= 0.4;
                    break;
                }
                case model::Dog::Direction::SOUTH: {
                    next_pos.y = std::max(road.GetStart().y, road.GetEnd().y);
                    next_pos.y += 0.4;
                    break;
                }
                case model::Dog::Direction::WEST: {
                    next_pos.x = std::min(road.GetStart().x, road.GetEnd().x);
                    next_pos.x -= 0.4;
                    break;
                }
                case model::Dog::Direction::EAST: {
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

        void MovePlayers(int time) {
        for (const auto& player : players_) {
            player->Move(time);
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