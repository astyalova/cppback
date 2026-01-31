#include "state_serialization.h"

#include "json_logger.h"

#include <boost/archive/text_iarchive.hpp>
#include <boost/archive/text_oarchive.hpp>
#include <boost/json.hpp>
#include <boost/serialization/string.hpp>
#include <boost/serialization/unordered_map.hpp>
#include <boost/serialization/vector.hpp>

#include <fstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

using namespace std::literals;

namespace {

struct DogState {
    std::uint64_t token = 0;
    std::string nickname;
    model::Dog::Coordinate coord{};
    model::Dog::Speed speed{};
    model::Direction dir = model::Direction::NORTH;
    std::vector<model::LostObject> bag;
    int bag_capacity = 0;
    model::Position prev_position{};
    int score = 0;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned) {
        ar & token;
        ar & nickname;
        ar & coord;
        ar & speed;
        ar & dir;
        ar & bag;
        ar & bag_capacity;
        ar & prev_position;
        ar & score;
    }
};

struct SessionState {
    std::string map_id;
    std::vector<DogState> dogs;
    std::vector<model::LostObject> loots;
    int next_loot_id = 0;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned) {
        ar & map_id;
        ar & dogs;
        ar & loots;
        ar & next_loot_id;
    }
};

struct PlayerState {
    std::string token;
    std::string map_id;
    std::uint64_t dog_id = 0;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned) {
        ar & token;
        ar & map_id;
        ar & dog_id;
    }
};

struct AppState {
    std::vector<SessionState> sessions;
    std::vector<PlayerState> players;

    template <typename Archive>
    void serialize(Archive& ar, const unsigned) {
        ar & sessions;
        ar & players;
    }
};

AppState CaptureState(const Application& app) {
    AppState state;

    for (const auto* session : app.GetGame().GetSessions()) {
        if (!session || !session->GetMap()) {
            continue;
        }
        SessionState session_state;
        session_state.map_id = *session->GetMap()->GetId();
        session_state.next_loot_id = session->GetNextLootId();

        for (const auto* dog : session->GetDogs()) {
            if (!dog) {
                continue;
            }
            DogState dog_state;
            dog_state.token = dog->GetToken();
            dog_state.nickname = dog->GetNickname();
            dog_state.coord = dog->GetCoord();
            dog_state.speed = dog->GetSpeed();
            dog_state.dir = dog->GetDir();
            dog_state.bag = dog->GetBag();
            dog_state.bag_capacity = dog->GetBagCapacity();
            dog_state.prev_position = dog->GetPrevPosition();
            dog_state.score = dog->GetScore();
            session_state.dogs.push_back(std::move(dog_state));
        }

        for (const auto& item : session->GetLostObjects()) {
            session_state.loots.push_back(item.second);
        }

        state.sessions.push_back(std::move(session_state));
    }

    for (const auto& saved_player : app.GetPlayers().GetSavedPlayers()) {
        state.players.push_back(PlayerState{saved_player.token, saved_player.map_id, saved_player.dog_id});
    }

    return state;
}

void ApplyState(Application& app, const AppState& state) {
    auto& game = app.GetGame();
    std::unordered_map<std::string, model::GameSession*> sessions_by_map;

    for (const auto& session_state : state.sessions) {
        auto map = game.FindMap(model::Map::Id{session_state.map_id});
        if (!map) {
            throw std::runtime_error("Unknown map id in state");
        }
        auto session = game.FindSession(map);
        if (!session) {
            session = game.CreateSession(map);
        }
        session->ClearState();

        std::unordered_map<int, model::LostObject> loots;
        for (const auto& loot : session_state.loots) {
            loots.emplace(static_cast<int>(loot.id), loot);
        }
        session->RestoreLostObjects(std::move(loots), session_state.next_loot_id);

        for (const auto& dog_state : session_state.dogs) {
            session->RestoreDog(
                dog_state.nickname,
                dog_state.token,
                dog_state.coord,
                dog_state.speed,
                dog_state.dir,
                dog_state.bag_capacity,
                dog_state.bag,
                dog_state.prev_position,
                dog_state.score
            );
        }
        sessions_by_map.emplace(session_state.map_id, session);
    }

    auto& players = app.GetPlayers();
    players.Clear();
    for (const auto& player_state : state.players) {
        auto it = sessions_by_map.find(player_state.map_id);
        if (it == sessions_by_map.end()) {
            throw std::runtime_error("Player refers to unknown map in state");
        }
        auto* session = it->second;
        auto* dog = session->FindDogByToken(player_state.dog_id);
        if (!dog) {
            throw std::runtime_error("Player refers to unknown dog in state");
        }
        players.AddWithToken(dog, session, player_state.token);
    }
}

}  // namespace

