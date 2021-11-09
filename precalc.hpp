#ifndef __PRECALC_HPP__
#define __PRECALC_HPP__

#include <vector>
#include <ranges>
#include <algorithm>
#include "const.hpp"
#include "graph.hpp"
#include "model/Game.hpp"

namespace precalc {

extern model::Specialty my_specialty;
extern std::vector<std::vector<int>> near_planets;
extern Graph<> regular_planets_graph;
extern Graph<> logist_planets_graph;
extern std::array<std::array<int, model::max_planet_index + 1>, model::max_planet_index + 1> regular_prev;
extern std::array<std::array<int, model::max_planet_index + 1>, model::max_planet_index + 1> logist_prev;
extern std::array<std::array<int, model::max_planet_index + 1>, model::max_planet_index + 1> regular_d;
extern std::array<std::array<int, model::max_planet_index + 1>, model::max_planet_index + 1> logist_d;
extern std::ranges::subrange<decltype(regular_d)::iterator> d;
extern std::ranges::subrange<decltype(regular_prev)::iterator> prev;

template<class F>
void calcDistances(auto &planets_graph, auto &d, auto &prev, F &&dist) {
    for (auto i : std::views::keys(model::game.planets)) {
        auto [dr, pr] = dijkstra(planets_graph, i, dist);
        std::ranges::copy(dr, d[i].begin());
        std::ranges::copy(pr, prev[i].begin());
    }
}

void prepare();

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