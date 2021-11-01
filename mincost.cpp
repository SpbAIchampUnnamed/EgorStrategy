#include "mincost.hpp"
#include "graph.hpp"

using namespace std;

namespace flows {

FlowResult mincost(FlowGraph &g, int s, int t) {
    vector<long long> p(g.n(), 0);
    FlowResult result{0, 0};
    while (1) {
        auto [d, prev] = dijkstra<true>(g, s, [&](int from, int e) -> pair<int, long long> {
            if (g.edgesData[e].flow == g.edgesData[e].cap)
                return pair(-1, -1);
            else
                return pair(g.edgesData[e].to, g.edgesData[e].cost + p[g.edgesData[e].to] - p[from]);
        });
        if (prev[t].first < 0)
            break;
        int i = t,  maxflow = inf;
        while (i != s) {
            auto [v, e] = prev[i];
            maxflow = min(maxflow, g.edgesData[e].cap - g.edgesData[e].flow);
            i = v;
        }
        result.flow += maxflow;
        i = t;
        while (i != s) {
            auto [v, e] = prev[i];
            g.edgesData[e].flow += maxflow;
            g.edgesData[e ^ 1].flow -= maxflow;
            result.cost += g.edgesData[e].cost * maxflow;
            i = v;
        }
        for (size_t i = 0; i < p.size(); ++i) {
            p[i] += d[i];
        }
    }
    return result;
}

};