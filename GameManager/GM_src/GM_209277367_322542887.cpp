#include "../GM_include/GM_209277367_322542887.h"

#include <string>
#include <iostream>
#include <map>
#include "../UserCommon/UC_include/ExtSatelliteView.h"

// Visualisation includes
#include "json.hpp"
#include <fstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <mutex>

#include "../Simulator/sim_include/AlgorithmRegistrar.h"

using std::move, std::endl, std::getline, std::make_unique, std::make_pair, std::filesystem::path;

using namespace GameManager_209277367_322542887;
REGISTER_GAME_MANAGER(GM_209277367_322542887);

// // Visualisation
// void GM_209277367_322542887::writeBoardToJson() const {
//     std::vector<std::vector<std::string>> serializable;
//
//     for (const auto& row : gameboard_) {
//         std::vector<std::string> new_row;
//         for (char cell : row) {
//             new_row.emplace_back(1, cell);
//         }
//         serializable.push_back(new_row);
//     }
//
//     nlohmann::json j;
//     j["board"] = serializable;
//     j["turn"] = turn_;
//     j["gameOver"] = gameOver_;
//     j["maxSteps"] = maxSteps_;
//     j["player1Tanks"] = numTanks1_;
//     j["player2Tanks"] = numTanks2_;
//
//     if (gameOver_) {
//         std::ostringstream winner_msg;
//
//         if (gameOverStatus_ == 1)
//             winner_msg << "Player 2 won with " << numTanks2_ << " tanks still alive";
//         else if (gameOverStatus_ == 2)
//             winner_msg << "Player 1 won with " << numTanks1_ << " tanks still alive";
//         else if (gameOverStatus_ == 3)
//             winner_msg << "Tie, both players have zero tanks";
//         else if (turn_ >= maxSteps_)
//             winner_msg << "Tie, reached max steps = " << maxSteps_
//                        << ", player 1 has " << numTanks1_
//                        << " tanks, player 2 has " << numTanks2_ << " tanks";
//
//         j["winner"] = winner_msg.str();
//     }
//
//     std::ofstream out("visualizer/game_state.json");
//     out << j.dump(2);
// }

// // Visualisation
// void GM_209277367_322542887::setVisualMode(const bool visual_mode) { this->visualMode_ = visual_mode; }

// Constructor for GameManager class
GM_209277367_322542887::GM_209277367_322542887(unique_ptr<PlayerFactory> player_factory, unique_ptr<TankAlgorithmFactory> tank_factory) : playerFactory_(
        std::move(player_factory)), tankFactory_(std::move(tank_factory)), player1_(nullptr),
    player2_(nullptr), gameResult_{}, numShells_(0), maxSteps_(0), failedInit_(false), gameOver_(false), width_(0), height_(0),
    turn_(0), noAmmoFlag_(false), gameOverStatus_(0), noAmmoTimer_(GAME_OVER_NO_AMMO) {}
    // visualMode_(false)


// Extract relevant value from a line of text
bool GM_209277367_322542887::extractLineValue(const std::string& line, int& value, const std::string& key, const size_t line_number) {
    string no_space_line;
    for (const char ch : line) { // Remove spaces from the line
        if (ch != ' ') no_space_line += ch;
    }

    // Check if the line has the correct format
    const string format = key + "=%d"; // Format for the line
    if (sscanf(no_space_line.c_str(), format.c_str(), &value) != 1) {
        errorLog_ << "Error: Invalid " << key << " format on line " << line_number << ".\n";
        failedInit_ = true;
        return false;
    }

    return true; // Successfully extraction
}

