#ifndef __REDUCE_HPP__
#define __REDUCE_HPP__

#include <functional>
#include <numeric>

namespace std::ranges {

template<class Range, class T, class Op = plus<T>>
auto reduce(Range &&range, T &&init, Op &&op = plus<T>()) {
    return reduce(range.begin(), range.end(), forward<T>(init), forward<Op>(op));
}

};

#endif