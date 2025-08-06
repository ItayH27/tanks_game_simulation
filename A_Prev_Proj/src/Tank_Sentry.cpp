# include "TankTypes.h"

// Constructor implementation
Tank_Sentry :: Tank_Sentry(const int player_index, const int tank_index) :
    ExtTankAlgorithm(player_index, tank_index), targetLoc_({-1, -1}){} // Constructor implementation

// Function to calculate the direction difference between two directions
int Tank_Sentry :: directionDiff(Direction from, Direction to) {
    int diff = static_cast<int>(to) - static_cast<int>(from);
    if (diff < 0) diff += 8;
    return diff;
}

// Function to get the next action to take
ActionRequest Tank_Sentry :: getAction() {
    // Check if the tank is in danger of shells
    const optional<Direction> danger_dir = isShotAt(shellLocations_);

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
    if (actionsQueue_.empty() && !justGotBattleinfo_ && !hasActiveTarget()) {
        actionsQueue_.push(ActionRequest::GetBattleInfo);
        justGotBattleinfo_ = true;
    }

    else { // Action queue is not empty
        justGotBattleinfo_ = false;

        // Evade the shell if in danger
        if(danger_dir.has_value() && turnsToEvade_ == 0){
            evadeShell(danger_dir.value(), gameboard);
        }
        // If an enemy is in range, shoot
        else if (isEnemyInLine(gameboard) && turnsToShoot_ == 0 && ammo_ > 0){
            shoot();
            return ActionRequest::Shoot;
        }
        // If the queue is empty, call the algorithm
        else if (actionsQueue_.empty()) {
            algo(gameboard); // Call the Sentry algorithm
        }
    }

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

    // Manage the location of the tank and decrease the cooldowns
    if (backwardsTimer_ == 0 && action != ActionRequest::GetBattleInfo) { updateLocation(action); }
    decreaseEvadeTurns();
    decreaseTurnsToShoot(action);
    decreaseShotDirCooldown();
    nonEmptyPop(); // Remove the action from the queue

    return action;
}

// Algorithm to calculate tanks anext actions
void Tank_Sentry :: algo(const vector<vector<char>>& gameboard) {
    // Get gameboard dimensions
    const int rows = static_cast<int>(gameboard.size()); // Y-axis
    const int cols = static_cast<int>(gameboard[0].size()); // X-axis

    // Tank location
    const int this_x = location_.first;
    const int this_y = location_.second;

    // Find the location of all enemy tanks
    vector<pair<int, int>> enemy_tanks;
    for (int i = 0; i < rows; i++) {
        for (int j = 0; j < cols; j++) {
            pair cell = {j, i};
            if (isdigit(gameboard[i][j]) && gameboard[i][j] != '0' + playerIndex_) {
                enemy_tanks.push_back(cell);
            }
        }
    }

    // Find the closest enemy tank, using euclidian distance
    pair closest_enemy = {-1, -1};
    double min_distance = INF;
    for (const auto& enemy : enemy_tanks) { // Iterate over all enemy tank locations
        const int diff_x = enemy.first - this_x;
        const int diff_y = enemy.second - this_y;

        // Calculate distance from the closest tank using euclidian distance, and compare to min distance
        if (const double euclidian_distance = sqrt(diff_x * diff_x + diff_y * diff_y); euclidian_distance < min_distance) {
            min_distance = euclidian_distance;
            closest_enemy = enemy;
        }
    }

    // Updated current target location
    targetLoc_ = closest_enemy;

    // Calculate the direction to the closest enemy tank
    const int dx = this_x - closest_enemy.first;
    const int dy = this_y - closest_enemy.second;
    const Direction dir_to_tank = diffToDir(dx, dy);

    // If the tanks are in the same row/column/diagonal and the canon is pointing to the row - shoot
    if (direction_ == dir_to_tank && (this_x == closest_enemy.first || this_y == closest_enemy.second ||
        abs(dx) == abs(dy)) && turnsToShoot_ == 0 && !friendlyInLine(dir_to_tank) && ammo_ > 0) {
        actionsQueue_.push(ActionRequest::Shoot);
    }

    // Calculate direction difference
    const int cw_diff = directionDiff(direction_, dir_to_tank); // Clockwise difference
    const int ccw_diff = directionDiff(dir_to_tank, direction_); // Counter-clockwise difference

    // Determine the action based on the direction difference
    if (cw_diff >= 2 && cw_diff <= 4) {
        actionsQueue_.push(ActionRequest::RotateRight90);
    }
    else if (ccw_diff >= 2 && ccw_diff <= 4) {
        actionsQueue_.push(ActionRequest::RotateLeft90);
    }
    else if (cw_diff == 1) {
        actionsQueue_.push(ActionRequest::RotateRight45);
    }
    else if (ccw_diff == 1) {
        actionsQueue_.push(ActionRequest::RotateLeft45);
    }
    else {
        actionsQueue_.push(ActionRequest::GetBattleInfo); // Default action if no other conditions are met
    }
}

// Function to check if tank has an active target
bool Tank_Sentry::hasActiveTarget() const {
    return targetLoc_.first != -1 && targetLoc_.second != -1; //
}
