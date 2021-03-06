#ifndef __BUILDING_SCHME_HPP__
#define __BUILDING_SCHME_HPP__

#include "model/Resource.hpp"
#include "model/BuildingType.hpp"
#include "utils/fraction.hpp"
#include <vector>
#include <array>
#include <optional>
#include <tuple>
#include <compare>

struct Transfer {
    int from, to;
    Fraction<long long> count;
    std::optional<model::Resource> res = std::nullopt;

};

struct BuildingScheme {
    int cost = 0;
    int mul = 1;
    std::vector<std::pair<int, model::BuildingType>> distribution;
    std::vector<Transfer> transfers;
};

BuildingScheme getInitialScheme();

double estimateBuildingScheme(BuildingScheme &building_scheme);

BuildingScheme improveBuildingScheme(BuildingScheme &building_Scheme);

int calculateBuildingSchemeCost(BuildingScheme &building_scheme);

#endif