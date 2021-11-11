#include "MyStrategy.hpp"
#include "graph.hpp"
#include "precalc.hpp"
#include "action_controller.hpp"
#include "dispatcher.hpp"
#include "building_scheme.hpp"
#include "building_scheme_optimization.hpp"
#include "statistics.hpp"
#include "mincost.hpp"
#include "exploration.hpp"
#include "monitoring.hpp"
#include "utils/reduce.hpp"
#include "coro/safe_lambda.hpp"
#include "const.hpp"
#include "assert.hpp"
#include <iostream>
#include <random>
#include <ranges>
#include <algorithm>
#include <queue>
#include <unordered_set>
#include <random>
#include <set>
#include <functional>

using namespace std;
using namespace model;
using namespace precalc;
using namespace coro;

mt19937 rnd(42);

MyStrategy::MyStrategy() {}

Task<void> harvest_coro(Dispatcher &dispatcher, int start_planet) {
    co_await dispatcher.wait(0, numeric_limits<int>::max());

    dispatcher.reset();
    ActionController controller(dispatcher);

    vector<pair<int, BuildingType>> distribution;

    for (auto &p : views::values(game.planets)) {
        if (precalc::d[p.id][start_planet] < precalc::d[max_planet_index - p.id][start_planet] && p.harvestableResource) {
            for (auto &[type, prop] : game.buildingProperties) {
                if (prop.produceResource == p.harvestableResource) {
                    distribution.emplace_back(p.id, type);
                }
            }
        }
    }

    ranges::sort(distribution, [start_planet](auto &a, auto &b) {
        int a_score = game.buildingProperties.at(a.second).produceScore;
        int b_score = game.buildingProperties.at(b.second).produceScore;
        return tuple(-a_score, precalc::d[start_planet][a.first], a.first)
             < tuple(-b_score, precalc::d[start_planet][b.first], b.first);
    });

    auto intercept = [&](auto self, int count, int my_planet, int enemy_planet) -> Task<void> {
        auto it = ranges::min_element(distribution, [&](auto &a, auto &b) {
            return precalc::d[a.first][enemy_planet] < precalc::d[b.first][enemy_planet];
        });
        int target = it->first;
        if (target == my_planet) {
            co_await dispatcher.wait(1);
        } else {
            co_await controller.move(my_planet, precalc::prev[target][my_planet], count);
        }
    };

    {
        int robots = getMyRobotsCount();
        for (auto &g : game.flyingWorkerGroups) {
            if (g.playerIndex != game.myIndex && !g.resource) {
                robots -= g.number;
            }
        }
        cerr << "robots = " << robots << "\n";
        for (auto it = distribution.begin(); it != distribution.end(); ++it) {
            robots -= game.buildingProperties.at(it->second).maxWorkers;
            if (robots < 0) {
                distribution.erase(it == distribution.begin() ? next(it) : it, distribution.end());
                break;
            }
        }
    }

    constexpr int stone_for_building = 50;
    constexpr int max_workers = 100;

    make_coro([&](auto self) -> Task<void> {
        while (1) {
            vector<pair<int, int>> targets;
            for (auto &g : game.flyingWorkerGroups) {
                if (g.playerIndex != game.myIndex && !g.resource) {
                    targets.emplace_back(g.targetPlanet, g.number);
                } 
            }
            for (auto &p : views::values(game.planets)) {
                if (ranges::reduce(p.resources | views::transform([](auto &x)->auto& { return x.second; }), 0)
                    && p.workerGroups.size() == 1 && p.workerGroups[0].playerIndex != game.myIndex)
                {
                    targets.emplace_back(p.id, p.workerGroups[0].number);
                }
            }
            auto cur = targets.begin();
            for (auto i : views::keys(game.planets)) {
                if (cur == targets.end())
                    break;
                if (getMyRobotsOnPlanet(i) - controller.reservedRobots[i] > 0 && (int) i != start_planet) {
                    auto it = ranges::find_if(distribution, [i](auto &x) {
                        return x.first == (int) i;
                    });
                    int c = getMyRobotsOnPlanet(i) - controller.reservedRobots[i];
                    if (it != distribution.end()) {
                        c -= max_workers;
                    }
                    if (c > 0) {
                        make_coro(intercept, c, i, cur->first).start();
                        cur->second -= c;
                        if (cur->second <= 0)
                            ++cur;
                    }
                }
            }
            co_await dispatcher.wait(1, 1);
        }
    }).start();

    make_coro([&](auto self) -> Task<void> {
        while (1) {
            for (auto i : views::keys(game.planets)) {
                if (getMyRobotsOnPlanet(i) - controller.reservedRobots[i] > 0 && (int) i != start_planet) {
                    auto it = ranges::find_if(distribution, [i](auto &x) {
                        return x.first == (int) i;
                    });
                    if (it != distribution.end()) {
                        if ((!game.planets[i].building || game.planets[i].building->buildingType != it->second)
                            && game.planets[i].resources[Resource::STONE] < stone_for_building) {
                            cerr << "Return from " << i << "\n";
                            int c = min(getMyRobotsOnPlanet(i) - controller.reservedRobots[i], stone_for_building);
                            controller.move(i, start_planet, c).start();
                        }
                    } else {
                        cerr << "Return from " << i << "\n";
                        controller.move(i, start_planet, getMyRobotsOnPlanet(i) - controller.reservedRobots[i]).start();
                    }
                }
            }
            co_await dispatcher.wait(1);
        }
    }).start();

    auto balance_task = make_coro([&](auto self) -> Task<void> {
        while (1) {
            flows::FlowGraph g(max_planet_index + 1);
            int s = g.addVertex();
            int t = g.addVertex();
            vector<int> inputs, outputs;
            for (auto [p, type] : distribution) {
                int balance = getMyRobotsOnPlanet(p) - controller.reservedRobots[p] - game.buildingProperties.at(type).maxWorkers;
                if (balance < 0) {
                    g.addEdge(p, t, 0, -balance);
                    outputs.emplace_back(p);
                } else if (balance > 0) {
                    g.addEdge(s, p, 0, balance);
                    inputs.emplace_back(p);
                }
            }

            for (auto i : inputs) {
                for (auto o : outputs) {
                    g.addEdge(i, o, precalc::d[i][o]);
                }
            }
            ranges::sort(outputs);
            flows::mincost(g, s, t);
            for (auto i : inputs) {
                for (auto x : g.edges[i]) {
                    auto &e = g.edgesData[x];
                    if (e.flow > 0 && ranges::binary_search(outputs, e.to)) {
                        controller.move(i, e.to, e.flow).start();
                    }
                }
            }
            size_t j = distribution.size() - 1;
            for (size_t i = 0; i < j; ++i) {
                auto [p, type] = distribution[i];
                int enemy = 0;
                for (auto &g : game.flyingWorkerGroups) {
                    if (g.playerIndex != game.myIndex && g.targetPlanet == p)
                        enemy += g.number;
                }
                for (auto &g : game.planets[p].workerGroups) {
                    if (g.playerIndex != game.myIndex)
                        enemy += g.number;
                }
                while (i < j && getMyRobotsOnPlanet(p) - controller.reservedRobots[p] + controller.onWayTo[p] < max_workers + enemy) {
                    auto [p2, type2] = distribution[j];
                    if (type2 == type)
                        break;
                    int c = min(getMyRobotsOnPlanet(p2) - controller.reservedRobots[p2],
                        max_workers + enemy - (getMyRobotsOnPlanet(p) - controller.reservedRobots[p] + controller.onWayTo[p]));
                    bool flag = 0;
                    if (c > 0)
                        flag = controller.move(p2, p, c).start().isDone();
                    cerr << c << " " << p2 << " " << p << " " << getMyRobotsOnPlanet(p2) << " " << controller.reservedRobots[p2] << "\n";
                    if (getMyRobotsOnPlanet(p2) - controller.reservedRobots[p2] <= 0 || flag)
                        --j;
                }
            }
            co_await dispatcher.wait(1);
        }
    }).start();

    for (auto [p, type] : distribution) {
        if (game.planets[p].building && game.planets[p].building->buildingType == type)
            continue;
        while (game.planets[start_planet].resources[Resource::STONE] < stone_for_building
            || getMyRobotsOnPlanet(start_planet) - controller.reservedRobots[start_planet] < stone_for_building)
            co_await dispatcher.wait(1);
        cerr << "start to " << p << "\n";
        make_coro([&, p, type](auto self) -> Task<void> {
            co_await controller.move(start_planet, p, stone_for_building, Resource::STONE).start();
            Action act;
            while (game.planets[p].building && game.planets[p].building->buildingType != type) {
                co_await dispatcher.doAndWait(act, 1);
            }
            act.buildings = {BuildingAction(p, type)};
            co_await dispatcher.doAndWait(act, 0);
        }).start();
        int c = min(
            max_workers - stone_for_building - (getMyRobotsOnPlanet(p) - controller.reservedRobots[p]),
            getMyRobotsOnPlanet(start_planet));
        if (c > 0)
            controller.move(start_planet, p, c).start();
        co_await dispatcher.wait(1);
    }

    make_coro([&](auto self) -> Task<void> {
        while (1) {
            if (int cnt = getMyRobotsOnPlanet(start_planet) - controller.reservedRobots[start_planet]; cnt > 0) {
                for (auto [p, type] : distribution) {
                    if (int have = getMyRobotsOnPlanet(p) - controller.reservedRobots[p]; have < max_workers) {
                        int c = min(max_workers - have, cnt);
                        controller.move(start_planet, p, c).start();
                        cnt -= c;
                    }
                }
                if (cnt > 0 && distribution.size()) {
                    controller.move(start_planet, distribution[0].first, cnt).start();
                }
            }
            co_await dispatcher.wait(1);
        }
    }).start();

    co_await balance_task;
}

