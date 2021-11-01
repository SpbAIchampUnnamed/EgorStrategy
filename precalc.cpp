#include "precalc.hpp"
#include "graph.hpp"
#include <utility>

using namespace std;
using namespace model;

namespace precalc {

Graph<> planets_graph;
std::vector<std::vector<int>> d;
std::vector<std::vector<int>> prev;

void prepare() {
    planets_graph = Graph(game.planets.size()); 
    for (auto &x : game.planets) {
        for (auto &y : game.planets) {
            if (planets_dist(x, y) <= game.maxTravelDistance) {
                planets_graph.edges[x.id].emplace_back(y.id);
                planets_graph.edges[y.id].emplace_back(x.id);
            }
        }
    }
    calcDistances([](int a, int b) {
        if (a == b)
            return pair(b, 0);
        return pair(b, planets_dist(game.planets[a], game.planets[b]) * (int) game.planets.size() + 1);
    });
}

};
