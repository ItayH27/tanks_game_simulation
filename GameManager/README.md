# GameManager — `GM_209277367_322542887`

Single-game engine for the **Tank Game (Assignment 3)**.  
Implements the full game loop (init → per-turn actions → termination), shell physics, collision rules, logging, and colored board rendering. The class is auto-registered so your Simulator can `dlopen` it and construct via a factory.

---

## TL;DR

- **API:** Implements `AbstractGameManager::run(...)` as required, including width/height, a read-only `SatelliteView` snapshot, players, and tank factories. :contentReference[oaicite:0]{index=0}  
- **Packaging:** Build as a shared object named exactly `GameManager_209277367_322542887.so` inside a `GameManager/` project. :contentReference[oaicite:1]{index=1}  
- **Auto-registration:** Uses `REGISTER_GAME_MANAGER(GM_209277367_322542887)` so the Simulator discovers it via the registrar. :contentReference[oaicite:2]{index=2}  
- **Verbose logs:** When constructed with `verbose=true` (propagated from `-verbose` CLI), writes a per-turn log (Assignment 2 style). :contentReference[oaicite:3]{index=3}

---

## What this GameManager does

### Board & entities
- **Cells used:**  
  - Tanks: `'1'`, `'2'`  
  - Walls: `'#'` (strong), `'$'` (weakened)  
  - Mine: `'@'`  
  - Shells: `'*'` (single), `'^'` (two shells stacked)  
  - Temporary tank-damage marks while moving onto shells: `'a'` (P1), `'b'` (P2)  
  - Destroyed-on-spawn marks from shooting next cell: `'c'` (P1), `'d'` (P2)  
  - Temporary marker for `GetBattleInfo`: `'%'` (only while composing the snapshot for the query)  
  - Empty: `' '`  

### Actions & validation
- **Supported actions:** `MoveForward`, `MoveBackward`, `RotateLeft/Right 45°/90°`, `Shoot`, `GetBattleInfo`, `DoNothing`. (Matches the common enum.) :contentReference[oaicite:4]{index=4}  
- **Validity checks:**  
  - `isValidMove` rejects stepping into `#`/`$`.  
  - `isValidShoot` requires `ammo > 0` and `turnsToShoot == 0`.  
  - All other actions are considered valid.  
- **Backward move rules:** initiating backward motion sets a “waiting” window; after the delay, one backward step may occur; issuing `MoveForward` cancels the pending backward move.

### Shell model
- **Step:** Each tick, shells advance one cell in their direction (wrapping at edges).  
- **Interact:**  
  - With `#` → to `'$'`, shell removed.  
  - With `'$'` → to `' '`, shell removed.  
  - With `'@'` → shell sits “above mine”: mark `'*'`, remember “above-mine” flag.  
  - With tank (`'1'`/`'2'`) → tank destroyed, cell cleared, shell removed.  
  - With shell (`'*'`) → if opposite directions, both shells removed & cell cleared; otherwise stack to `'^'`.  
- **Cleanup:** A shell vacates its previous cell restoring what was underneath (mine, stacked shell, tank damage marker, etc.).

### Turn flow (high-level)
1. **Snapshot** current board (`lastRoundGameboard_`).  
2. **Collect actions** from alive tanks (`getTankActions`).  
3. **Execute** per tank (`performTankActions`), honoring validity and backward-move timing.  
4. **Advance shells** twice per round (`moveShells` + `checkShellsCollide` in a loop).  
5. **Log** per-tank action strings (mark “(ignored)” on invalid) and update the colored board printout (optional).  
6. **Update status:** counts per player, no-ammo tracking, game-over flags.  
7. **Terminate** on:  
   - All tanks dead;  
   - `max_steps` reached;  
   - Both sides with zero shells for a prolonged period (40 turns here).  
   Assigns `GameResult.winner`, `reason` (`ALL_TANKS_DEAD | MAX_STEPS | ZERO_SHELLS`), `remaining_tanks`, `gameState`, and `rounds`. :contentReference[oaicite:5]{index=5}

---

## Logging & colored rendering

- **Verbose file:** When `verbose_` is true, opens `output_<map>_GM_209277367_322542887_<name1>_<name2>` and writes one line per turn:
  - Alive tank: prints its chosen action; appends `(ignored)` if action was invalid.  
  - Tank killed **this** turn: prints action with `(killed)` and increments dead-turn counter.  
  - Already dead: prints `killed`.  
- **TTY colors (`printBoard`)**:
  - `'1'` bright blue, `'2'` green, `'#'` white, `'$'` gray, `'@'` red, `'*'` yellow, others default.

> Note: Assignment 3 requires GameManager output files only if `-verbose` was passed to the Simulator; the Simulator should forward this to the GM constructor. :contentReference[oaicite:6]{index=6}

---

## Integration: how the Simulator uses it

- **Binary/namespace contract**  
  - Place this project under `GameManager/`.  
  - Build a shared object named `GameManager_209277367_322542887.so`.  
  - Keep all code under namespace `GameManager_209277367_322542887`.  
  - Export a `GameManagerFactory` via **auto-registration**:  
    ```cpp
    using namespace GameManager_209277367_322542887;
    REGISTER_GAME_MANAGER(GM_209277367_322542887);
    ```
  - The Simulator loads the `.so`, the registration code injects the factory into the registrar, and the Simulator constructs via `factory(verbose)` to call `run(...)`. :contentReference[oaicite:7]{index=7}

- **Run signature (must match the common headers)**  
  ```cpp
  GameResult run(size_t map_width, size_t map_height,
                 const SatelliteView& map, std::string map_name,
                 size_t max_steps, size_t num_shells,
                 Player& player1, std::string name1,
                 Player& player2, std::string name2,
                 TankAlgorithmFactory player1_tank_algo_factory,
                 TankAlgorithmFactory player2_tank_algo_factory);

## Design choices & invariants
- Alive state values: this GM treats `getIsAlive() == 0` as alive, == 1 as killed this turn, and otherwise as dead. The logger distinguishes these states (e.g., (killed) tagging and dead-turn counting).
- Wrap-around movement: nextLocation wraps (x±dx, y±dy) modulo board size, so edges are toroidal.
- No caching of external instances: Players and tank algorithms are used as provided; creation is expected to be cheap per the assignment guidance.
- No raw new/delete in user code: Uses std::unique_ptr for shells and tank info. (The assignment discourages manual new/delete and prefers RAII.)

---

[Back to Table of Contents](../README.md)
