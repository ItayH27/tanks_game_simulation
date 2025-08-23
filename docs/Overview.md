# Overview and Game Rules



## Project Summary

This project extends the Tank Game system with a **multithreaded simulator** that can run several games concurrently in two distinct modes:

- **Comparative Mode** - Runs multiple GameManager implementations on a single map with two algorithms, to compare the behavior of different GameManagers.
- **Competitive Mode** - Runs a tournament of multiple algorithms across multiple maps with a single GameManager, with scores aggregated into a leaderboard.

The design emphasizes:

- **Modular C++ projects** (`Simulator`, `GameManager`, `Algorithm`)
- **Dynamic loading** of `.so` files for algorithms and game managers
- **Automatic registration** of factories for discoverability
- **Safe memory management** using smart pointers (`unique_ptr` preferred)
- **Threaded execution** with user-controlled parallelism

---
## Algorithm Implementation

The algorithm implementation consist two elements: `TankAlgorithm` and `Player.`

### Tank Algorithm

Each tank of the algorithm is driven by a decision system that controls movement, shooting, and reactions to battlefield events.

In our implementation, we designed a tank that actively pursues the closest enemy using a BFS search. Its behavior follows these rules:
- Recomputes the path to the nearest enemy every five turns.
- Fires when the opponent is directly in its line of sight.
- Evades incoming shells.
- Shares internal state through the `BattleInfo` object.

### Player

The Player manages its tanks and processes satellite battlefield data.

On `GetBattleInfo`, the `SatelliteView` is passed to the `Player`, which updates each tank’s `BattleInfo`.
No pointers or references to the `SatelliteView` or `BattleInfo` are retained beyond their allowed lifetime.

In our implementation, we created an **Observant player**: 
- The player always reveals all the enemy tanks to his tanks, allowing them to choose thier target on their own.
- The player passes the `Battleinfo` as is.

---
GameManager Implementation
---
## Automatic Registration

The project uses automatic registration mechanism to allow `Simulator` to discover all `Player`, `TankAlgorithm` and `GameManager` implementations dynamically.

Each component type (Player, TankAlgorithm, GameManager) has a registration struct and a macro provided in the `common/` headers.

In the `Simulator` folder, we implemented **Registrar mechanism** to dynamically manage all loadable components: tanks algorithms, players and game managers.


### Registration Macros
- `REGISTER_TANK_ALGORITHM(MyTankAlgo)`  
- `REGISTER_PLAYER(MyPlayer)`  
- `REGISTER_GAME_MANAGER(MyGameManager)`   

These macros automatically connect new implementations to the registrar during `.so` loading.

### Usage Flow
1. Simulator loads an `.so` file (via `dlopen`).
2. Inside the `.so`, the macros `REGISTER_TANK_ALGORITHM`, `REGISTER_PLAYER`, or `REGISTER_GAME_MANAGER` are invoked, which make them to register themselves.
3. The registrar validates the registration.  
   - If valid → entry is kept and factories are available to the simulator.  
   - If invalid → the entry is discarded and an exeption is raised.
4. The simulator uses the factories to create algorithm and player instances for gameplay.

This mechanism enables **modular, plug-and-play algorithms, players and game managers** without changing simulator code.

---

## Threading Model

The simulator supports parallel execution of tank games through threading.
- Controlled via `num_threads` optional argument.
- Single-threaded if `num_threads = 1` or if it's missing.
- Multithreaded execution when `num_threads ≥ 2`, with main thread coordinating worker threads.
- Threads run simulations in parallel; results are aggregated safely.
- Locks are avoided when possible, but must be used when required.

---

## Game Elements

| Symbol | Meaning                                          |
|--------|--------------------------------------------------|
| `' '`  | Empty tile (corridor)                            |
| `'#'`  | Wall                                             |
| `'@'`  | Mine (destroys tank on contact)                  |
| `'1'`  | Tank belonging to Player 1 (starts facing left)  | 
| `'2'`  | Tank belonging to Player 2 (starts facing right) |
| `'*'`  | Artillery shell in mid-air                       |
| `'%'`  | Requesting tank (only in SatelliteView)          |
| `'&'`  | Out-of-bounds (SatelliteView only)               |

---

## Game Rules

- Game steps are limited by the `MaxSteps` parameter.
- Each tank has a fixed number of shells, defined by `NumShells`.
- Tanks begin with cannon directions: Player 1 → Left, Player 2 → Right.
- A player with no tanks at the game starts loses immediately.
- If both players have no tanks, the game ends in a tie.

### Tanks can be destroyed by:
- Hitting a mine
- Getting shot by an enemy shell
- Colliding with another tank

### Game ends when:
- One player remains
- All tanks are destroyed
- All tanks run out of ammo
- Maximum steps reached

---

[Back to Table of Contents](../README.md)