#include "exploration.hpp"
#include "precalc.hpp"
#include "model/Game.hpp"
#include "const.hpp"
#include "statistics.hpp"
#include <ranges>

namespace {

bool used[model::max_planet_index + 1] = {};

};

coro::Task<void> explore(ActionController &controller, int from, model::Specialty spec) {
    while (1) {
        auto it = std::ranges::find_if(precalc::near_planets[from], [](int p) {
            if (used[p]) {
                return false;
            } else {
                used[p] = getMyTeamRobotsOnPlanet(p);
                return !used[p];
            }
        });
        if (it == precalc::near_planets[from].end()) {
            break;
        } else {
            int p = *it;
            used[p] = 1;
            if (spec == precalc::my_specialty)
                co_await controller.move(from, p, 1).start();
            else if (spec == model::Specialty::LOGISTICS)
                co_await controller.dispatcher.wait(precalc::logist_real_distance(from, p), constants::exploration_prior);
            else
                co_await controller.dispatcher.wait(precalc::regular_real_distance(from, p), constants::exploration_prior);
            from = p;
        }
    }
}
