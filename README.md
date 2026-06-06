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

## Project Structure

```
conquest/
├── CMakeLists.txt        # Build configuration
├── README.md             # This file
├── .gitignore            # Ignored files
└── src/
    ├── core.h            # Core types, hex math, constants, Zobrist hashing
    ├── board.h           # Board API
    ├── board.cpp         # Map generation, hex queries, pathfinding
    ├── game.h            # Game logic API
    ├── game.cpp          # Influence, combat, moves, turns, tech, victory
    ├── ai.h              # AI API
    ├── ai.cpp            # Alpha-beta + quiescence + transposition table
    ├── renderer.h        # Renderer API
    ├── renderer.cpp      # SDL2+OpenGL renderer, bitmap font, procedural unit shapes
    └── main.cpp          # Entry point, event loop, game application
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
