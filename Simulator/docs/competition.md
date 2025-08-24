# Competitive Simulator

The **CompetitiveSimulator** runs a single GameManager across all maps in a folder with multiple algorithms. It dynamically loads `.so` files, schedules round‑robin matchups, executes games concurrently, and writes a ranked scoreboard.

---

## Initialization & Cleanup

- Constructor wires registrars (`AlgorithmRegistrar`, `GameManagerRegistrar`) and configures logging.
- Destructor safely closes all loaded `.so` handles (algorithms and GameManager), clears registrars, and releases internal caches.

---

## Main Workflow

### `run(mapsFolder, gameManagerSoPath, algorithmsFolder)`
1. Loads the chosen **GameManager** `.so` and captures its factory.  
2. Indexes all **Algorithm** `.so` files in `algorithmsFolder` (requires **≥2**).  
3. Scans `mapsFolder` for map files.  
4. **Schedules** games using the assignment’s round‑robin scheme (deduplicating symmetric pairs for even N).  
5. Runs all scheduled games (sequentially or with a thread pool).  
6. Writes results to `competition_<timestamp>.txt`, or prints to stdout if the file can’t be opened.

---

## Scheduling

- For map `k`, pair algorithms `i` vs. `(i + 1 + k % (N-1)) % N`.  
- Records each unique unordered pair exactly once per map (no mirrored duplicates when `N` is even at the “middle” round).  
- Tracks per‑algorithm usage to load/unload `.so` files lazily and safely.

---

## Execution

- **Lazy loading**: `ensureAlgorithmLoaded(name)` loads an algorithm `.so` and validates that both Player and TankAlgorithm factories registered.  
- **Per‑game**: creates Players from factories, runs the loaded GameManager on the map, updates the global score table, and decrements usage counts (unloading when no longer needed).  
- **Threading**: uses up to `num_threads` workers; single‑thread if omitted or `1`.

---

## Scoring

- **Win = 3**, **Tie = 1** (to both participants), **Loss = 0**.  
- Final output lists algorithms **sorted by total score (desc)**.

---

## CLI Usage

Run one GameManager across all maps with N algorithms:

    ./simulator_<ids> -competition \
      game_maps_folder=<maps_dir> \
      game_manager=<gm.so> \
      algorithms_folder=<algos_dir> \
      [num_threads=<N>] [-verbose] [-logger] [-debug]

- **game_maps_folder** – Folder containing map files.  
- **game_manager** – Path to the GameManager `.so`.  
- **algorithms_folder** – Folder containing ≥2 Algorithm `.so` files.  
- **num_threads** *(optional)* – Max worker threads (default 1).  
- **-verbose** *(optional)* – Extra simulator and GM output.  
- **-logger** *(optional)* – Enable logging.  
- **-debug** *(optional)* – Enable debug‑level logging.

**Output:**  
- `competition_<timestamp>.txt` written to `algorithms_folder`; falls back to stdout on failure.

---

[Back to Table of Contents](../README.md)
