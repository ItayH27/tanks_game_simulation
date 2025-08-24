# Tank Game Simulator

**Simulator**: a multithreaded framework that runs multiple Tank Games concurrently in two modes — **comparative** and **competitive**.

## Threading

- `num_threads` ≥ 2 spawns worker threads in addition to main.
- Single-thread if omitted or set to 1.
- Simulator avoids opening idle threads.
- Designed for safe concurrency;

---

## Map Format

A valid map file contains:
1. Map name  
2. `MaxSteps=<int>`  
3. `NumShells=<int>`  
4. `Rows=<int>`  
5. `Cols=<int>`  
6. Grid of `Rows × Cols` with characters: `# @ 1 2` and space.

Parsing:
- Extra rows/columns ignored with recovery logs.
- Unknown chars treated as space.
- Errors written to `input_errors.txt`.

---

## Dynamic Loading

- `.so` files are loaded with `dlopen`.  
- **Automatic Registration** via macros:
  - `REGISTER_GAME_MANAGER(Class)`
  - `REGISTER_PLAYER(Class)`
  - `REGISTER_TANK_ALGORITHM(Class)`

Factories register themselves on load, enabling the Simulator to construct instances dynamically.

---

## Documentation Links

- [Comparative Simulator](./docs/comparative.md)  
- [Competitive Simulator](./docs/competition.md)  
- [Command-Line Parser](./docs/parser.md)  

---

[Back to Table of Contents](../README.md)