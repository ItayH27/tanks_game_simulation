#include "ExtTankAlgorithm.h"
#include "ExtBattleInfo.h"
#include "GameManager.h"

// Constructor
ExtTankAlgorithm::ExtTankAlgorithm(int player_index, int tank_index) :location_(-1, -1), playerIndex_(player_index), tankIndex_(tank_index),
ammo_(0), alive_(true), turnsToShoot_(0), turnsToEvade_(0), backwardsFlag_(false),justMovedBackwardsFlag_(false), backwardsTimer_(0),
justGotBattleinfo_(false), firstBattleinfo_(true){
    if (player_index == 1) {
        direction_ = Direction::L;
    } else {
        direction_ = Direction::R;
    }
};

// Calculate the direction based on the difference in x and y
Direction ExtTankAlgorithm :: diffToDir(const int diff_x, const int diff_y, const int rows, const int cols){
    int pass = 0;
    auto dir = Direction::U;

    // Adjust a direction to account for cross-border movement
    if ((diff_x == 1 - cols && diff_y == -1) || (diff_x == cols - 1 && diff_y == 1) ||
            (diff_x == 1 && diff_y == 1 - rows) || (diff_x == -1 && diff_y == rows - 1)) {
        pass = 2;
    }
    else if ((diff_x == 1 - cols && diff_y == 0) || (diff_x == cols - 1 && diff_y == 0) ||
            (diff_x == 0 && diff_y == 1 - rows) || (diff_x == 0 && diff_y == rows - 1) ||
            (abs(diff_x) == cols - 1 && abs(diff_y) == rows - 1)) {
        pass = 4;
    }
    else if ((diff_x == 1 - cols && diff_y == 1) || (diff_x == cols - 1 && diff_y == -1) ||
            (diff_x == -1 && diff_y == 1 - rows) || (diff_x == 1 && diff_y == rows - 1)) {
        pass = 6;
    }

    // Decide a direction based on the values of x and y
    if (diff_x == 0 && diff_y > 0) dir = Direction::U;
    if (diff_x < 0 && diff_y > 0) dir = Direction::UR;
    if (diff_x < 0 && diff_y == 0) dir = Direction::R;
    if (diff_x < 0 && diff_y < 0) dir = Direction::DR;
    if (diff_x == 0 && diff_y < 0) dir = Direction::D;
    if (diff_x > 0 && diff_y < 0) dir = Direction::DL;
    if (diff_x > 0 && diff_y == 0) dir = Direction::L;
    if (diff_x > 0 && diff_y > 0) dir = Direction::UL;

    // Update direction based on the pass value
    dir = static_cast<Direction>((static_cast<int>(dir) + pass) % 8);

    return dir;
}

// Algorithm to evade a shell
void ExtTankAlgorithm :: evadeShell(Direction danger_dir, const vector<vector<char>>& gameboard) {

    // Empty the action queue
    while(!actionsQueue_.empty()) {
        actionsQueue_.pop();
    }

    // Get gameboard dimensions
    const int rows = static_cast<int>(gameboard.size()); // Y-axis
    const int cols = static_cast<int>(gameboard[0].size()); // X-axis

    // Get the opposite direction of the shell
    const auto opposite_danger_dir = static_cast<Direction>((static_cast<int>(danger_dir) + 4) % 8);

    for (int i = 0; i < 8; i++) { // Iterate through all directions
        auto curr_dir = static_cast<Direction>(i);
        if (curr_dir == danger_dir || curr_dir == opposite_danger_dir) {continue;} // Skip the direction of the shell

        // Calculate the new coordinates based on the current direction
        int new_x = (location_.first + directionMap.at(static_cast<Direction>(i)).first + cols) % cols;
        int new_y = (location_.second + directionMap.at(static_cast<Direction>(i)).second + rows) % rows;

        if (gameboard[new_y][new_x] == ' ') { // Check if the cell is empty
            bool curr_backwards_flag = backwardsFlag_;
           actionsToNextCell(location_, {new_x, new_y}, direction_,rows, cols,
               curr_backwards_flag, true); // Call the function to get the actions to the next cell
            break;
        }
    }
}

