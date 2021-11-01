#include "action_controller.hpp"
#include "precalc.hpp"
#include "statistics.hpp"
#include "const.hpp"
#include "assert.hpp"
#include <limits>

using namespace std;
using namespace model;

coro::Task<bool> ActionController::move(int from, int to, int count, std::optional<model::Resource> res) {
    onWayTo[to] += count;
    for (int i = from, next = 0; i != to; i = next) {
        constexpr int emeny_index = 1;
        if (getMyRobotsOnPlanet(i) - reservedRobots[i] - getPlayersRobotsOnPlanet(emeny_index, i) - count < 0) {
            onWayTo[to] -= count;
            co_return false;
        }
        reservedRobots[i] += count;
        if (res)
            reservedResources[i][(int) *res] += count;
        next = precalc::prev[to][i];
        vector move = {MoveAction(i, next, count, res)};
        co_await dispatcher.wait(0, constants::move_unreserve_prior);
        reservedRobots[i] -= count;
        if (res)
            reservedResources[i][(int) *res] -= count;
        int retrys = 0;
        while (!co_await dispatcher.doAndWait(Action(move, {}), precalc::d[next][i] / game.planets.size(), constants::move_reserve_prior)) {
            reservedRobots[i] += count;
            if (res)
                reservedResources[i][(int) *res] += count;
            ++retrys;
            co_await dispatcher.wait(1, constants::move_unreserve_prior + retrys);
            reservedRobots[i] -= count;
            if (res)
                reservedResources[i][(int) *res] -= count;
        }
    }
    onWayTo[to] -= count;
    co_return true;
}

coro::Task<bool> ActionController::waitWithPrior(int planet, int count, int ticks, int prior) {
    while (ticks--) {
        if (getMyRobotsOnPlanet(planet) - reservedRobots[planet] < count)
            co_return false;
        reservedRobots[planet] += count;
        co_await dispatcher.wait(0, constants::move_unreserve_prior);
        reservedRobots[planet] -= count;
        co_await dispatcher.wait(1, prior);
    }
    co_return getMyRobotsOnPlanet(planet) - reservedRobots[planet] >= count;
}
