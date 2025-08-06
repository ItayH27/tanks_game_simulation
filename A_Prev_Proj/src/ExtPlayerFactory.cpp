# include "ExtPlayerFactory.h"
#include "ConcentratedPlayer.h"
#include "ObservantPlayer.h"

using std::make_unique;

// Factory method to create a player based on the player index
unique_ptr<Player> ExtPlayerFactory::create(int player_index, size_t x, size_t y,
        size_t max_steps, size_t num_shells ) const {
    if (player_index == 1) { // Player is player 1
        return make_unique<ConcentratedPlayer>(player_index, x, y, max_steps, num_shells);
    }
    if (player_index == 2) { // Player is player 2
        return make_unique<ObservantPlayer>(player_index, x, y, max_steps, num_shells);
    }

    return nullptr;
}
