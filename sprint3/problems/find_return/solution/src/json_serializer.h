#pragma once

#include <boost/json.hpp>
#include <string>

#include "model.h"
#include "json_loader.h"

namespace json_serializer {

std::string SerializeMaps(const std::vector<model::Map>& maps);
std::string SerializeMap(const model::Map& map);
void SerializeRoads(const model::Map& map, boost::json::object& obj);
void SerializeBuildings(const model::Map& map, boost::json::object& obj);
void SerializeOffices(const model::Map& map, boost::json::object& obj);
void SerializeLootTypes(const model::Map& map, boost::json::object& obj);
}  // namespace json_serializer
