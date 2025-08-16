#include "../GM_include/GM_209277367_322542887.h"

#include "../../common/GameManagerRegistration.h"

using std::move, std::endl, std::getline, std::make_unique, std::make_pair, fs::path;

using namespace GameManager_209277367_322542887;
REGISTER_GAME_MANAGER(GM_209277367_322542887);

GM_209277367_322542887::GM_209277367_322542887(bool verbose) : verbose_(verbose) {}

/**
 * @brief Retrieves and stores the next actions for all tanks in the game.
 *
 * This method clears the current list of tank actions and then iterates over all tanks.
 * If a tank is alive, it requests the next action from the tank's algorithm.
 * Otherwise, it records a "DoNothing" action for that tank.
 *
 * @note The second value in the stored pair indicates whether the action is valid (true) or a placeholder (false).
 *
 * @details
 * - Alive tanks have their action fetched via `getTank()->getAction()`.
 * - Dead tanks are assigned `ActionRequest::DoNothing`.
 *
 * @param None
 * @return void
 */
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

/**
 * @brief Checks if a given action is valid for the specified tank.
 *
 * Determines if the requested action can be performed by the tank,
 * validating movement and shooting actions against the current game state.
 *
 * @param tank Reference to the TankInfo object representing the tank.
 * @param action The requested action to validate.
 * @return true if the action is allowed, false otherwise.
 *
 * @note
 * - Movement actions are validated via isValidMove().
 * - Shooting actions are validated via isValidShoot().
 * - All other actions are considered valid by default.
 */
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

/**
 * @brief Validates if the tank can move in the requested direction.
 *
 * Checks the next cell based on the tank's position and direction,
 * ensuring it is not blocked by walls or obstacles.
 *
 * @param tank Tank to validate movement for.
 * @param action Requested move action.
 * @return true if movement is allowed, false otherwise.
 */
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

/**
 * @brief Checks if the tank can shoot.
 *
 * Valid if it has ammo and no shooting cooldown.
 *
 * @param tank Tank to check.
 * @return true if shooting is allowed, false otherwise.
 */
bool GM_209277367_322542887::isValidShoot(const TankInfo& tank) {
    // Check if the tank has ammo and zeroed cooldown
    return tank.getAmmo() > 0 && tank.getTurnsToShoot() == 0;
}

/**
 * @brief Performs a shooting action for the given tank.
 *
 * Checks if the tank can shoot (has ammo and no cooldown).
 * If not, only decreases the cooldown.
 * Otherwise:
 * - Resets cooldown and decreases ammo.
 * - Determines the next cell in the firing direction.
 * - Updates the board and shell list based on what is hit (wall, tank, shell, mine, or empty space).
 *
 * @param tank Tank executing the shot.
 */
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

/**
 * @brief Moves a tank one cell according to the requested action.
 *
 * Clears the tank’s current board cell, interprets backward movement
 * by reversing direction, computes the next location, and delegates
 * resolution (collisions, board updates, state changes) to
 * handleTankCollisionAt().
 *
 * @param tank Tank to move.
 * @param action Movement action (forward or backward).
 */
void GM_209277367_322542887::moveTank(TankInfo& tank, const ActionRequest action) {
    auto [x, y] = tank.getLocation();
    Direction dir = tank.getDirection();

    gameboard_[y][x] = ' ';

    if (action == ActionRequest::MoveBackward) {
        dir = static_cast<Direction>((static_cast<int>(dir) + 4) % NUM_OF_DIRECTIONS);
    }

    auto [new_x, new_y] = nextLocation(x, y, dir);
    char next_cell = gameboard_[new_y][new_x];

    handleTankCollisionAt(tank, x, y, new_x, new_y, dir, next_cell);
}

