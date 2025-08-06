#pragma once

#include "TankAlgorithm.h"
#include "ActionRequest.h"
#include "BattleInfo.h"
#include "Direction.h"
#include <utility>
#include <queue>
#include <vector>
#include <iostream>
#include <limits>
#include <optional>
#include <cmath>

using std::pair, std::queue, std::vector, std::optional, std::abs;

static constexpr int INF = std::numeric_limits<int>::max(); // infinity

class ExtTankAlgorithm : public TankAlgorithm {
    protected:
        pair<int, int> location_;
        Direction direction_;
        queue<ActionRequest> actionsQueue_;
        int playerIndex_;
        int tankIndex_;
        int ammo_;
        bool alive_;
        int turnsToShoot_;
        int turnsToEvade_;
        bool backwardsFlag_;
        bool justMovedBackwardsFlag_;
        int backwardsTimer_;
        bool justGotBattleinfo_; //DEBUGMODE
        bool firstBattleinfo_; //DEBUGMODE
        Direction shotDir_;
        int shotDirCooldown_{};

        vector<vector<char>> gameboard;
        vector<pair<int,int>> shellLocations_;

        static Direction diffToDir(int diff_x, int diff_y, int rows = INF, int cols = INF);
        void evadeShell(Direction danger_dir, const vector<vector<char>>& gameboard);
        optional<Direction> actionsToNextCell(const pair<int, int> &curr, const pair<int, int> &next, Direction dir, int rows, int cols, bool& backwards_flag_, bool is_evade = false);
        bool isEnemyInLine(const vector<vector<char>>& gameboard) const;
        optional<Direction> isShotAt(const vector<pair<int,int>>& shells_locations) const;

        void shoot();
        void decreaseTurnsToShoot(ActionRequest action);
        void nonEmptyPop();
        void decreaseEvadeTurns();
        void updateLocation(ActionRequest action);
        void decreaseShotDirCooldown();
        bool friendlyInLine(const Direction& dir) const;

    public:
        ExtTankAlgorithm(int player_index, int tank_index);
        ExtTankAlgorithm(const ExtTankAlgorithm&) = delete; // Copy constructor
        ExtTankAlgorithm& operator=(const ExtTankAlgorithm&) = delete; // Copy assignment
        ExtTankAlgorithm(ExtTankAlgorithm&&) noexcept = delete; // Move constructor
        ExtTankAlgorithm& operator=(ExtTankAlgorithm&&) noexcept = delete; // Move assignment
        ~ExtTankAlgorithm() override = default; // Virtual destructor
       

        //Should both tanks have the same logic?
        void updateBattleInfo(BattleInfo& info) override; // Placeholder for the updateBattleInfo method

        virtual void algo(const vector<vector<char>>& gameboard) = 0;
};