#include "precalc.hpp"
#include "graph.hpp"
#include <utility>
#include <iostream>
#include <algorithm>

using namespace std;
using namespace model;

namespace precalc {

    Graph<> regular_planets_graph;
    Graph<> logist_planets_graph;
    std::vector <std::vector<int>> regular_prev;
    std::vector <std::vector<int>> logist_prev;
    std::vector <std::vector<int>> regular_d;
    std::vector <std::vector<int>> logist_d;
    std::ranges::subrange< decltype(regular_d)::iterator> d;
    std::ranges::subrange< decltype(regular_prev)::iterator> prev;
    std::vector <std::vector<int>> real_distance;
    std::vector <std::vector<int>> real_distance_logist;
    std::vector <std::vector<int>> near_planets;

    void prepare(model::Specialty s) {
        regular_planets_graph = Graph(game.planets.size());
        logist_planets_graph = Graph(game.planets.size());
        for (auto &x: game.planets) {
            for (auto &y: game.planets) {
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
        real_distance.resize(game.planets.size());
        real_distance_logist.resize(game.planets.size());
        for (int i = 0; i < game.planets.size(); i++) {
            real_distance[i].resize(game.planets.size());
            for (int j = 0; j < game.planets.size(); j++) {
                real_distance[i][j] = regular_d[i][j] / game.planets.size();
                real_distance_logist[i][j] = logist_d[i][j] / game.planets.size();
            }
        }
        near_planets.resize(game.planets.size());
        for (auto i = 0; i < game.planets.size(); i++) {
            near_planets[i].resize(game.planets.size());
            for (int j = 0; j < game.planets.size(); j++) {
                near_planets[i][j] = j;
            }
            sort(near_planets[i].begin(), near_planets[i].end(), [i](int a, int b) {
                return real_distance[a][i] < real_distance[b][i];
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
