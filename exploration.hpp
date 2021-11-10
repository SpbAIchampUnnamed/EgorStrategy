#ifndef __EXPLORATION_HPP__
#define __EXPLORATION_HPP__

#include "model/Specialty.hpp"
#include "coro/task.hpp"
#include "action_controller.hpp"

coro::Task<void> explore(ActionController &controller, int from, model::Specialty spec);

#endif