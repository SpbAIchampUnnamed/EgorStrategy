#ifndef _MY_STRATEGY_HPP_
#define _MY_STRATEGY_HPP_

#include "model/Game.hpp"
#include "model/Action.hpp"
#include "runner.hpp"

class MyStrategy {
public:

    MyStrategy();
    void play(Runner &runner);
};

#endif