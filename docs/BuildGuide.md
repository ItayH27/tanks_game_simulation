# Building and Running the Game

--- 

## Requirements
- CMake 3.14 (or greater)
- C++20-compatible compiler (e.g. g++)

## Build instructions
1. Create the build directory inside the root directory:
```bash
cd 322542887_209277367
cmake -B build
```

2. From root directory, run Cmake:
```bash
cmake --build build .
```
- This will create the following files:
    - `simulator_209277367_322542887` - Main game executable (used to run the simulator) in the root directory
    - `Algorithm_209277367_322542887.so` - Shared library for the algorithm in `Algorithm` directory
    - `GM_209277367_322542887.so` - Shared library for the algorithm in `GameManager` directory
    - (**Note:** on windows run executables with .exe)


- (GoogleTest will be automatically downloaded and built via FetchContent)

## Running the simulator

Running the simulator in **comparative mode** from root directory:
```bash
./simulator_209277367_322542887 -comparative
game_map=<game_map_filename> 
game_managers_folder=<game_managers_folder> 
algorithm1=<algorithm_so_filename>
algorithm2=<algorithm_so_filename> 
[num_threads=<num>] [-verbose]
```

Running the simulator in **competitive mode** from root directory:
```bash
./simulator_209277367_322542887 -competition
game_maps_folder=<game_maps_folder>
game_manager=<game_manager_so_filename>
algorithms_folder=<algorithms_folder>
[num_threads=<num>] [-verbose]
```
There are two optional arguments for each of the modes:
- `num_threads` - sets the number of threads for the simulation as follows:
    - If the argument is missing or if `num_threads = 1`, the program will use a **single thread** (the main thread).
    - If `num_threads  >= 2`, the program will interpret it as the **requested number of threads** for running the actual simulation in addition to the main thread.
    - Above means that the **total number of threads will never be 2**
    - **Note:** exact number of threads may be lower than requested in the command line, in the case there is no way to properly utilize the required number of threads.

- `-verbose` - when this flag is provided, each `GameManager` creates detailed output files (same format as Assignment 2).  
  Without this flag, only the simulator’s summary results file is generated.


## Outputs

The format of the output depends on the selected mode:

- **Comparative mode** - `comparative_results_<time>.txt` is created in the `game_managers_folder`.  
  Contains results of running all GameManagers on the same map with two algorithms.

- **Competitive mode** -`competition_<time>.txt` is created in the `algorithms_folder`.  
  Contains results of a tournament between all algorithms on all maps.


**GameManager outputs (only with `-verbose`)**  will be created in the root directory.

---

[Back to Table of Contents](../README.md)