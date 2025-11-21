#define _USE_MATH_DEFINES

#include <string>
#include <memory>

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "../src/collision_detector.h"

namespace collision_detector {

class ItemGatherer : public ItemGathererProvider {
public:
    virtual ~ItemGatherer() = default;

    size_t ItemsCount() {
        return items_.size();
    }

    Item GetItem(size_t idx) {
        return items_[idx];
    }

    size_t GatherersCount() {
        return gatherers_.size();
    }

    Gatherer GetGatherer(size_t idx) {
        return gatherers_[idx];
    }

    void AddItem(const Item& item) {
        items_.push_back(std::move(item));
    }

    void AddGatherer(const Gatherer& gatherer) {
        gatherers_.push_back(std::move(gatherer));
    }
private:
    std::vector<Item> items_;
    std::vector<Gatherer> gatherers_;
};

TEST_CASE("Movement along the x-axis", FindGatherEvents) {
    using Catch::Matchers::WithinRel;
    collision_detector::Item item{{12.5, 0}, 0.6};
    collision_detector::Gatherer gatherer{{0, 0}, {22.5, 0}, 0.6};
    collision_detector::ItemGatherer provider;
    provider.AddItem(item);
    provider.AddGatherer(gatherer);
    auto events = collision_detector::FindGatherEvents(provider);
    
    CHECK(events.size() == 1);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].sq_distance, WithinRel(0.0, 1e-9));
    CHECK_THAT(events[0].time, WithinRel((item.position.x/gatherer.end_pos.x), 1e-9)); 
}

TEST_CASE("Movement along the x-axis on the edge", FindGatherEvents) {
    using Catch::Matchers::WithinRel;
    collision_detector::Item item{{12.5, 0}, 0.6};
    collision_detector::Gatherer gatherer{{0, 0}, {12.5, 0}, 0.6};
    collision_detector::ItemGatherer;
    provider.AddItem(item);
    provider.AddGatherer(gatherer);
    auto events = collision_detector::FindGatherEvents(provider);
    
    CHECK(events.size() == 1);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].sq_distance, WithinRel(0.0, 1e-9));
    CHECK_THAT(events[0].time, WithinRel((item.position.x/gatherer.end_pos.x), 1e-9)); 
}

TEST_CASE("Movement along the x-axis on side", FindGatherEvents) {
    using Catch::Matchers::WithinRel;
    collision_detector::Item item{{12.5, 0.5}, 0.0};
    collision_detector::Gatherer gatherer{{0, 0.1}, {22.5, 0.1}, 0.6};
    collision_detector::ItemGatherer;
    provider.AddItem(item);
    provider.AddGatherer(gatherer);
    auto events = collision_detector::FindGatherEvents(provider);
    
    CHECK(events.size() == 1);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].sq_distance, WithinRel(0.16, 1e-9));
    CHECK_THAT(events[0].time, WithinRel((item.position.x/gatherer.end_pos.x), 1e-9)); 
}

TEST_CASE("Movement along y-axis", FindGatherEvents) {
    using Catch::Matchers::WithinRel;
    collision_detector::Item item{{0, 12.5}, 0.6};
    collision_detector::Gatherer gatherer{{0, 0}, {0, 22.5}, 0.6};
    collision_detector::ItemGatherer;
    provider.AddItem(item);
    provider.AddGatherer(gatherer);
    auto events = collision_detector::FindGatherEvents(provider);
    
    CHECK(events.size() == 1);
    CHECK(events[0].item_id == 0);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].sq_distance, WithinRel(0.0, 1e-9));
    CHECK_THAT(events[0].time, WithinRel((item.position.y/gatherer.end_pos.y), 1e-9)); 
}

TEST_CASE("Gatherer collects one of two items moving along the x-axis", FindGatherEvents) {
    using Catch::Matchers::WithinRel;
    collision_detector::Item item1{{42.5, 0}, 0.6};
    collision_detector::Item item2{{6.5, 0}, 0.6};
    collision_detector::Gatherer gatherer{{0, 0}, {22.5, 0}, 0.6};
    collision_detector::ItemGatherer;
    provider.AddItem(item1);
    provider.AddItem(item2);
    provider.AddGatherer(gatherer);
    auto events = collision_detector::FindGatherEvents(provider);

    CHECK(events.size() == 1);

    CHECK(events[0].item_id == 1);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].sq_distance, WithinRel(0.0, 1e-9));
    CHECK_THAT(events[0].time, WithinRel((item2.position.x/gatherer.end_pos.x), 1e-9)); 
}

TEST_CASE("Two gatherers collect two separate items moving along the x-axis and y-axis", FindGatherEvents) {
    using Catch::Matchers::WithinRel;
    collision_detector::Item item1{{0, 12.5}, 0.6};
    collision_detector::Item item2{{6.5, 0}, 0.6};
    collision_detector::Gatherer gatherer1{{0, 0}, {22.5, 0}, 0.6};
    collision_detector::Gatherer gatherer2{{0, 0}, {0, 22.5}, 0.6};
    collision_detector::ItemGatherer;
    provider.AddItem(item1);
    provider.AddItem(item2);
    provider.AddGatherer(gatherer1);
    provider.AddGatherer(gatherer2);
    auto events = collision_detector::FindGatherEvents(provider);
    
    CHECK(events.size() == 2);

    CHECK(events[0].item_id == 1);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].sq_distance, WithinRel(0.0, 1e-9));
    CHECK_THAT(events[0].time, WithinRel((item2.position.x/gatherer1.end_pos.x), 1e-9)); 
    
    CHECK(events[1].item_id == 0);
    CHECK(events[1].gatherer_id == 1);
    CHECK_THAT(events[1].sq_distance, WithinRel(0.0, 1e-9));
    CHECK_THAT(events[1].time, WithinRel((item1.position.y/gatherer2.end_pos.y), 1e-9)); 
}

TEST_CASE("Two gatherers collect three items moving along the x-axis and y-axis", FindGatherEvents) {
    using Catch::Matchers::WithinRel;
    collision_detector::Item item1{{12.5, 0}, 0.6};
    collision_detector::Item item2{{6.5, 0}, 0.6};
    collision_detector::Gatherer gatherer1{{0, 0}, {22.5, 0}, 0.6};
    collision_detector::Gatherer gatherer2{{0, 0}, {10, 0}, 0.6};
    collision_detector::ItemGatherer;
    provider.AddItem(item1);
    provider.AddItem(item2);
    provider.AddGatherer(gatherer1);
    provider.AddGatherer(gatherer2);
    auto events = collision_detector::FindGatherEvents(provider);
    
    CHECK(events.size() == 3);

    CHECK(events[0].item_id == 1);
    CHECK(events[0].gatherer_id == 0);
    CHECK_THAT(events[0].sq_distance, WithinRel(0.0, 1e-9));
    CHECK_THAT(events[0].time, WithinRel((item2.position.x/gatherer1.end_pos.x), 1e-9)); 

    CHECK(events[1].item_id == 0);
    CHECK(events[1].gatherer_id == 0);
    CHECK_THAT(events[1].sq_distance, WithinRel(0.0, 1e-9));
    CHECK_THAT(events[1].time, WithinRel((item1.position.x/gatherer1.end_pos.x), 1e-9)); 

    CHECK(events[2].item_id == 1);
    CHECK(events[2].gatherer_id == 1);
    CHECK_THAT(events[2].sq_distance, WithinRel(0.0, 1e-9));
    CHECK_THAT(events[2].time, WithinRel((item2.position.x/gatherer2.end_pos.x), 1e-9)); 
}
}; // collision_detector