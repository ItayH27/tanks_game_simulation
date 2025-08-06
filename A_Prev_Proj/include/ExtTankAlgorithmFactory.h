#pragma once

#include "TankAlgorithmFactory.h"
#include "TankAlgorithm.h"

using std::make_unique;

class ExtTankAlgorithmFactory final : public TankAlgorithmFactory {
    public:
        // Create a new tank algorithm based on the player index and tank index
        unique_ptr<TankAlgorithm> create(int player_index, int tank_index) const override;
};