#include "ExtBattleInfo.h"

#include <utility>

namespace UserCommon_209277367_322542887 {

// Constructor
ExtBattleInfo::ExtBattleInfo(const vector<vector<char>>& gameboard, const vector<pair<int,int>>& shells_location,
    const int num_shells, pair<int, int> initial_loc)
    : currGameboard_(gameboard), shellLocations_(shells_location), initialLoc_(std::move(initial_loc)), initialAmmo_(num_shells),
        tankIndex_(0), currAmmo_(0) {} // Updated to move semantics for initial loc

// Getters
vector<vector<char>> ExtBattleInfo::getGameboard() const{ return currGameboard_; }

vector<pair<int,int>> ExtBattleInfo::getShellsLocation() const{ return shellLocations_; }

int ExtBattleInfo::getCurrAmmo() const { return currAmmo_; }

int ExtBattleInfo::getTankIndex() const { return tankIndex_; }

pair<int, int> ExtBattleInfo::getInitialLoc() const { return initialLoc_; }

int ExtBattleInfo::getInitialAmmo() const { return initialAmmo_; }


// Setters
void ExtBattleInfo::setCurrAmmo(const int ammo) { currAmmo_ = ammo; }

void ExtBattleInfo::setTankIndex(const int tank_index) { this->tankIndex_ = tank_index; }

} // namespace UserCommon_209277367_322542887