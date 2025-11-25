#pragma once

#include <filesystem>
#include <boost/json.hpp>

#include "model.h"

namespace json_loader {

namespace keys {
    inline constexpr char ID[] = "id";
    inline constexpr char NAME[] = "name";

    // Roads
    inline constexpr char ROADS[] = "roads";
    inline constexpr char X0[] = "x0";
    inline constexpr char Y0[] = "y0";
    inline constexpr char X1[] = "x1";
    inline constexpr char Y1[] = "y1";

    // Buildings
    inline constexpr char BUILDINGS[] = "buildings";
    inline constexpr char X[] = "x";
    inline constexpr char Y[] = "y";
    inline constexpr char W[] = "w";
    inline constexpr char H[] = "h";

    // Offices
    inline constexpr char OFFICES[] = "offices";
    inline constexpr char OFFSET_X[] = "offsetX";
    inline constexpr char OFFSET_Y[] = "offsetY";

    // Loot
    inline constexpr char LOOTS[] = "lootTypes";
    inline constexpr char CONFIG[] = "lootGeneratorConfig";
    inline constexpr char PERIOD[] = "period";
    inline constexpr char BASE[] = "probabilityBase";
}

model::Game LoadGame(const std::filesystem::path& json_path);
void LoadRoads(model::Map& mp, const boost::json::value& obj);
void LoadBuildings(model::Map& mp, const boost::json::value& obj);
void LoadOffices(model::Map& mp, const boost::json::value& obj);
void LoadLootGenerator(model::Map& map, const boost::json::object& root);
void LoadLootTypes(model::Map& map, const boost::json::object& map_obj);
void LoadLoot(model::Map& map, const boost::json::value& map_val, const boost::json::object& root);

}  // namespace json_loader
