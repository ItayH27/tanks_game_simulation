#pragma once

#include "../common/Player.h"
#include "../common/SatelliteView.h"
#include "../UserCommon/UC_include/ExtBattleInfo.h"
#include <vector>
#include <utility>
#include <memory>
#include <map>

using std::map ,std::vector, std::unique_ptr, std::make_unique, std::pair;

namespace Algorithm_209277367_322542887 {
    struct TankStatus{
        pair<int, int> position;
        int ammo;
        bool alive;
    };

    class Player_209277367_322542887 : public Player{
    public:
        // Rule of 5
        Player_209277367_322542887(int player_index, int x, int y, size_t max_steps, size_t num_shells); // Constructor
        Player_209277367_322542887& operator=(const Player_209277367_322542887&) = delete; // Copy assignment operator deleted
        Player_209277367_322542887(Player_209277367_322542887&&) noexcept = delete; // Move constructor deleted
        Player_209277367_322542887& operator=(Player_209277367_322542887&&) noexcept = delete; // Move assignment operator deleted
        ~Player_209277367_322542887() override = default; // Destructor

    protected:
        // Function to initiate gameboard, shells locations and tank locations based on satellite view
        void initGameboardAndShells(vector<vector<char>>& gameboard, vector<pair<int,int>>& shells_location, SatelliteView &satellite_view,
            pair<int, int>& tank_location);

        // Function to update tank with battle info
        void updateTankWithBattleInfo(TankAlgorithm& tank, SatelliteView& satellite_view) override;

        int playerIndex_;
        int x_; // width
        int y_; // height
        size_t maxSteps_;
        size_t numShells_;
        map<int, TankStatus> tankStatus_;

    };
}