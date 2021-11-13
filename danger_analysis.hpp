#ifndef _DANGER_ANALYSIS
#define _DANGER_ANALYSIS

#include "coro/task.hpp"
#include "model/Game.hpp"
#include <vector>
#include <utility>

struct EnemyGroup {
    int count;
    int planet;
    int distToPlanet;
    model::Specialty specialty;
    int prevPlanet;
};

std::vector<std::vector<std::pair<int, int>>> getDangerousZones(const std::vector<int> &my_planets, const std::vector<EnemyGroup> &groups);

#endif
