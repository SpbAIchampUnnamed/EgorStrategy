#ifndef __CONST_HPP__
#define __CONST_HPP__

namespace constants {
    constexpr int stone_reserve_size = 1000;
    constexpr int product_planing_horizon = 100;

    constexpr long long max_valid_flow_denom = 1000;
    constexpr int flow_stabilization_prior = 10'000;

    constexpr int move_reserve_prior = 100'000;
    constexpr int move_unreserve_prior = -100'000;
    constexpr int stone_reserve_prior = 80'000;
    constexpr int static_robots_recalc_prior = 75'000;
    constexpr int building_task_initial_prior = 50'000;
    constexpr int repair_prior = 35'000;
    constexpr int cycle_main_prior = 21'000;
    constexpr int cycle_capture_prior = 20'000;
    constexpr int free_robots_prior = -10'000;
};

#endif