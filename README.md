# CONQUEST

A deterministic hex-based strategy game with perfect information and zero randomness.

## Overview

CONQUEST is a two-player turn-based strategy game played on a hexagonal grid. Every match is fully deterministic — no random number generation, no hidden information. Victory comes from superior strategy alone.

## Game Design

- **Hex Board**: 127-tile hexagonal grid (radius 6) with symmetric map generation
- **5 Unit Types**: Scout, Soldier, Fortress, Cannon, Commander — plus immovable HQ
- **Tech Tree**: 20 unique upgrades, draft 10 per game, research with energy
- **Victory**: Control 7 of 9 victory nodes, or destroy the enemy HQ
- **AI Opponent**: Alpha-beta search with iterative deepening, quiescence, and transposition table

## Core Mechanics

### Units
| Unit | Cost | Move | Attack | Defense | HP | Range | Special |
|------|------|------|--------|---------|-----|-------|---------|
| Scout | 2 | 4 | 1 | 0 | 2 | 1 | Fast recon |
| Soldier | 3 | 2 | 2 | 2 | 4 | 1 | Forms formations |
| Fortress | 4 | 0 | 0 | 4 | 8 | 0 | Immobile defense |
| Cannon | 5 | 1 | 4 | 1 | 3 | 3 | Ranged, min range 2, LOS required |
| Commander | 6 | 2 | 2 | 2 | 5 | 1 | Buffs nearby allies |
| HQ | — | 0 | 0 | 0 | 20 | 0 | Immovable, spawns units |

### Terrain
- **Plains**: Normal movement
- **Mountain**: Impassable, blocks cannon LOS
- **Water**: Impassable, does not block LOS
- **High Ground**: +1 range, +1 defense
- **Victory Node**: Capture to win

### Formations
- **Line** (3 soldiers in a row): +1 defense
- **Triangle** (3 soldiers in triangle): +1 attack

### Isolation
Units disconnected from their HQ suffer -2 attack, -2 defense, -1 move range.

### Influence & Territory
Units and HQs project influence. Tiles are owned by the player with higher influence. Victory nodes are controlled through influence.

### Phases
1. **Draft**: Each player picks 10 tech upgrades
2. **Research**: Spend energy to unlock drafted techs
3. **Action**: Move, attack, capture, fortify, spawn units

## Building

### Prerequisites
- C++17 compiler (GCC 7+, Clang 5+)
- CMake 3.16+
- SDL2 development libraries
- OpenGL (Mesa or vendor)

### Build Instructions
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### Running
```bash
./conquest          # default seed (18462)
./conquest 42       # custom seed
```

## Controls

| Key | Action |
|-----|--------|
| Left Click | Select unit / Execute move |
| Right Click | Cancel selection |
| Middle Click + Drag | Pan camera |
| Mouse Wheel | Zoom in/out |
| Enter | End turn / Confirm phase |
| F | Fortify selected unit |
| R | Toggle research mode |
| A | Toggle AI opponent |
| 1-5 | Select spawn unit type (when HQ selected) |
| N | New game (same seed) |
| H | Show help |
| Escape | Quit |

## Training Mode (Headless Self-Play)

CONQUEST includes a headless training mode for ML model training. It runs AI vs AI self-play games, collecting `(state, policy, value)` tuples for reinforcement learning or supervised learning.

### Building

```bash
mkdir build && cd build
cmake .. -DBUILD_GUI=OFF   # Headless only (no SDL2 needed)
# OR
cmake ..                    # Build both GUI and headless
make -j$(nproc)
```

This produces `conquest_train` (always built, no dependencies) and optionally `conquest` (GUI, requires SDL2+OpenGL).

### Running Self-Play

```bash
# Basic: 100 episodes, AI depth 3
./conquest_train --episodes 100 --depth 3 --output-dir ./training_data

# Fast: 1000 episodes, depth 2, shorter time limit
./conquest_train --episodes 1000 --depth 2 --time-limit 500 --output-dir ./data

# All options
./conquest_train --help
```

### Command-Line Options

