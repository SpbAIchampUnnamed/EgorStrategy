#ifndef __BUILDING_SCHEME_OPTIMIZATION_HPP__
#define __BUILDING_SCHEME_OPTIMIZATION_HPP__

#include "building_scheme.hpp"
#include "precalc.hpp"
#include "utils/fraction.hpp"
#include "simplex.hpp"
#include "mincost.hpp"
#include <algorithm>

template<class CalcType>
std::pair<CalcType, std::vector<Transfer>> getTransfersByDistribution(
    const std::vector<std::pair<int, model::BuildingType>> &distribution,
    int robots_limit,
    CalcType eps = 0)
{
    using namespace std;
    using namespace model;
    using TransVar = tuple<int, int, optional<Resource>>;
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
    size_t var_cnt = trans_vars.size() + 1;
    ranges::sort(trans_vars);
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
                        coefs[ranges::lower_bound(trans_vars, TransVar{f, p, bp.produceResource}) - trans_vars.begin()] = -1;
                    }
                }
                coefs[trans_vars.size()] = cnt;
                solver.addLeConstraint(move(coefs), 0);
            }
        }
    }
    vector<CalcType> limit_coefs(var_cnt, 0);
    for (auto [f, ftype] : distribution) {
        for (auto [t, ttype] : distribution) {
            int d = precalc::d[f][t] / game.planets.size();
            if (balance[(int) ftype] > 0 && balance[(int) ttype] < 0)
                limit_coefs[ranges::lower_bound(trans_vars, TransVar{f, t, nullopt}) - trans_vars.begin()] = d;
            auto &bp = game.buildingProperties.at(ftype);
            auto res = bp.produceResource;
            CalcType robots_by_res = (CalcType) bp.workAmount / bp.produceAmount;
            if (res && game.buildingProperties.at(ttype).workResources.contains(*res)) {
                limit_coefs[ranges::lower_bound(trans_vars, TransVar{f, t, res}) - trans_vars.begin()] = d + robots_by_res;
            }
        }
    }
    limit_coefs[trans_vars.size()] = game.buildingProperties.at(BuildingType::REPLICATOR).workAmount;
    solver.addLeConstraint(move(limit_coefs), robots_limit);
    vector<CalcType> target_coefs(var_cnt);
    target_coefs[trans_vars.size()] = 1;
    auto [t, ans] = solver.template solve<simplex_method::SolutionType::maximize>(move(target_coefs), eps);
    vector<Transfer> transfers;
    for (size_t i = 0; i < trans_vars.size(); ++i) {
        if (t[i] > 0) {
            auto [from, to, res] = trans_vars[i];
            transfers.emplace_back(from, to, t[i], res);
        }
    }
    return {ans, transfers};
}

#endif

