#include "json_serializer.h"
#include <boost/json.hpp>

namespace json_serializer {

std::string SerializeMaps(const std::vector<model::Map>& maps) {
    boost::json::array arr;
    for (const auto& map : maps) {
        boost::json::object obj;
        obj["id"] = *map.GetId(); 
        obj["name"] = map.GetName();
        arr.push_back(obj);
    }
    return boost::json::serialize(arr);
}

std::string SerializeMap(const model::Map& map) {
    boost::json::object obj;
    obj["id"] = *map.GetId();
    obj["name"] = map.GetName();

    // roads
    boost::json::array roads;
    for (const auto& r : map.GetRoads()) {
        boost::json::object road_obj;

        auto start = r.GetStart();
        auto end = r.GetEnd();

        road_obj["x0"] = start.x;
        road_obj["y0"] = start.y;

        if (r.IsHorizontal()) {
            road_obj["x1"] = end.x; 
        } else if (r.IsVertical()) {
            road_obj["y1"] = end.y;
        }

        roads.push_back(road_obj);
    }
    obj["roads"] = roads;

    // buildings
    boost::json::array buildings;
    for (const auto& b : map.GetBuildings()) {
        boost::json::object b_obj;

        auto pos = b.GetBounds().position;
        auto size = b.GetBounds().size;

        b_obj["x"] = pos.x;
        b_obj["y"] = pos.y;
        b_obj["w"] = size.width;
        b_obj["h"] = size.height;

        buildings.push_back(b_obj);
    }
    obj["buildings"] = buildings;

    // offices
    boost::json::array offices;
    for (const auto& o : map.GetOffices()) {
        boost::json::object o_obj;

        o_obj["id"] = *o.GetId();

        auto pos = o.GetPosition();
        auto offset = o.GetOffset();

        o_obj["x"] = pos.x;
        o_obj["y"] = pos.y;
        
        o_obj["offsetX"] = offset.dx;
        o_obj["offsetY"] = offset.dy;

        offices.push_back(o_obj);
    }
    obj["offices"] = offices;

    return boost::json::serialize(obj);
}

}  // namespace json_serializer