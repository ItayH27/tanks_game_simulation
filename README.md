# Tank Game Simulator - Advanced Topics in Programming Final Project

A Multi-Threaded Tank Game Simulator in C++ that dynamically loads game managers and algorithms .so files to run a large scale comparative and competitive battles in parallel. The game follows strict interface and file format requirements from the TAU HW3 specification.

---

## Contributors

- Maxim German  - 322542887
- Itay Hazan - 209277367

---

## Table of Contents

- [Overview and Game rules](docs/Overview.md)
- [Building and Running the Game](docs/BuildGuide.md)
- [Testing](docs/Testing.md)
- [Using Docker Container](docs/Docker.md)

---

## Project Structure

- `Simulator/` - C++ implementation of the multi-threaded simulator that runs multiple tank games in parallel.
- `Algorithm/` - C++ implementation of player logic and tank decision-making algorithms. 
- `GameManager/` - C++ implementation of the game manager, responsible for executing a single tank game.
- `common/` – Provided interfaces from the assignment (e.g. `TankAlgorithm`, `Player`, `SatelliteView`, `AbstractGameManager`); do not modify.
- `UserCommon/` -Shared code and interfaces developed by us, used across the Simulator, Algorithm, and GameManager projects.
- `CMakeLists.txt` – Root CMake configuration file for building the entire project, delegates to sub-project CMake files in `Simulator/`, `Algorithm/` and `GameManager/`.
- `tests/` – Unit tests written using Google Test to verify correctness of the modules.
- `docs/` - Folder containing all .md files
- `students.txt` - Text file with the contributors information.
