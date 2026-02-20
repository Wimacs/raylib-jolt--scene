#pragma once

#include "SceneSystem.h"
#include "engine/core/FixedStepClock.h"
#include "game/fps/FpsPlayerController.h"

#include <array>
#include <raylib.h>

namespace game::fps
{
class FpsSandboxRuntime
{
public:
    FpsSandboxRuntime(SceneSystem &scene, PhysicsWorld &physics_world);

    void Initialize();
    void Update(float frame_delta_seconds);

    void DrawWorld() const;
    void DrawOverlay() const;

private:
    void SetupDefaultCamera();
    void DrawGameplayMarkers() const;
    void EnterPlayerMode();
    void RespawnPlayer();
    void HandleHotkeys();

    SceneSystem &scene_;
    Camera3D camera_{};
    FpsPlayerController player_controller_;
    engine::core::FixedStepClock fixed_step_clock_;

    bool show_physics_debug_{true};
    bool show_help_overlay_{true};

    std::array<Vector3, 4> spawn_points_{
        Vector3{-14.0f, 1.10f, -14.0f},
        Vector3{14.0f, 1.10f, -14.0f},
        Vector3{-14.0f, 1.10f, 14.0f},
        Vector3{14.0f, 1.10f, 14.0f}};
    std::array<Vector3, 4> spawn_look_targets_{
        Vector3{-6.0f, 1.5f, -6.0f},
        Vector3{6.0f, 1.5f, -6.0f},
        Vector3{-6.0f, 1.5f, 6.0f},
        Vector3{6.0f, 1.5f, 6.0f}};
    std::array<Vector3, 5> pickup_points_{
        Vector3{0.0f, 0.2f, 0.0f},
        Vector3{-8.0f, 0.2f, 0.0f},
        Vector3{8.0f, 0.2f, 0.0f},
        Vector3{0.0f, 0.2f, -10.0f},
        Vector3{0.0f, 0.2f, 10.0f}};

    int current_spawn_point_index_{0};
};
} // namespace game::fps
