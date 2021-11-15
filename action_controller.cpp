#include "action_controller.hpp"
#include "precalc.hpp"
#include "statistics.hpp"
#include "const.hpp"
#include "assert.hpp"
#include <limits>

using namespace std;
using namespace model;

coro::Task<bool> ActionController::waitWithPrior(int planet, int count, int ticks, int prior) {
    while (ticks--) {
        if (getPlayersRobotsOnPlanet(playerId, planet) - reservedRobots[planet] < count)
            co_return false;
        reservedRobots[planet] += count;
        co_await dispatcher.wait(0, constants::move_unreserve_prior);
        reservedRobots[planet] -= count;
        co_await dispatcher.wait(1, prior);
    }
    co_return getPlayersRobotsOnPlanet(playerId, planet) - reservedRobots[planet] >= count;
}
