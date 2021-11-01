#ifndef __MINCOST_HPP__
#define __MINCOST_HPP__

#include "graph.hpp"
#include <utility>
#include <vector>
#include <limits>

namespace flows {

constexpr int inf = std::numeric_limits<int>::max() / 2;

struct FlowGraph: public Graph<int> {
    struct Edge {
        int to, cost, flow, cap;
    };
    std::vector<Edge> edgesData;
    FlowGraph() {}
    FlowGraph(size_t n): Graph<int>(n) {}
    int addVertex() {
        edges.emplace_back();
        return edges.size() - 1;
    }
    void addEdge(int from, int to, int cost, int cap = inf) {
        edges[from].emplace_back(edgesData.size());
        edgesData.emplace_back(Edge{to, cost, 0, cap});
        edges[to].emplace_back(edgesData.size());
        edgesData.emplace_back(Edge{from, -cost, 0, 0});
    }
};

struct FlowResult {
    int cost, flow;
};

FlowResult mincost(FlowGraph &graph, int s, int t);

};

#endif