#ifndef __DISPATCHER_HPP__
#define __DISPATCHER_HPP__

#include "model/Action.hpp"
#include "model/Game.hpp"
#include <type_traits>
#include <coroutine>
#include <queue>
#include <functional>
#include <vector>

struct WaitingTask {
    int tick;
    int priority;
    std::coroutine_handle<> handle;
    static int current_serial;
    int serial = current_serial++;
    auto operator<=>(const WaitingTask &other) const {
        if (tick != other.tick)
            return tick <=> other.tick;
        if (other.priority != priority)
            return other.priority <=> priority;
        return serial <=> other.serial;
    }
};

using TaskQueue = std::priority_queue<WaitingTask, std::vector<WaitingTask>, std::greater<WaitingTask>>;

class Dispatcher {
private:
    TaskQueue &queue;
    model::Action &controled;
public:
    struct [[nodiscard]] Awaiter {
        Dispatcher &dispatcher;
        int tick;
        int priority;
        bool ok = true;
        bool await_ready() {
            return !ok;
        }
        void await_suspend(std::coroutine_handle<> handle) {
            dispatcher.queue.emplace(tick, priority, handle);
        }
        bool await_resume() {
            return ok;
        }
    };

    Awaiter wait(int ticks = 1, int priority = 0) {
        return Awaiter{*this, model::game.currentTick + ticks, priority, true};
    }

    Awaiter doAndWait(const model::Action &action, int ticks = 1, int priority = 0) {
        size_t my_flying_groups = 0;
        for (auto &g : model::game.flyingWorkerGroups) {
            my_flying_groups += (g.playerIndex == model::game.myIndex);
        }
        size_t added_moves = 0;
        std::vector<int> index(action.moves.size(), -1); 
        for (size_t i = 0; i < action.moves.size(); ++i) {
            auto &new_move = action.moves[i];
            for (size_t j = 0; j < controled.moves.size(); ++j) {
                auto &old_move = controled.moves[j];
                if (old_move.startPlanet == new_move.startPlanet
                    && old_move.targetPlanet == new_move.targetPlanet
                    && old_move.takeResource == new_move.takeResource)
                {
                    index[i] = j;
                    break;
                }
            }
            if (index[i] == -1)
                ++added_moves;
        }
        if (added_moves + controled.moves.size() + my_flying_groups > (size_t) model::game.maxFlyingWorkerGroups) {
            return Awaiter{*this, 0, 0, false};
        }
        for (size_t i = 0; i < action.moves.size(); ++i) {
            if (index[i] == -1) {
                controled.moves.emplace_back(action.moves[i]);
            } else {
                controled.moves[index[i]].workerNumber += action.moves[i].workerNumber;
            }
        }
        controled.buildings.insert(controled.buildings.end(), action.buildings.begin(), action.buildings.end());
        return wait(ticks, priority);
    }

    void reset() {
        while (queue.size()) {
            queue.top().handle.destroy();
            queue.pop();
        }
        controled.clear();
    }

    Dispatcher(TaskQueue& queue, model::Action &action): queue(queue), controled(action) {}
};

#endif