/**
 * @brief Resolves the result of moving a tank into a target cell.
 *
 * Applies movement/collision effects based on the content of @p next_cell:
 * - ' ' (empty): moves the tank, updates its location, and writes its id to the board.
 * - '@' (mine): marks the moving tank as destroyed and clears the cell.
 * - '*' (shell): if the shell comes from the opposite direction, destroys the tank and removes the shell;
 *   otherwise the tank advances onto the cell (marked as 'a'/'b').
 * - default (another tank): destroys the moving tank, and also destroys the other tank if present.
 * In all destructive outcomes the target cell is cleared.
 *
 * @param tank      The tank being moved/collided.
 * @param old_x     Previous x-coordinate of the tank.
 * @param old_y     Previous y-coordinate of the tank.
 * @param new_x     Target x-coordinate to enter.
 * @param new_y     Target y-coordinate to enter.
 * @param dir       Movement direction of the tank.
 * @param next_cell Board symbol at (new_x, new_y) before resolving the move.
 */
void GM_209277367_322542887::handleTankCollisionAt(
    TankInfo& tank, int old_x, int old_y,
    int new_x, int new_y, Direction dir, char next_cell) {

    const int player_id = tank.getPlayerId();

    switch (next_cell) {
        case ' ': {
            gameboard_[new_y][new_x] = static_cast<char>('0' + player_id);
            tank.setLocation(new_x, new_y);
            break;
        }
        case '@': {
            int tank_index = getTankIndexAt(old_x, old_y);
            destroyedTanksIndices_.insert(tank_index);
            tanks_[tank_index]->increaseTurnsDead();
            gameboard_[new_y][new_x] = ' ';
            break;
        }
        case '*': {
            ShellIterator shell_it = getShellAt(new_x, new_y);
            int shell_dir = static_cast<int>((*shell_it)->getDirection());

            if (static_cast<int>(dir) == ((shell_dir + 4) % NUM_OF_DIRECTIONS)) {
                int tank_index = getTankIndexAt(old_x, old_y);
                destroyedTanksIndices_.insert(tank_index);
                tanks_[tank_index]->increaseTurnsDead();
                deleteShell(shell_it);
                gameboard_[new_y][new_x] = ' ';
            } else {
                gameboard_[new_y][new_x] = (player_id == 1) ? 'a' : 'b';
                tank.setLocation(new_x, new_y);
            }
            break;
        }
        default: { // Assume another tank
            int self_idx = getTankIndexAt(old_x, old_y);
            int other_idx = getTankIndexAt(new_x, new_y);

            destroyedTanksIndices_.insert(self_idx);
            tanks_[self_idx]->increaseTurnsDead();

            if (other_idx != -1) {
                destroyedTanksIndices_.insert(other_idx);
                tanks_[other_idx]->increaseTurnsDead();
            }

            gameboard_[new_y][new_x] = ' ';
            break;
        }
    }
}


/**
 * @brief Rotates a tank according to the given action.
 *
 * Determines the new direction based on rotation type
 * (left/right 45° or 90°) and updates the tank's orientation.
 * If the action is not a rotation, the direction remains unchanged.
 *
 * @param tank Tank to rotate.
 * @param action Rotation action request.
 */
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

/**
 * @brief Executes a requested action for the given tank.
 *
 * Handles movement, shooting, rotation, and special rules for moving backwards.
 * Includes timing and flag management for backward movement delays, cooldowns,
 * and invalid action handling. Also supports retrieving battle info for the tank.
 *
 * @param action The action to perform.
 * @param tank   Reference to the tank performing the action.
 * @return true if the action was successfully executed or valid, false otherwise.
 *
 * @note
 * - Backward movement may require a delay before execution.
 * - Invalid actions still reduce the shooting cooldown.
 * - GetBattleInfo temporarily modifies the gameboard to mark the tank’s position
 *   before restoring it.
 */
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
            auto* player = (tank.getPlayerId() == 1 ? player1_ : player2_); // Get the player based on tank ID
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

/**
 * @brief Executes the queued actions for all alive tanks.
 *
 * Iterates over @c tanks_, and for each alive tank calls @c performAction
 * with the corresponding entry in @c tankActions_. If execution fails,
 * marks the action’s validity flag (second of the pair) as false.
 *
 * @note Expects @c tankActions_ to be aligned with @c tanks_ and
 *       pre-populated (e.g., by @c getTankActions()).
 *
 * @return void
 */
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

