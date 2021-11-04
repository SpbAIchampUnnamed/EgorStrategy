#include "precalc.hpp"
#include "graph.hpp"
#include <utility>

using namespace std;
using namespace model;

namespace precalc {

Graph<> regular_planets_graph;
Graph<> logist_planets_graph;
std::vector<std::vector<int>> regular_prev;
std::vector<std::vector<int>> logist_prev;
std::vector<std::vector<int>> regular_d;
std::vector<std::vector<int>> logist_d;
std::ranges::subrange<decltype(regular_d)::iterator> d;
std::ranges::subrange<decltype(regular_prev)::iterator> prev;

void prepare(model::Specialty s) {
    regular_planets_graph = Graph(game.planets.size()); 
    logist_planets_graph = Graph(game.planets.size());
    for (auto &x : game.planets) {
        for (auto &y : game.planets) {
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
        return pair(b, planets_dist(game.planets[a], game.planets[b]) * (int) game.planets.size() + 1);
    });
    calcDistances(logist_planets_graph, logist_d, logist_prev, [](int a, int b) {
        if (a == b)
            return pair(b, 0);
        return pair(b, planets_dist(game.planets[a], game.planets[b]) * (int) game.planets.size() + 1);
    });
    if (s == Specialty::LOGISTICS) {
        d = decltype(d)(logist_d.begin(), logist_d.end());
        prev = decltype(prev)(logist_prev.begin(), logist_prev.end());
    } else {
        d = decltype(d)(regular_d.begin(), regular_d.end());
        prev = decltype(prev)(regular_prev.begin(), regular_prev.end());
    }
}

};
