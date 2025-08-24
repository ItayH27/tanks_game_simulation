# Comparative Simulator

The **ComparativeSimulator** runs all GameManagers in a given folder on a single map with two specified algorithms. It dynamically loads `.so` files, manages threading, and outputs results grouped by identical outcomes.

---

## Initialization & Cleanup

- Constructor validates registrars (`AlgorithmRegistrar`, `GameManagerRegistrar`) and sets up logging.
- Destructor clears results, resets algorithms, clears registrars, and safely `dlclose`s all `.so` handles.

---

## Main Workflow

### `run(mapPath, gmFolder, algorithmSoPath1, algorithmSoPath2)`
1. Reads and validates the map.  
2. Loads algorithm `.so` files and verifies factories.  
3. Collects all GameManager `.so` paths.  
4. Runs all games, possibly multi-threaded.  
5. Writes results to `comparative_results_<timestamp>.txt`.  

## CLI Usage

Run all GameManagers in a folder on a single map with two algorithms:

    ./simulator_<ids> -comparative \
      game_map=<map.txt> \
      game_managers_folder=<gm_folder> \
      algorithm1=<algo1.so> \
      algorithm2=<algo2.so> \
      [num_threads=<N>] [-verbose] [-logger] [-debug]

- **game_map** – Path to the input map file.  
- **game_managers_folder** – Folder containing compiled GameManager `.so` files.  
- **algorithm1/algorithm2** – Paths to Algorithm `.so` files. They may point to the same file.  
- **num_threads** *(optional)* – Number of threads for execution (default 1).  
- **-verbose** *(optional)* – Enables detailed logs.
- **-logger** *(optional)* - Enable logging.
- **-debug** *(optional)* - Enable debug logging.
---

[Back to Table of Contents](../README.md)
