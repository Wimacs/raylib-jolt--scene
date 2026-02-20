# raylib-jolt-scene

Interactive scene system based on [raylib](https://github.com/raysan5/raylib) + [Jolt Physics](https://github.com/jrouwe/JoltPhysics), inspired by [rodneylab/jolt-raylib-hello-world](https://github.com/rodneylab/jolt-raylib-hello-world).

## Implemented Features

- Full rigid-body simulation for scene objects (ground + dynamic bodies)
- FPS-only capsule controller (Jolt dynamic capsule body):
  - Mouse look
  - `W/A/S/D` movement
  - Sprint + jump
  - Spawn point switch + respawn
- FPS sandbox test map (`scene_fps_sandbox.json`) with:
  - Multiple slope angles
  - Staircase climb test segment
  - Arena blockout and physics props
- Physics debug view (collision wireframes)
- Resizable window support (`FLAG_WINDOW_RESIZABLE`)
- Scene JSON auto-load at startup
- Scene JSON constraints + motor support:
  - Constraint types: `fixed`, `point`, `distance`, `hinge`, `slider`
  - Motor modes: `off`, `velocity`, `position`
- Extended physics parameters per object in JSON:
  - Friction/restitution
  - Linear/angular damping
  - Gravity factor
  - Max linear/angular velocity
  - Sleeping, sensor
  - Manifold reduction, gyroscopic force, internal-edge removal
  - Custom mass
  - Motion quality (`discrete` / `linear_cast`)

## Controls

- Move: `W/A/S/D`
- Sprint: `Left Shift` / `Right Shift`
- Jump: `Space`
- Look: mouse
- Cycle spawn point: `F6`
- Respawn at selected spawn: `R`
- Toggle physics debug: `F2`
- Toggle help overlay: `F3`

## Build (Desktop)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## Run (Desktop)

```bash
./build/raylib_jolt_scene
```

## Build (WebAssembly)

Prerequisite: install and activate Emscripten SDK (`emcmake` available in PATH).

```bash
emcmake cmake -S . -B build-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-wasm -j
```

The WebAssembly build preloads:

- `scene_fps_sandbox.json`
- `scene_default_showcase.json`
- `scene_constraints_motor_example.json`

## Run (WebAssembly)

```bash
cp web/index.html build-wasm/index.html
cd build-wasm
python3 -m http.server 8080
```

Open <http://localhost:8080>.

## Scene JSON

Default startup scene is `scene_fps_sandbox.json` (auto-loaded on app startup).

Showcase coverage:

- All supported constraints: `fixed`, `point`, `distance`, `hinge`, `slider`
- All supported motor modes: `off`, `velocity`, `position`

Sample files:

- `scene_fps_sandbox.json` (FPS sandbox map, default)
- `scene_default_showcase.json` (full constraints + motor showcase)
- `scene_constraints_motor_example.json` (smaller example)

### Units

Scene files use SI-like units:

- Length: meter (`m`)
- Mass: kilogram (`kg`)
- Time: second (`s`)
- Force: newton (`N`)
- Torque: newton-meter (`N·m`)
- Angle: radian (`rad`)
- Angular velocity: rad/s

### JSON Structure (summary)

```json
{
  "version": 2,
  "units": {
    "length": "meter",
    "mass": "kilogram",
    "time": "second",
    "force": "newton",
    "torque": "newton_meter",
    "angle": "radian",
    "angular_velocity": "radian_per_second"
  },
  "objects": [
    {
      "id": 2,
      "shape": "box",
      "dynamic": true,
      "position": { "x": 0.0, "y": 3.0, "z": 0.0 },
      "rotation": { "x": 0.0, "y": 0.0, "z": 0.0, "w": 1.0 },
      "half_extents": { "x": 0.45, "y": 0.45, "z": 0.45 },
      "radius": 0.45,
      "half_height": 0.45,
      "color": { "r": 80, "g": 180, "b": 255, "a": 255 },
      "physics": {
        "friction": 0.4,
        "restitution": 0.1,
        "linear_damping": 0.02,
        "angular_damping": 0.02,
        "gravity_factor": 1.0,
        "max_linear_velocity": 500.0,
        "max_angular_velocity": 120.0,
        "allow_sleeping": true,
        "is_sensor": false,
        "use_manifold_reduction": true,
        "apply_gyroscopic_force": true,
        "enhanced_internal_edge_removal": false,
        "collide_kinematic_vs_non_dynamic": false,
        "allow_dynamic_or_kinematic": false,
        "use_custom_mass": true,
        "mass": 2.5,
        "use_linear_cast": true,
        "motion_quality": "linear_cast"
      }
    }
  ],
  "constraints": [
    {
      "id": "hinge_world_box2",
      "type": "hinge",
      "body1": 0,
      "body2": 2,
      "point1": { "x": 0.0, "y": 3.0, "z": 0.0 },
      "point2": { "x": 0.0, "y": 3.0, "z": 0.0 },
      "axis1": { "x": 0.0, "y": 0.0, "z": 1.0 },
      "axis2": { "x": 0.0, "y": 0.0, "z": 1.0 },
      "normal1": { "x": 0.0, "y": 1.0, "z": 0.0 },
      "normal2": { "x": 0.0, "y": 1.0, "z": 0.0 },
      "min_limit": -1.2,
      "max_limit": 1.2,
      "max_friction": 0.1,
      "auto_detect_point": false,
      "enabled": true,
      "motor": {
        "mode": "velocity",
        "target_velocity": 1.2,
        "target_position": 0.0,
        "spring_frequency": 2.0,
        "spring_damping": 0.6,
        "max_force": 2000.0,
        "max_torque": 600.0
      }
    }
  ]
}
```

## Project Layout

- `CMakeLists.txt`: dependencies and build setup (`raylib`, `Jolt`, `raygui`, `nlohmann/json`)
- `src/PhysicsWorld.h`, `src/PhysicsWorld.cpp`: Jolt wrapper, body creation, constraints/motors, world stepping
- `src/SceneSystem.h`, `src/SceneSystem.cpp`: scene management, picking/dragging, rendering, JSON serialization
- `src/game/fps/FpsPlayerController.*`: FPS capsule controller
- `src/game/fps/FpsSandboxRuntime.*`: FPS runtime loop, spawning, debug/help overlays
- `src/main.cpp`: minimal entrypoint, delegates to app runtime
- `scene_fps_sandbox.json`: default FPS sandbox scene
- `scene_default_showcase.json`: full constraints + motor showcase scene
- `scene_constraints_motor_example.json`: smaller constraints/motor example