// Function to read the game board from a file
void GM_209277367_322542887::readBoard(const string& file_path) {
    // input_error.txt initialisation
    bool has_errors = false; // Flag to indicate if there are any errors in the file
    ofstream input_errors("input_errors.txt"); // Open file to store errors

    // Open game log
    path actual_path(file_path);
    string file_name = actual_path.stem().string(); // Get the file name without extension
    gameLog_.open("output_" + file_name + ".txt");
    if (!gameLog_.is_open()) { // Failed to create game log
        std::cerr << "Failed to open game log file." << std::endl;
        remove("input_errors.txt");
        return;
    }

    // Open file
    ifstream file(file_path);
    if (!file) { // Failed to open file
        std::cerr << "Error: Failed to open file: " << file_path << endl;
        remove("input_errors.txt");
        failedInit_ = true;
        return;
    }

    string line;
    size_t line_number = 0;

    // Skip line 1 (map name)
    getline(file, line);
    ++line_number;

    // Line 2: MaxSteps
    if (!getline(file, line) || !extractLineValue(line, maxSteps_, "MaxSteps", line_number)) { // Failed to read line
        std::cerr << "Error: Missing MaxSteps.\n";
        remove("input_errors.txt");
        failedInit_ = true;
        return;
    }
    ++line_number;

    // Line 3: NumShells
    if (!getline(file, line) || !extractLineValue(line, numShells_, "NumShells", line_number)) { // Failed to read line
        std::cerr << "Error: Missing NumShells.\n";
        remove("input_errors.txt");
        failedInit_ = true;
        return;
    }
    ++line_number;

    // Line 4: Rows
    if (!getline(file, line) || !extractLineValue(line, height_, "Rows", line_number)) { // Failed to read line
        std::cerr << "Error: Missing Rows.\n";
        remove("input_errors.txt");
        failedInit_ = true;
        return;
    }
    ++line_number;

    // Line 5: Cols
    if (!getline(file, line) || !extractLineValue(line, width_, "Cols", line_number)) { // Failed to read line
        std::cerr << "Error: Missing Cols.\n";
        remove("input_errors.txt");
        failedInit_ = true;
        return;
    }
    ++line_number;

    gameboard_.resize(height_, vector<char>(width_, ' ')); // Resize the gameboard to the declared dimensions

    int tank_1_count = 0, tank_2_count = 0; // Count the number of tanks in each player
    int i = 0; // Current row
    int extra_rows = 0;
    int extra_cols = 0;

    // Read the rest of the file
    while (getline(file, line)) {
        if (i >= height_) { // Check if we have reached the end of the file
            ++extra_rows;
            continue;
        }

        // Check if line has more characters than expected width
        if (static_cast<int>(line.size()) > width_) {
            extra_cols += static_cast<int>(line.size()) - width_;
            input_errors << "Error recovered from: Extra " << (static_cast<int>(line.size()) - width_) << " columns at row "
                    << i << " ignored.\n";
            has_errors = true;
        }

        // Truncate the line to fit the declared width
        line = line.substr(0, width_);

        for (int j = 0; j < width_; ++j) { // Iterate through each cell in the line
            char cell = (j < static_cast<int>(line.size())) ? line[j] : ' ';  // Fill missing columns with spaces

            if (cell == '1') { // Cell contains a tank of player 1
                auto tank = tankFactory_->create(1, tank_1_count); // Create a new tank for player 1
                auto tank_info = make_unique<TankInfo>(tank_1_count, make_pair(j, i),
                    numShells_, 1, std::move(tank)); // Create a new TankInfo for the tank
                tanks_.push_back(std::move(tank_info)); // Add tank 1 to vector
                ++tank_1_count;

            } else if (cell == '2') { // Cell contains a tank of player 2
                auto tank = tankFactory_->create(2, tank_2_count); // Create a new tank for player 2
                auto tank_info = make_unique<TankInfo>(tank_2_count, make_pair(j, i),
                    numShells_, 2, std::move(tank)); // Create a new TankInfo for the tank
                tanks_.push_back(std::move(tank_info)); // Add tank 2 to vector
                ++tank_2_count;

            } else if (cell != '#' && cell != '@' && cell != ' ') { // Unknown character
                input_errors << "Error recovered from: Unknown character '" << cell << "' at row " << i << ", column " << j << ". Treated as space.\n";
                cell = ' ';
                has_errors = true;
            }

            gameboard_[i][j] = cell; // Update the gameboard with the new cell
        }

        ++i;
    }

    // Check for extra rows and columns
    if (extra_rows > 0) {
        input_errors << "Error recovered from: Extra " << extra_rows << " rows beyond declared height ignored.\n";
        has_errors = true;
    }
    // Check for extra columns
    if (extra_cols > 0) {
        input_errors << "Error recovered from: Extra " << extra_cols << " columns beyond declared width ignored.\n";
        has_errors = true;
    }

    // Check for missing tanks
    if (tank_1_count == 0 || tank_2_count == 0) { // Check for immediate game termination conditions
        if (tank_1_count == 0 && tank_2_count == 0) { // Check if both players have no tanks
            gameLog_ << "Tie, both players have zero tanks\n";

        } else if (tank_1_count == 0) { // Check if player 1 has no tanks
            gameLog_ << "Player 2 won with " << tank_2_count << " tanks still alive\n";

        } else if (tank_2_count == 0) { // Check if player 2 has no tanks
            gameLog_ << "Player 1 won with " << tank_1_count << " tanks still alive\n";
        }

        // Update the game state and flush the logs
        gameOver_ = true;
        gameLog_.flush();
        gameLog_.close();
    }

    // Delete input_errors.txt if no errors were found
    if (!has_errors) {
        remove("input_errors.txt");
    }
}

// Function to return if the game init failed
bool GM_209277367_322542887::failedInit() const {
    return failedInit_;
}

// Function to get actions for both tanks
void GM_209277367_322542887::getTankActions() {
    tankActions_.clear();

    // Get the actions for both tanks
    for (const auto & tank : tanks_) {
        if (tank->getIsAlive() == 0) {
            ActionRequest action = tank->getTank()->getAction();
            tankActions_.emplace_back(action, true);
        }
        else { tankActions_.emplace_back(ActionRequest::DoNothing, false); }
    }
}

