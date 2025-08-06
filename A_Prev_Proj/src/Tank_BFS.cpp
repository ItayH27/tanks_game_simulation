#include "TankTypes.h"


// Creates pi_graph using BFS algorithm
// and return a stack of the path from out tank to the closest enemy tank
stack<pair<int,int>> Tank_BFS::get_path_stack(const vector<vector<char>>& gameboard) const {
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

// Constructor implementation
Tank_BFS::Tank_BFS(const int player_index, const int tank_index) : ExtTankAlgorithm(player_index, tank_index) {}

// Function to get the next action of the tank
ActionRequest Tank_BFS::getAction() {
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
void Tank_BFS::algo(const vector<vector<char>>& gameboard) {
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
