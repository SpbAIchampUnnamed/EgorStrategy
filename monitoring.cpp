#include "monitoring.hpp"
#include "precalc.hpp"
#include "assert.hpp"
#include "const.hpp"
#include <iostream>

using namespace model;
using namespace std;
using namespace precalc;

coro::Task<void> monitor_planet(ActionController &controller, int from, int planet) {
    ASSERT(game.players[controller.playerId].specialty == Specialty::LOGISTICS);
    while (1) {
        bool under_attack = 0;
        auto enemy_groups = game.flyingWorkerGroups | views::filter([](auto &g) {
            return game.players[g.playerIndex].teamIndex != game.players[game.myIndex].teamIndex;
        });
        for (auto &g : enemy_groups) {
            if (g.targetPlanet == from && g.nextPlanetArrivalTick == game.currentTick + 1) {
                under_attack = 1;
                break;
            }
        }
        bool flag = 0;
        if (from != planet || under_attack) {
            for (auto x : logist_planets_graph.edges[from]) {
                if (x == from)
                    continue;
                if (logist_real_distance(from, x) + logist_real_distance(x, planet) == logist_real_distance(from, planet)
                    || under_attack) {
                    bool safe = 1;
                    for (auto &g : game.planets[x].workerGroups) {
                        if (game.players[g.playerIndex].teamIndex != game.players[game.myIndex].teamIndex) {
                            safe = 0;
                            break;
                        }
                    }
                    if (!safe) {
                        continue;
                    }
                    int arrival_tick = game.currentTick + logist_real_distance(from, x);
                    for (auto &g : enemy_groups) {
                        if (g.targetPlanet == x && g.nextPlanetArrivalTick <= arrival_tick) {
                            safe = 0;
                            break;
                        }
                    }
                    if (safe) {
                        if (!co_await controller.move(from, x, 1).start())
                            co_return;
                        from = x;
                        flag = 1;
                        break;
                    }
                }
            }
        }
        if (!flag) {
            bool res = co_await controller.waitWithPrior(from, 1, 1, constants::monitoring_prior).start();
            if (!res) {
                cerr << "Fail in monitoring " << from << " " << game.currentTick << "\n";
                co_return;
            }
        }
    }
}