// Function to check if an action is valid
bool GM_209277367_322542887::isValidAction(const TankInfo& tank, const ActionRequest action) const {
    switch(action) { // Check if the action is valid based on the tank's requested actions
        case ActionRequest::MoveForward:
        case ActionRequest::MoveBackward:
            return isValidMove(tank, action); // Check if the move is valid
        case ActionRequest::Shoot:
            return isValidShoot(tank); // Check if able to shoot
        default:
            return true;
    }
}

// Function to check if a move is valid
bool GM_209277367_322542887::isValidMove(const TankInfo& tank, const ActionRequest action) const {
    // Get the current location of the tank
    auto [fst, snd] = tank.getLocation();
    const int x = fst;
    const int y = snd;
    const Direction dir = tank.getDirection(); // Get the direction of the tank

    // Get the next cell based on the action
    char next_cell;
    if (action == ActionRequest::MoveBackward) { // If moving backwards, Update the technical direction
        auto [nx, ny] = nextLocation(x, y, dir, true);
        next_cell = gameboard_[ny][nx];
    } else { // If moving forward, Update the technical direction
        auto [nx, ny] = nextLocation(x, y, dir);
        next_cell = gameboard_[ny][nx];
    }

    return next_cell != '#' && next_cell != '$'; // Return true if the next cell is not a wall
}

// Function to check if shooting is valid
bool GM_209277367_322542887::isValidShoot(const TankInfo& tank) {
    // Check if the tank has ammo and zeroed cooldown
    return tank.getAmmo() > 0 && tank.getTurnsToShoot() == 0;
}

// Function to perform shoot action
void GM_209277367_322542887::shoot(TankInfo& tank) {
    if (!isValidShoot(tank)) { // Check if the shoot action is valid
        tank.decreaseTurnsToShoot();
        // cout << "Tank " << tank.getPlayerId() << "." << tank.getID() << " Tried to shoot illegally" << endl;
        return;
    }

    tank.resetTurnsToShoot(); // Zero the cooldown
    tank.decreaseAmmo();

    // Calculate the new position of the shell based on the tank's direction
    auto [fst, snd] = tank.getLocation();
    Direction dir = tank.getDirection();

    // Switch to check the next cell
    switch(auto [new_x, new_y] = nextLocation(fst, snd, dir); gameboard_[new_y][new_x]){
        case '#': {// If the next cell is a wall
            gameboard_[new_y][new_x] = '$'; // Weaken the wall
            // cout << "Tank " << tank.getPlayerId() << "." << tank.getID() << " Shot and weakened wall at (" << new_x << ", " << new_y << ")" << endl;
            break;}

        case '$': {// If the next cell is a weak wall
            gameboard_[new_y][new_x] = ' '; // Destroy the wall
            // cout << "Tank " << tank.getPlayerId() << "." << tank.getID() << " Shot and destroyed wall at (" << new_x << ", " << new_y << ")" << endl;
            break;}

        case '1': {  // If the next cell is occupied by tank 1
            gameboard_[new_y][new_x] = 'c'; // Update the game board with the new position of the tank
            shells_.emplace_back(make_unique<Shell>(new_x, new_y, dir)); // Add the shell to the list of shells
            break;}

        case '2': {// If the next cell is occupied by tank 2
            gameboard_[new_y][new_x] = 'd'; // Update the game board with the new position of the destroyed tank
            shells_.emplace_back(make_unique<Shell>(new_x, new_y, dir)); // Add the shell to the list of shells
            break;}

        case '*': { // If the next cell is a shell
            gameboard_[new_y][new_x] = ' '; // Remove both shells from the game board
            if (const auto shell_it = getShellAt(new_x, new_y); shell_it != shells_.end()) { // Find the shell at the new position
                deleteShell(shell_it); // Delete the shell
            }
            // cout << "Tank " << tank.getPlayerId() << "." << tank.getID() << " Shot a shell at (" << new_x << ", " << new_y << ")" << endl;
            break; }

        case '@': {// If the next cell is a mine
            auto shell_loc = pair(new_x, new_y); // Create a new shell location
            shells_.emplace_back(make_unique<Shell>(shell_loc, dir)); // Add the shell to the list of shells
            gameboard_[new_y][new_x] = '*'; // Mark the shell's position on the game board
            shells_.back()->setAboveMine(true); // Set the shell to be above the mine
            break;}

        default: {// If the next cell is empty
            gameboard_[new_y][new_x] = '*'; // Mark the shell's position on the game board
            shells_.emplace_back(make_unique<Shell>(new_x, new_y, dir)); // Add the shell to the list of shells
            break;}
    }
}