// Function to fill action queue with actions based on next cell
optional<Direction> ExtTankAlgorithm :: actionsToNextCell(const pair<int, int> &curr, const pair<int, int> &next,
    Direction dir, const int rows, const int cols, bool& backwards_flag_, const bool is_evade) {

    const int dx = curr.first - next.first;
    const int dy = curr.second - next.second;
    Direction diff = diffToDir(dx, dy, rows, cols); // Calculate the difference in direction

    // Switch case on the direction of movement
    switch ((static_cast<int>(dir) - static_cast<int>(diff) + 8) % 8) {
    case 0: // Move forward
        actionsQueue_.push(ActionRequest::MoveForward);
        backwards_flag_ = false;
        if (is_evade) { // If evading
            turnsToEvade_ = 1;
        }
        return std::nullopt;

    case 1: // Move left-forward
        actionsQueue_.push(ActionRequest::RotateLeft45);
        actionsQueue_.push(ActionRequest::MoveForward);
        backwards_flag_ = false;
        if (is_evade) { // If evading
            turnsToEvade_ = 2;
        }
        return static_cast<Direction>((static_cast<int>(dir) - 1 + 8 ) % 8);

    case 2: // Move left
        actionsQueue_.push(ActionRequest::RotateLeft90);
        actionsQueue_.push(ActionRequest::MoveForward);
        backwards_flag_ = false;
        if (is_evade) { // If evading
            turnsToEvade_ = 2;
        }
        return static_cast<Direction>((static_cast<int>(dir) - 2 + 8 ) % 8);

    case 3: // Move left-backward
        actionsQueue_.push(ActionRequest::RotateLeft90);
        actionsQueue_.push(ActionRequest::RotateLeft45);
        actionsQueue_.push(ActionRequest::MoveForward);
        backwards_flag_ = false;
        if (is_evade) { // If evading
            turnsToEvade_ = 3;
        }
        return static_cast<Direction>((static_cast<int>(dir) - 3 + 8 ) % 8);

    case 4: // Move backward
        // If supposed to evade backwards - prefer to shoot
        if (is_evade && turnsToEvade_ == 0 && ammo_ > 0 && turnsToShoot_ == 0){
            actionsQueue_.push(ActionRequest::Shoot);
            turnsToEvade_ = 1;
            backwards_flag_ = false;
            return std::nullopt;
        }

        // Default backwards movement
        actionsQueue_.push(ActionRequest::MoveBackward);
        backwards_flag_ = true;
        return std::nullopt;

    case 5: // Move right-backward
        actionsQueue_.push(ActionRequest::RotateRight90);
        actionsQueue_.push(ActionRequest::RotateRight45);
        actionsQueue_.push(ActionRequest::MoveForward);
        backwards_flag_ = false;
        if (is_evade) { // If evading
            turnsToEvade_ = 3;
        }
        return static_cast<Direction>((static_cast<int>(dir) + 3 + 8 ) % 8);

    case 6: // Move right
        actionsQueue_.push(ActionRequest::RotateRight90);
        actionsQueue_.push(ActionRequest::MoveForward);
        backwards_flag_ = false;
        if (is_evade) { // If evading
            turnsToEvade_ = 2;
        }
        return static_cast<Direction>((static_cast<int>(dir) + 2 + 8 ) % 8);

    case 7: // Move right-forward
        actionsQueue_.push(ActionRequest::RotateRight45);
        actionsQueue_.push(ActionRequest::MoveForward);
        backwards_flag_ = false;
        if (is_evade) { // If evading
            turnsToEvade_ = 2;
        }
        return static_cast<Direction>((static_cast<int>(dir) + 1 + 8 ) % 8);

    default: // No direction change
        return std::nullopt;
    }
}

