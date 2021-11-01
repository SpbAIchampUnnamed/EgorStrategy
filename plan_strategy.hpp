#ifndef __PLAN_STRATEGY_HPP__
#define __PLAN_STRATEGY_HPP__

#include "utils/stringable_enum.hpp"
#include <cinttypes>

namespace plans {

ENUM_CLASS(PlanStrategy, int8_t, FULL, FULL_TASK, PARTIAL_TASK)

};

#endif