// Function to move tank
void GM_209277367_322542887::moveTank(TankInfo& tank, const ActionRequest action) {
    // Get the current location of the tank
    auto [fst, snd] = tank.getLocation();
    const int x = fst;
    const int y = snd;
    Direction dir = tank.getDirection(); // Get the direction of the tank

    gameboard_[y][x] = ' '; // Clear the previous position of the tank
    if (action == ActionRequest::MoveBackward) { // If moving backward, Update technical direction
        dir = static_cast<Direction>((static_cast<int>(dir) + 4) % NUM_OF_DIRECTIONS);
    }

    // Calculate the new position based on the direction
    auto [new_x, new_y] = nextLocation(x, y, dir);
    char next_cell = gameboard_[new_y][new_x];

    // Check the next cell for obstacles
    if (next_cell == ' ') { // If the next cell is empty
        // Update the tank's location and gameboard
        gameboard_[new_y][new_x] = static_cast<char>('0' + tank.getPlayerId());
        tank.setLocation(new_x, new_y);

    } else if (next_cell == '@') {// If the next cell is a mine
        // Update the tank's location and gameboard
        const int tank_index = getTankIndexAt(x, y);
        destroyedTanksIndices_.insert(tank_index); // Mark the tank for deletion
        tanks_[tank_index]->increaseTurnsDead();
        gameboard_[new_y][new_x] = ' ';

        // Log the action
        // cout << "Tank " << tank.getPlayerId() << "." << tank.getID() << " Hit a mine at (" << new_x << ", " << new_y << ")" << endl;
    }
    else if (next_cell == '*') {
        const ShellIterator shell_it = getShellAt(new_x, new_y); // Get the other shell at the new position
        int other_shell_dir = static_cast<int>((*shell_it)->getDirection()); // Get the direction of the other shell

        // Destroy tank if opposite of shells movement
        if (static_cast<int>(dir) == ((other_shell_dir + 4) % NUM_OF_DIRECTIONS)) {
            const int tank_index = getTankIndexAt(x, y);
            destroyedTanksIndices_.insert(tank_index); // Mark the current tank for deletion
            tanks_[tank_index]->increaseTurnsDead();
            deleteShell(shell_it); // Delete the shell
            gameboard_[new_y][new_x] = ' ';
        }

        else {
            gameboard_[new_y][new_x] = tank.getPlayerId() == 1 ? 'a' : 'b';
            tank.setLocation(new_x, new_y);
        }
    }

    else { // If the next cell is occupied by another tank
        int tank_index = getTankIndexAt(x, y);
        destroyedTanksIndices_.insert(tank_index); // Mark the current tank for deletion
        tanks_[tank_index]->increaseTurnsDead();
        tank_index = getTankIndexAt(new_x, new_y);
        destroyedTanksIndices_.insert(tank_index); // Mark the other tank for deletion
        tanks_[tank_index]->increaseTurnsDead();
        gameboard_[new_y][new_x] = ' ';

        // Log the action
        // cout << "Tank " << tank.getPlayerId() << "." << tank.getID() << " Collided with another tank at (" << new_x << ", " << new_y << ")" << endl;
    }
}

// Function to rotate tank
void GM_209277367_322542887::rotate(TankInfo& tank, const ActionRequest action) {
    // Get the current direction of the tank
    Direction dir = tank.getDirection();
    Direction new_dir = dir;

    // Rotate the tank based on the action
    switch (action) {
        case ActionRequest::RotateLeft45:
            new_dir = static_cast<Direction>((static_cast<int>(dir) - 1 + NUM_OF_DIRECTIONS) % NUM_OF_DIRECTIONS);
            break;
        case ActionRequest::RotateRight45:
            new_dir = static_cast<Direction>((static_cast<int>(dir) + 1) % NUM_OF_DIRECTIONS);
            break;
        case ActionRequest::RotateLeft90:
            new_dir = static_cast<Direction>((static_cast<int>(dir) - 2 + NUM_OF_DIRECTIONS) % NUM_OF_DIRECTIONS);
            break;
        case ActionRequest::RotateRight90:
            new_dir = static_cast<Direction>((static_cast<int>(dir) + 2) % NUM_OF_DIRECTIONS);
            break;
        default:
            break; // No rotation
    }

    tank.setDirection(new_dir); // Update tanks direction
}

