#ifndef __STATISTICS_HPP__
#define __STATISTICS_HPP__

#include <unordered_map>
#include "model/Game.hpp"
#include "utils/fraction.hpp"

std::unordered_map<model::BuildingType, Fraction<int>> getBuildingCounts(model::BuildingType type, bool normalized = 1);
std::unordered_map<model::BuildingType, Fraction<int>> getRobotCounts(model::BuildingType type, bool normalized = 1);

int getPlayersRobotsCount(int player);
int getMyRobotsCount();
int getMyTeamRobotsCount();
int getPlayersRobotsOnPlanet(int player, int planet);
int getMyRobotsOnPlanet(int planet);
int getMyTeamRobotsOnPlanet(int planet);

#endif