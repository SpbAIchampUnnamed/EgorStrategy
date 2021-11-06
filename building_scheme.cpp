#include "building_scheme.hpp"
#include "model/Game.hpp"
#include "statistics.hpp"
#include "precalc.hpp"
#include "simplex.hpp"
#include "mincost.hpp"
#include "utils/reduce.hpp"
#include "assert.hpp"
#include <iostream>
#include <ranges>

using namespace std;
using namespace model;

BuildingScheme getInitialScheme() {
    auto start_planet = ranges::find_if(game.planets, [](auto &p) {
        return p.workerGroups.size() && game.players[p.workerGroups[0].playerIndex].teamIndex == game.players[game.myIndex].teamIndex;
    })->id;
    int my_robots = getMyTeamRobotsCount();
    cerr << my_robots << "\n";
    auto cnts = getRobotCounts(BuildingType::REPLICATOR);
    int mul_max = ceil(
            game.buildingProperties.at(BuildingType::REPLICATOR).maxWorkers / cnts[BuildingType::REPLICATOR]);
    for (int mul = 5; mul >= 1; --mul) {
        cerr << "MUL = " << mul << "\n";
        int robots = 0;
        vector<BuildingType> buildings;
        for (auto [type, cnt] : cnts) {
            auto full_buildings = cnt * mul / game.buildingProperties.at(type).maxWorkers;
            for (int i = 0; i < ceil(full_buildings); ++i)
                buildings.emplace_back(type);
            robots += ceil(cnt * mul);
            cerr << to_string(type) << " " << full_buildings << "(" << ceil(full_buildings) << ")\n";
        }
        sort(buildings.begin(), buildings.end(), [](auto a, auto b) {
            auto &bp_a = game.buildingProperties.at(a);
            auto &bp_b = game.buildingProperties.at(b);
            if (bp_a.produceResource.has_value() != bp_b.produceResource.has_value())
                return bp_a.produceResource.has_value() < bp_b.produceResource.has_value();
            else if (bp_a.harvest != bp_b.harvest)
                return bp_a.harvest > bp_b.harvest;
            else
                return (int) a < (int) b;
        });
        BuildingScheme best_scheme;
        best_scheme.cost = 1e6;
        auto estimate_cost = [&](BuildingScheme &scheme) {
            auto &distribution = scheme.distribution;
            vector<tuple<int, int, int, optional<Resource>>> transfers;
            int ans = 0;
            using MappingType = vector<pair<int, int>>;
            auto addTransfers = [&]<bool add = false>(
                flows::FlowGraph &&g,
                MappingType &&from_mapping,
                MappingType &&to_mapping,
                int s, int t,
                optional<Resource> res)
            {
                for (auto [vertex_from, planet_from] : from_mapping) {
                    for (auto [vertex_to, planet_to] : to_mapping) {
                        g.addEdge(vertex_from, vertex_to, precalc::logist_real_distance(planet_from, planet_to));
                    }
                }
                ranges::sort(to_mapping);
                auto [c, f] = flows::mincost(g, s, t);
                if constexpr (add) {
                    for (auto [v, p] : from_mapping) {
                        for (auto x : g.edges[v]) {
                            auto &e = g.edgesData[x];
                            if (!e.flow)
                                continue;
                            if (auto it = ranges::lower_bound(to_mapping, pair(e.to, 0)); it != to_mapping.end() && it->first == e.to) {
                                transfers.emplace_back(p, it->second, e.flow, res);
                            }
                        }
                    }
                }
                ans += c;
            };
            for (auto from : EnumValues<BuildingType>::list) {
                if (!game.buildingProperties.contains(from))
                    continue;
                if (!game.buildingProperties.at(from).produceResource)
                    continue;
                auto res = *game.buildingProperties.at(from).produceResource;
                vector<int> inputs;
                auto &bp = game.buildingProperties.at(from);
                int cnt = bp.maxWorkers * bp.produceAmount / bp.workAmount;
                flows::FlowGraph g;
                int s = g.addVertex();
                MappingType from_mapping, to_mapping;
                for (auto [p, type] : distribution) {
                    if (type == from) {
                        int v = g.addVertex();
                        from_mapping.emplace_back(v, p);
                        g.addEdge(s, v, 0, cnt);
                    }
                }
                int t = g.addVertex();
                for (auto to : EnumValues<BuildingType>::list) {
                    if (!game.buildingProperties.contains(to))
                        continue;
                    auto &bp = game.buildingProperties.at(to);
                    if (bp.workResources.contains(res)) {
                        int imm = g.addVertex();
                        g.addEdge(imm, t, 0, cnts[to].num * mul * bp.workResources.at(res) / bp.workAmount);
                        for (auto [p, type] : distribution) {
                            if (type == to) {
                                int v = g.addVertex();
                                to_mapping.emplace_back(v, p);
                                g.addEdge(v, imm, 0, bp.maxWorkers / bp.workAmount * bp.workResources.at(res));
                            }
                        }
                    }
                }
                addTransfers(move(g), move(from_mapping), move(to_mapping), s, t, res);
            }
            {
                MappingType from_mapping, to_mapping;
                vector<int> balance(game.planets.size(), 0);
                flows::FlowGraph g;
                for (auto [f, t, c, _] : transfers) {
                    balance[f] -= c;
                    balance[t] += c;
                }
                int s = g.addVertex();
                int t = g.addVertex();
                for (size_t i = 0; i < game.planets.size(); ++i) {
                    if (balance[i] < 0) {
                        int v = g.addVertex();
                        to_mapping.emplace_back(v, i);
                        g.addEdge(v, t, 0, -balance[i]);
                    } else if (balance[i] > 0) {
                        int v = g.addVertex();
                        from_mapping.emplace_back(v, i);
                        g.addEdge(s, v, 0, balance[i]);
                    }
                }
                addTransfers.operator()<false>(move(g), move(from_mapping), move(to_mapping), s, t, nullopt);
            }
            return ans;
        };
        vector<pair<int, BuildingScheme>> schemes;
        for (size_t i = 0; i < game.planets.size(); ++i) {
            if (game.planets[i].workerGroups.size())
                continue;
            schemes.emplace_back();
            auto &[cost, smul, distribution, transfers] = schemes.back().second;
            smul = mul;
            auto planets = game.planets;
            sort(planets.begin(), planets.end(), [i](auto &a, auto &b) {
                if (precalc::logist_d[i][a.id] != precalc::logist_d[i][b.id])
                    return precalc::logist_d[i][a.id] < precalc::logist_d[i][b.id];
                else
                    return a.id < b.id;
            });
            for (auto type : buildings) {
                auto criterium = [type, start_planet](auto &p)->bool {
                    if (p.id < 0 || p.workerGroups.size())
                        return 0;
                    if (game.buildingProperties.at(type).harvest)
                        return p.harvestableResource == game.buildingProperties.at(type).produceResource;
                    else
                        return 1;
                };
                auto it = ranges::find_if(planets, criterium);
                ASSERT(it != planets.end());
                distribution.emplace_back(it->id, type);
                it->id = -1;
            }
            schemes.back().first = estimate_cost(schemes.back().second);
        }
        ranges::stable_sort(schemes, [](auto &a, auto &b) {
            return a.first < b.first;
        });
        for (auto &[est_cost, scheme] : schemes) {
            if (est_cost >= best_scheme.cost) {
                break;
            }
            auto &[cost, smul, distribution, transfers] = scheme;
            array<int, EnumValues<BuildingType>::list.size()> balance{};
            for (auto type : EnumValues<BuildingType>::list) {
                if (!game.buildingProperties.contains(type))
                    continue;
                auto &bp = game.buildingProperties.at(type);
                if (bp.produceResource)
                    balance[(int) type] -= bp.produceAmount;
                for (auto [_, cnt] : bp.workResources) {
                    balance[(int) type] += cnt;
                }
            }
            using TransVar = tuple<int, int, optional<Resource>>;
            vector<TransVar> trans_vars;
            for (auto [f, ftype] : distribution) {
                for (auto [t, ttype] : distribution) {
                    if (balance[(int) ftype] > 0 && balance[(int) ttype] < 0)
                        trans_vars.emplace_back(f, t, nullopt);
                    auto res = game.buildingProperties.at(ftype).produceResource; 
                    if (res && game.buildingProperties.at(ttype).workResources.contains(*res)) {
                        trans_vars.emplace_back(f, t, res);
                    }
                }
            }
            size_t var_cnt = trans_vars.size();
            ranges::sort(trans_vars);
            using CalcType = double;
            simplex_method::Solver<CalcType> solver(var_cnt);
            for (auto [p, type] : distribution) {
                auto &bp = game.buildingProperties.at(type);
                if (!bp.produceResource)
                    continue;
                int max_prod = bp.maxWorkers * bp.produceAmount / bp.workAmount;
                vector<CalcType> coefs(var_cnt, 0);
                for (auto [t, type] : distribution) {
                    if (!game.buildingProperties.at(type).workResources.contains(*bp.produceResource))
                        continue;
                    coefs[ranges::lower_bound(trans_vars, TransVar{p, t, bp.produceResource}) - trans_vars.begin()] = 1;
                }
                solver.addLeConstraint(move(coefs), max_prod);
            }
            for (auto [p, type] : distribution) {
                auto &bp = game.buildingProperties.at(type);
                vector<CalcType> coefs(var_cnt, 0);
                if (balance[(int) type] > 0) {
                    for (auto [t, type] : distribution) {
                        if (balance[(int) type] < 0)
                            coefs[ranges::lower_bound(trans_vars, TransVar{p, t, nullopt}) - trans_vars.begin()] = 1;
                    }
                }
                if (bp.produceResource) {
                    for (auto [t, type] : distribution) {
                        if (!game.buildingProperties.at(type).workResources.contains(*bp.produceResource))
                            continue;
                        coefs[ranges::lower_bound(trans_vars, TransVar{p, t, bp.produceResource}) - trans_vars.begin()] = 1;
                    }
                }
                for (auto [f, ft] : distribution) {
                    auto &bp = game.buildingProperties.at(ft);
                    if (bp.produceResource && game.buildingProperties.at(type).workResources.contains(*bp.produceResource))
                        coefs[ranges::lower_bound(trans_vars, TransVar{f, p, bp.produceResource}) - trans_vars.begin()] = -1;
                    if (balance[(int) type] < 0 && balance[(int) ft] > 0)
                        coefs[ranges::lower_bound(trans_vars, TransVar{f, p, nullopt}) - trans_vars.begin()] = -1;
                }
                solver.addLeConstraint(coefs, 0);
                for (auto &x : coefs)
                    x = -x;
                solver.addLeConstraint(move(coefs), 0);
            }
            for (auto [p, type] : distribution) {
                auto &bp = game.buildingProperties.at(type);
                if (!bp.produceResource)
                    continue;
                for (auto [res, cnt] : bp.workResources) {
                    vector<CalcType> coefs(var_cnt, 0);
                    for (auto [t, type] : distribution) {
                        if (!game.buildingProperties.at(type).workResources.contains(*bp.produceResource))
                            continue;
                        coefs[ranges::lower_bound(trans_vars, TransVar{p, t, bp.produceResource}) - trans_vars.begin()] = cnt;
                    }
                    for (auto [f, ft] : distribution) {
                        auto &bp = game.buildingProperties.at(ft);
                        if (bp.produceResource == res)
                            coefs[ranges::lower_bound(trans_vars, TransVar{f, p, bp.produceResource}) - trans_vars.begin()] = -1;
                    }
                    solver.addLeConstraint(move(coefs), 0);
                }
            }
            for (auto [p, type] : distribution) {
                if (type == BuildingType::REPLICATOR) {
                    for (auto [res, cnt] : game.buildingProperties.at(BuildingType::REPLICATOR).workResources) {
                        vector<CalcType> coefs(var_cnt, 0);
                        for (auto [f, ft] : distribution) {
                            auto &bp = game.buildingProperties.at(ft);
                            if (bp.produceResource == res) {
                                coefs[ranges::lower_bound(trans_vars, TransVar{f, p, bp.produceResource}) - trans_vars.begin()] = 1;
                            }
                        }
                        solver.addGeConstraint(move(coefs), cnt * mul);
                    }
                }
            }
            vector<CalcType> target_coefs(var_cnt, 0);
            for (auto [f, ftype] : distribution) {
                for (auto [t, ttype] : distribution) {
                    int d = precalc::logist_real_distance(f, t);
                    if (balance[(int) ftype] > 0 && balance[(int) ttype] < 0)
                        target_coefs[ranges::lower_bound(trans_vars, TransVar{f, t, nullopt}) - trans_vars.begin()] = d;
                    auto res = game.buildingProperties.at(ftype).produceResource;
                    if (res && game.buildingProperties.at(ttype).workResources.contains(*res)) {
                        target_coefs[ranges::lower_bound(trans_vars, TransVar{f, t, res}) - trans_vars.begin()] = d;
                    }
                }
            }
            auto [t, ans] = solver.solve<simplex_method::SolutionType::minimize>(target_coefs, 0.001);
            scheme.cost = 0;
            for (size_t i = 0; i < trans_vars.size(); ++i) {
                if (t[i] > 0) {
                    auto [from, to, res] = trans_vars[i];
                    transfers.emplace_back(from, to, round(t[i]), res);
                    scheme.cost += precalc::logist_real_distance(from, to) * round(t[i]);
                }
            }
            if (scheme.cost < best_scheme.cost) {
                cerr << scheme.cost << "\n";
                best_scheme = scheme;
            }
        }
        int dist = 0, alt_dist = 0;
        for (auto [p, _] : best_scheme.distribution) {
            dist += precalc::logist_d[start_planet][p];
            p = game.planets.size() - 1 - p;
            alt_dist += precalc::logist_d[start_planet][p];
        }
        if (alt_dist < dist) {
            for (auto &[p, _] : best_scheme.distribution) {
                p = game.planets.size() - 1 - p;
            }
            for (auto &[from, to, cnt, res] : best_scheme.transfers) {
                from = game.planets.size() - 1 - from;
                to = game.planets.size() - 1 - to;
            }
        }
        cerr << "total_robots = " << robots << " + " << best_scheme.cost << " = " << robots + best_scheme.cost << "\n";
        if (robots + best_scheme.cost <= my_robots) {
            best_scheme = improveBuildingScheme(best_scheme);
            cerr << "new_total_robots = " << robots << " + " << best_scheme.cost << " = " << robots + best_scheme.cost << "\n";
            return best_scheme;
        }
    }
    cerr << "CAN NOT BUILD RELAIBLE SCHEME\n";
    terminate();
}

