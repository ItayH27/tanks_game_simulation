# MapCheck

## Overview
`mapcheck` is a **standalone map validator, normalizer, and statistics tool** for the Tanks Game.

It verifies that a map file strictly follows the **Assignment 2 map file format**:

- **Header (first 5 lines):**
  1. Map name / description  
  2. `MaxSteps = <NUM>`  
  3. `NumShells = <NUM>`  
  4. `Rows = <NUM>`  
  5. `Cols = <NUM>`  

- **Grid (from line 6):**
  Exactly `<Rows>` lines × `<Cols>` columns.  
  Only legal characters allowed:
  - space → empty cell  
  - `#` → wall  
  - `@` → mine  
  - `1` → tank (player 1)  
  - `2` → tank (player 2)  

If the file is malformed, `mapcheck` prints detailed errors and warnings.

---

## Features
- **Validation**: Ensures headers are present and parseable, grid dimensions are correct, and symbols are legal.
- **Warnings**: Detects edge cases like zero tanks for a player (immediate loss/tie at game start).
- **Strict mode (`--strict`)**: Fails if the raw file doesn’t already have exactly `<Rows>` lines of exactly `<Cols>` characters.
- **Normalization (`--write-normalized <out.txt>`)**: Writes a repaired map file where rows/cols are padded or trimmed to match the header.
- **JSON output (`--json`)** *(bonus)*: Prints a machine-readable validation report instead of human text.
- **Statistics mode (`--stats`)**: Analyze one or many maps and report counts and percentages of walls, mines, empty spaces, and tanks.
- **CSV output (`--csv`)**: When combined with `--stats`, prints results in CSV format (to stdout; redirect to file to save).

---

## Build
```bash
cd mapcheck
cmake -B build
cmake --build build -j
```

## CLI Usage

### Validation mode (single file)
Validate a single map file:

```bash
cd build
./mapcheck <map.txt> [--strict] [--json] [--write-normalized <out.txt>]
```

### Statistics mode
Analyze one or more map files for content density:

```bash
cd build
./mapcheck --stats <map1.txt> [<map2.txt> ...] [--csv > <stats.csv>]
```

## Notes
- Normalized maps created with mapcheck are fully valid Assignment 2 map files and can be used directly in the simulator.
- Use --stats --csv to quickly build a dataset of map characteristics for analysis.
