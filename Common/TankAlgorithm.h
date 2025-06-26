#pragma once

#ifndef TANKALGORITHM_H
#define TANKALGORITHM_H

#include <memory>
#include <functional>
#include "ActionRequest.h"
#include "BattleInfo.h"

using std::function, std::unique_ptr;

class TankAlgorithm {
public:
    virtual ~TankAlgorithm() {}
    virtual ActionRequest getAction() = 0;
    virtual void updateBattleInfo(BattleInfo& info) = 0;
};

using TankAlgorithmFactory = function<unique_ptr<TankAlgorithm>(int player_index, int tank_index)>;

#endif //TANKALGORITHM_H