| Option | Default | Description |
|--------|---------|-------------|
| `--episodes N` | 100 | Number of self-play games |
| `--depth N` | 3 | AI search depth |
| `--time-limit N` | 1000 | AI time limit per move (ms) |
| `--temperature F` | 1.0 | Policy temperature (higher = more random) |
| `--no-priors` | off | Use uniform policy instead of heuristic priors |
| `--max-turns N` | 200 | Max turns before declaring a draw |
| `--seed N` | 1 | Starting map seed |
| `--output-dir DIR` | ./training_data | Output directory |
| `--single-file` | off | Write all data to one file |
| `--verbose` | off | Print per-episode details |
| `--no-dedup` | off | Disable position deduplication |
| `--sequential-draft` | off | Sequential (non-random) tech draft |

### Data Format

Each episode file contains binary `(state, policy, value)` tuples:

- **State features**: 3568 floats (28 channels × 127 hexes + 12 global features)
- **Policy**: 1018 floats (move/attack/spawn/capture/fortify/end-turn probabilities)
- **Value**: 1 float (game outcome from current player's perspective: +1 win, -1 loss, 0 draw)

### Loading Data in Python

```python
from train_loader import ConquestDataset, ConquestTorchDataset

# Load all episodes from a directory
dataset = ConquestDataset("training_data/")
features = dataset.get_features_array()  # (N, 3568) numpy array
policies = dataset.get_policy_array()     # (N, 1018) numpy array
values = dataset.get_value_array()        # (N,) numpy array

# Use with PyTorch
from torch.utils.data import DataLoader
torch_dataset = ConquestTorchDataset("training_data/")
loader = DataLoader(torch_dataset, batch_size=64, shuffle=True)
for batch in loader:
    features = batch["features"]  # (B, 3568)
    policy   = batch["policy"]    # (B, 1018)
    value    = batch["value"]      # (B,)
```

### Feature Encoding

The state is encoded as an AlphaZero-style channel layout:

**Per-hex channels (127 hexes × 28 channels):**
- Ch 0-4: Terrain one-hot (plains, mountain, water, high_ground, victory)
- Ch 5: Elevation
- Ch 6-7: Influence (P1, P2, normalized)
- Ch 8-9: Ownership (P1, P2, binary)
- Ch 10-16: P1 unit type one-hot
- Ch 17-23: P2 unit type one-hot
- Ch 24-25: Unit HP ratio (P1, P2)
- Ch 26-27: Unit AP (P1, P2, normalized)

**Global features (12 floats):**
- Current player, turn, energy, victory nodes, tiles, units, techs

### Policy Encoding

The policy vector has 1018 slots covering all possible actions:
- Slots 0-126: Move to hex
- Slots 127-253: Attack hex
- Slots 254-888: Spawn (5 unit types × 127 hexes)
- Slots 889-1015: Capture hex
- Slot 1016: Fortify
- Slot 1017: End turn

## Project Structure

```
conquest/
├── CMakeLists.txt        # Build configuration (GUI + headless targets)
├── README.md             # This file
├── .gitignore            # Ignored files
├── tools/
│   └── train_loader.py   # Python data loader (numpy + PyTorch)
└── src/
    ├── core.h            # Core types, hex math, constants, Zobrist hashing
    ├── board.h           # Board API
    ├── board.cpp         # Map generation, hex queries, pathfinding
    ├── game.h            # Game logic API
    ├── game.cpp          # Influence, combat, moves, turns, tech, victory
    ├── ai.h              # AI API
    ├── ai.cpp            # Alpha-beta + quiescence + transposition table
    ├── state_encoder.h   # ML feature extraction API
    ├── state_encoder.cpp # State→tensor, move→policy slot encoding
    ├── trainer.h         # Training mode API
    ├── trainer.cpp       # Self-play loop, data collection
    ├── train_main.cpp    # Headless entry point (conquest_train)
    ├── renderer.h        # Renderer API (GUI only)
    ├── renderer.cpp      # SDL2+OpenGL renderer, bitmap font (GUI only)
    └── main.cpp          # GUI entry point (conquest)
```

## Architecture

The codebase follows a clean separation of concerns:
- **core.h**: All type definitions, constants, and hex math — zero dependencies
- **board**: Map generation and spatial queries
- **game**: Rules engine — move generation, validation, application
- **ai**: Search algorithm with eval function
- **renderer**: Pure rendering, no game logic
- **main**: Application glue — input handling, game flow, UI state

## License

All rights reserved.
