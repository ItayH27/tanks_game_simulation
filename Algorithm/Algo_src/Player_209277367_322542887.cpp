#include "Player_209277367_322542887.h"
#include "../common/PlayerRegistration.h"
#include "TankAlgorithm_209277367_322542887.h"

using namespace Algorithm_209277367_322542887;
REGISTER_PLAYER(Player_209277367_322542887);

// Constructor
Player_209277367_322542887::Player_209277367_322542887(const int player_index, const int x, const int y, const size_t max_steps, const size_t num_shells)
    : playerIndex_(player_index), x_(x), y_(y), maxSteps_(max_steps), numShells_(num_shells) {
}

// Function to update tank with battle info
void Player_209277367_322542887::updateTankWithBattleInfo(TankAlgorithm& tank, SatelliteView& satellite_view) {
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


// Function to init gameboard and shell locations based on the satellite_view
void Player_209277367_322542887::initGameboardAndShells(vector<vector<char>>& gameboard, vector<pair<int,int>>& shells_location, SatelliteView &satellite_view,
        pair<int, int>& tank_location) {

    gameboard.resize(y_, vector<char>(x_, ' ')); // Resize the gameboard to match satellite_view

    // Iterate over the satellite_view and update the gameboard
    for (int i = 0; i < y_; ++i){
        for (int j = 0; j < x_; ++j){
            const char obj = satellite_view.getObjectAt(j, i);
            gameboard[i][j] = obj; // Update the gameboard with the object from satellite_view

            if (obj == '*') { // Found shell
                shells_location.emplace_back(j, i);
            }

            else if (obj == '%') { // Found self
                tank_location = {j, i};

            }
        }
    }
}