Task<void> main_coro(array<Dispatcher, EnumValues<Specialty>::list.size()> &dispatchers) {
    auto &dispatcher = dispatchers[(int) precalc::my_specialty];

    auto controllers = [&]<size_t... i>(index_sequence<i...>) {
        return array{ActionController(dispatchers[i], i)...};
    }(make_index_sequence<EnumValues<Specialty>::list.size()> {});

    auto &controller = controllers[(int) precalc::my_specialty];

    co_await dispatcher.doAndWait(Action({}, {}, precalc::my_specialty), 0, numeric_limits<int>::max());

    {
        vector<Task<void>> explorers;
        for (auto spec : EnumValues<Specialty>::list) {
            int start = (int) spec;
            for (int i = 0; i < 3; ++i) {
                explorers.emplace_back(explore(controllers[(int) spec], start, spec).start());
            }
        }
        for (auto t : explorers) {
            co_await t;
        }
    }

    // auto start_time = clock();

    auto [cost, best_mul, distribution, _] = getInitialScheme();

    // {
    //     int robots = getMyRobotsCount();
    //     auto prod = getTransfersByDistribution<Fraction<long long>>(distribution, robots).first;
    //     cerr << "prod = " << prod << "\n";
    //     for (int i = 0; i < 100 && clock() - start_time < CLOCKS_PER_SEC * 0.6; ++i) {
    //         int x = rnd() % distribution.size();
    //         int y = x;
    //         while (y == x)
    //             y = rnd() % distribution.size();
    //         swap(distribution[x].second, distribution[y].second);
    //         auto new_prod = getTransfersByDistribution<Fraction<long long>>(distribution, robots).first;
    //         if (new_prod > prod) {
    //             i = 0;
    //             prod = new_prod;
    //             cerr << "new prod = " << prod << "\n";
    //         } else {
    //             swap(distribution[x].second, distribution[y].second);
    //         }
    //     }
    //     cerr << "Optimize end\n";
    // }

    auto start_planet = (*ranges::find_if(views::values(game.planets), [](auto &p) {
        return p.workerGroups.size() > 0
            && p.workerGroups[0].playerIndex == game.myIndex
            && p.building
            && p.building->buildingType == BuildingType::QUARRY;
    })).id;

    {
        array<bool, max_planet_index + 1> used{};
        auto fill_used = [&](int i) {
            int x = game.planets[i].x;
            int y = game.planets[i].y;
            for (auto &p : views::values(game.planets)) {
                if (abs(p.x - x) + abs(p.y - y) <= *game.viewDistance) {
                    used[p.id] = 1;
                }
            }
        };
        for (auto p : views::keys(distribution))
            fill_used(p);
        for (auto i : views::keys(game.planets)) {
            if (!used[i]) {
                monitor_planet(controllers[(int) Specialty::LOGISTICS], (int) Specialty::LOGISTICS, i).start();
                fill_used(i);
            }
        }
    }

    ranges::sort(distribution, [start_planet](auto &a, auto &b) {
        // auto &a_bp = game.buildingProperties.at(a.second);
        // auto &b_bp = game.buildingProperties.at(b.second);
        // if (a_bp.harvest != b_bp.harvest)
        //     return pair(a_bp.harvest, a.first) > pair(b_bp.harvest, b.first);
        // if (a_bp.harvest)
        //     return pair(a_bp.produceScore, a.first) > pair(b_bp.produceScore, b.first);
        return pair(precalc::d[a.first][start_planet], a.first) < pair(precalc::d[b.first][start_planet], b.first);
    });

    vector<Transfer> transfers;
    vector<int> transfers_index;

    // auto [prod, transfers] = getTransfersByDistribution<Fraction<long long>>(distribution, getMyRobotsOnPlanet(start_planet));

    // ranges::sort(transfers, [](auto &x, auto &y) {
    //     if (x.from != y.from)
    //         return x.from < y.from;
    //     else
    //         return x.to < y.to;
    // });

    // cerr << prod << " robots per tick \n";

    // for (auto [f, t, c, r] : transfers) {
    //     cerr << f << "\t" << t << "\t" << c << "\t" << (r ? to_string(*r) : "NULL"sv) << "\n";
    // }

    // cerr << "\n";

    vector<Task<void>> tasks;

    int stone_required = 0;

    make_coro([&](auto self) -> Task<void> {
        int maxWorkers = game.buildingProperties.at(BuildingType::QUARRY).maxWorkers;
        int reserved = 0;
        vector<int> indx;
        ranges::copy(views::values(game.planets) | views::filter([](auto &p) {
            return !p.building || p.building->buildingType != BuildingType::QUARRY;
        }) | views::transform([](auto &p) {
            return p.id;
        }), back_inserter(indx));
        ranges::sort(indx, [start_planet](int a, int b) {
            return pair(precalc::d[start_planet][a], a) < pair(precalc::d[start_planet][b], b);
        });
        while (1) {
            bool flag = 1;
            Action act;
            if (!game.planets[start_planet].building || game.planets[start_planet].building->buildingType != BuildingType::QUARRY) {
                if (game.planets[start_planet].building)
                    act.buildings.emplace_back(start_planet, nullopt);
                else
                    act.buildings.emplace_back(start_planet, BuildingType::QUARRY);
            }

            if (game.planets[start_planet].resources[Resource::STONE] < constants::stone_reserve_size) {
                int c = min(
                    getMyRobotsOnPlanet(start_planet) - controller.reservedRobots[start_planet],
                    maxWorkers - reserved - controller.onWayTo[start_planet]);
                if (c > 0) {
                    reserved += c;
                    ASSERT(getMyRobotsOnPlanet(start_planet) >= controller.reservedRobots[start_planet]);
                }
            } else {
                reserved = 0;
                flag = 0;
            }
            if (flag) {
                cerr << "Stone reserve, " << game.currentTick << ": " << controller.reservedRobots[start_planet] << " ";
                cerr << reserved << " " << controller.onWayTo[start_planet] << "\n";
                for (auto i : indx) {
                    if (game.planets[i].building && game.planets[i].building->buildingType == BuildingType::FARM)
                        continue;
                    if (reserved + controller.onWayTo[start_planet] >= maxWorkers)
                        break;
                    int c = getMyRobotsOnPlanet(i) - controller.reservedRobots[i];
                    if (c > 0) {
                        controller.move(i, start_planet, min(c, maxWorkers - reserved - controller.onWayTo[start_planet])).start();
                    }
                }
            }
            co_await dispatcher.doAndWait(act, 0, constants::move_reserve_prior);
            if (reserved) {
                if (!co_await controller.waitWithPrior(start_planet, reserved, 1, constants::stone_reserve_prior).start())
                    reserved = 0;
            } else {
                co_await dispatcher.wait(1, constants::stone_reserve_prior);
            }
        }
    }).start();

    vector<pair<int, BuildingType>> my_distribution(0);
    {
        int total_stone = 0;
        vector<optional<BuildingType>> planned(max_planet_index + 1, nullopt);
        vector<bool> used(max_planet_index + 1);
        vector<bool> my_used(max_planet_index + 1);
        for (auto [p, type] : distribution) {
            planned[p] = type;
            total_stone += game.buildingProperties.at(type).buildResources.at(Resource::STONE);
        }
        auto generate_custom_distribution = [&] (int speciality) {
            int stone_used = 0;
            for (size_t i = 0; i < near_planets[speciality].size(); ++i) {
                if (planned[near_planets[speciality][i]] != nullopt && !used[near_planets[speciality][i]]) {
                    int planet_index = near_planets[speciality][i];
                    used[planet_index] = true;
                    if ((int) my_specialty == speciality) {
                        my_distribution.emplace_back(pair<int, BuildingType>(planet_index, planned[planet_index].value()));
                        my_used[planet_index] = true;
                    }
                    stone_used += game.buildingProperties.at(planned[planet_index].value()).buildResources.at(Resource::STONE);
                    if (stone_used >= total_stone / 3 && speciality != 0) {
                        break;
                    }
                }
            }
        };
        for (int i = 2; i >= 0; i--) {
            generate_custom_distribution(i);
        }
        for (auto [p, type] : distribution) {
            if (!my_used[p]) {
                my_distribution.emplace_back(p, type);
            }
        }
    }

    for (int prior = constants::building_task_initial_prior; auto [p, type] : distribution) {
        tasks.emplace_back(make_coro([&, p, type, prior=prior--](auto self) -> Task<void> {
            int stone_for_building = game.buildingProperties.at(type).buildResources.at(Resource::STONE);
            while (1) {
                auto building_done = [&](int = 0) {
                    return game.planets[p].building && game.planets[p].building->buildingType == type;
                };
                if (building_done()) {
                    co_await dispatcher.wait(1, prior);
                } else {
                    int need_robots = stone_for_building;
                    stone_required += stone_for_building;
                    int runing_tasks = 0;
                    while (need_robots > 0 || runing_tasks > 0) {
                        if (building_done()) {
                            stone_required -= need_robots;
                            need_robots = 0;
                        }
                        int cnt = min(game.planets[start_planet].resources[Resource::STONE]
                                - controller.reservedResources[start_planet][(int) Resource::STONE],
                            getMyRobotsOnPlanet(start_planet) - controller.reservedRobots[start_planet]
                        );
                        ASSERT(cnt >= 0);
                        cnt = min(cnt, need_robots);
                        need_robots -= cnt;
                        stone_required -= cnt;
                        if (cnt > 0) {
                            ++runing_tasks;
                            make_coro([&, cnt](auto self) -> Task<void> {
                                if (co_await controller.move(start_planet, p, cnt, Resource::STONE, building_done).start()) {
                                    if (game.planets[p].resources[Resource::STONE] < stone_for_building) {
                                        --runing_tasks;
                                        co_return;
                                    }
                                    Action act({}, {BuildingAction(p, nullopt)});
                                    if (game.planets[p].building) {
                                        if (game.planets[p].building->buildingType == type) {
                                            --runing_tasks;
                                            co_return;
                                        }
                                        co_await dispatcher.doAndWait(act, 0, constants::move_reserve_prior);
                                        int time = ceil(Fraction(game.planets[p].building->health, cnt));
                                        if (!co_await controller.waitWithPrior(p, cnt, time, prior).start()) {
                                            stone_required += cnt;
                                            need_robots += cnt;
                                            --runing_tasks;
                                            co_return;
                                        }
                                    }
                                    act.buildings = {BuildingAction(p, type)};
                                    int time = ceil(Fraction(game.buildingProperties.at(type).maxHealth, cnt));
                                    co_await dispatcher.doAndWait(act, 0, constants::move_reserve_prior);
                                    if (!co_await controller.waitWithPrior(p, cnt, time, prior).start()) {
                                        stone_required += cnt;
                                        need_robots += cnt;
                                        --runing_tasks;
                                        co_return;
                                    }
                                } else if (!building_done()) {
                                    stone_required += cnt;
                                    need_robots += cnt;
                                }
                                --runing_tasks;
                            }).start();
                        } else {
                            cerr << "Wait robots for build " << to_string(type) << " on " << p << "\n";
                        }
                        co_await dispatcher.wait(1, prior);
                    }
                }
            }
        }).start());
    }

    tasks.emplace_back(make_coro([&](auto self) -> Task<void> {
        while (1) {
            ASSERT(stone_required >= 0);
            auto need = [&]() {
                return stone_required - (getMyRobotsOnPlanet(start_planet)
                    - controller.reservedRobots[start_planet]
                    + controller.onWayTo[start_planet]);
            };
            if (need() > 0) {
                bool f = 0;
                unordered_set<BuildingType> used;
                for (auto i : views::keys(game.planets)) {
                    if ((int) i == start_planet)
                        continue;
                    if (game.planets[i].building && !used.contains(game.planets[i].building->buildingType)) {
                        used.emplace(game.planets[i].building->buildingType);
                        continue;
                    }
                    int c = min(getMyRobotsOnPlanet(i) - controller.reservedRobots[i], need());
                    ASSERT(c >= 0);
                    if (c > 0) {
                        f = 1;
                        controller.move(i, start_planet, c).start();
                    }
                    if (need() <= 0)
                        break;
                }
                if (!f && !controller.onWayTo[start_planet]) {
                    for (auto i : views::keys(game.planets)) {
                        if ((int) i == start_planet)
                            continue;
                        int c = min(getMyRobotsOnPlanet(i) - controller.reservedRobots[i], need());
                        ASSERT(c >= 0);
                        if (c > 0) {
                            f = 1;
                            controller.move(i, start_planet, c).start();
                        }
                        if (need() <= 0)
                            break;
                    }
                }
            }
            int reserved = min(stone_required, getMyRobotsOnPlanet(start_planet) - controller.reservedRobots[start_planet]);
            cerr << "reserved = " << reserved << "\n";
            if (reserved > 0)
                co_await controller.waitWithPrior(start_planet, reserved, 1, constants::repair_prior).start();
            else
                co_await dispatcher.wait(1, constants::repair_prior);
        }
    }).start());

    int batching_factor = 1;
    vector<int> static_robots(max_planet_index + 1);
    vector<Fraction<long long>> flow;

    make_coro([&](auto self) -> Task<void> {    
        vector<Fraction<long long>> pre_static_robots(max_planet_index + 1);
        while (1) {
            // check rush

            // int sum = 0;
            // for (auto &g : game.flyingWorkerGroups) {
            //     if (g.playerIndex != game.myIndex) {
            //         if (g.resource)
            //             sum -= g.number;
            //         else
            //             sum += g.number;
            //     }
            // }

            // if (sum >= 600) {
            //     harvest_coro(dispatcher, start_planet).start();
            //     co_return;
            // }

            // recalc transfers

            array<int, EnumValues<Specialty>::list.size()> robots{};

            ranges::fill(robots, getMyRobotsCount());

            for (size_t i = 0; i < game.players.size(); ++i) {
                auto &p = game.players[i];
                if (p.teamIndex == game.players[game.myIndex].teamIndex && p.specialty) {
                    robots[(int) *p.specialty] = getPlayersRobotsCount(i);
                }
            }

            // Use combat robots for interception
            // for (auto &g : game.flyingWorkerGroups) {
            //     if (g.playerIndex != game.myIndex) {
            //         auto it = ranges::find_if(distribution, [p = g.targetPlanet](auto &x) {
            //             return x.first == p;
            //         });
            //         if (it != distribution.end()) {
            //             robots -= g.number;
            //         }
            //     }
            // }

            auto scheme = getTransfersByDistribution<double>(distribution, robots, 0.001);
            auto &prod = scheme.prod;
            auto new_transfers = views::iota((size_t) 0, scheme.transfers.size()) | views::filter([&](size_t i) {
                return scheme.specialty[i] == precalc::my_specialty;
            }) | views::transform([&](size_t i) -> auto& {
                return scheme.transfers[i];
            });

            for (auto &x : scheme.transfers) {
                x.count.round_to_denom(constants::max_valid_flow_denom);
            }

            for (auto [from, to, cnt, res] : new_transfers) {
                cerr << from << "\t" << to << "\t" << cnt << "\t" << (res ? to_string(*res) : "NULL"sv).substr(0, 7) << "\n";
            }

            // cerr << prod << "\n";

            // for (size_t i = 0; i < new_transfers.size(); ++i) {
            //     auto &[from, to, count, res] = new_transfers[i];
            //     auto spec = scheme.specialty[i];
            //     cerr << from << "\t" << to << "\t" << count << "\t" << (res ? to_string(*res) : "NULL"sv).substr(0, 7);
            //     cerr << "\t" << to_string(spec).substr(0, 7) << "\n";
            // }

            // cerr << "\n";

            for (size_t i = 0; i < distribution.size(); ++i) {
                auto [p, type] = distribution[i];
                for (auto spec : EnumValues<Specialty>::list) {
                    if (scheme.static_robots[i][(int) spec] != 0) {
                        cerr << scheme.static_robots[i][(int) spec] << "\trobots of " << to_string(spec).substr(0, 7) << "\ton ";
                        cerr << p << "\t" << to_string(type) << "\n";
                    }
                }
            }

            // terminate();

            auto cmp = [&](int x, int y) {
                auto &xt = transfers[x];
                auto &yt = transfers[y];
                if (xt.from != yt.from)
                    return xt.from < yt.from;
                else
                    return xt.to < yt.to;
            };
            for (auto &[from, to, cnt, res] : transfers)
                cnt = 0;
            transfers.emplace_back();
            for (auto &t : new_transfers) {
                transfers.back() = t;
                auto range = ranges::equal_range(transfers_index, transfers.size() - 1, cmp);
                bool f = 0;
                for (auto i : range) {
                    auto &w = transfers[i];
                    if (t.res == w.res) {
                        w.count = t.count;
                        f = 1;
                        break;
                    }
                }
                if (!f) {
                    transfers_index.insert(range.end(), transfers.size() - 1);
                    transfers.emplace_back();
                }
            }
            transfers.pop_back();
            flow.resize(transfers.size(), 0);
            cerr << prod << " robots per tick\n";

            batching_factor = ceil(Fraction<long long>(ranges::reduce(transfers | views::transform([](auto &t) {
                return precalc::real_distance(t.from, t.to);
            }), 0), game.maxFlyingWorkerGroups));

            batching_factor = max(1, batching_factor);

            cerr << "batching_factor = " << batching_factor << "\n";

            // recalc static_robots

            ranges::fill(pre_static_robots, 0);

            for (size_t i = 0; i < distribution.size(); ++i) {
                auto p = distribution[i].first;
                pre_static_robots[p] = scheme.static_robots[i][(int) precalc::my_specialty];
                pre_static_robots[p].round_to_denom(constants::max_valid_flow_denom);
            }

            for (auto [p, type] : distribution) {
                auto &bp = game.buildingProperties.at(type);
                if (bp.produceResource) {
                    Fraction extra_prodict(game.planets[p].resources[*bp.produceResource], constants::product_planing_horizon);
                    pre_static_robots[p] -= extra_prodict / bp.produceAmount * bp.workAmount;
                    if (pre_static_robots[p] < 0) {
                        pre_static_robots[p] = 0;
                    }
                }
            }
            ranges::transform(pre_static_robots, static_robots.begin(), [](auto &x) { return ceil(x); });

            set<BuildingType> full;

            for (auto [p, type] : distribution)
                full.emplace(type);

            for (auto [p, type] : distribution) {
                auto &bp = game.buildingProperties.at(type);
                if (static_robots[p] + 1 < bp.maxWorkers) {
                    full.erase(type);
                } else {
                    // cerr << p << " " << static_robots[p] << " " << bp.maxWorkers << " " << to_string(type) << "\n";
                }
            }
            
            // for (auto &g : game.flyingWorkerGroups) {
            //     if (g.playerIndex != game.myIndex) {
            //         auto it = ranges::find_if(distribution, [p = g.targetPlanet](auto &x) {
            //             return x.first == p;
            //         });
            //         if (it != distribution.end()) {
            //             static_robots[g.targetPlanet] += g.number;
            //         }
            //     }
            // }

            // REWRITE THIS SHIT!!!
            if (full.size() && 0) {
                vector<Planet> planets;
                ranges::copy(views::values(game.planets), back_inserter(planets));
                for (auto [p, type] : distribution)
                    planets[p].id = -1;
                sort(planets.begin(), planets.end(), [&](auto &a, auto &b) {
                    auto dist_sum = [&](int i) {
                        if (i == -1)
                            return numeric_limits<int>::max();
                        int ans = 0;
                        for (auto [p, type] : distribution) {
                            ans += precalc::d[p][i];
                        }
                        return ans;
                    };
                    return pair(dist_sum(a.id), a.id) < pair(dist_sum(b.id), b.id);
                });
                for (auto t : full) {
                    auto &bp = game.buildingProperties.at(t);
                    auto criterium = [t](auto &p)->bool {
                        if (p.id < 0 || p.workerGroups.size() || p.building)
                            return 0;
                        if (game.buildingProperties.at(t).harvest)
                            return p.harvestableResource == game.buildingProperties.at(t).produceResource;
                        else
                            return 1;
                    };
                    auto it = ranges::find_if(planets, criterium);
                    if (it == planets.end()) {
                        continue;
                    }
                    int p = it->id;
                    it->id = -1;

                    make_coro([&, p, type=t, prior = constants::building_task_initial_prior - int(distribution.size())]
                        (auto self) -> Task<void> {
                        int stone_for_building = game.buildingProperties.at(type).buildResources.at(Resource::STONE);
                        while (1) {
                            if (game.planets[p].building && game.planets[p].building->buildingType == type) {
                                co_await dispatcher.wait(1, prior);
                            } else {
                                int need_robots = stone_for_building;
                                stone_required += stone_for_building;
                                int runing_tasks = 0;
                                while (need_robots > 0 || runing_tasks > 0) {
                                    int cnt = min(game.planets[start_planet].resources[Resource::STONE]
                                            - controller.reservedResources[start_planet][(int) Resource::STONE],
                                        getMyRobotsOnPlanet(start_planet) - controller.reservedRobots[start_planet]
                                    );
                                    cnt = min(cnt, need_robots);
                                    need_robots -= cnt;
                                    stone_required -= cnt;
                                    if (cnt > 0) {
                                        ++runing_tasks;
                                        make_coro([&, cnt](auto self) -> Task<void> {
                                            if (co_await controller.move(start_planet, p, cnt, Resource::STONE).start()) {
                                                if (game.planets[p].resources[Resource::STONE] < stone_for_building) {
                                                    --runing_tasks;
                                                    co_return;
                                                }
                                                Action act({}, {BuildingAction(p, nullopt)});
                                                if (game.planets[p].building) {
                                                    if (game.planets[p].building->buildingType == type) {
                                                        --runing_tasks;
                                                        co_return;
                                                    }
                                                    co_await dispatcher.doAndWait(act, 0, constants::move_reserve_prior);
                                                    int time = ceil(Fraction(game.planets[p].building->health, cnt));
                                                    if (!co_await controller.waitWithPrior(p, cnt, time, prior).start()) {
                                                        stone_required += cnt;
                                                        need_robots += cnt;
                                                        --runing_tasks;
                                                        co_return;
                                                    }
                                                }
                                                act.buildings = {BuildingAction(p, type)};
                                                int time = ceil(Fraction(game.buildingProperties.at(type).maxHealth, cnt));
                                                co_await dispatcher.doAndWait(act, 0, constants::move_reserve_prior);
                                                if (!co_await controller.waitWithPrior(p, cnt, time, prior).start()) {
                                                    stone_required += cnt;
                                                    need_robots += cnt;
                                                    --runing_tasks;
                                                    co_return;
                                                }
                                            } else {
                                                stone_required += cnt;
                                                need_robots += cnt;
                                            }
                                            --runing_tasks;
                                        }).start();
                                    } else {
                                        cerr << "Wait robots for build " << to_string(type) << " on " << p << "\n";
                                    }
                                    co_await dispatcher.wait(1, prior);
                                }
                            }
                        }
                    }).start();

                    distribution.emplace_back(p, t);
                }
            }
            co_await dispatcher.wait(1, constants::static_robots_recalc_prior);
        }
    }).start();

    int starts = 0;

    make_coro([&](auto self) -> Task<void> {
        while (1) {
            co_await dispatcher.wait(1, constants::cycle_main_prior - 1);
            for (size_t i = 0; i < transfers.size(); ++i) {
                if (transfers[i].count > 0)
                    cerr << flow[i] / transfers[i].count << " ";
                else
                    cerr << "0/0 ";
            }
            cerr << "\n";
            cerr << starts << " groups starts\n";
            starts = 0;
        }
    }).start();

    auto group_loop = [&](auto self, vector<int> path, int cnt) -> Task<void> {
        int len = 0;
        for (auto i : path) {
            auto [from, to, cnt, res] = transfers[i];
            len += precalc::real_distance(from, to);
        }
        for (auto i : path) {
            flow[i] += Fraction(cnt, len);
        }
        size_t next_index = 0;
        while (cnt > 0) {
            Fraction<long long> extra_robots = 0;
            for (auto i : path) {
                auto [from, to, cnt, res] = transfers[i];
                extra_robots = max(extra_robots, flow[i] - cnt);
            }
            // cerr << extra_robots << " " << len << " " << cnt << "\n";
            extra_robots = min(extra_robots, Fraction<long long>(cnt, len));
            if (extra_robots >= Fraction(1, len)) {
                for (auto i : path) {
                    flow[i] -= Fraction<long long>(floor(extra_robots * len), len);
                }
                cnt -= floor(extra_robots * len);
            }
            if (!cnt)
                break;
            ASSERT(cnt > 0);
            auto [from, to, _, res] = transfers[path[next_index]];
            ++next_index;
            if (next_index == path.size())
                next_index = 0;
            if (int d = cnt - (getMyRobotsOnPlanet(from) - controller.reservedRobots[from] - static_robots[from]); d > 0) {
                d = min(d, cnt);
                for (auto i : path) {
                    flow[i] -= Fraction(d, len);
                }
                cnt -= d;
            }
            if (!cnt)
                break;
            int ticks = (game.currentTick % 2) ^ ((game.planets[from].x + game.planets[from].y) % 2);
            if (!co_await controller.waitWithPrior(from, cnt, ticks, constants::cycle_main_prior).start()) {
                for (auto i : path) {
                    flow[i] -= Fraction(cnt, len);
                }
                break;
            }
            if (res && game.planets[from].resources[*res] - controller.reservedResources[from][(int) *res] < cnt) {
                cerr << "skip " << to_string(*res) << " on " << from << " in tick " << game.currentTick;
                cerr << " (" << game.planets[from].resources[*res] << " - ";
                cerr << controller.reservedResources[from][(int) *res] << " < " << cnt << ")\n";
                res = nullopt;
            }
            ++starts;
            if (!co_await controller.move(from, to, cnt, res).start()) {
                for (auto i : path) {
                    flow[i] -= Fraction(cnt, len);
                }
                break;
            } else {
                co_await dispatcher.wait(0, constants::cycle_main_prior);
            }
        }
    };

    make_coro([&](auto self) -> Task<void> {
        while (1) {
            for (size_t i = 0; i < transfers.size(); ++i) {
                auto [from, to, cnt, res] = transfers[i];
                while (flow[i] < cnt) {
                    int robots = getMyRobotsOnPlanet(from) - controller.reservedRobots[from] - static_robots[from];
                    if (robots <= 0)
                        break;
                    vector<char> used(max_planet_index + 1, 0);
                    vector<int> path = {(int) i};
                    auto dfs = [&](auto &dfs, int v, int t) -> bool {
                        used[v] = 1;
                        struct fict {};
                        auto children = ranges::equal_range(transfers_index, fict{}, [&](auto x, auto y) {
                            if constexpr (is_same_v<decltype(x), fict> && is_same_v<decltype(y), fict>)
                                return false;
                            else if constexpr (is_same_v<decltype(x), fict>)
                                return v < transfers[y].from;
                            else if constexpr (is_same_v<decltype(y), fict>)
                                return transfers[x].from < v;
                            else
                                return transfers[x].from < transfers[y].from;
                        });
                        for (auto i : children) {
                            auto [from, to, cnt, res] = transfers[i];
                            ASSERT(from == v);
                            if (flow[i] >= cnt)
                                continue;
                            if (used[to])
                                continue;
                            path.emplace_back(i);
                            if (to == t || dfs(dfs, to, t)) {
                                return 1;
                            } else {
                                path.pop_back();
                            }
                        }
                        return 0;
                    };
                    if (dfs(dfs, to, from)) {
                        ASSERT(path.size());
                        // cerr << flow[i] << "\n";
                        auto was = flow[i];
                        make_coro(group_loop, move(path), robots).start();
                        if (flow[i] == was)
                            break;
                        ranges::fill(used, 0);
                    } else {
                        break;
                    }
                }
                if (flow[i] < cnt) {
                    cerr << "need\t" << cnt - flow[i] << "\ton\t" << from << " - " << to << " in tick " << game.currentTick << "\n";
                }
            }
            // co_await dispatcher.wait(batching_factor, constants::cycle_capture_prior);
            co_await dispatcher.wait(1, constants::cycle_capture_prior);
        }
    }).start();

    make_coro([&](auto self) -> Task<void> {
        while (1) {
            for (size_t i = 0; i < transfers.size(); ++i) {
                flow[i].round_to_denom(constants::max_valid_flow_denom);
            }
            co_await dispatcher.wait(1, constants::flow_stabilization_prior);
        }
    }).start();

    tasks.emplace_back(make_coro([&](auto self) -> Task<void> {
        co_await dispatcher.wait(0, constants::free_robots_prior);
        while (1) {
            flows::FlowGraph g(max_planet_index + 1);
            int s = g.addVertex();
            int t = g.addVertex();
            vector<int> inputs, outputs;
            for (size_t i = 0; i < transfers.size(); ++i) {
                auto [from, to, cnt, res] = transfers[i];
                if (game.planets[from].building && flow[i] < cnt) {
                    outputs.emplace_back(from);
                    ASSERT(ceil(precalc::real_distance(from, to) * (cnt - flow[i])) > 0);
                    g.addEdge(from, t, 0, ceil(precalc::real_distance(from, to) * (cnt - flow[i])));
                }
            }
            for (auto i : views::keys(game.planets)) {
                int balance = getMyRobotsOnPlanet(i) - controller.reservedRobots[i] - static_robots[i] + controller.onWayTo[i];
                if (balance > 0) {
                    // cerr << balance << " free robots on planet " << i << " in tick " << game.currentTick << "\n";
                    if (balance > controller.onWayTo[i])
                        g.addEdge(s, i, 0, balance - controller.onWayTo[i]);
                    inputs.emplace_back(i);
                } else if (balance < 0) {
                    outputs.emplace_back(i);
                    g.addEdge(i, t, 0, -balance);
                }
            }

            ranges::sort(outputs);
            auto to_remove = ranges::unique(outputs);
            outputs.erase(to_remove.begin(), to_remove.end());
            for (auto i : inputs) {
                for (auto o : outputs) {
                    g.addEdge(i, o, precalc::d[i][o]);
                }
            }

            flows::mincost(g, s, t);
            for (auto i : inputs) {
                for (auto x : g.edges[i]) {
                    auto &e = g.edgesData[x];
                    if (e.flow > 0 && ranges::binary_search(outputs, e.to)) {
                        controller.move(i, e.to, e.flow).start();
                    }
                }
            }
            for (auto i : views::keys(game.planets)) {
                int c = getMyRobotsOnPlanet(i) - controller.reservedRobots[i];
                if (c > 0 && ranges::find_if(distribution, [i=int(i)](auto &x) { return x.first == i; }) == distribution.end()) {
                    auto filtered = distribution | views::filter([] (auto &x) {
                        auto [p, type] = x;
                        return game.planets[p].building && game.planets[p].building->buildingType == type;
                    });
                    auto it = ranges::min_element(filtered, [i](auto &a, auto &b) {
                        return precalc::d[i][a.first] < precalc::d[i][b.first];
                    });
                    if (it != filtered.end())
                        controller.move(i, it->first, c).start();
                }
            }
            co_await dispatcher.wait(1, constants::free_robots_prior);
        }
    }).start());

    for (auto x : tasks) {
        co_await x;
    }
}

void MyStrategy::play(Runner &runner)
{
    precalc::prepare();

    TaskQueue queue;
    array<Action, EnumValues<Specialty>::list.size()> acts;
    auto dispatchers = [&]<size_t... i>(index_sequence<i...>) {
        return array{Dispatcher(queue, acts[i])...};
    }(make_index_sequence<EnumValues<Specialty>::list.size()>{});

    main_coro(dispatchers).start();

    while (!queue.empty()) {
        while (queue.top().tick > game.currentTick) {
            runner.update(acts[(int) precalc::my_specialty]);
            precalc::prepare();
            for (auto &act : acts)
                act.clear();
        }
        auto h = queue.top().handle;
        queue.pop();
        h.resume();
    }
}