// Function to check if an enemy tank is in range
bool ExtTankAlgorithm :: isEnemyInLine(const vector<vector<char>>& gameboard) const {
    if (gameboard.empty()) return false; // Check if the gameboard is empty

    // Get gameboard dimensions
    const int rows = static_cast<int>(gameboard.size()); // Y-axis
    const int cols = static_cast<int>(gameboard[0].size()); // X-axis

    // Iterate over gameboard
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            const pair cell = {i, j};

            // Check if the cell is an enemy tank
            if (isdigit(gameboard[i][j]) && gameboard[i][j] != '0' + playerIndex_) {
                const int diff_x = location_.first - cell.second;
                const int diff_y = location_.second - cell.first;
                const Direction dir_to_tank = diffToDir(diff_x, diff_y);

                // Check if the tank is in range and there is no danger for friendly fire
                if (direction_ == dir_to_tank && (location_.first == cell.second || location_.second == cell.first ||
                        abs(diff_x) == abs(diff_y)) && !friendlyInLine(dir_to_tank)) {
                    return true;
                }
            }
        }
    }

    return false; // No enemy tank in range
}

// Function to check if the tank is in danger of shells from a distance of 5 cells at least
optional<Direction> ExtTankAlgorithm :: isShotAt(const vector<pair<int,int>>& shells_locations) const {
    // Iterate over all shells locations
    for (auto& shell : shells_locations){
        const int diff_x = location_.first - shell.first;

        // Check if the shell is in range of at list 5 cells
        if (const int diff_y = location_.second - shell.second; abs(diff_x) <= 5 && abs(diff_y) <= 5
            && location_ != shell){
            const Direction danger_dir = diffToDir(diff_x, diff_y); // Calculate the dir which the shell is coming from

            // Check if the tank is in danger of the same shell it shot
            if (danger_dir == shotDir_ && shotDirCooldown_ > 0){
                continue; // We don't want to evade from our own shot
            }

            // Check if the shell is in the same row/column/diagonal
            if (diff_x == 0 || diff_y == 0 || abs(diff_x) == abs(diff_y) ){
                return diffToDir(diff_x, diff_y);
            }
        }
    }

    return std::nullopt; // Return nullopt if no danger is detected
}

// Perform shoot action
void ExtTankAlgorithm :: shoot() {
    ammo_ = std::max(0, ammo_ - 1); // Decrease the ammo count
    turnsToShoot_ = 4; // Set the cooldown for shooting
    shotDir_ = direction_; // Save the direction of the shot
    shotDirCooldown_ = 4; // Set the cooldown for the shot direction
}

// Function to decrease turns to shoot
void ExtTankAlgorithm :: decreaseTurnsToShoot(const ActionRequest action) {
    if (turnsToShoot_ > 0 && action != ActionRequest::Shoot) turnsToShoot_--;
}

// Update tanks location and orientation based on the action
void ExtTankAlgorithm :: updateLocation(const ActionRequest action) {
    // Get gameboard dimensions
    const int rows = static_cast<int>(gameboard.size()); // Y-axis
    const int cols = static_cast<int>(gameboard[0].size()); // X-axis

    switch(action){ // Switch case over the action
        // For movement - update the location
        case ActionRequest::MoveForward:{
            backwardsFlag_ = false;
            // Move to the next location
            auto [fst, snd] = directionMap.at(direction_);
            location_ = {(location_.first + fst + cols) % cols, (location_.second + snd + rows) % rows};
            break;
        }
        case ActionRequest::MoveBackward:{
            backwardsFlag_ = true;
            // Move to the next location
            auto [fst, snd] = directionMap.at(direction_);
            location_ = {(location_.first - fst + cols) % cols , (location_.second - snd + rows) % rows};
            break;
        }

        // For rotation - update the direction
        case ActionRequest::RotateLeft90:
            // Update the direction
            direction_ = static_cast<Direction>((static_cast<int>(direction_) - 2 + 8) % 8);
            break;
        case ActionRequest::RotateRight90:
            direction_ = static_cast<Direction>((static_cast<int>(direction_) + 2) % 8);
            break;
        case ActionRequest::RotateLeft45:
            direction_ = static_cast<Direction>((static_cast<int>(direction_) - 1 + 8) % 8);
            break;
        case ActionRequest::RotateRight45:
            direction_ = static_cast<Direction>((static_cast<int>(direction_) + 1) % 8);
            break;
        // Default for any other action
        default:
            break;
    }
}

