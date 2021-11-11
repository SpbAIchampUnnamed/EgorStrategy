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
    while (model::game.planetsCount < 0 || model::game.planets.size() < (size_t) model::game.planetsCount) {
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
            int p;
            if (spec == model::Specialty::LOGISTICS) {
                p = precalc::logist_prev[*it][from];
            } else {
                p = precalc::regular_prev[*it][from];
            }
            used[p] = 1;
            if (!co_await controller.move(from, p, 1).start()) {
                break;
            }
            from = p;
        }
    }
}