/**
 * @brief Checks the current status of all tanks and updates game state flags.
 *
 * Counts remaining alive tanks, checks for ammo depletion, and determines
 * if the game is over due to no tanks left or one player losing all tanks.
 * Updates flags such as @c noAmmoFlag_, @c gameOver_, and @c gameOverStatus_,
 * as well as player tank counts for logging.
 *
 * @note
 * - @c gameOverStatus_ values:
 *   - 1: Player 1 lost all tanks
 *   - 2: Player 2 lost all tanks
 *   - 3: No tanks left for either player
 * - @c noAmmoFlag_ is set if all remaining tanks have zero ammo.
 */
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


/**
 * @brief Finds the index of a tank located at the given coordinates.
 *
 * Iterates through @c tanks_ to match the given (x, y) position with a tank's location.
 *
 * @param x X-coordinate to search.
 * @param y Y-coordinate to search.
 * @return Tank index if found, -1 if no tank is at the given location.
 */
int GM_209277367_322542887::getTankIndexAt(int x, int y) const {
    const pair<int, int> tank_loc = make_pair(x, y);

    for (int i = 0; i < static_cast<int>(tanks_.size()); ++i) { // Check if tank is at location
        if (tanks_[i]->getLocation() == tank_loc) {
            return i;
        }
    }
    return -1; // Tank not found
}

/**
 * @brief Retrieves an iterator to the shell at the specified coordinates.
 *
 * Searches through @c shells_ for a shell whose location matches (x, y).
 *
 * @param x X-coordinate to search.
 * @param y Y-coordinate to search.
 * @return Iterator to the matching shell, or @c shells_.end() if not found.
 */
ShellIterator GM_209277367_322542887::getShellAt(const int x, const int y) {
    for (auto it = shells_.begin(); it != shells_.end(); ++it) { // Check if shell is at location
        if (auto [fst, snd] = (*it)->getLocation(); fst == x && snd == y) {
            return it;
        }
    }

    return shells_.end(); // Return end iterator if no shell found
}

/**
 * @brief Removes a shell from the game.
 *
 * Erases the shell at the given iterator position in @c shells_.
 *
 * @param it Iterator pointing to the shell to remove.
 * @return Iterator to the next element after the erased shell.
 */
ShellIterator GM_209277367_322542887::deleteShell(ShellIterator it) {
    if (it != shells_.end()) {
        it = shells_.erase(it); // Remove the shell from the vector
    }

    return it;
}

/**
 * @brief Updates the position of all shells and resolves interactions.
 *
 * Iterates over the provided list of shells, moving each one based on its
 * direction and handling collisions or interactions with tanks, other shells,
 * and various map objects.
 *
 * @param shells Reference to the vector of active shells.
 *
 * @note
 * - Calls helper functions to handle spawning on a tank, clearing the previous position,
 *   resolving collisions with other shells, or moving into the next cell.
 * - May remove shells from the list during iteration.
 */
void GM_209277367_322542887::moveShells(std::vector<std::unique_ptr<Shell>>& shells) {
    for (auto it = shells.begin(); it != shells.end();) {
        Shell& shell = **it;
        auto [x, y] = shell.getLocation();
        Direction dir = shell.getDirection();
        auto [new_x, new_y] = nextLocation(x, y, dir);
        const char next_cell = gameboard_[new_y][new_x];

        if (handleShellSpawnOnTank(shell, it)) continue;

        clearPreviousShellPosition(shell);

        if (next_cell == '*') {
            if (handleShellCollision(shell, new_x, new_y, dir, it)) return;
        } else {
            handleShellMoveToNextCell(shell, new_x, new_y, next_cell, it);
        }
    }
}

