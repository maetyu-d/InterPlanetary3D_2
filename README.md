# Dead Cube World

A bleak voxel-based, local 2 player duel inspired by Minecraft, but also WWI trench warefare, drone warfare in Ukraine, British government leafleting on domestic atomic bomb shelters, and the opening sequence of David Lynch's Eraserhead, all rendered with GLFW and modern OpenGL. The world is a six-faced cube planet under a low burning sky, with monochrome terrain, heavy ash fog, rare electric-blue fuel seams, toxic-yellow plutonium pockets, a dying scorched sun, and an orbiting satellite with a live downward camera feed.

## Build and Run

```sh
cmake -S . -B build
cmake --build build
./build/cube_world
```

## Controls

The game runs in local two-player split-screen. Player 1 starts on the top face; player 2 starts on the opposite face. Each half of the screen has its own satellite feed.

### Player 1

- `WASD` to move
- Mouse to look
- Look direction controls the headlamp
- `Space` to jump
- Hold `Left Shift` to sprint
- Hold left click to mine a block
- Hold right click to build against the highlighted face
- The mining/building tool damages the other player at close range
- `1-2` to select normal or hard building blocks
- `M` to cycle mine/build, missile, and atomic missile modes
- Left click in missile or atomic missile mode to fire; after one second, the top-right feed switches to the missile nose camera
- Rockets cost 1 fuel and 1 plutonium; atomic rockets cost 5 fuel and 5 plutonium
- Mine fuel/plutonium or blast resource pockets to collect resources
- `O` to cycle the satellite between current-face, opposite-face, and polar orbits
- `-` and `=` to lower/raise the satellite orbit
- `T` to cycle dead, desert, and lush green worlds
- `R` to regenerate the world
- `F1` to release/capture the mouse
- `Esc` to quit

### Player 2

- `IJKL` to move
- Arrow keys to look
- `Enter` to jump
- Hold `Right Shift` to sprint
- `P` to cycle mine/build, missile, and atomic missile modes
- `Right Control` to fire the selected missile type
- In mine/build mode, `Right Control` swings the tool at close range
