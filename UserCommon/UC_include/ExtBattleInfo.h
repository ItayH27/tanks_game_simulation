#pragma once

#include "../common/BattleInfo.h"
#include <utility>
#include <vector>

using std::vector, std::pair;

class ExtBattleInfo final : public BattleInfo {
    vector<vector<char>> currGameboard_;
    vector<pair<int,int>> shellLocations_;
    pair<int,int> initialLoc_;
    int initialAmmo_;
    int tankIndex_;
    int currAmmo_;

    public:
        // Rule of 5
        ExtBattleInfo(const vector<vector<char>>& gameboard, const vector<pair<int,int>>& shells_location,
            int num_shells, pair<int, int> initial_loc); // Constructor
        ExtBattleInfo(const ExtBattleInfo& other) = default;
        ExtBattleInfo& operator=(const ExtBattleInfo& other) = delete;
        ExtBattleInfo(ExtBattleInfo&& other) noexcept = delete;
        ExtBattleInfo& operator=(ExtBattleInfo&& other) noexcept = delete;
        ~ExtBattleInfo() override = default;

        // Getters
        vector<vector<char>> getGameboard() const;
        vector<pair<int,int>> getShellsLocation() const;
        int getTankIndex() const;
        int getCurrAmmo() const;
        int getInitialAmmo() const;
        pair<int ,int> getInitialLoc() const;

        // Setters
        void setTankIndex(int tank_index);
        void setCurrAmmo(int ammo);
};