/**
 * @brief Restores the gameboard cell previously occupied by a shell.
 *
 * Updates the cell at the shell’s current location based on what was underneath:
 * - If above a mine, restores '@' and clears the above-mine flag.
 * - If part of a stacked shell ('^'), reverts to '*'.
 * - If overlapping a damaged tank ('a' or 'b'), restores the original tank ('1' or '2').
 * - Otherwise, clears the cell to an empty space unless it holds a tank or mine.
 *
 * @param shell Reference to the shell whose previous position is being cleared.
 */
void GM_209277367_322542887::clearPreviousShellPosition(Shell& shell) {
    auto [x, y] = shell.getLocation();

    if (shell.isAboveMine()) {
        gameboard_[y][x] = '@';
        shell.setAboveMine(false);
    } else if (gameboard_[y][x] == '^') {
        gameboard_[y][x] = '*';
    } else if (gameboard_[y][x] == 'a' || gameboard_[y][x] == 'b') {
        gameboard_[y][x] = (gameboard_[y][x] == 'a') ? '1' : '2';
    } else if (gameboard_[y][x] != '1' && gameboard_[y][x] != '2' && gameboard_[y][x] != '@') {
        gameboard_[y][x] = ' ';
    }
}

/**
 * @brief Handles the case where a shell is spawned directly on a tank.
 *
 * Checks if the shell's current position corresponds to a destroyed tank marker ('c' or 'd').
 * If so, marks the tank as destroyed, updates its state, clears the board cell, and removes the shell.
 *
 * @param shell Reference to the shell being processed.
 * @param it Iterator pointing to the shell in @c shells_.
 * @return true if a shell-tank spawn collision was handled and the shell removed, false otherwise.
 */
bool GM_209277367_322542887::handleShellSpawnOnTank(Shell& shell, ShellIterator& it) {
    auto [x, y] = shell.getLocation();
    char cell = gameboard_[y][x];
    if (cell == 'c' || cell == 'd') {
        int tank_index = getTankIndexAt(x, y);
        if (tank_index != -1) {
            destroyedTanksIndices_.insert(tank_index);
            tanks_[tank_index]->increaseTurnsDead();
            gameboard_[y][x] = ' ';
            it = shells_.erase(it);
            return true;
        }
    }
    return false;
}

/**
 * @brief Resolves a shell–shell collision at (x, y).
 *
 * If the two shells move in opposite directions, removes both shells
 * and clears the board cell. Otherwise, stacks the shells at (x, y)
 * by marking '^' and advances the iterator.
 *
 * @param shell The active shell being advanced.
 * @param x     Target x-coordinate.
 * @param y     Target y-coordinate.
 * @param dir   Direction of the active shell.
 * @param it    Iterator to the active shell; updated if erased.
 * @return true if @c shells_ becomes empty after handling the collision, false otherwise.
 */
bool GM_209277367_322542887::handleShellCollision(Shell& shell, int x, int y, Direction dir, ShellIterator& it) {
    ShellIterator other_shell_it = getShellAt(x, y);
    Direction other_dir = (*other_shell_it)->getDirection();

    auto areOppositeDirections = [](Direction d1, Direction d2) {
        return static_cast<int>(d1) == (static_cast<int>(d2) + 4) % NUM_OF_DIRECTIONS;
    };

    if (areOppositeDirections(dir, other_dir)) {
        gameboard_[y][x] = ' ';

        if (it < other_shell_it) {
            deleteShell(other_shell_it);
            it = shells_.erase(it);
        } else {
            shells_.erase(it);
            it = deleteShell(other_shell_it);
        }

        return shells_.empty();
    } else {
        shell.setLocation(x, y);
        gameboard_[y][x] = '^';
        ++it;
        return false;
    }
}

/**
 * @brief Moves a shell into the next cell and applies effects.
 *
 * Updates the board and shell list based on the target cell:
 * - '#': weaken wall to '$' and remove the shell.
 * - '$': destroy wall (set to space) and remove the shell.
 * - '1'/'2': destroy the tank at (x, y), clear the cell, and remove the shell.
 * - '@': place shell above a mine (mark '*', set above-mine flag), advance iterator.
 * - ' ': move shell to (x, y) and mark '*'.
 * - default: no special handling; advance iterator.
 *
 * @param shell   The shell being advanced.
 * @param x       Target x-coordinate.
 * @param y       Target y-coordinate.
 * @param next_cell Board symbol at (x, y).
 * @param it      Iterator to the shell; may be updated if erased.
 */
