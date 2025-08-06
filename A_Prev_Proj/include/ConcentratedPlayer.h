#pragma once

#include "ExtPlayer.h"
#include "SatelliteView.h"
#include <algorithm>
#include <limits>
#include <cmath>

using std::pair, std::vector, std::max, std::min;

class ConcentratedPlayer final : public ExtPlayer {
public:
    using ExtPlayer::ExtPlayer;

private:
    // Function to find the closest enemy to the tank
    static pair<int, int> findClosestEnemy(const vector<pair<int, int>>& enemy_tanks, const pair<int, int>& tank_location);

    // Function to initiate gameboard, shells locations based on satellite view
    void initGameboardAndShells(vector<vector<char>>& gameboard, vector<pair<int, int>>& shells_location, SatelliteView &satellite_view,
     pair<int, int>&  tank_location) override;
};