BuildingScheme improveBuildingScheme(BuildingScheme &building_scheme) {
    BuildingScheme new_building_scheme = building_scheme;
    double best_value = estimateBuildingScheme(new_building_scheme);
    vector <optional<BuildingType>> planet_building(game.planets.size(), nullopt);
    for (auto &[planet_index, building]: new_building_scheme.distribution) {
        planet_building[planet_index] = building;
    }
    for (auto loop = 0; loop < 10; loop++) {
        for (auto distribution = building_scheme.distribution; auto &[planet_index, building] : distribution) {
            if (building == BuildingType::MINES ||
                building == BuildingType::CAREER ||
                building == BuildingType::QUARRY ||
                building == BuildingType::FARM) {
                continue;
            }
            for (auto new_planet_index_find = 0; new_planet_index_find < 30; new_planet_index_find++) {
                int new_planet_index = precalc::near_planets[planet_index][new_planet_index_find];
                if (planet_building[new_planet_index] == BuildingType::MINES ||
                    planet_building[new_planet_index] == BuildingType::CAREER ||
                    planet_building[new_planet_index] == BuildingType::FARM ||
                    planet_building[new_planet_index] == BuildingType::QUARRY) {
                    continue;
                }
                for (Transfer &transfer: new_building_scheme.transfers) {
                    if (transfer.to == planet_index) transfer.to = new_planet_index;
                    else if (transfer.from == planet_index) transfer.from = new_planet_index;
                    else if (transfer.to == new_planet_index) transfer.to = planet_index;
                    else if (transfer.from == new_planet_index) transfer.from = planet_index;
                }
                auto old_planet_building = planet_building[new_planet_index];
                planet_building[new_planet_index] = planet_building[planet_index];
                planet_building[planet_index] = old_planet_building;
                double value = estimateBuildingScheme(new_building_scheme);
                if (value < best_value) {
                    building_scheme = new_building_scheme;
                    building_scheme.cost = calculateBuildingSchemeCost(building_scheme);
                    best_value = value;
                }
            }

        }
    }
    return building_scheme;
}

