# Command-Line Parser (CmdParser)

The **CmdParser** normalizes, validates, and extracts simulator arguments for the two required modes — **comparative** and **competition** — enforcing the assignment’s rules on required keys, file/folder validity, optional flags, and error reporting.

---

## Responsibilities

- **Normalization**: Accepts `key=value`, `key = value`, and split‐token `key`, `=`, `value` forms; recognizes switches `-comparative`, `-competition`, `-verbose`, `-logger[=path]`, `-debug`.  
- **Validation**: Ensures exactly one mode is chosen; checks **required keys**, detects **duplicates**, rejects **unsupported keys**, verifies **files/folders**, and parses `num_threads` as a positive integer.
- **Result**: Returns a `ParseResult` with mode, paths, flags, `numThreads`, and an error message when invalid.

---

## Key Sets by Mode

- **Comparative**: `game_map`, `game_managers_folder`, `algorithm1`, `algorithm2`, `[num_threads]`.
- **Competition**: `game_maps_folder`, `game_manager`, `algorithms_folder`, `[num_threads]`.

Unknown keys are reported as **Invalid argument**; missing keys are reported as **Missing required argument**.

---

## Normalization Details

- Trims whitespace around keys/values.  
- Collects `k=v` regardless of spacing.  
- Tracks **unsupported tokens** and **duplicate keys** for consolidated error output.  
- Flags:
  - `-verbose` → `ParseResult.verbose = true`
  - `-logger` or `-logger=<path>` or `-logger = <path>` → `enableLogging` and optional `logFile`
  - `-debug` → `debug = true`
- `num_threads`: digits‐only, `> 0`; defaults to `1` when absent. Invalid forms fail parsing.

---

## Validation Flow

1. **Mode**: Exactly one of `-comparative` or `-competition`.  
2. **Collect errors**: duplicates, unsupported tokens, missing required keys, bad `num_threads`.  
3. **Filesystem checks**:  
   - Files must exist & be regular files (`game_map`, `algorithm1`, `algorithm2`, `game_manager`).  
   - Folders must exist & be non‐empty (`game_managers_folder`, `game_maps_folder`, `algorithms_folder`). 
4. **ParseResult**: filled on success; otherwise `.valid = false` with aggregated messages.
---

## Notes & Conventions

- The parser’s required keys and run modes mirror the assignment’s specification exactly. 
- Factories and registration macros (`REGISTER_*`) exist in **common/** headers and are enforced later by the simulator; the parser only validates CLI inputs.

---

[Back to Table of Contents](../README.md)
