# InterPlanetary 3D

A bleak voxel-based, local 2 player duel inspired by Minecraft and Worms, but also WWI trench warefare, drone warfare in Ukraine, British government leafleting on domestic atomic bomb shelters, and the opening sequence of David Lynch's Eraserhead, all rendered with GLFW and modern OpenGL. Four differet planet types are available - bleak, desert, green, and brutalist, but each planet is always a 3-D cube with rare electric-blue fuel seams, toxic-yellow plutonium pockets, and an orbiting satellite with a live downward-facing camera feed. Players start on opposite faces of the planet, and there's an optional forcefield to restrict digging tactices if desired. There are also four different game modes - freeplay first to three, turn-based simultaneous, turn-based alternate, and turn-based alternate single screen.

## Screenshots

<img width="1392" height="926" alt="Screenshot 2026-05-03 at 18 43 07" src="https://github.com/user-attachments/assets/267ee69e-2be3-4823-838a-d42526d1b245" />

<img width="1392" height="926" alt="Screenshot 2026-05-03 at 18 46 14" src="https://github.com/user-attachments/assets/59c56145-ce56-4784-9758-5b997bc82b51" />

<img width="1392" height="926" alt="Screenshot 2026-05-03 at 18 45 45" src="https://github.com/user-attachments/assets/15cd8a08-dd93-4d68-ae4f-92d80a2a9312" />

<img width="1392" height="926" alt="Screenshot 2026-05-03 at 18 43 21" src="https://github.com/user-attachments/assets/3f77cafd-c400-4738-b844-e7c85e0aac34" />

<img width="1392" height="926" alt="Screenshot 2026-05-03 at 18 43 38" src="https://github.com/user-attachments/assets/6a380629-49a2-48bd-9ea3-a065a13af522" />

<img width="1392" height="926" alt="Screenshot 2026-05-03 at 18 43 53" src="https://github.com/user-attachments/assets/e6756429-2017-4983-95a7-47d42cec0f67" />




## Build and Run

```sh
cmake -S . -B build
cmake --build build
./build/InterPlanetary3D
```

## Controls

The game opens on a title screen. Use arrow keys or `WASD` to choose game type, world type, whether player 2 uses keys or a gamepad, whether the forcefield is on, and whether weapons are all/missiles/shotguns, then press `Enter` or `Space` to start. Most modes run in local two-player split-screen; `1 SCR ALT` uses one shared screen that follows the active hunter. Player 1 starts on the top face; player 2 starts on the opposite face. Each player has a satellite feed in split-screen modes.

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
- `M` to cycle mine/build and the enabled weapon modes
- Left click in shotgun mode to fire a short-range block-breaking shotgun
- Left click in missile or atomic missile mode to fire; after one second, the top-right feed switches to the missile nose camera
- Rockets cost 1 fuel and 1 plutonium; atomic rockets cost 5 fuel and 5 plutonium
- Mine fuel/plutonium or blast resource pockets to collect resources
- `O` to cycle the satellite between current-face, opposite-face, and polar orbits
- `-` and `=` to lower/raise the satellite orbit
- `F1` to release/capture the mouse
- `Esc` to quit

### Player 2

- `IJKL` to move
- Arrow keys to look
- `Enter` to jump
- Hold `Right Shift` to sprint
- `P` to cycle mine/build and the enabled weapon modes
- `Right Control` to fire the selected shotgun or missile type
- In mine/build mode, `Right Control` swings the tool at close range

If player 2 is set to gamepad: left stick moves, right stick looks, `A` jumps, left bumper sprints, `Y` cycles tools, and right bumper/right trigger fires or swings.
