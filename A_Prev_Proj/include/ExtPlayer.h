#pragma once

#include "Player.h"
#include "SatelliteView.h"
#include "ExtBattleInfo.h"
#include <vector>
#include <utility>
#include <memory>
#include <map>

using std::map ,std::vector, std::unique_ptr, std::make_unique;

struct TankStatus{
    pair<int, int> position;
    int ammo;
    bool alive;
};

class ExtPlayer : public Player{
public:
    // Rule of 5
    ExtPlayer(int player_index, int x, int y, size_t max_steps, size_t num_shells); // Constructor
    ExtPlayer& operator=(const ExtPlayer&) = delete; // Copy assignment operator deleted
    ExtPlayer(ExtPlayer&&) noexcept = delete; // Move constructor deleted
    ExtPlayer& operator=(ExtPlayer&&) noexcept = delete; // Move assignment operator deleted
    ~ExtPlayer() override = default; // Destructor
        
protected:
    // Function to initiate gameboard, shells locations and tank locations based on satellite view
    virtual void initGameboardAndShells(vector<vector<char>>& gameboard, vector<pair<int,int>>& shells_location, SatelliteView &satellite_view,
        pair<int, int>& tank_location) = 0;

    // Function to update tank with battle info
    void updateTankWithBattleInfo(TankAlgorithm& tank, SatelliteView& satellite_view) override;
    
    int playerIndex_;
    int x_; // width
    int y_; // height
    size_t maxSteps_;
    size_t numShells_;
    map<int, TankStatus> tankStatus_;

};