#ifndef __CORO_SAFE_LAMBDA_HPP__
#define __CORO_SAFE_LAMBDA_HPP__

#include "coro/task.hpp"
#include <type_traits>
#include <utility>
#include <memory>

namespace coro {

/*
Usage: make_coro([]<class Self>(const Self&, ...) -> LambdaTask<Self, Ret> {...})
instead of
[](...) -> Task<Ret> {...}()
*/

template<class Lambda, class Ret>
class LambdaTask: Task<Ret> {
    template<class F, class... Args>
    friend Task<typename std::invoke_result_t<std::decay_t<F>, const F&, Args&&...>::ResultType> make_coro(F &&f, Args&&... args);
private:
    LambdaTask(std::coroutine_handle<typename Task<Ret>::promise_type> h): Task<Ret>(h) {}
public:
    using Task<Ret>::promise;
    using ResultType = Ret;

    LambdaTask(const Task<Ret> &other): Task<Ret>(other) {}

    LambdaTask() = default;

    struct promise_type: public Task<Ret>::promise_type {
        template<class F, class... Args>
        friend Task<typename std::invoke_result_t<std::decay_t<F>, const F&, Args&&...>::ResultType> make_coro(F &&f, Args&&... args);
    public:
        using Task<Ret>::promise_type::handle;

        promise_type(): Task<Ret>::promise_type(this) {}
    private:
        std::unique_ptr<Lambda> self;
    };
};

template<class F, class... Args>
Task<typename std::invoke_result_t<std::decay_t<F>, const F&, Args&&...>::ResultType> make_coro(F &&f, Args&&... args) {
    auto ptr = std::make_unique<std::decay_t<F>>(forward<F>(f));
    auto lambda_task = (*ptr)(*ptr, std::forward<Args>(args)...);
    static_cast<decltype(f(f, std::forward<Args>(args)...))::promise_type *>(lambda_task.promise)->self = std::move(ptr);
    return lambda_task;
}

};

#endif