void GM_209277367_322542887::handleShellMoveToNextCell(Shell& shell, int x, int y, char next_cell, ShellIterator& it) {
    switch (next_cell) {
        case '#':
            gameboard_[y][x] = '$';
            it = shells_.erase(it);
            break;
        case '$':
            gameboard_[y][x] = ' ';
            it = shells_.erase(it);
            break;
        case '1':
        case '2': {
            int tank_index = getTankIndexAt(x, y);
            if (tank_index != -1) {
                destroyedTanksIndices_.insert(tank_index);
                tanks_[tank_index]->increaseTurnsDead();
                gameboard_[y][x] = ' ';
                it = shells_.erase(it);
            }
            break;
        }
        case '@':
            shell.setLocation(x, y);
            gameboard_[y][x] = '*';
            shell.setAboveMine(true);
            ++it;
            break;
        case ' ':
            shell.setLocation(x, y);
            gameboard_[y][x] = '*';
            ++it;
            break;
        default:
            ++it;
            break;
    }
}

/**
 * @brief Collapses shells that occupy the same cell and updates the board.
 *
 * Groups shells by their (x,y) location. If only one shell exists at a cell,
 * it is kept; if multiple shells share a cell, they are all removed and the
 * board cell is cleared to space.
 *
 * @note Rebuilds @c shells_ from the grouped map, removing collided shells.
 */
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

/**
 * @brief Initializes the game board and spawns tanks from a satellite snapshot.
 *
 * Copies all cells from @p gameBoard into @c gameboard_, creates tank
 * algorithms/infos for any '1'/'2' cells (using the respective factories),
 * and records their initial locations and ammo. If either side has zero tanks,
 * marks the game as over and writes a brief result to the verbose log.
 *
 * @param gameBoard Immutable snapshot providing map objects via getObjectAt().
 * @return true on successful initialization.
 *
 * @note Cell semantics: '1'/'2' = player tanks; other symbols are copied verbatim.
 *       Tank coordinates are stored as (x=j, y=i).
 */
bool GM_209277367_322542887::initiateGame(const SatelliteView& gameBoard) {
    // reset state if this can be called more than once
    tanks_.clear();

    int tank_1_count = 0, tank_2_count = 0;
    gameboard_.assign(height_, std::vector<char>(width_, ' '));

    for (int i = 0; i < height_; ++i) {
        for (int j = 0; j < width_; ++j) {
            char cell = gameBoard.getObjectAt(static_cast<size_t>(j), static_cast<size_t>(i));
            gameboard_[i][j] = cell; // copy snapshot into our board

            if (cell == '1' || cell == '2') {
                int player = cell - '0';
                int& tankCount = (player == 1) ? tank_1_count : tank_2_count;
                auto& factory = (player == 1) ? player1TankFactory_ : player2TankFactory_;

                auto tank = factory(player, tankCount);
                auto tankInfo = std::make_unique<TankInfo>(
                    tankCount, std::make_pair(j, i), numShells_, player, std::move(tank)
                );
                tanks_.push_back(std::move(tankInfo));
                ++tankCount;
            }
        }
    }

    // If a side has zero tanks, mark the game as over and log.
    if (tank_1_count == 0 || tank_2_count == 0) {
        if (verbose_) {
            if (tank_1_count == 0 && tank_2_count == 0) {
                gameLog_ << "Tie, both players have zero tanks\n";
            } else {
                int winner = (tank_1_count == 0) ? 2 : 1;
                int remaining = (winner == 1) ? tank_1_count : tank_2_count; // winner's count
                gameLog_ << "Player " << winner << " won with " << remaining << " tanks still alive\n";
            }
        }
        gameOver_ = true;
        if (verbose_) { gameLog_.flush(); gameLog_.close(); }
    }

    return true;
}

