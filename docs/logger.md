# Logger

A lightweight logging utility for the **Tank Game Simulator**.  
Supports multiple log levels (`info`, `warn`, `error`, `debug`) and can be enabled or disabled via command line flags.

## Features

- **Thread-safe** - safe to call from multiple worker threads in the simulator.  
- **Configurable verbosity**:
  - Enable with `-logger`
  - Show extra diagnostic details with `-debug`
- **Log levels**:
  - `info` - lifecycle events and major milestones (simulation start/end, results written, winners).
  - `warn` - recoverable issues (e.g., failed to create a single `GameManager`, fallback to stdout).
  - `error` - hard failures that prevent progress (map load failure, `.so` load failure).
  - `debug` - detailed per-thread and per-file information for development and troubleshooting.

## Example 
```bash
2025-08-24 07:59:26.351 INFO  [tid 140737466611648] Starting competitive simulation...
2025-08-24 07:59:26.353 DEBUG [tid 140737466611648] Loading GameManager from: /workspace/tanks_game_simulation 1_gm_so/GameManager_test_209277367_322542887.so
2025-08-24 07:59:26.357 INFO  [tid 140737466611648] Successfully loaded GameManager: GameManager_test_209277367_322542887
```

## CLI
- `-logger` - enable logging.
- `-debug` - enable debug level logs in addition to info, warn, error.
- **Note:** Without the `-logger` flag no logging will be performed. This includes debugging (even if `-debug` present).

--- 

[Back to Table of Contents](../README.md)
