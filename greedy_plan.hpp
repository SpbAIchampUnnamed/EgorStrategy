#ifndef __GREEDY_PLAN_HPP__
#define __GREEDY_PLAN_HPP__

#include "model/Game.hpp"
#include "precalc.hpp"
#include "plan_strategy.hpp"
#include <algorithm>
#include <type_traits>
#include <vector>
#include <utility>
#include <unordered_map>
#include <tuple>

namespace plans {

template<
    PlanStrategy strategy,
    class StaticRobotsContiner,
    class ReservedRobotsContiner,
    class ReservedResourcesContiner,
    class RequestsContiner
>
std::optional<std::vector<std::pair<int, model::MoveAction>>>
greedyPlan(
    const StaticRobotsContiner &static_robots,
    ReservedRobotsContiner &reserved_robots,
    ReservedResourcesContiner &reserved_resources,
    const RequestsContiner &requests,
    int extra_ticks = 0) 
{
    using namespace std;
    using namespace model;
    using precalc::d;

    vector<pair<int, MoveAction>> result;
    struct Path {
        int start, intermediate, count;
    };

    auto restore = [&]() {
        if constexpr (strategy == PlanStrategy::FULL) {
            return [&, reserved_robots_cp = reserved_robots, reserved_resources_cp = reserved_resources]() {
                reserved_resources = reserved_resources_cp;
                reserved_robots = reserved_robots_cp;
            };
        } else {
            return []{};
        }
    }();

    for (auto [target, res, cnt] : requests) {
        auto cmp = [&](auto &a, auto &b) {
            if (d[a.start][a.intermediate] + d[a.intermediate][target] != d[a.start][a.intermediate] + d[a.intermediate][target])
                return d[a.start][a.intermediate] + d[a.intermediate][target] < d[a.start][a.intermediate] + d[a.intermediate][target];
            else
                return a.count > b.count;
        };
        int best_start = 0, best_dist = precalc::d[0][target];
        vector<Path> candidates;
        for (size_t i = 0; i < game.planets.size(); ++i) {
            int free_robots = -static_robots[i] - reserved_robots[i];
            for (auto &g : game.planets[i].workerGroups) {
                if (g.playerIndex == game.myIndex) {
                    free_robots += g.number;
                    break;
                }
            }
            if (free_robots <= 0)
                continue;
            vector<Path> imm;
            for (size_t j = 0; j < game.planets.size(); ++j) {
                auto &p = game.planets[j];
                if (p.workerGroups.size() && p.workerGroups[0].playerIndex != game.myIndex)
                    continue;
                int avaible_res = -reserved_resources[j][(int) res];
                if (p.resources.contains(res))
                    avaible_res += p.resources.at(res);
                if (p.building &&
                    game.buildingProperties.at(p.building->buildingType).produceResource == res &&
                    game.buildingProperties.at(p.building->buildingType).harvest)
                {
                    auto &bp = game.buildingProperties.at(p.building->buildingType);
                    int res_per_tick = min(bp.maxWorkers, static_robots[j]) / bp.workAmount * bp.produceAmount;
                    avaible_res += res_per_tick * (d[i][j] + extra_ticks);
                }
                if (avaible_res > 0) {
                    imm.emplace_back(Path{(int) i, (int) j, avaible_res});
                }
            }
            sort(imm.begin(), imm.end(), cmp);
            int r = free_robots;
            auto it = imm.begin();
            while (r > 0 && it != imm.end()) {
                candidates.emplace_back(Path{it->start, it->intermediate, min(r, it->count)});
                r -= it->count;
                ++it;
            }
        }
        if constexpr (strategy == PlanStrategy::FULL || strategy == PlanStrategy::FULL_TASK) {
            int sum = 0;
            for (auto &x : candidates)
                sum += x.count;
            if (sum < cnt) {
                if constexpr (strategy == PlanStrategy::FULL) {
                    restore();
                    return nullopt;
                } else {
                    continue;
                }
            }
        }
        sort(candidates.begin(), candidates.end(), cmp);
        auto it = candidates.begin();
        while (cnt > 0 && it != candidates.end()) {
            int c = min(cnt, it->count);
            cnt -= c;
            reserved_resources[it->intermediate][(int) res] += c;
            if (it->intermediate != target) {
                reserved_robots[it->start] += c;
                if (it->start != it->intermediate)
                    result.emplace_back(extra_ticks, MoveAction(it->start, it->intermediate, c));
                result.emplace_back(d[it->start][it->intermediate] + extra_ticks, MoveAction(it->intermediate, target, c, res));
            }
        }
    }
    return result;
}

};

#endif