double estimateBuildingScheme(BuildingScheme &building_scheme) {
    double value = 0;
    for (Transfer &transfer: building_scheme.transfers) {
        value += (1.0 / 3) * precalc::regular_real_distance(transfer.from, transfer.to) * transfer.count.num;
        value += (2.0 / 3) * precalc::logist_real_distance(transfer.from, transfer.to) * transfer.count.num;
    }
//    for (auto &p1: building_scheme.distribution) {
//        for (auto &p2: building_scheme.distribution) {
//            value += (1.0 / 15) * precalc::regular_real_distance(p1.first, p2.first);
//        }
//    }
//    int maxDistFrom0 = 100, maxDistFrom1 = 100, maxDistFrom2 = 100;
//    for (auto &p: building_scheme.distribution) {
//        maxDistFrom0 = max(maxDistFrom0, precalc::regular_real_distance(0, p.first));
//        maxDistFrom1 = max(maxDistFrom1, precalc::regular_real_distance(1, p.first));
//        maxDistFrom2 = max(maxDistFrom2, precalc::regular_real_distance(2, p.first));
//    }
//    value += (6.0 / 3) * building_scheme.distribution.size() * maxDistFrom0;
//    value += (6.0 / 3) * building_scheme.distribution.size() * maxDistFrom1;
//    value += (6.0 / 3) * building_scheme.distribution.size() * maxDistFrom2;
    return value;
}

int calculateBuildingSchemeCost(BuildingScheme &building_scheme) {
    int cost = 0;
    for (auto &[from, to, count, res] : building_scheme.transfers) {
        cost += count.num * precalc::regular_real_distance(from, to);
    }
    return cost;
}