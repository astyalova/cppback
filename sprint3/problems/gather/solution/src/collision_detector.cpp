#include "collision_detector.h"
#include <cassert>

namespace collision_detector {

CollectionResult TryCollectPoint(geom::Point2D a, geom::Point2D b, geom::Point2D c) {
    // Проверим, что перемещение ненулевое.
    // Тут приходится использовать строгое равенство, а не приближённое,
    // пскольку при сборе заказов придётся учитывать перемещение даже на небольшое
    // расстояние.
    assert(b.x != a.x || b.y != a.y);
    const double u_x = c.x - a.x;
    const double u_y = c.y - a.y;
    const double v_x = b.x - a.x;
    const double v_y = b.y - a.y;
    const double u_dot_v = u_x * v_x + u_y * v_y;
    const double u_len2 = u_x * u_x + u_y * u_y;
    const double v_len2 = v_x * v_x + v_y * v_y;
    const double proj_ratio = u_dot_v / v_len2;
    const double sq_distance = u_len2 - (u_dot_v * u_dot_v) / v_len2;

    return CollectionResult(sq_distance, proj_ratio);
}

// В задании на разработку тестов реализовывать следующую функцию не нужно -
// она будет линковаться извне.

std::vector<GatheringEvent> FindGatherEvents(const ItemGathererProvider& provider) {
    std::vector<GatheringEvent> res;

    for(int i = 0; i < provider.GatherersCount(); ++i) {
        for(int j = 0; j < provider.ItemsCount(); ++j) {
            auto gatherer = provider.GetGatherer(i);
            auto item = provider.GetItem(j);
            auto collect = TryCollectPoint(gatherer.start_pos, gatherer.end_pos, item.position);

            if(collect.IsCollected(item.width + gatherer.width)) {
                res.push_back({j, i, collect.sq_distance, collect.proj_ratio});
            }
        }
    }

    std::sort(res.begin(), res.end(), [](GatheringEvent& lhs, GatheringEvent& rhs) {
        return lhs.time < rhs.time;
    });
    return res;
}


}  // namespace collision_detector