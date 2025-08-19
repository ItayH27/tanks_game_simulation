#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <set>
#include <iostream>
#include <map>
#include <mutex>
#include <filesystem>

#include "AbstractGameManager.h"
#include "../../common/GameResult.h"
#include "../../common/TankAlgorithm.h" // FIX
#include "../common/SatelliteView.h"
#include "../common/Player.h"
#include "TankInfo.h"
#include "../UserCommon/UC_include/Shell.h"
#include "../common/ActionRequest.h"
#include "../../common/SatelliteView.h"
#include "../../common/ActionRequest.h"
#include "../UserCommon/UC_include/ExtSatelliteView.h"


#define GAME_OVER_NO_AMMO 40 // Number of turns to wait after no ammo condition is met
#define NUM_OF_DIRECTIONS 8 // Number of directions
#define PLAYER_1_WIN 1
#define PLAYER_2_WIN 2
#define TIE 0
#define NO_SHELLS_GAME_OVER 2
#define TIMER_GAME_OVER 1
#define ALL_TANKS_DEAD 0

using std::unique_ptr, std::string, std::vector, std::ifstream, std::ofstream, std::set, std::cout, std::endl, std::move;
using TankIterator = std::vector<std::unique_ptr<TankInfo>>::iterator;
using ShellIterator = std::vector<std::unique_ptr<Shell>>::iterator;
using namespace UserCommon_209277367_322542887;
namespace fs = std::filesystem;

namespace GameManager_209277367_322542887 {
    class GM_209277367_322542887 : public AbstractGameManager {

    public:
        explicit GM_209277367_322542887(bool verbose); // Constructor
        GM_209277367_322542887& operator=(const GM_209277367_322542887&) = delete; // Copy assignment
        GM_209277367_322542887(GM_209277367_322542887&&) noexcept = delete; // Move constructor
        GM_209277367_322542887& operator=(GM_209277367_322542887&&) noexcept = delete; // Move assignment
        ~GM_209277367_322542887() = default; // Destructor

        GameResult run(size_t map_width, size_t map_height, const SatelliteView& map, string map_name,
            size_t max_steps, size_t num_shells, Player& player1, string name1, Player& player2, string name2,
            TankAlgorithmFactory player1_tank_algo_factory, TankAlgorithmFactory player2_tank_algo_factory) override;
        pair<int, int> getGameboardSize() const;

        void setVisualMode(bool visual_mode); // Visualisation

    private:
        function<std::unique_ptr<TankAlgorithm>(int, int)> player1TankFactory_; // Factory for creating tank algorithms
        function<std::unique_ptr<TankAlgorithm>(int, int)> player2TankFactory_;
        Player* player1_; // Player 1
        Player* player2_; // Player 2
        vector<vector<char>> gameboard_; // Game board represented as a 2D vector
        vector<unique_ptr<TankInfo>> tanks_;
        set<size_t> destroyedTanksIndices_; // Set of tank indices to delete
        vector<unique_ptr<Shell>> shells_; // Shells fired by tanks
        ofstream gameLog_; // Log file for game events
        GameResult gameResult_;
        int numShells_{}; // Number of shells for each tank
        int maxSteps_{}; // Maximum steps for the game
        bool gameOver_{}; // Flag to indicate if the game is over
        int width_{}; // Width of the game board
        int height_{}; // Height of the game board
        int turn_{}; // Current turn number
        bool noAmmoFlag_ = false; // Flag to indicate if all tanks are out of ammo
        size_t gameOverStatus_{}; // Status indicating the reason for game over
        size_t noAmmoTimer_ = false; // Timer for no ammo condition
        size_t numTanks1_ = 0;
        size_t numTanks2_ = 0;
        bool verbose_ = false;
        vector<vector<char>> lastRoundGameboard_;
        vector<pair<ActionRequest, bool>> tankActions_;

        // bool visualMode_; // Visualisation

        // Base functions
        void getTankActions();
        bool performAction(ActionRequest action, TankInfo& tank);
        void performTankActions();
        void checkTanksStatus();
        void moveShells(vector<unique_ptr<Shell>>& shells);
        void checkShellsCollide();
        int getTankIndexAt(int x, int y) const;
        bool isValidAction(const TankInfo& tank, ActionRequest action) const;
        static bool isValidShoot(const TankInfo& tank);
        bool isValidMove(const TankInfo& tank, ActionRequest action) const;
        void shoot(TankInfo& tank);
        void moveTank(TankInfo& tank, ActionRequest action);
        static void rotate(TankInfo& tank, ActionRequest action);
        ShellIterator getShellAt(int x, int y);
        ShellIterator deleteShell(ShellIterator it);

        // Support functions
        pair<int, int> nextLocation(int x, int y, Direction dir, bool backwards = false) const;
        void printBoard() const;
        static string getEnumName(Direction dir);
        static string getEnumName(ActionRequest action) ;
        void updateGameLog();
        void updateGameResult(int winner, int reason, vector<size_t> remaining_tanks,
            vector<vector<char>>& game_state, size_t rounds);
        bool initiateGame(const SatelliteView& gameBoard);
        void handleTankCollisionAt( TankInfo& tank, int old_x, int old_y, int new_x, int new_y, Direction dir, char next_cell);
        void clearPreviousShellPosition(Shell& shell);
        bool handleShellSpawnOnTank(Shell& shell, ShellIterator& it);
        bool handleShellCollision(Shell& shell, int x, int y, Direction dir, ShellIterator& it);
        void handleShellMoveToNextCell(Shell& shell, int x, int y, char next_cell, ShellIterator& it);
        void openVerboseLog(const std::string& mapName,
                                            const std::string& player1Name,
                                            const std::string& player2Name,
                                            size_t maxSteps,
                                            size_t numShells);
        void closeVerboseLog();
    };
}