// Function to perform actions for both tanks
bool GM_209277367_322542887::performAction(const ActionRequest action, TankInfo& tank) {
    // Deal with moving backwards
    if (tank.justMovedBackwards() && action == ActionRequest::MoveBackward){ // If the tank just moved backwards
        if (isValidAction(tank, ActionRequest::MoveBackward)) { // Check if the action is valid
            moveTank(tank, ActionRequest::MoveBackward);
            return true;
        }

        return false;
    }

    // Check if tank was moving backwards but now performing other action
    if (tank.justMovedBackwards() &&
        action != ActionRequest::MoveBackward) { tank.switchJustMovedBackwardsFlag(); }

    if (action == ActionRequest::MoveBackward && !tank.isMovingBackwards()) { // If tank now starting to move backwards
        if (tank.justMovedBackwards()){ tank.zeroTurnsToBackwards(); } // Can immediately move backwards
        tank.switchBackwardsFlag();
    }

    if (tank.isMovingBackwards()){ // If tank is moving backwards
        tank.decreaseTurnsToShoot(); // Decrease turns to shoot anyway

        if (action == ActionRequest::MoveForward) { // Tank wants to cancel backwards move
            tank.switchBackwardsFlag(); // No longer wants to move backwards
            tank.restartTurnsToBackwards();
            return false;
        }

        if (tank.getTurnsToBackwards() == 0) { // If tank is now eligible to move backwards
            if (isValidAction(tank, ActionRequest::MoveBackward)) { // Check if the action is valid
                moveTank(tank, ActionRequest::MoveBackward);
                tank.switchJustMovedBackwardsFlag();
            }

            tank.restartTurnsToBackwards();
            tank.switchBackwardsFlag();
            return false; // Tanks moves, but registered action is ignored
        }

        bool succ;
        if (tank.getTurnsToBackwards() == 2 ) { succ = true; }// Just requested backwards, which is valid
        else { succ = false; } // Still waiting for backwards move, current action is ignored

        tank.decreaseTurnsToBackwards(); // Decrease turns to backwards

        return succ;
    }

    if (!isValidAction(tank, action)) { // Check if the action is valid
        tank.decreaseTurnsToShoot(); // Decrease turns to shoot if action is invalid
        // cout << "Tank " << tank.getPlayerId() << "." << tank.getID() << " Invalid action: " << getEnumName(action) << endl;
        return false;
    }

    // Perform the action based on the action type
    switch (action) {
        case ActionRequest::MoveForward:
            moveTank(tank, action);
            tank.decreaseTurnsToShoot();
            break;
        case ActionRequest::Shoot:
            shoot(tank); // Perform the shoot action
            break;
        case ActionRequest::DoNothing:
            tank.decreaseTurnsToShoot();
            break;
        case ActionRequest::MoveBackward:
            break;

        case ActionRequest::GetBattleInfo: { // Get battle info
            auto* player = (tank.getPlayerId() == 1 ? player1_.get() : player2_.get()); // Get the player based on tank ID
            TankAlgorithm& tank_algo = *tank.getTank(); // Get the tank algorithm
            const char curr_loc = lastRoundGameboard_[tank.getLocation().second][tank.getLocation().first]; // Get the current tank location
            lastRoundGameboard_[tank.getLocation().second][tank.getLocation().first] = '%'; // Update the gameboard with the tank's position
            const auto satellite_view = make_unique<ExtSatelliteView>(width_, height_, lastRoundGameboard_); // Create a new satellite view
            player->updateTankWithBattleInfo(tank_algo, *satellite_view);
            lastRoundGameboard_[tank.getLocation().second][tank.getLocation().first] = curr_loc; // Restore the tank's position on the gameboard
            tank.decreaseTurnsToShoot();
            break; }

        default: // Rotate tank
            rotate(tank, action);
            tank.decreaseTurnsToShoot();
            break;
    }

    return true;
}

// Function to perform all tank actions
void GM_209277367_322542887::performTankActions() {
    // Iterate through all tanks
    for (size_t i = 0; i < tanks_.size(); ++i) {

        // std::cout << "Tank " << tanks_[i]->getPlayerId() << "." << tanks_[i]->getID() << " Performing action: " <<
        //    getEnumName(tankActions_[i].first) << endl;

        if (tanks_[i]->getIsAlive() == 0) {
            bool succ = performAction(tankActions_[i].first, *tanks_[i]);
            if (!succ) { tankActions_[i].second = false; }
        }
    }
}

// Function to check if tanks are out of ammo and check for tanks alive
void GM_209277367_322542887::checkTanksStatus() {
    const size_t tank_count = tanks_.size() - destroyedTanksIndices_.size();
    size_t no_ammo_count = 0;
    size_t player_1_count = 0;
    size_t player_2_count = 0;

    if (tank_count == 0) { // Check not tanks are left
        gameOver_ = true;
        gameOverStatus_ = 3; // No tanks left
        return;
    }

    for (const auto & tank : tanks_) { // Check if tanks are out of ammo
        if (tank->getIsAlive() == 0 && tank->getAmmo() <= 0) {
            ++no_ammo_count;
        }

        if (tank->getPlayerId() == 1 && tank->getIsAlive() == 0) { // Found player 1's tank
            ++player_1_count;
        } else if (tank->getPlayerId() == 2 && tank->getIsAlive() == 0) { // Found player 2's tank
            ++player_2_count;
        }
    }

    if (no_ammo_count == tank_count) { noAmmoFlag_ = true; } // If tanks are out of ammo, set flag
    if (player_1_count == 0) { // If player 1 is out of tanks
        gameOverStatus_ = 1;
        gameOver_ = true;
    }

    else if (player_2_count == 0) { // If player 2 is out of tanks
        gameOverStatus_ = 2;
        gameOver_ = true;
    }

    // Update the number of tanks for each player for logging
    numTanks1_ =  player_1_count;
    numTanks2_ = player_2_count;
}


// Function to get tank index at a specific location
int GM_209277367_322542887::getTankIndexAt(int x, int y) const {
    const pair<int, int> tank_loc = make_pair(x, y);

    for (int i = 0; i < static_cast<int>(tanks_.size()); ++i) { // Check if tank is at location
        if (tanks_[i]->getLocation() == tank_loc) {
            return i;
        }
    }
    return -1; // Tank not found
}

