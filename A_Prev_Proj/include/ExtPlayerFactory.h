#pragma once

# include "PlayerFactory.h"

class ExtPlayerFactory final : public PlayerFactory {
    public:
        // Create a new player based on the player index
        unique_ptr<Player> create(int player_index, size_t x, size_t y,
            size_t max_steps, size_t num_shells ) const override;
};