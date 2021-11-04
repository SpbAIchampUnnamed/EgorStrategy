#include "precalc.hpp"
#include "graph.hpp"
#include <utility>
#include <iostream>
#include <algorithm>

using namespace std;
using namespace model;

namespace precalc {

Graph<> planets_graph;
std::vector<std::vector<int>> d;
std::vector<std::vector<int>> prev;
std::vector<std::vector<int>> real_distance;
std::vector<std::vector<int>> near_planets;


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
    real_distance.resize(game.planets.size());
    for (int i = 0; i < game.planets.size(); i++) {
        real_distance[i].resize(game.planets.size());
        for (int j = 0; j < game.planets.size(); j++) {
            real_distance[i][j] = d[i][j] / game.planets.size();
        }
    }
    near_planets.resize(game.planets.size());
    for (auto i = 0; i < game.planets.size(); i++) {
        near_planets[i].resize(game.planets.size());
        for (int j = 0; j < game.planets.size(); j++) {
            near_planets[i][j] = j;
        }
        sort(near_planets[i].begin(), near_planets[i].end(), [i] (int a, int b) {
            return real_distance[a][i] < real_distance[b][i];
        });
    }
}




};