// Function to get shell at a specific location (by itertator)
ShellIterator GM_209277367_322542887::getShellAt(const int x, const int y) {
    for (auto it = shells_.begin(); it != shells_.end(); ++it) { // Check if shell is at location
        if (auto [fst, snd] = (*it)->getLocation(); fst == x && snd == y) {
            return it;
        }
    }

    return shells_.end(); // Return end iterator if no shell found
}

// Function to delete shell given an iterator
ShellIterator GM_209277367_322542887::deleteShell(ShellIterator it) {
    if (it != shells_.end()) {
        it = shells_.erase(it); // Remove the shell from the vector
    }

    return it;
}

// Function to move shells
void GM_209277367_322542887::moveShells(vector<unique_ptr<Shell>>& shells) {
    for (auto it = shells.begin(); it != shells.end();) { // Iterate through all shells
        Shell& shell = **it; // De-reference the iterator to get the shell
        auto [fst, snd] = shell.getLocation();
        const int x = fst;
        const int y = snd;
        Direction dir = shell.getDirection();

        // Calculate the next location of the shell
        auto [new_x, new_y] = nextLocation(x, y, dir);
        const char next_cell = gameboard_[new_y][new_x];

        // Shells spawned exactly on tanks
        if (gameboard_[y][x] == 'c' || gameboard_[y][x] == 'd') {
            if (const int tank_index = getTankIndexAt(x, y); tank_index != -1) {
                destroyedTanksIndices_.insert(tank_index); // Mark tank 1 for deletion
                tanks_[tank_index]->increaseTurnsDead();
                gameboard_[y][x] = ' '; // Remove the tank from the gameboard

                // cout << "Shell hit and destroyed players 1's tank  at: " << x << ", " << y << endl; // Log the shell collision
                it = shells.erase(it); // Remove the shell from the vector
                break;
            }
        }

        // Unique cases of shell movement
        if (shell.isAboveMine()) { gameboard_[y][x] = '@'; shell.setAboveMine(false); } // Mark the shell's position on the gameboard
        if (gameboard_[y][x] == '^') { gameboard_[y][x] = '*'; } // Clear the previous position of the shell
        if (gameboard_[y][x] == 'a' || gameboard_[y][x] == 'b') { gameboard_[y][x] = gameboard_[y][x] == 'a' ? '1': '2'; } // Clear the previous position of the shell
        if (!(gameboard_[y][x] == '1' || gameboard_[y][x] == '2' || gameboard_[y][x] == '@')){ gameboard_[y][x] = ' '; } // Clear the previous position of the shell

        // Check if the next cell is a wall, weakened wall, or tank
        switch(next_cell){
            case '#': // If next cell is a wall
                gameboard_[new_y][new_x] = '$'; // Damage the wall
                // cout << "Shell hit and weakened wall at: " << new_x << ", " << new_y << endl; // Log the shell collision
                it = shells.erase(it); // Remove the shell from the vector
                break;

            case '$': // If next cell is a weakened wall
                gameboard_[new_y][new_x] = ' '; // Destroy the wall
                // cout << "Shell hit and destroyed wall at: " << new_x << ", " << new_y << endl; // Log the shell collision
                it = shells.erase(it); // Remove the shell from the vector
                break;

            case '1': {// If next cell is occupied by tank 1
                if (const int tank_index = getTankIndexAt(new_x, new_y); tank_index != -1) { //
                    //delete_tank(tank_it); // Delete tank 1
                    destroyedTanksIndices_.insert(tank_index); // Mark tank 1 for deletion
                    tanks_[tank_index]->increaseTurnsDead();
                    gameboard_[new_y][new_x] = ' '; // Remove the tank from the gameboard

                    // cout << "Shell hit and destroyed players 1's tank  at: " << new_x << ", " << new_y << endl; // Log the shell collision
                    it = shells.erase(it); // Remove the shell from the vector
                }
                break;}

            case '2':{
                if (const int tank_index = getTankIndexAt(new_x, new_y); tank_index != -1) {
                    gameboard_[new_y][new_x] = ' '; // Remove the tank from the gameboard
                    destroyedTanksIndices_.insert(tank_index); // Mark tank 2 for deletion
                    tanks_[tank_index]->increaseTurnsDead();
                    // cout << "Shell hit and destroyed players 2's tank  at: " << new_x << ", " << new_y << endl; // Log the shell collision
                    it = shells.erase(it); // Remove the shell from the vector
                }
                break;}

            case '@': // If next cell is a mine
                shell.setLocation(new_x, new_y); // Update the shell's location
                gameboard_[new_y][new_x] = '*'; // Mark intersection of shell and mine
                shell.setAboveMine(true); // Set the shell to be above the mine
                ++it; // Move to the next shell
                break;

            case '*': {// If next cell is a shell
                // cout << "Shells collided at: " << new_x << ", " << new_y << endl; // Log the shell collision

                ShellIterator other_shell_it = getShellAt(new_x, new_y); // Get the other shell at the new position
                const int other_shell_dir = static_cast<int>((*other_shell_it)->getDirection());
                if (static_cast<int>(dir) == ((other_shell_dir + 4) % NUM_OF_DIRECTIONS)) { // If the shells are in opposite directions
                    gameboard_[new_y][new_x] = ' '; // Mark the shell's position on the game board

                    if (it < other_shell_it) { // If current shell is before the other shell
                        deleteShell(other_shell_it); // Delete the other shell

                        it = shells.erase(it); // Remove the shell from the vector
                    }
                    else {
                        shells.erase(it); // Remove the shell from the vector

                        it = deleteShell(other_shell_it); // Delete the other shell
                    }

                    if (shells.empty()) { // If there are no more shells
                        goto last_elem; // If the iterator reaches the end, break out of the loop
                    }
                }
                else { // If the shells are not in opposite directions
                    (*it)->setLocation(new_x, new_y); // Update the shell's location
                    gameboard_[new_y][new_x] = '^'; // Mark the new position of the shell on the game board
                    ++it; // Move to the next shell if no deletion occurs
                }
                break;}

            case ' ': // If next cell is empty
                shell.setLocation(new_x, new_y); // Update the shell's location
                gameboard_[new_y][new_x] = '*'; // Mark the new position of the shell on the gameboard
                ++it; // Move to the next shell
                break;
            default: ;
        }
    }
last_elem:
    return;
}

