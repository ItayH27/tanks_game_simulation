#pragma once


# include <cstddef>
# include "TankAlgorithm.h"
# include "SatelliteView.h"

class Player {
public:
    Player( int player_index, size_t x, size_t y, size_t max_steps, size_t num_shells ) {
        (void)player_index; // Unused parameter
        (void)x; // Unused parameter
        (void)y; // Unused parameter
        (void)max_steps; // Unused parameter
        (void)num_shells; // Unused parameter
        
    }
    virtual ~Player() {}
    virtual void updateTankWithBattleInfo(TankAlgorithm& tank, SatelliteView& satellite_view) = 0;
};
    