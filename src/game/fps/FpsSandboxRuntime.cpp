#include "game/fps/FpsSandboxRuntime.h"

#include <raymath.h>

namespace game::fps
{
FpsSandboxRuntime::FpsSandboxRuntime(SceneSystem &scene,
                                     PhysicsWorld &physics_world)
    : scene_(scene),
      player_controller_(physics_world),
      fixed_step_clock_(1.0f / 120.0f, 8)
{
}

void FpsSandboxRuntime::Initialize()
{
    SetupDefaultCamera();
    EnterPlayerMode();
}

void FpsSandboxRuntime::Update(float frame_delta_seconds)
{
    HandleHotkeys();

    player_controller_.GatherInput();

    const int substeps = fixed_step_clock_.ConsumeSteps(frame_delta_seconds);
    for (int i = 0; i < substeps; ++i)
    {
        player_controller_.FixedUpdate(fixed_step_clock_.FixedDeltaSeconds());
        scene_.Step(fixed_step_clock_.FixedDeltaSeconds());
    }

    player_controller_.UpdateCamera(camera_);
}

void FpsSandboxRuntime::DrawWorld() const
{
    BeginMode3D(camera_);

    DrawGrid(60, 1.0f);
    DrawGameplayMarkers();

    scene_.Draw(0);

    if (show_physics_debug_)
    {
        scene_.DrawPhysicsDebug(true);
    }

    EndMode3D();
}

void FpsSandboxRuntime::DrawOverlay() const
{
    const int cx = GetScreenWidth() / 2;
    const int cy = GetScreenHeight() / 2;

    DrawLine(cx - 6, cy, cx + 6, cy, Fade(BLACK, 0.65f));
    DrawLine(cx, cy - 6, cx, cy + 6, Fade(BLACK, 0.65f));

    if (!show_help_overlay_)
    {
        return;
    }

    const int margin = 16;
    const int panel_w = 620;
    const int panel_h = 150;
    const int panel_x = margin;
    const int panel_y = margin;

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, Fade(RAYWHITE, 0.90f));
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, Fade(DARKGRAY, 0.65f));

    DrawText("FPS Sandbox", panel_x + 14, panel_y + 10, 24, BLACK);
    DrawText("WASD Move | Shift Sprint | Space Jump | Mouse Look", panel_x + 14, panel_y + 44, 18, DARKGRAY);
    DrawText("R Respawn | F6 Next Spawn | F2 Physics Debug | F3 Toggle Help", panel_x + 14, panel_y + 68, 18, DARKGRAY);
    DrawText(TextFormat("Spawn: %d/%d", current_spawn_point_index_ + 1, static_cast<int>(spawn_points_.size())),
             panel_x + 14,
             panel_y + 96,
             18,
             DARKGRAY);
    DrawText(TextFormat("Grounded: %s", player_controller_.IsGrounded() ? "Yes" : "No"),
             panel_x + 180,
             panel_y + 96,
             18,
             DARKGRAY);
    DrawText(TextFormat("FPS: %d", GetFPS()),
             panel_x + 14,
             panel_y + 118,
             18,
             DARKGRAY);
}

void FpsSandboxRuntime::SetupDefaultCamera()
{
    camera_.position = Vector3{8.0f, 7.0f, 10.0f};
    camera_.target = Vector3{0.0f, 1.5f, 0.0f};
    camera_.up = Vector3{0.0f, 1.0f, 0.0f};
    camera_.fovy = 75.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void FpsSandboxRuntime::DrawGameplayMarkers() const
{
    for (size_t i = 0; i < spawn_points_.size(); ++i)
    {
        const Vector3 marker = spawn_points_[i];
        Color color = Color{20, 180, 90, 200};
        if (static_cast<int>(i) == current_spawn_point_index_)
        {
            color = Color{40, 230, 120, 255};
        }

        DrawCylinder(
            Vector3{marker.x, marker.y + 0.08f, marker.z},
            0.35f,
            0.35f,
            0.16f,
            16,
            color);
        DrawCylinderWires(
            Vector3{marker.x, marker.y + 0.08f, marker.z},
            0.35f,
            0.35f,
            0.16f,
            16,
            Fade(BLACK, 0.5f));
    }

    for (const Vector3 &marker : pickup_points_)
    {
        DrawCylinder(
            Vector3{marker.x, marker.y + 0.06f, marker.z},
            0.26f,
            0.26f,
            0.12f,
            16,
            Color{220, 145, 35, 230});
        DrawCylinderWires(
            Vector3{marker.x, marker.y + 0.06f, marker.z},
            0.26f,
            0.26f,
            0.12f,
            16,
            Fade(BLACK, 0.45f));
    }
}

void FpsSandboxRuntime::EnterPlayerMode()
{
    const Vector3 spawn_position = spawn_points_[current_spawn_point_index_];
    const Vector3 look_direction = Vector3Subtract(
        spawn_look_targets_[current_spawn_point_index_],
        spawn_position);

    if (player_controller_.ActivateAt(spawn_position, look_direction))
    {
        player_controller_.UpdateCamera(camera_);
    }
}

void FpsSandboxRuntime::RespawnPlayer()
{
    const Vector3 spawn_position = spawn_points_[current_spawn_point_index_];
    const Vector3 look_direction = Vector3Subtract(
        spawn_look_targets_[current_spawn_point_index_],
        spawn_position);
    player_controller_.RespawnAt(spawn_position, look_direction);
}

void FpsSandboxRuntime::HandleHotkeys()
{
    if (IsKeyPressed(KEY_F2))
    {
        show_physics_debug_ = !show_physics_debug_;
    }
    if (IsKeyPressed(KEY_F3))
    {
        show_help_overlay_ = !show_help_overlay_;
    }

    if (IsKeyPressed(KEY_F6))
    {
        current_spawn_point_index_ =
            (current_spawn_point_index_ + 1) % static_cast<int>(spawn_points_.size());
        RespawnPlayer();
    }

    if (IsKeyPressed(KEY_R))
    {
        RespawnPlayer();
    }
}
} // namespace game::fps
