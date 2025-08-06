#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <set>
#include <iostream>
#include "PlayerFactory.h"
#include "TankAlgorithmFactory.h"
#include "SatelliteView.h"
#include "Player.h"
#include "TankInfo.h"
#include "Shell.h"

#define GAME_OVER_NO_AMMO 40 // Number of turns to wait after no ammo condition is met
#define NUM_OF_DIRECTIONS 8 // Number of directions

using std::unique_ptr, std::string, std::vector, std::ifstream, std::ofstream, std::set, std::cout, std::endl;
using TankIterator = std::vector<std::unique_ptr<TankInfo>>::iterator;
using ShellIterator = std::vector<std::unique_ptr<Shell>>::iterator;


class GameManager {
public:
    GameManager(unique_ptr<PlayerFactory> player_factory, unique_ptr<TankAlgorithmFactory> tank_factory); // Constructor
    GameManager& operator=(const GameManager&) = delete; // Copy assignment
    GameManager(GameManager&&) noexcept = delete; // Move constructor
    GameManager& operator=(GameManager&&) noexcept = delete; // Move assignment
    ~GameManager() = default; // Destructor

    void readBoard(const string& file_path);
    bool failedInit() const;
    void run(); // Function to start the game
    pair<int, int> getGameboardSize() const;

    void setVisualMode(bool visual_mode); // Visualisation

private:
    unique_ptr<PlayerFactory> playerFactory_; // Factory for creating players
    unique_ptr<TankAlgorithmFactory> tankFactory_; // Factory for creating tank algorithms
    unique_ptr<SatelliteView> satellite_view_; // Satellite view for the game
    unique_ptr<Player> player1_; // Player 1
    unique_ptr<Player> player2_; // Player 2
    vector<vector<char>> gameboard_; // Game board represented as a 2D vector
    vector<unique_ptr<TankInfo>> tanks_;
    set<size_t> destroyedTanksIndices_; // Set of tank indices to delete
    vector<unique_ptr<Shell>> shells_; // Shells fired by tanks
    ofstream gameLog_; // Log file for game events
    ofstream errorLog_; // Log file for errors
    int numShells_{}; // Number of shells for each tank
    int maxSteps_{}; // Maximum steps for the game
    bool failedInit_; // Flag to indicate if initialization failed
    bool gameOver_; // Flag to indicate if the game is over
    int width_{}; // Width of the game board
    int height_{}; // Height of the game board
    int turn_; // Current turn number
    bool noAmmoFlag_; // Flag to indicate if all tanks are out of ammo
    size_t gameOverStatus_{}; // Status indicating the reason for game over
    size_t noAmmoTimer_; // Timer for no ammo condition
    size_t numTanks1_ = 0;
    size_t numTanks2_ = 0;
    vector<vector<char>> lastRoundGameboard_;
    vector<pair<ActionRequest, bool>> tankActions_;

    bool visualMode_; // Visualisation

    // Base functions
    bool extractLineValue(const string& line, int& value, const string& key, size_t line_number);
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

    void writeBoardToJson() const; // Visualisation
};
