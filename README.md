# Tank Game Simulator - Advanced Topics in Programming Final Project

A Multi-Threaded Tank Game Simulator in C++ that dynamically loads game managers and algorithms .so files to run a large scale comparative and competitive battles in parallel. 

---

## Contributors

- Maxim German  - 322542887
- Itay Hazan - 209277367

---

## Table of Contents

- [Overview and Game rules](docs/Overview.md)
    - [Algorithm Implementation](Algorithm/README.md)
    - [GameManager Implementation](GameManager/README.md)
    - [Simulators Implementation](Simulator/README.md)
- [Building and Running the Game](docs/BuildGuide.md)
- Bonuses:
    - Dynamic loading and unloading of .so files (in both comparative and competitive).
    - [Logger](docs/logger.md)
    - [Automatic tesitng (via Gtest)](tests/README.md)
    - [Using Docker Container](docs/Docker.md)
    - [Map Genarator](MapGenerator/README.md)
    - [Map Checker](MapChecker/README.md)

---

## Project Structure

- `Simulator/` - Implementation of the multi-threaded simulator that runs multiple tank games in parallel.
- `Algorithm/` - Implementation of player logic and tank decision-making algorithms. 
- `GameManager/` - Implementation of the game manager, responsible for executing a single tank game.
- `common/` – Provided interfaces from the assignment (e.g. `TankAlgorithm`, `Player`, `SatelliteView`, `AbstractGameManager`); 
- `UserCommon/` -Shared code and interfaces developed by us, used across the Simulator, Algorithm, and GameManager projects.
- `CMakeLists.txt` – Root CMake configuration file for building the entire project, delegates to sub-project CMake files in `Simulator/`, `Algorithm/` and `GameManager/`.
- `tests/` – Unit tests written using Google Test to verify correctness of the modules.
- `docs/` - Folder containing all .md files.
- `MapChecker/` - Contains tools for validating and analyzing map files (checking dimensions, symbols, formatting, and statistics).
- `MapGenrator/` - Contains utilities for procedurally generating valid game maps, with customizable parameters (rows, columns, shells, etc.).
- `students.txt` - Text file with the contributors information.
