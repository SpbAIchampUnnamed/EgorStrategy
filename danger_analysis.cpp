#include "danger_analysis.hpp"
#include "precalc.hpp"

using namespace std;
using namespace model;

vector<vector<pair<int, int>>> getDangerousZones(const vector<int> &my_planets, const vector<EnemyGroup> &groups) {
    vector<vector<pair<int, int>>> ans(my_planets.size());
    auto logist_planets_graph = precalc::logist_planets_graph;
    auto regular_planets_graph = precalc::regular_planets_graph;
    for (auto p : my_planets) {
        logist_planets_graph.edges[p].clear();
        regular_planets_graph.edges[p].clear();
    }
    for (size_t i = 0; i < groups.size(); ++i) {
        auto &g = groups[i];
        auto &graph = g.specialty == Specialty::LOGISTICS ? logist_planets_graph : regular_planets_graph;
        auto dist = [&g](int a, int b) {
            if (g.specialty == Specialty::LOGISTICS)
                return precalc::logist_real_distance(a, b);
            else
                return precalc::regular_real_distance(a, b);
        };
        auto [d, p] = dijkstra(graph, g.planet, [&](int v, int to) {
            if (dist(g.prevPlanet, to) != dist(g.prevPlanet, g.planet) + dist(g.planet, to))
                return pair(-1, 0);
            return pair(to, dist(v, to));
        });
        for (size_t j = 0; j < my_planets.size(); ++j) {
            int p = my_planets[i];
            if (d[p] == dist(g.planet, p)) {
                ans[j].emplace_back(i, d[p] + g.distToPlanet);
            }
        }
    }
    return ans;
}
