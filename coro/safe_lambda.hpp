#ifndef __CORO_SAFE_LAMBDA_HPP__
#define __CORO_SAFE_LAMBDA_HPP__

#include "coro/task.hpp"
#include <type_traits>
#include <utility>
#include <memory>

namespace coro {

/*
Usage: make_coro([](auto self, ...) -> Task<Ret> {...})
instead of
[](...) -> Task<Ret> {...}()
*/

template<class F, class... Args>
Task<typename std::invoke_result_t<std::remove_reference_t<F>, std::unique_ptr<std::remove_reference_t<F>>, Args&&...>::ResultType>
make_coro(F &&f, Args&&... args) {
    static_assert(!std::is_invocable_v<std::remove_reference_t<F>, const std::unique_ptr<std::remove_reference_t<F>>&, Args&&...>);
    auto ptr = std::make_unique<std::remove_reference_t<F>>(forward<F>(f));
    return (*ptr)(std::move(ptr), std::forward<Args>(args)...);
}

};

#endif
