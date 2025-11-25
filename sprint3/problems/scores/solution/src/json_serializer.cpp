#include "json_serializer.h"
#include <boost/json.hpp>
#include "extra_data.h"

namespace json_serializer {

std::string SerializeMaps(const std::vector<model::Map>& maps) {
    boost::json::array arr;
    for (const auto& map : maps) {
        boost::json::object obj;
        obj[json_loader::keys::ID] = *map.GetId(); 
        obj[json_loader::keys::NAME] = map.GetName();
        arr.push_back(obj);
    }
    return boost::json::serialize(arr);
}

std::string SerializeMap(const model::Map& map) {
    boost::json::object obj;
    obj[json_loader::keys::ID] = *map.GetId();
    obj[json_loader::keys::NAME] = map.GetName();

    // roads
    SerializeRoads(map, obj);

    // buildings
    SerializeBuildings(map, obj);

    // offices
    SerializeOffices(map, obj);

    // loots
    SerializeLootTypes(map, obj);
    return boost::json::serialize(obj);
}

void SerializeRoads(const model::Map& map, boost::json::object& obj) {
    boost::json::array roads;
    for (const auto& r : map.GetRoads()) {
        boost::json::object road_obj;

        auto start = r.GetStart();
        auto end = r.GetEnd();

        road_obj[json_loader::keys::X0] = start.x;
        road_obj[json_loader::keys::Y0] = start.y;

        if (r.IsHorizontal()) {
            road_obj[json_loader::keys::X1] = end.x; 
        } else if (r.IsVertical()) {
            road_obj[json_loader::keys::Y1] = end.y;
        }

        roads.push_back(road_obj);
    }
    obj[json_loader::keys::ROADS] = roads;
}

void SerializeBuildings(const model::Map& map, boost::json::object& obj) {
    boost::json::array buildings;
    for (const auto& b : map.GetBuildings()) {
        boost::json::object b_obj;

        auto pos = b.GetBounds().position;
        auto size = b.GetBounds().size;

        b_obj[json_loader::keys::X] = pos.x;
        b_obj[json_loader::keys::Y] = pos.y;
        b_obj[json_loader::keys::W] = size.width;
        b_obj[json_loader::keys::H] = size.height;

        buildings.push_back(b_obj);
    }
    obj[json_loader::keys::BUILDINGS] = buildings;
}

void SerializeOffices(const model::Map& map, boost::json::object& obj) {
    boost::json::array offices;
    for (const auto& o : map.GetOffices()) {
        boost::json::object o_obj;

        o_obj[json_loader::keys::ID] = *o.GetId();

        auto pos = o.GetPosition();
        auto offset = o.GetOffset();

        o_obj[json_loader::keys::X] = pos.x;
        o_obj[json_loader::keys::Y] = pos.y;
        
        o_obj[json_loader::keys::OFFSET_X] = offset.dx;
        o_obj[json_loader::keys::OFFSET_Y] = offset.dy;

        offices.push_back(o_obj);
    }
    obj[json_loader::keys::OFFICES] = offices;
}

void SerializeLootTypes(const model::Map& map, boost::json::object& obj) {
    const auto* loot_types = extra_data::ExtraDataRepository::GetInstance().GetLootTypes(map.GetId());
    if (!loot_types) return;

    obj["lootTypes"] = *loot_types;  // Просто возвращаем массив для фронтенда
}

}  // namespace json_serializer