// Function to pop action from the actions queue if not empty
void ExtTankAlgorithm :: nonEmptyPop() {
    if (!actionsQueue_.empty()) actionsQueue_.pop();
}

// Function to decrease turns to evade if possible
void ExtTankAlgorithm :: decreaseEvadeTurns() { if (turnsToEvade_ > 0) turnsToEvade_--; }

// Function to decrease turns to shoot in certain direction if possible
void ExtTankAlgorithm :: decreaseShotDirCooldown() { if (shotDirCooldown_ > 0) shotDirCooldown_--; }

// Function to update the tank with battleinfo from player
void ExtTankAlgorithm :: updateBattleInfo(BattleInfo& info){
    auto battle_info = dynamic_cast<ExtBattleInfo&>(info); // Cast the battleinfo to ExtBattleInfo

    // If it's the first time the tank receives battleinfo - initialize the tank's ammo and location
    if (firstBattleinfo_) {
        firstBattleinfo_ = false;
        this->ammo_ = battle_info.getInitialAmmo();
        this->location_ = battle_info.getInitialLoc();
    }

    // Update the battle information of the tank
    this->gameboard = battle_info.getGameboard(); // Get the gameboard from the battle info
    this->shellLocations_ = battle_info.getShellsLocation(); // Get the shells locations from the battle info

    // Update battle info with the current tank's information
    battle_info.setTankIndex(tankIndex_); // Set the current tank index
    battle_info.setCurrAmmo(ammo_); // Set the current ammo count
}

// Function to check if there is a friendly tank in the line of fire
bool ExtTankAlgorithm::friendlyInLine(const Direction& dir) const {
    // Get the coordinates of the tank and gameboard dimensions
    const int tank_x = location_.first;
    const int tank_y = location_.second;
    int x = tank_x, y = tank_y;

    const int rows = static_cast<int>(gameboard.size());
    const int cols = static_cast<int>(gameboard[0].size());

    // Get the enemy ID to search in map
    const int enemy_id = playerIndex_ == 1 ? 2 : 1;

    const bool is_cardinal_dir = dir == Direction::U || dir == Direction::D || dir == Direction::L || dir == Direction::R;
    const int diff_x = directionMap.at(dir).first;
    const int diff_y = directionMap.at(dir).second;

    // Search for friendly tanks in the line of fire
    if (is_cardinal_dir) {
        do {
            x = (x + diff_x + cols) % cols;
            y = (y + diff_y + rows) % rows;

            char cell = gameboard[y][x];
            if (cell == '0' + enemy_id) { // Found enemy tank
                return false;
            }
            if (cell == '0' + playerIndex_) { // Found friendly tank
                return true;
            }
        } while (x != tank_x || y != tank_y);

        return true; // The tank it's self is in the line of fire
    }

    while (true) {
        x += diff_x;
        y += diff_y;

        // check bounds *before* access
        if (x < 0 || x >= cols || y < 0 || y >= rows)
            break;

        char cell = gameboard[y][x];
        if (cell == '0' + enemy_id) { // Found enemy tank
            return false;
        }
        if (cell == '0' + playerIndex_) { // Found friendly tank
            return true;
        }
    }

    return false; // No friendly tank in line of fire
}
