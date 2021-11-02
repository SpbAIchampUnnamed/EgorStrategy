#ifndef __SIMPLEX_HPP__
#define __SIMPLEX_HPP__

#include "assert.hpp"
#include <vector>
#include <utility>
#include <algorithm>
#include <ranges>
#include <numeric>

namespace simplex_method {

enum class SolutionType {minimize, maximize};

template<class T>
class Solver {
private:
    size_t vars;
    std::vector<std::vector<T>> c;
    std::vector<T> b;
    std::vector<int> help;

    void addToBasis(size_t s, size_t r, T eps) {
        size_t m = c.size();
        size_t n = c[0].size();
        T mul = T(1) / c[r][s];
        for (auto &x : c[r])
            x *= mul;
        c[r][s] = 1;
        b[r] *= mul;
        for (size_t i = 0; i < m; ++i) {
            if (i == r)
                continue;
            T mul = c[i][s];
            #pragma GCC ivdep
            for (size_t j = 0; j < n; ++j) {
                c[i][j] -= c[r][j] * mul;
            }
            b[i] -= b[r] * mul;
            ASSERT(b[i] >= -eps);
        }
    }
public:
    Solver(size_t vars): vars(vars) {}
    void addLeConstraint(std::vector<T> coefs, T v) {
        ASSERT(coefs.size() == vars);
        coefs.insert(coefs.end(), c.size() + 1, T(0));
        coefs.back() = T(1);
        for (auto &v : c)
            v.emplace_back(0);
        c.emplace_back(std::move(coefs));
        b.emplace_back(v);
    }

    void addGeConstraint(std::vector<T> coefs, T v) {
        ASSERT(coefs.size() == vars);
        coefs.insert(coefs.end(), c.size() + 1, T(0));
        coefs.back() = T(-1);
        for (auto &v : c)
            v.emplace_back(0);
        help.emplace_back(c.size());
        c.emplace_back(std::move(coefs));
        b.emplace_back(v);
    }

    void phase(std::vector<T> coefs, T &ans, std::vector<int> &basis, T eps = 0) {
        size_t m = c.size();
        size_t n = c[0].size();
        while (1) {
            auto it = std::ranges::max_element(coefs);
            if (*it <= eps)
                break;
            size_t s = it - coefs.begin();
            auto candidates = std::views::iota((size_t) 0, b.size()) | std::views::filter([&](int i) {
                return c[i][s] > eps;
            });
            auto r_it = std::ranges::min_element(candidates, [&](auto i, auto j) {
                return b[i] / c[i][s] < b[j] / c[j][s];
            });
            ASSERT(r_it != candidates.end());
            auto r = *r_it;
            addToBasis(s, r, eps);
            basis[r] = s;
            auto mul = coefs[s];
            #pragma GCC ivdep
            for (size_t i = 0; i < n; ++i) {
                coefs[i] -= c[r][i] * mul;
            }
            ans += b[r] * mul;
        }
    }

    template<SolutionType type>
    std::pair<std::vector<T>, T> solve(std::vector<T> coefs, T eps = 0) {
        ASSERT(coefs.size() == vars);
        size_t m = c.size();
        size_t n = c[0].size();
        T ans = 0;
        coefs.insert(coefs.end(), m, T(0));
        std::vector<int> basis(m);
        std::iota(basis.begin(), basis.end(), vars);

        if (!help.empty()) {
            for (auto &v : c) {
                v.insert(v.end(), help.size() + 1, T(0));
            }
            coefs.insert(coefs.end(), help.size() + 1, T(0));
            for (size_t i = 0; i < help.size(); ++i) {
                basis[help[i]] = n + i;
                c[help[i]][n + i] = 1;
            }
            #pragma GCC ivdep
            for (size_t i = 0; i < vars; ++i)
                coefs[i] = -coefs[i];
            coefs.back() = 1;
            c.emplace_back(std::move(coefs));
            b.emplace_back(0);
            basis.emplace_back(n + help.size());
            std::vector<T> help_coefs(n + help.size() + 1, 0);
            T help_ans = 0;
            for (auto i : help) {
                for (size_t j = 0; j < n; ++j) {
                    help_coefs[j] += c[i][j];
                }
                help_ans -= b[i];
            }
            phase(std::move(help_coefs), help_ans, basis, eps);
            ASSERT(std::abs(help_ans) <= eps);
            size_t target = n + help.size();

            size_t bs = basis.size();
            for (size_t i = 0; i < basis.size(); ++i) {
                size_t x = basis[i];
                if (x >= n && x != target) {
                    ASSERT(std::abs(b[i]) <= eps);
                    size_t nv = x;
                    if (std::abs(c[i][target]) > eps) {
                        nv = target;
                    } else {
                        for (size_t j = 0; j < n; ++j) {
                            if (std::abs(c[i][j]) > eps) {
                                nv = j;
                                break;
                            }
                        }
                    }
                    if (nv == x) {
                        --bs;
                        --m;
                        std::swap(basis[i], basis[bs]);
                        std::swap(b[i], b[bs]);
                        c[i].swap(c[bs]);
                        --i;
                    } else {
                        addToBasis(nv, i, eps);
                        basis[i] = nv;
                    }
                }
            }
            c.erase(c.begin() + bs, c.end());
            b.erase(b.begin() + bs, b.end());
            basis.erase(basis.begin() + bs, basis.end());

            for (auto &v : c)
                v.resize(n);
            size_t r = std::ranges::find(basis, target) - basis.begin();
            ASSERT(r <= m);
            c[r].swap(c.back());
            std::swap(b[r], b.back());
            for (auto &v : c)
                v.resize(n);
            ans = std::move(b.back());
            coefs = std::move(c.back());
            c.pop_back();
            b.pop_back();

            if constexpr (type == SolutionType::maximize) {
                for (size_t i = 0; i < n; ++i)
                    coefs[i] = -coefs[i];
            }
        }

        phase(std::move(coefs), ans, basis, eps);
        
        std::vector<T> values(vars, 0);
        #pragma GCC ivdep
        for (size_t i = 0; i < m; ++i) {
            if ((size_t) basis[i] < vars) {
                values[basis[i]] = b[i];
                ASSERT(values[basis[i]] >= -eps);
                if (values[basis[i]] < 0)
                    values[basis[i]] = -values[basis[i]];
            }
        }
        return std::pair(values, ans);
    }
};

};

#endif