#pragma once
#include "DynArray.h"
#include <stdexcept>

class Graph {
    int n_;
    DynArray<DynArray<int>> adjacency_;
    DynArray<char> matrix_;
    DynArray<int> indegree_;

    void check(int vertex) const {
        if (vertex < 0 || vertex >= n_)
            throw std::out_of_range("Graph vertex out of range");
    }

public:
    explicit Graph(int n) : n_(n) {
        if (n < 0) throw std::invalid_argument("Graph size cannot be negative");
        for (int i = 0; i < n_; ++i) {
            adjacency_.push_back(DynArray<int>());
            indegree_.push_back(0);
        }
        for (int i = 0; i < n_ * n_; ++i) matrix_.push_back(0);
    }

    void addEdge(int u, int v, bool directed = false) {
        check(u);
        check(v);
        if (!hasEdge(u, v)) {
            adjacency_[u].push_back(v);
            matrix_[static_cast<std::size_t>(u) * n_ + v] = 1;
            ++indegree_[v];
        }
        if (!directed && !hasEdge(v, u)) {
            adjacency_[v].push_back(u);
            matrix_[static_cast<std::size_t>(v) * n_ + u] = 1;
            ++indegree_[u];
        }
    }

    bool hasEdge(int u, int v) const {
        check(u);
        check(v);
        return matrix_[static_cast<std::size_t>(u) * n_ + v] != 0;
    }

    const DynArray<int>& neighbors(int u) const { check(u); return adjacency_[u]; }
    int inDegree(int u) const { check(u); return indegree_[u]; }
    int size() const { return n_; }
};
