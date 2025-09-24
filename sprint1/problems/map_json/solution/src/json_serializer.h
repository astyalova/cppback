#pragma once
#include "model.h"
#include <string>

namespace json_serializer {

std::string SerializeMaps(const std::vector<model::Map>& maps);
std::string SerializeMap(const model::Map& map);

}  // namespace json_serializer
