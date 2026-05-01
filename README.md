# Dead Cube World

A bleak C++ voxel sandbox inspired by Minecraft, rendered with GLFW and modern OpenGL. The world is a six-faced cube planet under a low burning sky, with monochrome terrain, heavy ash fog, rare electric-blue fuel seams, toxic-yellow plutonium pockets, a dying scorched sun, and an orbiting satellite with a live downward camera feed.

## Build and Run

```sh
cmake -S . -B build
cmake --build build
./build/cube_world
```

## Controls

- `WASD` to move
- Mouse to look
- Look direction controls the headlamp
- `Space` to jump
- Hold `Left Shift` to sprint
- Hold left click to mine a block
- Hold right click to build against the highlighted face
- `1-2` to select normal or hard building blocks
- `O` to cycle the satellite between current-face, opposite-face, and polar orbits
- `R` to regenerate the world
- `F1` to release/capture the mouse
- `Esc` to quit
