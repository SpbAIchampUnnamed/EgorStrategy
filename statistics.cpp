#include "statistics.hpp"
#include "utils/reduce.hpp"
#include <ranges>
#include <deque>
#include <numeric>

using namespace std;
using namespace model;

template<class F>
unordered_map<BuildingType, Fraction<int>> getCounts(BuildingType type, bool normalized, F &&product_ratio) {
    deque<pair<BuildingType, Fraction<int>>> q;
    q.emplace_back(type, 1);
    auto resToBuilding = [&](Resource r) {
        for (auto &[bt, bp] : game.buildingProperties) {
            if (bp.produceResource.has_value() && *bp.produceResource == r) {
                return bt;
            }
        }
        return BuildingType{};
    };
    unordered_map<BuildingType, Fraction<int>> cnt;
    while (q.size()) {
        auto [v, k] = q.front();
        cnt[v] += k;
        q.pop_front();
        for (auto [res, c] : game.buildingProperties.at(v).workResources) {
            auto type = resToBuilding(res);
            q.emplace_back(type, k * c * product_ratio(v, type) / game.buildingProperties.at(type).produceAmount);
        }
    }
    if (normalized) {
        int mul = 1;
        for (auto &[type, c] : cnt) {
            mul = lcm(mul, c.denom);
        }
        for (auto &[type, c] : cnt) {
            c *= mul;
        }
    }
    return cnt;
}

unordered_map<BuildingType, Fraction<int>> getBuildingCounts(BuildingType type, bool normalized) {
    return getCounts(type, normalized, [](BuildingType a, BuildingType b) {
        auto worksByBuilding = [&](BuildingType type) {
            auto &p = game.buildingProperties.at(type);
            return Fraction(p.maxWorkers, p.workAmount);
        };
        return worksByBuilding(a) / worksByBuilding(b);
    });
}

unordered_map<BuildingType, Fraction<int>> getRobotCounts(BuildingType type, bool normalized) {
    return getCounts(type, normalized, [](BuildingType a, BuildingType b) {
        return Fraction(game.buildingProperties.at(b).workAmount, game.buildingProperties.at(a).workAmount);
    });
}

template<class Groups>
int getPlayersRobotsInGroups(int player, const Groups &groups) {
    auto to_count = views::filter([player] (auto &g) { return g.playerIndex == player; })
        | views::transform([] (auto &g) { return g.number; });
    return ranges::reduce(groups | to_count, 0);
}

int getPlayersRobotsOnPlanet(int player, int planet) {
    return getPlayersRobotsInGroups(player, game.planets[planet].workerGroups);
}

int getMyRobotsOnPlanet(int planet) {
    return getPlayersRobotsOnPlanet(game.myIndex, planet);
}

int getPlayersRobotsCount(int player) {
    int ans = getPlayersRobotsInGroups(player, game.flyingWorkerGroups);
    for (auto i : views::keys(game.planets))
        ans += getPlayersRobotsOnPlanet(player, i);
    return ans;
}

int getMyRobotsCount() {
    return getPlayersRobotsCount(game.myIndex);
}

int getMyTeamRobotsCount() {
    int ans = 0;
    for (size_t i = 0; i < game.players.size(); ++i) {
        auto &p = game.players[i];
        if (p.teamIndex == game.players[game.myIndex].teamIndex) {
            ans += getPlayersRobotsCount(i);
        }
    }
    return ans;
}


int getMyTeamRobotsOnPlanet(int planet) {
    int ans = 0;
    for (size_t i = 0; i < game.players.size(); ++i) {
        auto &p = game.players[i];
        if (p.teamIndex == game.players[game.myIndex].teamIndex) {
            ans += getPlayersRobotsOnPlanet(i, planet);
        }
    }
    return ans;
}
