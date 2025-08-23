# Overview and Game Rules



## Project Summary

This project extends the Tank Game system with a **multithreaded simulator** that can run several games concurrently in two distinct modes:

- **Comparative** - Runs multiple GameManager implementations on a single map with two algorithms, to compare the behavior of different GameManagers.
- **Competitive** - Runs a tournament of multiple algorithms across multiple maps with a single GameManager, with scores aggregated into a leaderboard.

The design emphasizes:

- **Modular C++ projects** (`Simulator`, `GameManager`, `Algorithm`)
- **Dynamic loading** of `.so` files for algorithms and game managers
- **Automatic registration** of factories for discoverability
- **Safe memory management** using smart pointers (`unique_ptr` preferred)
- **Threaded execution** with user-controlled parallelism

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

## Threading Model
- Controlled via `num_threads` argument.
- Single-threaded if `num_threads` missing or =1.
- Multithreaded execution when ≥2, with main thread coordinating worker threads.
- Threads run simulations in parallel; results are aggregated safely.
- Locks are avoided when possible, but must be used when required.

---

[Back to Table of Contents](../README.md)