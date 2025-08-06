#include "ExtPlayer.h"

// Constructor
ExtPlayer::ExtPlayer(const int player_index, const int x, const int y, const size_t max_steps, const size_t num_shells)
    : Player(player_index, x, y, max_steps, num_shells),
      playerIndex_(player_index), x_(x), y_(y), maxSteps_(max_steps), numShells_(num_shells) {
}

// Function to update tank with battle info
void ExtPlayer::updateTankWithBattleInfo(TankAlgorithm& tank, SatelliteView& satellite_view) {
    vector<vector<char>> gameboard;
    vector<pair<int, int>> shells_location;
    pair<int, int> tank_location;

    // Initialize the gameboard with satellite_view and gather shell locations
    initGameboardAndShells(gameboard, shells_location, satellite_view, tank_location);

    // Create a new battle info object
    const unique_ptr<BattleInfo> battleInfo = make_unique<ExtBattleInfo>(gameboard, shells_location, numShells_, tank_location);

    // Update tank with battle info -
    // If it's the first time the tank receives battle info, initialize the tank's ammo and location
    // otherwise, update the tank's gameboard and shell locations
    // and update the battle info with the current tank's information
    tank.updateBattleInfo(*battleInfo);

    // Receive updates from tank
    const int tank_index = dynamic_cast<ExtBattleInfo*>(battleInfo.get())->getTankIndex();
    tankStatus_[tank_index].ammo = dynamic_cast<ExtBattleInfo&>(*battleInfo).getCurrAmmo();
}