// Function to check for shell collisions
void GM_209277367_322542887::checkShellsCollide() {
    map<pair<int, int>, vector<unique_ptr<Shell>>> shell_map;

    // Create a map to store shells by location
    for (auto& shell : shells_) {
        pair<int, int> loc = shell->getLocation();
        shell_map[loc].emplace_back(std::move(shell));
    }

    shells_.clear(); // Clear the shells vector of nullptrs

    // Iterate over the map to check for collisions
    for (auto& [loc, shell_lst] : shell_map) {
        if (shell_lst.size() == 1) {
            shells_.emplace_back(std::move(shell_lst[0]));
        }
        else {
            gameboard_[loc.second][loc.first] = ' ';
        }
    }
}

// Function to run the game
void GM_209277367_322542887::run() {
    // Initialize players
    player1_ = playerFactory_->create(1, width_, height_, maxSteps_, numShells_);
    player2_ = playerFactory_->create(2, width_, height_, maxSteps_, numShells_);

    // std::cout << "\nGame Started!" << endl;
    // if (visualMode_) { writeBoardToJson(); }// Visualisation

    // Game loop
    while (!gameOver_) { // Main game loop
        // if (visualMode_) { writeBoardToJson(); }// Visualisation
        // if (visualMode_) {
        //     // Visualisation
        //     std::filesystem::path flagPath = std::filesystem::current_path() / "visualizer" / "step.flag";
        //     // std::cout << "Waiting for: " << flagPath << std::endl;
        //
        //     while (!std::filesystem::exists(flagPath)) {
        //         std::this_thread::sleep_for(std::chrono::milliseconds(100));
        //     }
        //     std::filesystem::remove(flagPath);
        // }

        // Set the gameboard to the last round's gameboard
        lastRoundGameboard_ = gameboard_;

        // Check if the maximum number of turns has been reached
        if (turn_ >= maxSteps_) {
            gameOver_ = true; // Set the game over flag
            gameLog_ << "Tie, reached max steps = " << maxSteps_ << ", player 1 has " << numTanks1_ << " tanks, player 2 has "
               << numTanks2_ << " tanks" << endl;
            break; // Exit the loop
        }
        // std::cout << "\nTurn: " << turn_ << endl; // Print the current turn number

        getTankActions(); // Get actions for both tanks and update battle_info_requested
        performTankActions(); // Perform actions for both tanks

        for (size_t i = 0; i < 2; ++i) { // Iterate through each tank
            moveShells(shells_); // Move the shells
            checkShellsCollide(); // Check for shell collisions
        }

        updateGameLog();

        // std::cout << "\nGame Board after turn " << turn_ << ":" << endl; // Print the game board after each turn
        // printBoard(); // Print the game board

        checkTanksStatus(); // Check if tanks are out of ammo and check for tanks alive

        if (noAmmoFlag_) { // If both tanks are out of ammo
            noAmmoTimer_--; // Decrease the no ammo timer
            if (noAmmoTimer_ == 0) { // Check if the timer has reached zero
                updateGameResult(TIE, NO_SHELLS_GAME_OVER, {numTanks1_, numTanks2_},
                    satellite_view_, turn_);
                gameOver_ = true; // Set game_over to true if both tanks are out of ammo for GAME_OVER_NO_AMMO turns
            gameLog_ << "Tie, both players have zero shells for " << GAME_OVER_NO_AMMO << " steps" << endl; // Print message if both tanks are out of ammo
            }
        }

        if (gameOver_) { // Check if the game is over
            if (gameOverStatus_ == 3) { // Both players are missing tanks
                updateGameResult(TIE, ALL_TANKS_DEAD, {0, 0}, satellite_view_, turn_);
                gameLog_ << "Tie, both players have zero tanks" <<  endl;
            } else if (gameOverStatus_ == 1) { // Player 1 has no tanks left
                updateGameResult(PLAYER_2_WIN, ALL_TANKS_DEAD, {0, numTanks2_}, turn_);
                gameLog_ << "Player 2 won with " << numTanks2_ << " tanks still alive" << endl;
            } else if (gameOverStatus_ == 2) { // Player 2 has no tanks left
                updateGameResult(PLAYER_1_WIN, ALL_TANKS_DEAD, {numTanks1_, 0}, turn_);
                gameLog_ << "Player 1 won with " <<  numTanks1_ << " tanks still alive" << endl;
            }

            // if (visualMode_) { writeBoardToJson(); }// Visualisation
            // break; // Exit the game loop if the game is over
        }

        ++turn_; // Increment the turn counter
    }
}

