#include "precalc.hpp"
#include "graph.hpp"
#include <utility>
#include <iostream>
#include <algorithm>

using namespace std;
using namespace model;

namespace precalc {

    model::Specialty my_specialty;
    Graph<> regular_planets_graph(max_planet_index + 1);;
    Graph<> logist_planets_graph(max_planet_index + 1);
    std::array<std::array<int, model::max_planet_index + 1>, model::max_planet_index + 1> regular_prev;
    std::array<std::array<int, model::max_planet_index + 1>, model::max_planet_index + 1> logist_prev;
    std::array<std::array<int, model::max_planet_index + 1>, model::max_planet_index + 1> regular_d;
    std::array<std::array<int, model::max_planet_index + 1>, model::max_planet_index + 1> logist_d;
    std::ranges::subrange<decltype(regular_d)::iterator> d;
    std::ranges::subrange<decltype(regular_prev)::iterator> prev;
    std::vector<std::vector<int>> near_planets;

    void prepare() {
        if (game.currentTick == 0) {
            for (size_t i = 0; i < 3; ++i) {
                if (game.planets[i].workerGroups.size() && game.planets[i].workerGroups[0].playerIndex == game.myIndex) {
                    my_specialty = (Specialty) i;
                }
            }
        }
        for (auto i : views::keys(game.planets)) {
            regular_planets_graph.edges[i].clear();
        }
        for (auto &x: views::values(game.planets)) {
            for (auto &y: views::values(game.planets)) {
                if (planets_dist(x, y) <= game.maxTravelDistance) {
                    regular_planets_graph.edges[x.id].emplace_back(y.id);
                    regular_planets_graph.edges[y.id].emplace_back(x.id);
                }
                if (planets_dist(x, y) <= game.maxTravelDistance + game.logisticsUpgrade) {
                    logist_planets_graph.edges[x.id].emplace_back(y.id);
                    logist_planets_graph.edges[y.id].emplace_back(x.id);
                }
            }
        }
        calcDistances(regular_planets_graph, regular_d, regular_prev, [](int a, int b) {
            if (a == b)
                return pair(b, 0);
            return pair(b, planets_dist(game.planets[a], game.planets[b]) << constants::planet_bits | 1);
        });
        calcDistances(logist_planets_graph, logist_d, logist_prev, [](int a, int b) {
            if (a == b)
                return pair(b, 0);
            return pair(b, planets_dist(game.planets[a], game.planets[b]) << constants::planet_bits | 1);
        });

        if (my_specialty == Specialty::LOGISTICS) {
            d = decltype(d)(logist_d.begin(), logist_d.end());
            prev = decltype(prev)(logist_prev.begin(), logist_prev.end());
        } else {
            d = decltype(d)(regular_d.begin(), regular_d.end());
            prev = decltype(prev)(regular_prev.begin(), regular_prev.end());
        }
        near_planets.resize(max_planet_index + 1);
        for (size_t i = 0; i < game.planets.size(); i++) {
            near_planets[i].resize(game.planets.size());
            ranges::copy(views::keys(game.planets), near_planets[i].begin());
            sort(near_planets[i].begin(), near_planets[i].end(), [i](int a, int b) {
                return pair(regular_d[a][i], a) < pair(regular_d[b][i], b);
            });
        }
    }


};
