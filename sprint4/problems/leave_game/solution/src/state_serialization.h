#pragma once

#include "application.h"

#include <chrono>
#include <filesystem>
#include <optional>

namespace state_serialization {

class StateManager {
public:
    StateManager(Application& app, std::filesystem::path state_file,
                 std::optional<std::chrono::milliseconds> save_period);

    void Load();
    void Save();
    void OnTick(std::chrono::milliseconds delta);

private:
    Application& app_;
    std::filesystem::path state_file_;
    std::optional<std::chrono::milliseconds> save_period_;
    std::chrono::milliseconds since_last_save_{0};
};

void SaveState(const Application& app, const std::filesystem::path& state_file);
void LoadState(Application& app, const std::filesystem::path& state_file);

}  // namespace state_serialization
