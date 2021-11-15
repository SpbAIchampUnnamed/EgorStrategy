#ifndef __MONITORING_HPP__
#define __MONITORING_HPP__

#include "coro/task.hpp"
#include "action_controller.hpp"

extern bool enable_monitoring;

coro::Task<void> monitor_planet(ActionController &controller, int from, int planet);

#endif