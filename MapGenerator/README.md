# MapGen & MapDraw

## Overview
This folder contains two standalone utilities that extend the Tanks Game project:

1. **mapgen** - Random Map Generator  
   Generates valid Assignment-2/3 map files with customizable parameters.

2. **mapdraw** - Map Visualizer  
   Renders existing map files either as ASCII (with optional colors) or as a BMP image.

Both tools are independent of the Simulator/GameManager/Algorithm builds.

---

## Build
```bash
cd MapGenerator
cmake -B build
cmake --build build -j
```

## Map Generator (mapgen)
### Usage:
```bash
./mapgen [options]
```
### Options:
- `--rows <N> `/ `--cols <N>` - grid size
- `--max-steps <N>` - value for MaxSteps in header
- `--num-shells <N>` - value for NumShells in header
- `--tanks <N>` / `--tanks2 <N>` - number of tanks for each player
- `--p-wall <0..1>` - probability of placing a wall #
- `--p-mine <0..1>` - probability of placing a mine @
- `--no-border-walls` - disable automatic border walls
- `--seed <N>` - random seed for reproducibility
- `--name <string>` - map name/description (line 1 of file)
- `--out <file>` - output file path (default: random_map.txt)

### Example:
```bash
./mapgen --rows 25 --cols 60 --max-steps 6000 --num-shells 30 \
         --tanks1 3 --tanks2 3 --p-wall 0.08 --p-mine 0.02 \
         --name "Medium Arena" --out medium_arena.txt
```

## Map Draw (mapdraw)
### Usage:
```bash
./mapdraw --in <map.txt> [--ascii] [--bmp <out.bmp>] [--cell <N>] [--no-color]
```

### Options:
- `--in <map.txt>` - input map file (Assignment-2 format)
- `--ascii` - render to terminal using ASCII (default on)
- `--no-color` - disable ANSI colors in ASCII output
- `--bmp <file.bmp>` - export to 24-bit BMP image
- `--cell <N>` - pixel size per cell in BMP (default: 16)

### Examples:
```bash
// ASCII preview with colors:
./mapdraw --in medium_arena.txt --ascii

// ASCII preview without colors:
./mapdraw --in medium_arena.txt --ascii --no-color

// Export to BMP (12×12 pixels per cell):
./mapdraw --in medium_arena.txt --bmp arena.bmp --cell 12
```

## Notes:
- Both tools strictly follow the Assignment-2/3 map file format:
    - Lines 1-5 are header (name, MaxSteps, NumShells, Rows, Cols).
    - Grid starts at line 6 and is padded/truncated to match declared Rows × Cols.
    -Legal characters: space (empty), # (wall), @ (mine), 1 (tank P1), 2 (tank P2).
- mapgen and mapdraw are standalone and do not depend on the Simulator.

---

[Back to Table of Contents](../README.md)
