#include "extra_data.h"

namespace extra_data {

    void ExtraDataRepository::SetLootTypes(MapId id, boost::json::array loot_types) {
        loot_types_map_[std::move(id)] = std::move(loot_types);

        std::vector<model::LootType> loot_data;
        for (const auto& loot_item : loot_types) {
            const auto& obj = loot_item.as_object();
            model::LootType loot_type;
            loot_type.name = obj.at("name").as_string().c_str();
            loot_type.value = obj.at("value").as_int64();
            loot_data.push_back(loot_type);
        }
        loot_types_data_[id] = std::move(loot_data);
    }

    const boost::json::array* ExtraDataRepository::GetLootTypes(MapId id) const {
        auto it = loot_types_map_.find(id);
        return it != loot_types_map_.end() ? &it->second : nullptr;
    }

    int ExtraDataRepository::GetLootValue(MapId id, size_t type_index) const {
        auto it = loot_types_data_.find(id);
        if (it == loot_types_data_.end() || type_index >= it->second.size()) {
            return 0;
        }
        return it->second[type_index].value;
    }

    const std::vector<model::LootType>& ExtraDataRepository::GetLootTypesData(MapId id) const {
        static const std::vector<model::LootType> empty;
        auto it = loot_types_data_.find(id);
        return it != loot_types_data_.end() ? it->second : empty;
    }

    void ExtraDataRepository::SetLootGenerator(MapId id, loot_gen::LootGenerator generator) {
        loot_generators_.insert_or_assign(std::move(id), std::move(generator));
    }

    loot_gen::LootGenerator* ExtraDataRepository::GetLootGenerator(MapId id) {
        auto it = loot_generators_.find(id);
        return it != loot_generators_.end() ? &it->second : nullptr;
    }

    void ExtraDataRepository::Clear() {
        loot_types_map_.clear();
        loot_generators_.clear();
    }

}  // namespace extra_data