// Function to calc next location
pair<int, int> GM_209277367_322542887::nextLocation(const int x, const int y, const Direction dir, const bool backwards) const {
    auto [dx, dy] = directionMap.at(dir);
    if (backwards) { // If the direction is backwards, reverse the direction
        dx = -dx;
        dy = -dy;
    }
    return {(x + dx + width_) % width_, (y + dy + height_) % height_}; // Calculate the next location
}

void GM_209277367_322542887::updateGameResult(int winner, int reason, vector<size_t> remaining_tanks,
    unique_ptr<SatelliteView> game_state, size_t rounds) {
    gameResult_.winner = winner;
    gameResult_.reason = static_cast<GameResult::Reason>(reason);
    gameResult_.remaining_tanks = remaining_tanks;
    gameResult_.gameState = move(game_state);
    gameResult_.rounds = rounds;
}

// Function to print gameboard
void GM_209277367_322542887::printBoard() const {
    for (const auto& row : gameboard_) {
        for (const auto& cell : row) {
            switch (cell) {
                case '1': // Tank 1 - Bright Blue
                    std::cout << "\033[94m" << cell << "\033[0m";
                    break;
                case '2': // Tank 2 - Green
                    std::cout << "\033[32m" << cell << "\033[0m";
                    break;
                case '#': // Walls - White
                    std::cout << "\033[37m" << cell << "\033[0m";
                    break;
                case '$': // Obstacles - Grey
                    std::cout << "\033[90m" << cell << "\033[0m";
                    break;
                case '@': // Mines - Red
                    std::cout << "\033[31m" << cell << "\033[0m";
                    break;
                case '*': // Shells - Yellow
                    std::cout << "\033[33m" << cell << "\033[0m";
                    break;
                default: // Default (empty spaces or other characters)
                    std::cout << cell;
                    break;
            }
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

// Function to translate direction to string
std::string GM_209277367_322542887::getEnumName(const Direction dir){
    switch (dir) {
        case Direction::U:  return "U";
        case Direction::UR: return "UR";
        case Direction::R:  return "R";
        case Direction::DR: return "DR";
        case Direction::D:  return "D";
        case Direction::DL: return "DL";
        case Direction::L:  return "L";
        case Direction::UL: return "UL";
        default:            return "Unknown Direction";
    }
}

// Function to translate action to string
std::string GM_209277367_322542887::getEnumName(const ActionRequest action) {
    switch (action) {
        case ActionRequest::MoveForward:     return "MoveForward";
        case ActionRequest::MoveBackward:    return "MoveBackward";
        case ActionRequest::RotateLeft90:    return "RotateLeft90";
        case ActionRequest::RotateRight90:   return "RotateRight90";
        case ActionRequest::RotateLeft45:    return "RotateLeft45";
        case ActionRequest::RotateRight45:   return "RotateRight45";
        case ActionRequest::Shoot:           return "Shoot";
        case ActionRequest::GetBattleInfo:   return "GetBattleInfo";
        case ActionRequest::DoNothing:       return "DoNothing";
        default:                             return "Unknown Action";
    }
}

pair<int, int> GM_209277367_322542887::getGameboardSize() const {
    return {width_, height_};
}

void GM_209277367_322542887::updateGameLog() {
    for (int i = 0; i < static_cast<int>(tanks_.size()); ++i) {
        if (i != 0) { gameLog_ << " "; }

        int tank_state = tanks_[i]->getIsAlive();
        if (tank_state == 0) {
            gameLog_ << getEnumName(tankActions_[i].first);
            if (!tankActions_[i].second) { gameLog_ << " (ignored)"; }
        }
        else if (tank_state == 1) {
            if (!tankActions_[i].second) { gameLog_ << " (ignored)"; }
            gameLog_ << getEnumName(tankActions_[i].first) << " (killed)";
            tanks_[i]->increaseTurnsDead();
        }
        else {gameLog_ << "killed"; }

        if (i != static_cast<int>(tanks_.size()) - 1) { gameLog_ << ","; }
    }

    gameLog_ << endl;
}