namespace boost::serialization {

template <typename Archive>
void serialize(Archive& ar, model::Position& pos, const unsigned) {
    ar & pos.x;
    ar & pos.y;
}

template <typename Archive>
void serialize(Archive& ar, model::Dog::Coordinate& coord, const unsigned) {
    ar & coord.x;
    ar & coord.y;
}

template <typename Archive>
void serialize(Archive& ar, model::Dog::Speed& speed, const unsigned) {
    ar & speed.x;
    ar & speed.y;
}

template <typename Archive>
void serialize(Archive& ar, model::LostObject& obj, const unsigned) {
    ar & obj.id;
    ar & obj.type;
    ar & obj.position;
    ar & obj.value;
}

template <typename Archive>
void serialize(Archive& ar, model::Direction& dir, const unsigned) {
    int value = static_cast<int>(dir);
    ar & value;
    if constexpr (Archive::is_loading::value) {
        dir = static_cast<model::Direction>(value);
    }
}

}  // namespace boost::serialization

namespace state_serialization {

StateManager::StateManager(Application& app, std::filesystem::path state_file,
                           std::optional<std::chrono::milliseconds> save_period)
    : app_(app)
    , state_file_(std::move(state_file))
    , save_period_(save_period) {}

void StateManager::Load() {
    if (!std::filesystem::exists(state_file_)) {
        return;
    }
    LoadState(app_, state_file_);
}

void StateManager::Save() {
    SaveState(app_, state_file_);
}

void StateManager::OnTick(std::chrono::milliseconds delta) {
    if (!save_period_ || *save_period_ < std::chrono::milliseconds{0}) {
        return;
    }
    since_last_save_ += delta;
    if (since_last_save_ < *save_period_) {
        return;
    }
    try {
        Save();
        since_last_save_ = std::chrono::milliseconds{0};
    } catch (const std::exception& ex) {
        json_logger::LogData("state save failed"sv, boost::json::object{{"error", ex.what()}});
    }
}

void SaveState(const Application& app, const std::filesystem::path& state_file) {
    auto tmp_file = state_file;
    tmp_file += ".tmp";

    std::ofstream out(tmp_file, std::ios::binary | std::ios::trunc);
    if (!out) {
        throw std::runtime_error("Failed to open state file for writing");
    }

    boost::archive::text_oarchive archive(out);
    auto state = CaptureState(app);
    archive << state;

    out.flush();
    if (!out) {
        throw std::runtime_error("Failed to write state file");
    }

    std::error_code ec;
    std::filesystem::rename(tmp_file, state_file, ec);
    if (ec) {
        std::filesystem::remove(state_file, ec);
        ec.clear();
        std::filesystem::rename(tmp_file, state_file, ec);
    }
    if (ec) {
        throw std::runtime_error("Failed to replace state file");
    }
}

void LoadState(Application& app, const std::filesystem::path& state_file) {
    std::ifstream in(state_file, std::ios::binary);
    if (!in) {
        throw std::runtime_error("Failed to open state file for reading");
    }

    boost::archive::text_iarchive archive(in);
    AppState state;
    archive >> state;
    ApplyState(app, state);
}

}  // namespace state_serialization
