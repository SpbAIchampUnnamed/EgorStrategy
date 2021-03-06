#ifndef __ACTION_CONTROLLER_HPP__
#define __ACTION_CONTROLLER_HPP__

#include "model/Game.hpp"
#include "model/Resource.hpp"
#include "statistics.hpp"
#include "precalc.hpp"
#include "const.hpp"
#include "dispatcher.hpp"
#include "coro/task.hpp"
#include <vector>
#include <array>
#include <optional>

struct ActionController {
    Dispatcher &dispatcher;
    std::vector<int> reservedRobots;
    std::vector<int> onWayTo;
    std::vector<std::array<int, model::EnumValues<model::Resource>::list.size()>> reservedResources;
    int playerId;
    ActionController(Dispatcher &dispatcher, int playerId = model::game.myIndex):
        dispatcher(dispatcher),
        reservedRobots(model::max_planet_index + 1, 0),
        onWayTo(model::max_planet_index + 1),
        reservedResources(model::max_planet_index + 1),
        playerId(playerId) {}
    template<class Callback>
    coro::Task<bool> move(int from, int to, int count, std::optional<model::Resource> res, Callback callback) {
        onWayTo[to] += count;
        for (int i = from, next = 0; i != to; i = next) {
            if (callback(i)) {
                onWayTo[to] -= count;
                co_return false;
            }
            int enemy_robots = 0;
            for (auto &g : model::game.planets[i].workerGroups) {
                if (model::game.players[g.playerIndex].teamIndex != model::game.players[playerId].teamIndex) {
                    enemy_robots += g.number;
                }
            }
            if (getPlayersRobotsOnPlanet(playerId, i) - reservedRobots[i] - enemy_robots - count < 0) {
                onWayTo[to] -= count;
                co_return false;
            }
            reservedRobots[i] += count;
            if (res)
                reservedResources[i][(int) *res] += count;
            if (model::game.players[playerId].specialty == model::Specialty::LOGISTICS)
                next = precalc::logist_prev[to][i];
            else
                next = precalc::regular_prev[to][i];
            std::vector move = {model::MoveAction(i, next, count, res)};
            co_await dispatcher.wait(0, constants::move_unreserve_prior);
            reservedRobots[i] -= count;
            if (res)
                reservedResources[i][(int) *res] -= count;
            int retrys = 0;
            int dist;
            if (model::game.players[playerId].specialty == model::Specialty::LOGISTICS)
                dist = precalc::logist_real_distance(next, i);
            else
                dist = precalc::regular_real_distance(next, i);
            while (!co_await dispatcher.doAndWait(model::Action(move, {}), dist, constants::move_reserve_prior)) {
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

    coro::Task<bool> move(int from, int to, int count, std::optional<model::Resource> res = std::nullopt) {
        return move(from, to, count, res, [](int) { return false; });
    }

    coro::Task<bool> waitWithPrior(int planet, int count, int ticks, int peior);
};

#endif