/**
 * @brief Runs a single Tank Game from initialization to termination.
 *
 * Initializes game dimensions, limits, players, and algorithm factories,
 * builds the initial board via initiateGame(), then executes the main loop
 * until a terminal condition is reached (max steps, all tanks dead, or
 * prolonged zero-ammo state).
 *
 * Per turn:
 * - Snapshots @c gameboard_ to @c lastRoundGameboard_.
 * - Collects actions (getTankActions) and executes them (performTankActions).
 * - Advances shells and resolves collisions (moveShells, checkShellsCollide).
 * - Logs the state (updateGameLog) and updates win/termination flags
 *   (checkTanksStatus, no-ammo timer handling).
 * - Finalizes the result with updateGameResult() when the game ends.
 *
 * @param map_width  Board width.
 * @param map_height Board height.
 * @param map        Read-only map snapshot used to initialize the game.
 * @param map_name   Map identifier (not used internally here).
 * @param max_steps  Maximum number of rounds to simulate.
 * @param num_shells Initial shells per tank.
 * @param player1    Player interface for player 1.
 * @param name1      Player 1 display name (not used internally here).
 * @param player2    Player interface for player 2.
 * @param name2      Player 2 display name (not used internally here).
 * @param player1_tank_algo_factory Factory for player 1 tank algorithms.
 * @param player2_tank_algo_factory Factory for player 2 tank algorithms.
 * @return Final @c GameResult moved out of @c gameResult_.
 */
