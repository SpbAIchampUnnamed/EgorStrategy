#ifndef __BUILDING_SCHEME_OPTIMIZATION_HPP__
#define __BUILDING_SCHEME_OPTIMIZATION_HPP__

#include "building_scheme.hpp"
#include "precalc.hpp"
#include "utils/fraction.hpp"
#include "simplex.hpp"
#include "mincost.hpp"
#include <algorithm>
#include <tuple>
#include <array>

template<class CalcType>
struct ProductionScheme {
    CalcType prod;
    std::vector<Transfer> transfers;
    std::vector<model::Specialty> specialty;
    std::vector<std::array<CalcType, model::EnumValues<model::Specialty>::list.size()>> static_robots;
};

template<class CalcType>
ProductionScheme<CalcType> getTransfersByDistribution(
    const std::vector<std::pair<int, model::BuildingType>> &distribution,
    std::array<int, model::EnumValues<model::Specialty>::list.size()> robots_limits,
    CalcType eps = 0)
{
    using namespace std;
    using namespace model;
    using TransVar = tuple<int, int, optional<Resource>, Specialty>;
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
            if (balance[(int) ftype] > 0 && balance[(int) ttype] < 0) {
                for (auto spec : EnumValues<Specialty>::list) {
                    trans_vars.emplace_back(f, t, nullopt, spec);
                }
            }
            auto res = game.buildingProperties.at(ftype).produceResource; 
            if (res && game.buildingProperties.at(ttype).workResources.contains(*res)) {
                for (auto spec : EnumValues<Specialty>::list) {
                    trans_vars.emplace_back(f, t, res, spec);
                }
            }
        }
    }
    size_t var_cnt = trans_vars.size() + distribution.size() * EnumValues<Specialty>::list.size();
    size_t trans_vars_index = 0;
    array<size_t, EnumValues<Specialty>::list.size()> static_vars_index;
    static_vars_index[(int) Specialty::LOGISTICS] = trans_vars.size();
    static_vars_index[(int) Specialty::PRODUCTION] = trans_vars.size() + distribution.size();
    static_vars_index[(int) Specialty::COMBAT] = trans_vars.size() + distribution.size() * 2;

    ranges::sort(trans_vars);
    simplex_method::Solver<CalcType> solver(var_cnt);

    // worker count <= maxWorkers
    for (size_t i = 0; i < distribution.size(); ++i) {
        auto [p, type] = distribution[i];
        auto &bp = game.buildingProperties.at(type);
        vector<CalcType> coefs(var_cnt, 0);
        for (auto index : static_vars_index)
            coefs[index + i] = 1;
        solver.addLeConstraint(move(coefs), bp.maxWorkers);
    }

    // output * workAmount - work * produceAmount <= 0
    // output / produceAmount <= work * workAmount
    for (size_t i = 0; i < distribution.size(); ++i) {
        auto [p, type] = distribution[i];
        auto &bp = game.buildingProperties.at(type);
        if (!bp.produceResource)
            continue;
        vector<CalcType> coefs(var_cnt, 0);
        for (auto index : static_vars_index)
            coefs[index + i] = -1;
        coefs[static_vars_index[(int) Specialty::PRODUCTION] + i] -= CalcType(game.productionUpgrade) / 100;
        for (auto index : static_vars_index)
            coefs[index + i] *= bp.produceAmount;
        for (auto [t, type] : distribution) {
            if (!game.buildingProperties.at(type).workResources.contains(*bp.produceResource))
                continue;
            for (auto spec : EnumValues<Specialty>::list) {
                auto index = ranges::lower_bound(trans_vars, TransVar{p, t, bp.produceResource, spec}) - trans_vars.begin();
                coefs[trans_vars_index + index] = bp.workAmount;
            }
        }
        solver.addLeConstraint(move(coefs), 0);
    }

    // robots_in = robots_out
    for (auto spec : EnumValues<Specialty>::list) {
        for (auto [p, type] : distribution) {
            auto &bp = game.buildingProperties.at(type);
            vector<CalcType> coefs(var_cnt, 0);
            if (balance[(int) type] > 0) {
                for (auto [t, type] : distribution) {
                    if (balance[(int) type] < 0) {
                        coefs[ranges::lower_bound(trans_vars, TransVar{p, t, nullopt, spec}) - trans_vars.begin()] = 1;
                    }
                }
            }
            if (bp.produceResource) {
                for (auto [t, type] : distribution) {
                    if (!game.buildingProperties.at(type).workResources.contains(*bp.produceResource))
                        continue;
                    coefs[ranges::lower_bound(trans_vars, TransVar{p, t, bp.produceResource, spec}) - trans_vars.begin()] = 1;
                }
            }
            for (auto [f, ft] : distribution) {
                auto &bp = game.buildingProperties.at(ft);
                if (bp.produceResource && game.buildingProperties.at(type).workResources.contains(*bp.produceResource)) {
                    coefs[ranges::lower_bound(trans_vars, TransVar{f, p, bp.produceResource, spec}) - trans_vars.begin()] = -1;
                }
                if (balance[(int) type] < 0 && balance[(int) ft] > 0) {
                    coefs[ranges::lower_bound(trans_vars, TransVar{f, p, nullopt, spec}) - trans_vars.begin()] = -1;
                }
            }
            solver.addLeConstraint(coefs, 0);
            for (auto &x : coefs)
                x = -x;
            solver.addLeConstraint(move(coefs), 0);
        }
    }

    // work * resource_per_product_need - input * workAmount <= 0
    // work / workAmoint <= input / resource_per_product_need
    for (size_t i = 0; i < distribution.size(); ++i) {
        auto [p, type] = distribution[i];
        auto &bp = game.buildingProperties.at(type);
        for (auto [res, cnt] : bp.workResources) {
            vector<CalcType> coefs(var_cnt, 0);

            for (auto index : static_vars_index)
                coefs[index + i] = 1;
            coefs[static_vars_index[(int) Specialty::PRODUCTION] + i] += CalcType(game.productionUpgrade) / 100;
            for (auto index : static_vars_index)
                coefs[index + i] *= cnt;

            for (auto [f, ft] : distribution) {
                auto &fbp = game.buildingProperties.at(ft);
                if (fbp.produceResource == res) {
                    for (auto spec : EnumValues<Specialty>::list) {
                        auto index = ranges::lower_bound(trans_vars, TransVar{f, p, fbp.produceResource, spec}) - trans_vars.begin();
                        coefs[trans_vars_index + index] = -bp.workAmount;
                    }
                }
            }
            solver.addLeConstraint(move(coefs), 0);
        }
    }

    // transfers * dist + static_robots <= robots
    for (auto spec : EnumValues<Specialty>::list) {
        vector<CalcType> limit_coefs(var_cnt, 0);
        for (auto [f, ftype] : distribution) {
            for (auto [t, ttype] : distribution) {
                int d = (spec == Specialty::LOGISTICS ? precalc::logist_real_distance(f, t) : precalc::regular_real_distance(f, t));
                if (balance[(int) ftype] > 0 && balance[(int) ttype] < 0)
                    limit_coefs[ranges::lower_bound(trans_vars, TransVar{f, t, nullopt, spec}) - trans_vars.begin()] = d;
            }
        }
        for (size_t i = 0; i < distribution.size(); ++i) {
            limit_coefs[static_vars_index[(int) spec] + i] = 1;
        }
        solver.addLeConstraint(move(limit_coefs), robots_limits[(int) spec]);
    }
    vector<CalcType> target_coefs(var_cnt);
    for (size_t i = 0; i < distribution.size(); ++i) {
        auto [p, type] = distribution[i];
        if (type == BuildingType::REPLICATOR) {
            for (auto spec : EnumValues<Specialty>::list) {
                target_coefs[static_vars_index[(int) spec] + i] = 1;
                if (spec == Specialty::PRODUCTION) {
                    target_coefs[static_vars_index[(int) spec] + i] += CalcType(game.productionUpgrade) / 100;
                }
                target_coefs[static_vars_index[(int) spec] + i] /= game.buildingProperties.at(BuildingType::REPLICATOR).workAmount;
            }
        }
    }
    auto [t, ans] = solver.template solve<simplex_method::SolutionType::maximize>(move(target_coefs), eps);
    ProductionScheme<CalcType> scheme;
    scheme.prod = ans;
    scheme.static_robots.resize(distribution.size());
    for (size_t i = 0; i < trans_vars.size(); ++i) {
        if (t[i] > 0) {
            auto [from, to, res, spec] = trans_vars[i];
            scheme.transfers.emplace_back(from, to, t[i], res);
            scheme.specialty.emplace_back(spec);
        }
    }
    for (auto spec : EnumValues<Specialty>::list) {
        for (size_t i = 0; i < distribution.size(); ++i) {
            scheme.static_robots[i][(int) spec] = t[static_vars_index[(int) spec] + i];
        }
    }
    return scheme;
}

#endif

