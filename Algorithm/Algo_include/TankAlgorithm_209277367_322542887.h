#pragma once

#include "../common/TankAlgorithm.h"
#include "../common/ActionRequest.h"
#include "../common/BattleInfo.h"
#include "../../UserCommon/UC_include/Direction.h"
#include <utility>
#include <queue>
#include <stack>
#include <vector>
#include <iostream>
#include <limits>
#include <optional>
#include <cmath>

using std::pair, std::queue, std::vector, std::optional, std::abs, std::stack, std::sqrt;
using namespace UserCommon_209277367_322542887;

static constexpr int INF = std::numeric_limits<int>::max(); // infinity

namespace Algorithm_209277367_322542887 {
    class TankAlgorithm_209277367_322542887 : public TankAlgorithm {
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
        TankAlgorithm_209277367_322542887(int player_index, int tank_index);
        TankAlgorithm_209277367_322542887(const TankAlgorithm_209277367_322542887&) = delete; // Copy constructor
        TankAlgorithm_209277367_322542887& operator=(const TankAlgorithm_209277367_322542887&) = delete; // Copy assignment
        TankAlgorithm_209277367_322542887(TankAlgorithm_209277367_322542887&&) noexcept = delete; // Move constructor
        TankAlgorithm_209277367_322542887& operator=(TankAlgorithm_209277367_322542887&&) noexcept = delete; // Move assignment
        ~TankAlgorithm_209277367_322542887() override = default; // Virtual destructor


        //Should both tanks have the same logic?
        void updateBattleInfo(BattleInfo& info) override; // Placeholder for the updateBattleInfo method

        ActionRequest getAction() override; // Get the next action to take

        void algo(const vector<vector<char>>& gameboard);

    private:
        stack<pair<int,int>> get_path_stack(const vector<vector<char>>& gameboard) const; // Get the path stack from the gameboard

    };
}