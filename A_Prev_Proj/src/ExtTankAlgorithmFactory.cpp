#include "TankTypes.h"
#include "ExtTankAlgorithmFactory.h"

// Create a new TankAlgorithm based on the player_index and tank_index
unique_ptr<TankAlgorithm> ExtTankAlgorithmFactory::create(int player_index, int tank_index) const {
    // For an odd player_index, if tank_index is even, create a Tank_BFS and if it's odd, create a Tank_Sentry.
    if (player_index % 2 == 1) {
        if (tank_index % 2 == 0) {
            return make_unique<Tank_BFS>(player_index, tank_index);
        }

        if (tank_index % 2 == 1) {
            return make_unique<Tank_Sentry>(player_index, tank_index);
        }
        return nullptr;
    }

   // For an odd player_index, if tank_index is even, create a Tank_Sentry, and if it's odd, create a Tank_BFS.
    if (tank_index % 2 == 0) {
        return make_unique<Tank_Sentry>(player_index, tank_index);
    }
    if (tank_index % 2 == 1) {
        return make_unique<Tank_BFS>(player_index, tank_index);
    }

    return nullptr;
}