GameResult GM_209277367_322542887::run(size_t map_width, size_t map_height, const SatelliteView& map, string map_name,
        size_t max_steps, size_t num_shells, Player& player1, string name1, Player& player2, string name2,
        TankAlgorithmFactory player1_tank_algo_factory, TankAlgorithmFactory player2_tank_algo_factory) {

    (void)name1, (void)name2, (void)map_name;
    width_ = map_width, height_ = map_height, maxSteps_ = max_steps, numShells_ = num_shells, player1_ = &player1, player2_ = &player2;
    player1TankFactory_ = player1_tank_algo_factory;
    player2TankFactory_ = player2_tank_algo_factory;

    initiateGame(map); // Copy game board and initiate tanks

    std::cout << "\nGame Started!" << endl;

    // Game loop
    while (!gameOver_) { // Main game loop
        // Set the gameboard to the last round's gameboard
        lastRoundGameboard_ = gameboard_;

        // Check if the maximum number of turns has been reached
        if (turn_ >= maxSteps_) {
            gameOver_ = true; // Set the game over flag
            if (verbose_) gameLog_ << "Tie, reached max steps = " << maxSteps_ << ", player 1 has " << numTanks1_ << " tanks, player 2 has "
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

        std::cout << "\nGame Board after turn " << turn_ << ":" << endl; // Print the game board after each turn
        printBoard(); // Print the game board

        checkTanksStatus(); // Check if tanks are out of ammo and check for tanks alive

        if (noAmmoFlag_) { // If both tanks are out of ammo
            noAmmoTimer_--; // Decrease the no ammo timer
            if (noAmmoTimer_ == 0) { // Check if the timer has reached zero
                updateGameResult(TIE, NO_SHELLS_GAME_OVER, {numTanks1_, numTanks2_},
                    std::move(satellite_view_), turn_);
                gameOver_ = true; // Set game_over to true if both tanks are out of ammo for GAME_OVER_NO_AMMO turns
            if (verbose_) gameLog_ << "Tie, both players have zero shells for " << GAME_OVER_NO_AMMO << " steps" << endl; // Print message if both tanks are out of ammo
            }
        }

        if (gameOver_) { // Check if the game is over
            if (gameOverStatus_ == 3) { // Both players are missing tanks
                updateGameResult(TIE, ALL_TANKS_DEAD, {0, 0}, std::move(satellite_view_), turn_);
                if (verbose_) gameLog_ << "Tie, both players have zero tanks" <<  endl;
            } else if (gameOverStatus_ == 1) { // Player 1 has no tanks left
                updateGameResult(PLAYER_2_WIN, ALL_TANKS_DEAD, {0, numTanks2_}, std::move(satellite_view_) ,turn_);
                if (verbose_) gameLog_ << "Player 2 won with " << numTanks2_ << " tanks still alive" << endl;
            } else if (gameOverStatus_ == 2) { // Player 2 has no tanks left
                updateGameResult(PLAYER_1_WIN, ALL_TANKS_DEAD, {numTanks1_, 0}, std::move(satellite_view_) , turn_);
                if (verbose_) gameLog_ << "Player 1 won with " <<  numTanks1_ << " tanks still alive" << endl;
            }

            break; // Exit the game loop if the game is over
        }

        ++turn_; // Increment the turn counter
    }

    return std::move(gameResult_);
}

/**
 * @brief Calculates the next board coordinates from a starting point and direction.
 *
 * Uses the @c directionMap to translate a @p dir into (dx, dy) offsets.
 * If @p backwards is true, reverses the direction. Wraps around the board
 * edges using modulo width/height to ensure valid coordinates.
 *
 * @param x         Current x-coordinate.
 * @param y         Current y-coordinate.
 * @param dir       Movement direction.
 * @param backwards Whether to move in the opposite direction.
 * @return Pair of (new_x, new_y) coordinates.
 */
pair<int, int> GM_209277367_322542887::nextLocation(const int x, const int y, const Direction dir, const bool backwards) const {
    auto [dx, dy] = directionMap.at(dir);
    if (backwards) { // If the direction is backwards, reverse the direction
        dx = -dx;
        dy = -dy;
    }
    return {(x + dx + width_) % width_, (y + dy + height_) % height_}; // Calculate the next location
}

/**
 * @brief Updates the stored final game result.
 *
 * Sets the winner, reason, remaining tanks per player, final game state,
 * and total number of rounds in @c gameResult_.
 *
 * @param winner          ID of the winning player (0 for tie).
 * @param reason          Integer castable to @c GameResult::Reason.
 * @param remaining_tanks Vector of remaining tanks per player.
 * @param game_state      Final game board state as a SatelliteView.
 * @param rounds          Total rounds played.
 */
void GM_209277367_322542887::updateGameResult(int winner, int reason, vector<size_t> remaining_tanks,
    unique_ptr<SatelliteView> game_state, size_t rounds) {
    gameResult_.winner = winner;
    gameResult_.reason = static_cast<GameResult::Reason>(reason);
    gameResult_.remaining_tanks = remaining_tanks;
    gameResult_.gameState = std::move(game_state);
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

/**
 * @brief Writes the current turn’s tank actions and statuses to the game log.
 *
 * For each tank:
 * - If alive, logs its chosen action (marking "(ignored)" if invalid).
 * - If just killed this turn, logs action with "(killed)" and increments its dead-turn counter.
 * - If already dead, logs "killed".
 *
 * Entries are separated by spaces and commas, one turn per log line.
 * Logging occurs only if @c verbose_ is true.
 */
void GM_209277367_322542887::updateGameLog() {
    for (int i = 0; i < static_cast<int>(tanks_.size()); ++i) {
        if (i != 0) { if (verbose_) gameLog_ << " "; }

        int tank_state = tanks_[i]->getIsAlive();
        if (tank_state == 0) {
            if (verbose_) gameLog_ << getEnumName(tankActions_[i].first);
            if (!tankActions_[i].second) { if (verbose_) gameLog_ << " (ignored)"; }
        }
        else if (tank_state == 1) {
            if (!tankActions_[i].second) { if (verbose_) gameLog_ << " (ignored)"; }
            if (verbose_) gameLog_ << getEnumName(tankActions_[i].first) << " (killed)";
            tanks_[i]->increaseTurnsDead();
        }
        else {if (verbose_) gameLog_ << "killed"; }

        if (i != static_cast<int>(tanks_.size()) - 1) { if (verbose_) gameLog_ << ","; }
    }

    if (verbose_) gameLog_ << endl;
}
