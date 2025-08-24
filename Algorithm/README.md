# Player & Tank Algorithm (209277367_322542887) — README

## Overview
This package contains a **Player** and a **TankAlgorithm** implementation for the Tank Game in Assignment 3. It compiles into a shared object that the Simulator loads dynamically. The logic focuses on:
- Safe targeting (avoid friendly fire).
- Reactive evasion from nearby shells.
- Short-path pursuit of the nearest enemy via BFS.
- Reasonable movement/orientation decisions with cooldowns and backtracking control.

## Components

### Player_209277367_322542887
**Role:** Converts a `SatelliteView` snapshot into an internal grid and collects metadata needed by the tank AI each turn.

Key responsibilities:
- Build a `gameboard` (`vector<vector<char>>`) from `SatelliteView` (X=columns, Y=rows).
- Collect `shells_location` positions (`'*'`).
- Detect and cache own tank position (`'%'`).
- Construct an `ExtBattleInfo` (derived from `BattleInfo`) that includes:
  - `gameboard`
  - `shells_location`
  - `numShells_`
  - `tank_location`
- Call `tank.updateBattleInfo(battleInfo)` every time step.
- Track per-tank status (e.g., ammo received back from the tank via `ExtBattleInfo`).

Constructor parameters:
- `player_index`: 1 or 2 (used to disambiguate friend/foe).
- `x`, `y`: dimensions of the board (columns, rows).
- `max_steps`, `num_shells`: scenario constraints passed through to `ExtBattleInfo`.

Registration:
- `REGISTER_PLAYER(Player_209277367_322542887)` makes the factory discoverable at dlopen time (Simulator side).

### TankAlgorithm_209277367_322542887
**Role:** Decides a single action per turn based on recent `BattleInfo`, current orientation, ammo/cooldowns, and local hazards.

State highlights:
- `location_` (x,y), `direction_` (8-way), `ammo_`, `alive_`.
- Cooldowns and flags: `turnsToShoot_`, `turnsToEvade_`, `shotDir_` & `shotDirCooldown_`, `backwardsFlag_`, `justMovedBackwardsFlag_`, `backwardsTimer_`, `justGotBattleinfo_`, `firstBattleinfo_`.
- FIFO `actionsQueue_` of `ActionRequest`.

Registration:
- `REGISTER_TANK_ALGORITHM(TankAlgorithm_209277367_322542887)` exposes the algorithm factory to the Simulator’s registrar.

## Gameboard Encoding & Symbols
- `' '` — empty
- `'#'`, `'@'`, `'$'` — blocking/hazardous tiles (treated as non-traversable for BFS)
- `'*'` — shell location
- `'%'` — self (used by Player to seed `tank_location`)
- `'1'`/`'2'` (digits) — tank entities; compared against `playerIndex_` to determine friend (`'0'+playerIndex_`) vs. enemy (`'0'+enemy_id`)

## Turn Flow (Data & Decisions)

1. **Player step**
   - Reads `SatelliteView` and rebuilds `gameboard`, `shells_location`, and own `tank_location`.
   - Creates `ExtBattleInfo` with the above and calls `tank.updateBattleInfo(...)`.

2. **TankAlgorithm step (`getAction`)**
   - Cooldown accounting for previous actions.
   - If currently backing up and `backwardsTimer_ > 0` → stall with `DoNothing` while counting down.
   - If just switched out of a backward move → normalize orientation & flags.
   - If `actionsQueue_` empty and didn’t request info last turn → enqueue `GetBattleInfo`.
   - Otherwise, prioritize:
     1. **Evade shells** if danger is detected (`isShotAt`) and not already evading.
     2. **Shoot** if an enemy is aligned and safe (`isEnemyInLine` & `!friendlyInLine`), with ammo and no shoot cooldown.
     3. **Pathfind** via BFS to the nearest enemy and enqueue steps (`algo`) if queue empty.

3. **Action finalization**
   - Pop the next queued action (or `DoNothing` if empty).
   - Apply state transitions:
     - `Shoot` → decrements ammo, sets `turnsToShoot_` & `shotDirCooldown_` and remembers `shotDir_`.
     - `MoveBackward` → engage `backwardsFlag_` & `backwardsTimer_` (short lockout).
     - Any move/rotation → update `location_`/`direction_` with wrap-around arithmetic (toroidal board).
   - Decrement `turnsToEvade_`, shooting cooldowns, and shot-direction cooldown.
   - Return action.

## Cooldowns & Safety
- **Shooting:** `turnsToShoot_` blocks rapid fire; `shotDir_` & `shotDirCooldown_` prevent evading from your own shot.
- **Evasion:** `turnsToEvade_` avoids overreacting to persistent shell lines.
- **Backwards handling:** `backwardsTimer_` imposes brief “recovery” after reversing to avoid oscillations.

## Integration & Build

### Expected Foldering (per assignment)
- Place sources under your **Algorithm** project so they compile into a unique `.so`:
  - Example artifact: `Algorithm_209277367_322542887.so`
  - Namespace: `Algorithm_209277367_322542887`
- Ensure the **Simulator** project includes the registration `.cpp` implementations and performs `dlopen` on your algorithm `.so`.

### Registration Macros
- `REGISTER_PLAYER(Player_209277367_322542887)`
- `REGISTER_TANK_ALGORITHM(TankAlgorithm_209277367_322542887)`
These must appear at global scope in the corresponding `.cpp` files so the factories self-register when the `.so` is loaded.

## Configuration & Parameters
- **Constructor defaults:**
  - Initial facing: `L` for player 1, `R` for player 2 (symmetry).
  - Cooldowns initialize to 0; `firstBattleinfo_` triggers one-time ammo/location initialization from `ExtBattleInfo`.
- **Editable constants:** Evade window (≤5 cells), shoot cooldown (4 turns), backward recovery (2 turns) — change with caution to keep behavior stable.

## Assumptions & Constraints
- The board is **toroidal** for movement (wrap-around mod arithmetic).
- `ExtBattleInfo` is available in your project and exposes:
  - Initial/current ammo, initial location, and methods to set tank index/current ammo (used by Player↔Tank feedback).
- `SatelliteView` returns static snapshots (Simulator provides updated snapshots each round).
- The algorithm is single-threaded per tank; the Simulator may run multiple games/tanks concurrently (no shared mutable state across tanks).

## Example Usage (Simulator-side)
1. Build `Algorithm_209277367_322542887.so`.
2. Launch comparative or competitive runs pointing to your `.so`:
   - Comparative: pass two algorithm `.so` paths (can be the same) plus a single `game_map`.
   - Competitive: pass one `game_manager` `.so`, an `algorithms_folder` with N `.so` files, and a `game_maps_folder`.
3. Ensure your `.so` name and namespace follow the IDs convention so registration is unique.

---

[Back to Table of Contents](../README.md)