# Simulator Implementation

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
