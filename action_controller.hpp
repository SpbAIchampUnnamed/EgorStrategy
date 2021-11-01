#ifndef __ACTION_CONTROLLER_HPP__
#define __ACTION_CONTROLLER_HPP__

#include "model/Game.hpp"
#include "model/Resource.hpp"
#include "coro/task.hpp"
#include "dispatcher.hpp"
#include <vector>
#include <array>
#include <optional>

struct ActionController {
    Dispatcher &dispatcher;
    std::vector<int> reservedRobots;
    std::vector<int> onWayTo;
    std::vector<std::array<int, model::EnumValues<model::Resource>::list.size()>> reservedResources;
    ActionController(Dispatcher &dispatcher):
        dispatcher(dispatcher),
        reservedRobots(model::game.planets.size(), 0),
        onWayTo(model::game.planets.size()),
        reservedResources(model::game.planets.size()) {}
    coro::Task<bool> move(int from, int to, int count, std::optional<model::Resource> = std::nullopt);
    coro::Task<bool> waitWithPrior(int planet, int count, int ticks, int peior);
};

#endif