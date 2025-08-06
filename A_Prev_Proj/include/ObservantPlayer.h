#pragma once

#include "ExtPlayer.h"
#include "SatelliteView.h"

using std::pair;
using std::vector;

class ObservantPlayer final : public ExtPlayer {
public:
    using ExtPlayer::ExtPlayer;

private:
    // Function to initiate gameboard, shells locations based on satellite view
    void initGameboardAndShells(vector<vector<char>>& gameboard, vector<pair<int,int>>& shells_location, SatelliteView &satellite_view,
        pair<int, int>& tank_location) override;

};