#ifndef __CORO_TASK_HPP__
#define __CORO_TASK_HPP__

#include "assert.hpp"
#include <coroutine>
#include <type_traits>
#include <algorithm>

namespace coro {

namespace detail {

template<class Task>
class PromiseBase {
    friend class Task::Awaiter;
    friend Task;
protected:
    std::vector<std::coroutine_handle<>> continuations;
    int refCnt = 1;
    std::coroutine_handle<> handle;

    template<class Derived>
    PromiseBase(Derived *self): handle(std::coroutine_handle<Derived>::from_promise(*self)) {}
public:

    Task get_return_object() {
        return Task(std::coroutine_handle<typename Task::promise_type>::from_promise(static_cast<typename Task::promise_type&>(*this)));
    }

    auto initial_suspend() noexcept {
        return std::suspend_always{};
    }

    auto final_suspend() noexcept {
        struct FinalAwaiter {
            bool ready;
            bool await_ready() noexcept {
                return ready; 
            }
            void await_suspend(std::coroutine_handle<>) noexcept {};
            void await_resume() noexcept {}
        };
        for (auto c : continuations) {
            c.resume();
        }
        continuations.clear();
        --refCnt;
        return FinalAwaiter{refCnt == 0};
    }

    void unhandled_exception() {
        throw;
    }

    ~PromiseBase() {
        for (auto c : continuations) {
            c.destroy();
        }
    }
};

template<class Task, class R>
class Promise: public PromiseBase<Task> {
    friend class Task::Awaiter;
protected:
    template<class Derived>
    Promise(Derived *self): PromiseBase<Task>(self) {}
    R result;
public:
    Promise(): PromiseBase<Task>(this) {}

    template<class T>
    void return_value(T &&res) {
        result = std::forward<T>(res);
    }
};

template<class Task>
class Promise<Task, void>: public PromiseBase<Task> {
protected:
    template<class Derived>
    Promise(Derived *self): PromiseBase<Task>(self) {}
public:
    Promise(): PromiseBase<Task>(this) {}

    void return_void() {}
};

};

template<class Result = void>
class [[nodiscard]] Task {
    template<class, class>
    friend class detail::PromiseBase;
protected:
    template<class>
    class Promise;
public:
    using promise_type = detail::Promise<Task, Result>;
    using ResultType = Result;
protected:
    promise_type *promise = nullptr;

    template<class OtherPromise>
    Task(std::coroutine_handle<OtherPromise> handle): promise(&handle.promise()) {
        ++promise->refCnt;
    }

    struct Awaiter {
        promise_type *promise;

        bool await_ready() {
            return promise->handle.done();
        }

        template<class Promise>
        void await_suspend(std::coroutine_handle<Promise> cont) {
            promise->continuations.emplace_back(cont);
        }

        std::conditional_t<std::is_void_v<Result>, void, std::conditional_t<std::is_void_v<Result>, int, Result>&> await_resume() {
            if constexpr (!std::is_void_v<Result>)
                return promise->result;
        }
    };
    
public:

    Awaiter operator co_await() {
        return Awaiter{promise};
    }

    Task() = default;
    Task(const Task &other): promise(other.promise) {
        if (promise)
            promise->refCnt++;
    }
    Task(Task &&other): promise(other.promise) {
        other.promise = nullptr;
    }
    Task &operator=(Task tmp) {
        std::swap(promise, tmp.promise);
        return *this;
    }

    operator bool() const noexcept {
        return (bool) promise;
    }

    Task &start() {
        promise->handle.resume();
        return *this;
    }

    bool isDone() {
        return promise->handle.done();
    }

    std::conditional_t<std::is_void_v<Result>, void, std::conditional_t<std::is_void_v<Result>, int, Result>&> getResult() {
        ASSERT(promise->handle.done());
        return promise->result;
    }

    ~Task() {
        if (promise && --promise->refCnt == 0 && promise->handle.done()) {
            
            promise->handle.destroy();
        }
    }
};

};

#endif