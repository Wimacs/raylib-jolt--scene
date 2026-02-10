# raylib-jolt-scene

Interactive scene system based on [raylib](https://github.com/raysan5/raylib) + [Jolt Physics](https://github.com/jrouwe/JoltPhysics), inspired by [rodneylab/jolt-raylib-hello-world](https://github.com/rodneylab/jolt-raylib-hello-world).

## Features

- Full rigid-body simulation for all objects (ground + dynamic bodies)
- Runtime object creation with multiple shapes:
  - Box
  - Sphere
  - Capsule
  - Cylinder
- Object deletion:
  - Right click: delete hovered object
  - `Delete` / `Backspace`: delete selected object
- Object dragging:
  - Left click and hold on dynamic object to drag
- Physics debug view:
  - Wireframe collision-shape overlay
  - Helps verify collision/render size alignment and rotation behavior
- English GUI panel (raygui):
  - Spawn shape selector
  - Live size/radius/height controls
  - Add and delete actions
  - Debug toggles and runtime stats

## Controls

- Camera: `W/A/S/D` + mouse (raylib free camera)
- Spawn quick keys: `1/2/3/4` for Box/Sphere/Capsule/Cylinder
- Drag: left mouse hold
- Delete: right mouse (hovered) or `Delete/Backspace` (selected)
- `F1`: toggle UI panel
- `F2`: toggle physics debug view
- `F3`: toggle help overlay

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run

```bash
./build/raylib_jolt_scene
```

## Project Layout

- `CMakeLists.txt`: dependencies and build configuration (raylib + Jolt + raygui)
- `src/PhysicsWorld.h`, `src/PhysicsWorld.cpp`: Jolt world wrapper (init, step, pick, body creation/removal, debug shape cache)
- `src/SceneSystem.h`, `src/SceneSystem.cpp`: scene object management, picking, dragging, rendering, physics debug draw
- `src/main.cpp`: input handling, English GUI panel, app loop
