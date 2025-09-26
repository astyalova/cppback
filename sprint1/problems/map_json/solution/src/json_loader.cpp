#include "json_loader.h"
#include <boost/json.hpp>
#include "model.h"
#include <boost/system/system_error.hpp>

#include <fstream>
#include <sstream>
#include <string>
#include <iostream>


namespace json_loader {

model::Game LoadGame(const std::filesystem::path& json_path) {
    // Загрузить содержимое файла json_path, например, в виде строки
    std::ifstream file(json_path);
    std::stringstream buffer;
    model::Game game;

    buffer << file.rdbuf();
    std::string json_string = buffer.str();

    // Распарсить строку как JSON, используя boost::json::parse
    try {
        boost::json::value jv = boost::json::parse(json_string);
        auto root_obj = jv.as_object();
        auto map = root_obj.at("maps").as_array();

        for(const auto& obj : map) {
            auto mp = model::Map{model::Map::Id(obj.at("id").as_string().data()), obj.at("name").as_string().data()};

            //roads
            assert(obj.at("roads").as_array().size() > 0);
            for(const auto& road : obj.as_object().at("roads").as_array()) {
                auto road_data = road.as_object();

                auto start = model::Point{static_cast<model::Coord>(road_data.at("x0").as_int64()), 
                                          static_cast<model::Coord>(road_data.at("y0").as_int64())};

                if(road_data.find("x1") != road_data.end()) {
                    mp.AddRoad(model::Road{model::Road::HORIZONTAL, start, static_cast<model::Coord>(road_data.at("x1").as_int64())});
                } else {
                    mp.AddRoad(model::Road{model::Road::VERTICAL, start, static_cast<model::Coord>(road_data.at("y1").as_int64())});
                }
            }

            //buildings
            for(const auto& build : obj.at("buildings").as_array()) {
                auto build_data = build.as_object();

                mp.AddBuilding(model::Building {
                    model::Rectangle {
                        model::Point{static_cast<model::Coord>(build_data.at("x").as_int64()),
                                     static_cast<model::Coord>(build_data.at("y").as_int64())},
                        model::Size{static_cast<model::Dimension>(build_data.at("w").as_int64()), 
                                    static_cast<model::Dimension>(build_data.at("h").as_int64())
                        }                    
                    }
                });
            }

            //offices

            for(const auto& office : obj.at("offices").as_array()) {
                auto office_data = office.as_object();

                mp.AddOffice(model::Office {
                    model::Office::Id{office_data.at("id").as_string().data()},
                    model::Point{static_cast<model::Coord>(office_data.at("x").as_int64()),
                                 static_cast<model::Coord>(office_data.at("y").as_int64())},
                    model::Offset{static_cast<model::Dimension>(office_data.at("offsetX").as_int64()), 
                                 static_cast<model::Dimension>(office_data.at("offsetY").as_int64())}
                });
            }
            game.AddMap(std::move(mp));
        }

    } catch(...) {
        throw;
    }

    return game;
}

}  // namespace json_loader