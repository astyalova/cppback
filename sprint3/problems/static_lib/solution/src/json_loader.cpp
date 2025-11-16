#include "json_loader.h"
#include "model.h"

#include <boost/system/system_error.hpp>
#include <boost/json.hpp>
#include <fstream>
#include <stdexcept>
#include <sstream>
#include <string>
#include <iostream>
#include "extra_data.h"

namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path) {
    std::ifstream file(json_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file.");
    }

    std::stringstream buffer;
    model::Game game;

    buffer << file.rdbuf();
    std::string json_string = buffer.str();

    try {
        boost::json::value jv = boost::json::parse(json_string);
        auto root_obj = jv.as_object();
        double default_speed = root_obj.at("defaultDogSpeed").as_double();
        auto map_array = root_obj.at("maps").as_array();

        for (const auto& obj_val : map_array) {
            const auto& obj = obj_val.as_object();

            double speed = obj.contains("dogSpeed") ? obj.at("dogSpeed").as_double() : default_speed;

            auto mp = model::Map{
                model::Map::Id(obj.at(keys::ID).as_string().data()),
                obj.at(keys::NAME).as_string().data(),
                speed
            };

            LoadRoads(mp, obj);
            LoadBuildings(mp, obj);
            LoadOffices(mp, obj);
            LoadLoot(mp, obj, root_obj);
            game.AddMap(std::move(mp));
        }

    } catch (const std::exception& e) {
        throw std::runtime_error("JSON parsing error: " + std::string(e.what()));
    }
    return game;
}


void LoadRoads(model::Map& mp, const boost::json::value& obj) {
    if(obj.at(keys::ROADS).as_array().size() < 1) {
        throw std::runtime_error("Incorrect map");
    }

    for(const auto& road : obj.as_object().at(keys::ROADS).as_array()) {
        auto road_data = road.as_object();

        auto start = model::Point{static_cast<model::Coord>(road_data.at(keys::X0).as_int64()), 
                                  static_cast<model::Coord>(road_data.at(keys::Y0).as_int64())};

        if(road_data.find(keys::X1) != road_data.end()) {
            mp.AddRoad(model::Road{model::Road::HORIZONTAL, start, static_cast<model::Coord>(road_data.at(keys::X1).as_int64())});
        } else {
            mp.AddRoad(model::Road{model::Road::VERTICAL, start, static_cast<model::Coord>(road_data.at(keys::Y1).as_int64())});
        }
    }
}

void LoadBuildings(model::Map& mp, const boost::json::value& obj) {
    for(const auto& build : obj.at(keys::BUILDINGS).as_array()) {
        auto build_data = build.as_object();

        mp.AddBuilding(model::Building {
            model::Rectangle {
                model::Point{static_cast<model::Coord>(build_data.at(keys::X).as_int64()),
                             static_cast<model::Coord>(build_data.at(keys::Y).as_int64())},
                model::Size{static_cast<model::Dimension>(build_data.at(keys::W).as_int64()), 
                            static_cast<model::Dimension>(build_data.at(keys::H).as_int64())}                    
            }
        });
    }
}

void LoadOffices(model::Map& mp, const boost::json::value& obj) {
    for(const auto& office : obj.at(keys::OFFICES).as_array()) {
        auto office_data = office.as_object();

        mp.AddOffice(model::Office {
            model::Office::Id{office_data.at(keys::ID).as_string().data()},
            model::Point{static_cast<model::Coord>(office_data.at(keys::X).as_int64()),
                         static_cast<model::Coord>(office_data.at(keys::Y).as_int64())},
            model::Offset{static_cast<model::Dimension>(office_data.at(keys::OFFSET_X).as_int64()), 
                          static_cast<model::Dimension>(office_data.at(keys::OFFSET_Y).as_int64())}
        });
    }
}

void LoadLootGenerator(model::Map& map, const boost::json::object& root) {
    if (!root.contains("lootGeneratorConfig")) return;

    const auto& cfg = root.at("lootGeneratorConfig").as_object();
    double period_sec = cfg.at("period").as_double();
    double probability = cfg.at("probability").as_double();

    loot_gen::LootGenerator generator{
        std::chrono::milliseconds(static_cast<int>(period_sec * 1000)),
        probability
    };

    extra_data::ExtraDataRepository::GetInstance().SetLootGenerator(map.GetId(), generator);
    map.SetLootGenerator(std::move(generator));
}

void LoadLootTypes(model::Map& map, const boost::json::object& map_obj) {
    if (!map_obj.contains("lootTypes")) return;

    const auto& loot_array = map_obj.at("lootTypes").as_array();
    if (loot_array.empty()) return;

    extra_data::ExtraDataRepository::GetInstance().SetLootTypes(map.GetId(), loot_array);
    map.SetLootTypeCount(static_cast<int>(loot_array.size()));
}

void LoadLoot(model::Map& map, const boost::json::value& map_val, const boost::json::object& root) {
    const auto& map_obj = map_val.as_object();
    LoadLootGenerator(map, root);
    LoadLootTypes(map, map_obj);
}

}  // namespace json_loader
