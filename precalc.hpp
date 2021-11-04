#ifndef __PRECALC_HPP__
#define __PRECALC_HPP__

#include <vector>
#include "graph.hpp"
#include "model/Game.hpp"

namespace precalc {

extern Graph<> planets_graph;
extern std::vector<std::vector<int>> d;
extern std::vector<std::vector<int>> prev;
extern std::vector<std::vector<int>> real_distance;
extern std::vector<std::vector<int>> near_planets;

template<class F>
void calcDistances(F &&dist) {
    d.resize(model::game.planets.size());
    prev.resize(model::game.planets.size());
    for (size_t i = 0; i < d.size(); ++i) {
        auto [dr, pr] = dijkstra(planets_graph, i, dist);
        d[i] = std::move(dr);
        prev[i] = std::move(pr);
    }
}


void prepare();

};

#endif