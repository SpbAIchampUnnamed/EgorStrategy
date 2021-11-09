#ifndef __GRAPH_HPP__
#define __GRAPH_HPP__

#include "model/Game.hpp"
#include <vector>
#include <cmath>
#include <queue>
#include <limits>
#include <utility>
#include <type_traits>

template<class Edge = int>
struct Graph {
    std::vector<std::vector<Edge>> edges;

    Graph() = default;

    Graph(size_t n): edges(n) {}

    size_t n() const {
        return edges.size();
    }
};

template<bool return_edge = false, class Edge, class F>
auto dijkstra(const Graph<Edge> &g, int start, F &&edge_func) {
    using namespace std;
    using DistType = decltype(edge_func(0, declval<Edge&>()).second);
    vector<DistType> d(g.n(), numeric_limits<DistType>::max() / 2);
    vector<conditional_t<return_edge, pair<int, Edge>, int>> p(g.n());
    for (auto &x : p) {
        if constexpr (return_edge) {
            x.first = -1;
        } else {
            x = -1;
        }
    }
    d[start] = 0;
    priority_queue<pair<DistType, int>, vector<pair<DistType, int>>, greater<pair<DistType, int>>> q;
    q.emplace(0, start);
    while (q.size()) {
        auto [dv, v] = q.top();
        q.pop();
        if (dv > d[v])
            continue;
        for (auto e : g.edges[v]) {
            auto [x, dvx] = edge_func(v, e);
            if (x < 0)
                continue;
            auto dx = dv + dvx;
            if (dx < d[x]) {
                d[x] = dx;
                if constexpr (return_edge) {
                    p[x] = {v, e};
                } else {
                    p[x] = v;
                }
                q.emplace(dx, x);
            }
        }
    }
    return pair(d, p);
}

#endif