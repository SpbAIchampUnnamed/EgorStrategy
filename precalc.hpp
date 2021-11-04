#ifndef __PRECALC_HPP__
#define __PRECALC_HPP__

#include <vector>
#include <ranges>
#include "const.hpp"
#include "graph.hpp"
#include "model/Game.hpp"

namespace precalc {

extern std::vector<std::vector<int>> near_planets;
extern std::vector <std::vector<int>> real_distance_logist;
extern Graph<> regular_planets_graph;
extern Graph<> logist_planets_graph;
extern std::vector<std::vector<int>> regular_prev;
extern std::vector<std::vector<int>> logist_prev;
extern std::vector<std::vector<int>> regular_d;
extern std::vector<std::vector<int>> logist_d;
extern std::ranges::subrange<decltype(regular_d)::iterator> d;
extern std::ranges::subrange<decltype(regular_prev)::iterator> prev;

template<class F>
void calcDistances(auto &planets_graph, auto &d, auto &prev, F &&dist) {
    d.resize(model::game.planets.size());
    prev.resize(model::game.planets.size());
    for (size_t i = 0; i < d.size(); ++i) {
        auto [dr, pr] = dijkstra(planets_graph, i, dist);
        d[i] = std::move(dr);
        prev[i] = std::move(pr);
    }
}

void prepare(model::Specialty = model::Specialty::COMBAT);

inline int real_distance(int x, int y) {
    return d[x][y] >> constants::planet_bits;
}

inline int logist_real_distance(int x, int y) {
    return logist_d[x][y] >> constants::planet_bits;
}

inline int regular_real_distance(int x, int y) {
    return regular_d[x][y] >> constants::planet_bits;
}

};

#endif