#include "TankAlgorithm_209277367_322542887.h"

#include "ExtBattleInfo.h"

#include "TankAlgorithmRegisteration.h"

using namespace Algorithm_209277367_322542887;
REGISTER_TANK_ALGORITHM(TankAlgorithm_209277367_322542887);

// Constructor
TankAlgorithm_209277367_322542887::TankAlgorithm_209277367_322542887(int player_index, int tank_index) :location_(-1, -1), playerIndex_(player_index), tankIndex_(tank_index),
ammo_(0), alive_(true), turnsToShoot_(0), turnsToEvade_(0), backwardsFlag_(false),justMovedBackwardsFlag_(false), backwardsTimer_(0),
justGotBattleinfo_(false), firstBattleinfo_(true){
    if (player_index == 1) {
        direction_ = Direction::L;
    } else {
        direction_ = Direction::R;
    }
};

// Calculate the direction based on the difference in x and y
Direction TankAlgorithm_209277367_322542887 :: diffToDir(const int diff_x, const int diff_y, const int rows, const int cols){
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
void TankAlgorithm_209277367_322542887 :: evadeShell(Direction danger_dir, const vector<vector<char>>& gameboard) {

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
optional<Direction> TankAlgorithm_209277367_322542887 :: actionsToNextCell(const pair<int, int> &curr, const pair<int, int> &next,
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
bool TankAlgorithm_209277367_322542887 :: isEnemyInLine(const vector<vector<char>>& gameboard) const {
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
optional<Direction> TankAlgorithm_209277367_322542887 :: isShotAt(const vector<pair<int,int>>& shells_locations) const {
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
void TankAlgorithm_209277367_322542887 :: shoot() {
    ammo_ = std::max(0, ammo_ - 1); // Decrease the ammo count
    turnsToShoot_ = 4; // Set the cooldown for shooting
    shotDir_ = direction_; // Save the direction of the shot
    shotDirCooldown_ = 4; // Set the cooldown for the shot direction
}

// Function to decrease turns to shoot
void TankAlgorithm_209277367_322542887 :: decreaseTurnsToShoot(const ActionRequest action) {
    if (turnsToShoot_ > 0 && action != ActionRequest::Shoot) turnsToShoot_--;
}

// Update tanks location and orientation based on the action
void TankAlgorithm_209277367_322542887 :: updateLocation(const ActionRequest action) {
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
void TankAlgorithm_209277367_322542887 :: nonEmptyPop() {
    if (!actionsQueue_.empty()) actionsQueue_.pop();
}

// Function to decrease turns to evade if possible
void TankAlgorithm_209277367_322542887 :: decreaseEvadeTurns() { if (turnsToEvade_ > 0) turnsToEvade_--; }

// Function to decrease turns to shoot in certain direction if possible
void TankAlgorithm_209277367_322542887 :: decreaseShotDirCooldown() { if (shotDirCooldown_ > 0) shotDirCooldown_--; }

// Function to update the tank with battleinfo from player
void TankAlgorithm_209277367_322542887 :: updateBattleInfo(BattleInfo& info){
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
bool TankAlgorithm_209277367_322542887::friendlyInLine(const Direction& dir) const {
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



// Creates pi_graph using BFS algorithm
// and return a stack of the path from out tank to the closest enemy tank
stack<pair<int,int>> TankAlgorithm_209277367_322542887::get_path_stack(const vector<vector<char>>& gameboard) const {
    // Get gameboard dimensions
    const int rows = static_cast<int>(gameboard.size()); // Y-axis
    const int cols = static_cast<int>(gameboard[0].size()); // X-axis

    // Get the stating location of the tank
    int x_start = location_.first;
    int y_start = location_.second;

    vector visited(rows, vector(cols, false)); // Matrix to keep track of visited cells
    vector pi_graph(rows, vector<pair<int,int>>(cols, {-2, -2})); // Matrix to keep track of the parent of each cell
    queue<pair<int, int>> bfs_queue; // Queue to store the cells to be visited

    bfs_queue.emplace(x_start, y_start); // Push the starting cell into the queue
    visited[y_start][x_start] = true; // Mark the starting cell as visited
    pi_graph[y_start][x_start] = {-1, -1}; // Starting cell doesn't have a parent
    bool found = false; // Flag to indicate if a target is found
    pair<int,int> end_cell; // Variable to store the coordinates of the target cell

    // BFS algorithm implementation for finding the closest enemy tank
    while(!bfs_queue.empty()){

        // get the current cell from the queue
        auto [fst, snd] = bfs_queue.front();
        bfs_queue.pop();

        // Check for each cell around the curr location
        for (int i = 0; i < 8; i++){
            int new_x = (fst + directionMap.at(static_cast<Direction>(i)).first + cols) % cols;
            int new_y = (snd + directionMap.at(static_cast<Direction>(i)).second + rows) % rows;

            // Check if the new cell is an enemy tank
            if (const char new_cell = gameboard[new_y][new_x]; isdigit(new_cell) && new_cell != '0' + playerIndex_){
                visited[new_y][new_x] = true; // Mark the cell as visited
                pi_graph[new_y][new_x] = {fst, snd}; // Set the parent of the cell to the current cell
                end_cell = {new_x, new_y}; // Store the coordinates of the enemy tank
                found = true; // Mark we found a target
                break;
            }

            // If the new cell is not visited and not a wall or mine, add it to the queue
            if (!visited[new_y][new_x]){
                visited[new_y][new_x] = true; // Mark the cell as visited
                // If the new cell is a wall or mine, skip it
                char new_cell = gameboard[new_y][new_x];
                if (new_cell == '#' || new_cell == '@' || new_cell == '$' || new_cell == '0' + playerIndex_) {
                    continue;
                }
                pi_graph[new_y][new_x] = {fst, snd}; // Set the parent of the cell to the current cell
                bfs_queue.emplace(new_x, new_y); // Add the new cell to the queue
            }
        }

        // If a target is found, break out of the loop
        if (found){
            break;
        }
    }

    // If after all the process no target is found, return an empty stack
    if (!found){
        return {};
    }

    // After a target is found, reconstruct the path from the target to the tank
    stack<pair<int,int>> path; // Stack to store the path
    pair<int,int> curr = end_cell; // Start from the target cell

    // Reconstruct the path by following the parent cells
    while (curr != std::make_pair(x_start, y_start)){
        path.push(curr); // Push the current cell into the stack
        curr = pi_graph[curr.second][curr.first]; // Move to the parent cell
    }

    return path;
}


// Function to get the next action of the tank
ActionRequest TankAlgorithm_209277367_322542887::getAction() {
    // Check if the tank is in danger of shells
    const optional<Direction> dangerDir = isShotAt(shellLocations_);

    // If the tank is currently moving backwards, decrease the backwards timer and return DoNothing
    if (backwardsTimer_ > 0 && backwardsFlag_){
        backwardsTimer_--;
        decreaseEvadeTurns();
        decreaseTurnsToShoot(ActionRequest::DoNothing);
        decreaseShotDirCooldown();
        return ActionRequest::DoNothing;
    }

    // If the tank just moved backwards, update the orientation
    // set the backwards flag and the just moved backwards flag
    if (backwardsFlag_ and !justMovedBackwardsFlag_) {
        updateLocation(ActionRequest::MoveBackward);
        backwardsFlag_ = false;
        justMovedBackwardsFlag_ = true;
    }

    // If the queue is empty and didn't get BattleInfo last turn,
    // get BattleInfo for next moves
    if (actionsQueue_.empty() && !justGotBattleinfo_){
        actionsQueue_.push(ActionRequest::GetBattleInfo);
        justGotBattleinfo_ = true;
    }

    else {
        justGotBattleinfo_ = false;

        // Evade the shell if in danger
        if(dangerDir.has_value() && turnsToEvade_ == 0){
            evadeShell(dangerDir.value(), gameboard);
        }

        // If an enemy is in range, shoot
        else if (isEnemyInLine(gameboard) && turnsToShoot_ == 0 && ammo_ > 0){
            shoot();
            return ActionRequest::Shoot;
        }
        // If the queue is empty, call the algorithm
        else if (actionsQueue_.empty()) {
            algo(gameboard); // Call the BFS algorithm
        }
    }

    // Get the next action
    ActionRequest action = ActionRequest::DoNothing;
    if (!actionsQueue_.empty()) { // If the queue is empty, return DoNothing
        action = actionsQueue_.front(); // Else, get the next action from the queue
    }

    // Manage the ammo count, backwards flag, just moved backwards flag
    if (action == ActionRequest::Shoot){ shoot(); }
    else if (action == ActionRequest::MoveBackward) {
        if (!justMovedBackwardsFlag_) { backwardsTimer_ = 2; }
        backwardsFlag_ = true;
    }
    else {
        backwardsFlag_ = false;
        justMovedBackwardsFlag_ = false;
    }

    // Manage the orientation of the tank and decrease all cooldowns
    if (backwardsTimer_ == 0 && action != ActionRequest::GetBattleInfo) { updateLocation(action); }
    decreaseEvadeTurns(); //
    decreaseTurnsToShoot(action);
    decreaseShotDirCooldown();
    nonEmptyPop(); // Remove the action from the queue

    return action;
}


// Updates the actions_queue with the appropriate actions to the path
// that the BFS algorithm found
void TankAlgorithm_209277367_322542887::algo(const vector<vector<char>>& gameboard) {
    // Get gameboard dimensions
    const int rows = static_cast<int>(gameboard.size()); // Y-axis
    const int cols = static_cast<int>(gameboard[0].size()); // X-axis

    // Empty the action queue to start fresh
    while(!actionsQueue_.empty()) {
        actionsQueue_.pop();
    }

    // Get the path stack of locations from the BFS algorithm
    stack<pair<int, int>> path_stack = get_path_stack(gameboard);

    // If the other tank is not reachable, try to shoot to make way
    if (path_stack.empty() && ammo_ > 0 && turnsToShoot_ == 0 && !friendlyInLine(direction_)){ // MAXCHANGE
        actionsQueue_.push(ActionRequest::Shoot);
    }

    // Initialize the curr status of the tank
    pair <int, int> curr_loc = location_;
    Direction curr_dir = direction_;
    bool curr_backwards_flag = backwardsFlag_;

    // Fill the action queue with the actions to take
    while (!path_stack.empty() && actionsQueue_.size() < 5){
        pair<int, int> next_loc = path_stack.top();
        path_stack.pop();

        // push the next action from curr loc to next loc
        auto temp_dir = actionsToNextCell(curr_loc, next_loc, curr_dir, rows, cols, curr_backwards_flag);

        // Update the canon direction
        if (temp_dir.has_value()){
            curr_dir = temp_dir.value();
        }

        curr_loc = next_loc; // Iterate to next location
    }
}
