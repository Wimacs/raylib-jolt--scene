#include "game/fps/FpsSandboxRuntime.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game::fps
{
FpsSandboxRuntime::FpsSandboxRuntime(SceneSystem &scene,
                                     PhysicsWorld &physics_world)
    : scene_(scene),
      physics_world_(physics_world),
      player_controller_(physics_world),
      fixed_step_clock_(1.0f / 120.0f, 8)
{
}

void FpsSandboxRuntime::Initialize()
{
    SetupDefaultCamera();
    EnterPlayerMode();
    SpawnTrainingBots();
}

void FpsSandboxRuntime::Update(float frame_delta_seconds)
{
    HandleHotkeys();

    player_controller_.GatherInput();
    UpdateWeaponState(frame_delta_seconds);
    UpdateImpactDecals(frame_delta_seconds);
    hit_marker_remaining_seconds_ = std::max(
        0.0f,
        hit_marker_remaining_seconds_ - frame_delta_seconds);

    const int substeps = fixed_step_clock_.ConsumeSteps(frame_delta_seconds);
    for (int i = 0; i < substeps; ++i)
    {
        player_controller_.FixedUpdate(fixed_step_clock_.FixedDeltaSeconds());
        scene_.Step(fixed_step_clock_.FixedDeltaSeconds());
        UpdateTrainingBots(fixed_step_clock_.FixedDeltaSeconds());
        UpdateBullets(fixed_step_clock_.FixedDeltaSeconds());
    }

    player_controller_.UpdateCamera(camera_);
    camera_.fovy = Lerp(hip_fov_, ads_fov_, aim_blend_);
    UpdateRecoil(frame_delta_seconds);
    ApplyRecoilToCamera();
    HandleWeaponFire();
    ApplyRecoilToCamera();
}

void FpsSandboxRuntime::DrawWorld() const
{
    BeginMode3D(camera_);

    DrawGrid(60, 1.0f);
    DrawGameplayMarkers();

    scene_.Draw(0);
    DrawImpactDecals();
    DrawBullets();
    DrawGrappleVisual();
    DrawWeaponViewModel();

    if (show_physics_debug_)
    {
        scene_.DrawPhysicsDebug(true, player_controller_.BodyId());
    }

    EndMode3D();
}

void FpsSandboxRuntime::DrawOverlay() const
{
    const int cx = GetScreenWidth() / 2;
    const int cy = GetScreenHeight() / 2;
    const int cross_radius = aiming_down_sights_ ? 6 : 9;
    const Color cross_color = aiming_down_sights_ ? Fade(BLACK, 0.85f) : Fade(BLACK, 0.65f);

    DrawLine(cx - cross_radius, cy, cx + cross_radius, cy, cross_color);
    DrawLine(cx, cy - cross_radius, cx, cy + cross_radius, cross_color);
    DrawHitMarker();

    DrawWeaponHud();

    if (!show_help_overlay_)
    {
        return;
    }

    const int margin = 16;
    const int panel_w = 780;
    const int panel_h = 214;
    const int panel_x = margin;
    const int panel_y = margin;

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, Fade(RAYWHITE, 0.90f));
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, Fade(DARKGRAY, 0.65f));

    DrawText("FPS Sandbox", panel_x + 14, panel_y + 10, 24, BLACK);
    DrawText("WASD Move | Shift Sprint | Space Jump | Mouse Look", panel_x + 14, panel_y + 44, 18, DARKGRAY);
    DrawText("LMB Fire | RMB ADS | R Reload | 1/2 Switch Weapon | Q Grapple", panel_x + 14, panel_y + 68, 18, DARKGRAY);
    DrawText("Wallrun: Hold W near wall while airborne", panel_x + 14, panel_y + 92, 18, DARKGRAY);
    DrawText("T Respawn | F6 Next Spawn | F2 Physics Debug | F3 Toggle Help", panel_x + 14, panel_y + 116, 18, DARKGRAY);
    DrawText(TextFormat("Spawn: %d/%d", current_spawn_point_index_ + 1, static_cast<int>(spawn_points_.size())),
             panel_x + 14,
             panel_y + 144,
             18,
             DARKGRAY);
    DrawText(TextFormat("Grounded: %s", player_controller_.IsGrounded() ? "Yes" : "No"),
             panel_x + 190,
             panel_y + 144,
             18,
             DARKGRAY);
    DrawText(TextFormat("Wallrun: %s", player_controller_.IsWallRunning() ? "On" : "Off"),
             panel_x + 350,
             panel_y + 144,
             18,
             DARKGRAY);
    DrawText(TextFormat("Grapple: %s", player_controller_.IsGrappling() ? "On" : "Off"),
             panel_x + 470,
             panel_y + 144,
             18,
             DARKGRAY);
    DrawText(TextFormat("ADS: %s", aiming_down_sights_ ? "On" : "Off"),
             panel_x + 14,
             panel_y + 168,
             18,
             DARKGRAY);
    DrawText(TextFormat("Active Bullets: %d", static_cast<int>(bullets_.size())),
             panel_x + 180,
             panel_y + 168,
             18,
             DARKGRAY);
    DrawText(TextFormat("FPS: %d", GetFPS()),
             panel_x + 470,
             panel_y + 168,
             18,
             DARKGRAY);
    DrawText(TextFormat("Current Weapon: %s", CurrentWeapon().display_name),
             panel_x + 14,
             panel_y + 190,
             18,
             DARKGRAY);
    DrawText(TextFormat("Robot Hits: %d  Headshots: %d", robot_hit_count_, robot_headshot_count_),
             panel_x + 300,
             panel_y + 190,
             18,
             DARKGRAY);
}

void FpsSandboxRuntime::DrawWeaponHud() const
{
    const WeaponSlot &active = CurrentWeapon();
    const WeaponSlot &other =
        weapon_slots_[(current_weapon_index_ + 1) % static_cast<int>(weapon_slots_.size())];

    const int panel_w = 410;
    const int panel_h = 154;
    const int panel_x = GetScreenWidth() - panel_w - 18;
    const int panel_y = GetScreenHeight() - panel_h - 18;

    DrawRectangle(panel_x, panel_y, panel_w, panel_h, Fade(Color{18, 20, 28, 255}, 0.84f));
    DrawRectangleLines(panel_x, panel_y, panel_w, panel_h, Fade(active.accent, 0.85f));
    DrawRectangle(panel_x + panel_w - 8, panel_y, 8, panel_h, active.accent);

    DrawText(active.display_name, panel_x + 18, panel_y + 14, 24, Fade(RAYWHITE, 0.97f));
    DrawText(other.display_name, panel_x + 20, panel_y + 112, 16, Fade(LIGHTGRAY, 0.82f));

    if (reloading_)
    {
        DrawText("RELOADING", panel_x + 170, panel_y + 20, 16, Color{245, 198, 88, 255});
    }

    DrawText(TextFormat("%02d", std::max(0, active.ammo_in_magazine)),
             panel_x + 170,
             panel_y + 44,
             60,
             Fade(RAYWHITE, 0.98f));
    DrawText("/", panel_x + 260, panel_y + 65, 34, Fade(LIGHTGRAY, 0.85f));
    DrawText(TextFormat("%03d", std::max(0, active.reserve_ammo)),
             panel_x + 285,
             panel_y + 72,
             28,
             Fade(LIGHTGRAY, 0.90f));

    DrawText(TextFormat("%02d", std::max(0, other.ammo_in_magazine)),
             panel_x + 132,
             panel_y + 112,
             22,
             Fade(LIGHTGRAY, 0.85f));
    DrawText(TextFormat("/%03d", std::max(0, other.reserve_ammo)),
             panel_x + 164,
             panel_y + 114,
             18,
             Fade(GRAY, 0.90f));
}

void FpsSandboxRuntime::SetupDefaultCamera()
{
    camera_.position = Vector3{8.0f, 7.0f, 10.0f};
    camera_.target = Vector3{0.0f, 1.5f, 0.0f};
    camera_.up = Vector3{0.0f, 1.0f, 0.0f};
    camera_.fovy = 75.0f;
    camera_.projection = CAMERA_PERSPECTIVE;
}

void FpsSandboxRuntime::SpawnTrainingBots()
{
    training_robots_.clear();
    training_bot_time_ = 0.0f;

    ScenePhysics body_physics{};
    body_physics.friction = 0.38f;
    body_physics.restitution = 0.02f;
    body_physics.linear_damping = 3.8f;
    body_physics.angular_damping = 7.0f;
    body_physics.gravity_factor = 0.0f;
    body_physics.max_linear_velocity = 45.0f;
    body_physics.max_angular_velocity = 10.0f;
    body_physics.allow_sleeping = false;
    body_physics.use_custom_mass = true;
    body_physics.mass = 95.0f;

    ScenePhysics head_physics = body_physics;
    head_physics.is_sensor = true;
    head_physics.mass = 30.0f;

    struct PatrolSeed
    {
        Vector3 origin;
        float radius;
        float speed;
        float phase;
    };

    const std::array<PatrolSeed, 4> seeds{
        PatrolSeed{Vector3{-11.0f, 1.12f, 2.5f}, 2.7f, 0.92f, 0.0f},
        PatrolSeed{Vector3{10.6f, 1.15f, -3.5f}, 2.3f, 1.10f, 1.7f},
        PatrolSeed{Vector3{3.6f, 1.08f, 11.0f}, 2.1f, 0.86f, 3.4f},
        PatrolSeed{Vector3{-1.8f, 1.10f, -11.8f}, 2.5f, 1.02f, 5.2f}};

    const float body_half_height = 0.42f;
    const float body_radius = 0.26f;
    const float head_radius = 0.21f;
    const float head_offset = body_half_height + body_radius + head_radius + 0.08f;

    for (const PatrolSeed &seed : seeds)
    {
        const int body_object_id = scene_.AddCapsule(
            seed.origin,
            body_half_height,
            body_radius,
            Color{118, 158, 210, 255},
            body_physics);
        const int head_object_id = scene_.AddSphere(
            Vector3Add(seed.origin, Vector3{0.0f, head_offset, 0.0f}),
            head_radius,
            true,
            Color{226, 234, 248, 255},
            head_physics);
        if (body_object_id == 0 || head_object_id == 0)
        {
            continue;
        }

        TrainingRobot robot{};
        robot.body_object_id = body_object_id;
        robot.head_object_id = head_object_id;
        robot.body_id = BodyIdForSceneObject(body_object_id);
        robot.head_id = BodyIdForSceneObject(head_object_id);
        robot.patrol_origin = seed.origin;
        robot.patrol_radius = seed.radius;
        robot.patrol_speed = seed.speed;
        robot.phase = seed.phase;
        robot.bob_amplitude = 0.12f;
        robot.bob_speed = 2.1f;
        if (robot.body_id.IsInvalid() || robot.head_id.IsInvalid())
        {
            continue;
        }

        training_robots_.push_back(robot);
    }
}

void FpsSandboxRuntime::UpdateTrainingBots(float fixed_delta_seconds)
{
    if (training_robots_.empty())
    {
        return;
    }

    training_bot_time_ += fixed_delta_seconds;
    const Quaternion upright = QuaternionIdentity();
    const float head_offset = 0.97f;

    for (TrainingRobot &robot : training_robots_)
    {
        if (!physics_world_.IsBodyAdded(robot.body_id) ||
            !physics_world_.IsBodyAdded(robot.head_id))
        {
            continue;
        }

        const float orbit = training_bot_time_ * robot.patrol_speed + robot.phase;
        const float sin_orbit = std::sinf(orbit);
        const float cos_orbit = std::cosf(orbit);
        const float bob = std::sinf(training_bot_time_ * robot.bob_speed + robot.phase * 1.6f) *
            robot.bob_amplitude;

        const Vector3 body_target{
            robot.patrol_origin.x + cos_orbit * robot.patrol_radius,
            robot.patrol_origin.y + bob,
            robot.patrol_origin.z + sin_orbit * robot.patrol_radius};

        Vector3 tangent{-sin_orbit, 0.0f, cos_orbit};
        if (Vector3LengthSqr(tangent) > 0.0001f)
        {
            tangent = Vector3Normalize(tangent);
        }
        const Vector3 patrol_velocity = Vector3Scale(
            tangent,
            robot.patrol_radius * robot.patrol_speed);

        physics_world_.SetBodyTransform(robot.body_id, body_target, upright, true);
        physics_world_.SetBodyLinearVelocity(robot.body_id, patrol_velocity, true);
        physics_world_.SetBodyAngularVelocity(robot.body_id, Vector3Zero(), false);

        const float head_bob = std::sinf(training_bot_time_ * 3.4f + robot.phase) * 0.028f;
        const Vector3 head_target = Vector3Add(
            body_target,
            Vector3{0.0f, head_offset + head_bob, 0.0f});
        physics_world_.SetBodyTransform(robot.head_id, head_target, upright, true);
        physics_world_.SetBodyLinearVelocity(robot.head_id, patrol_velocity, true);
        physics_world_.SetBodyAngularVelocity(robot.head_id, Vector3Zero(), false);
    }
}

void FpsSandboxRuntime::DrawHitMarker() const
{
    if (hit_marker_remaining_seconds_ <= 0.0f)
    {
        return;
    }

    const float life_ratio = std::clamp(
        hit_marker_remaining_seconds_ / std::max(0.0001f, hit_marker_duration_seconds_),
        0.0f,
        1.0f);
    const float progress = 1.0f - life_ratio;
    const float inner = Lerp(19.0f, 10.0f, progress);
    const float outer = inner + Lerp(11.0f, 7.0f, progress);
    const float thickness = hit_marker_critical_ ? 3.4f : 2.4f;
    const Color base_color = hit_marker_critical_
        ? Color{232, 48, 52, 255}
        : Color{250, 250, 250, 255};
    const Color marker_color = Fade(base_color, 0.32f + 0.68f * life_ratio);

    const Vector2 center{
        static_cast<float>(GetScreenWidth()) * 0.5f,
        static_cast<float>(GetScreenHeight()) * 0.5f};

    DrawLineEx(Vector2{center.x - inner, center.y - inner},
               Vector2{center.x - outer, center.y - outer},
               thickness,
               marker_color);
    DrawLineEx(Vector2{center.x + inner, center.y + inner},
               Vector2{center.x + outer, center.y + outer},
               thickness,
               marker_color);
    DrawLineEx(Vector2{center.x + inner, center.y - inner},
               Vector2{center.x + outer, center.y - outer},
               thickness,
               marker_color);
    DrawLineEx(Vector2{center.x - inner, center.y + inner},
               Vector2{center.x - outer, center.y + outer},
               thickness,
               marker_color);

    if (hit_marker_critical_)
    {
        DrawCircleLines(static_cast<int>(center.x),
                        static_cast<int>(center.y),
                        4.0f + 2.0f * progress,
                        Fade(marker_color, 0.85f));
    }
}

void FpsSandboxRuntime::TriggerHitMarker(bool critical_hit)
{
    hit_marker_remaining_seconds_ = hit_marker_duration_seconds_;
    hit_marker_critical_ = critical_hit;
    ++robot_hit_count_;
    if (critical_hit)
    {
        ++robot_headshot_count_;
    }
}

bool FpsSandboxRuntime::ResolveRobotHit(JPH::BodyID body_id, bool &out_headshot) const
{
    out_headshot = false;
    if (body_id.IsInvalid())
    {
        return false;
    }

    for (const TrainingRobot &robot : training_robots_)
    {
        if (body_id == robot.head_id)
        {
            out_headshot = true;
            return true;
        }
        if (body_id == robot.body_id)
        {
            return true;
        }
    }
    return false;
}

JPH::BodyID FpsSandboxRuntime::BodyIdForSceneObject(int object_id) const
{
    for (const SceneObject &object : scene_.Objects())
    {
        if (object.id == object_id)
        {
            return object.body_id;
        }
    }
    return JPH::BodyID();
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

void FpsSandboxRuntime::DrawBullets() const
{
    for (const BallisticBullet &bullet : bullets_)
    {
        const float camera_distance = Vector3Distance(camera_.position, bullet.current_position);
        if (camera_distance < 0.30f)
        {
            continue;
        }

        const float life_alpha = std::clamp(
            bullet.remaining_life_seconds / std::max(0.0001f, bullet_life_seconds_),
            0.0f,
            1.0f);
        const float near_fade = std::clamp((camera_distance - 0.30f) / 1.00f, 0.0f, 1.0f);
        const float trail_alpha = (0.18f + 0.45f * life_alpha) * near_fade;
        DrawLine3D(bullet.previous_position,
                   bullet.current_position,
                   Fade(Color{255, 195, 80, 255}, trail_alpha));
    }
}

void FpsSandboxRuntime::DrawImpactDecals() const
{
    for (const ImpactDecal &decal : impact_decals_)
    {
        const float life_alpha = std::clamp(
            decal.remaining_life_seconds / std::max(0.0001f, decal_life_seconds_),
            0.0f,
            1.0f);
        const float camera_distance = Vector3Distance(camera_.position, decal.position);
        if (camera_distance < 0.20f)
        {
            continue;
        }

        Vector3 normal = decal.normal;
        if (Vector3LengthSqr(normal) < 0.0001f)
        {
            normal = Vector3{0.0f, 1.0f, 0.0f};
        }
        else
        {
            normal = Vector3Normalize(normal);
        }

        const float near_fade = std::clamp((camera_distance - 0.20f) / 1.10f, 0.0f, 1.0f);
        const float alpha = near_fade * life_alpha;
        if (alpha <= 0.001f)
        {
            continue;
        }

        const Vector3 start = Vector3Add(decal.position, Vector3Scale(normal, 0.0015f));
        const Vector3 end = Vector3Add(start, Vector3Scale(normal, 0.008f));
        const float radius = decal.radius * (0.90f + 0.10f * life_alpha);
        DrawCylinderEx(start,
                       end,
                       radius,
                       radius * 0.95f,
                       10,
                       Fade(Color{30, 28, 26, 255}, 0.55f * alpha));
    }
}

void FpsSandboxRuntime::DrawGrappleVisual() const
{
    if (!player_controller_.IsGrappling())
    {
        return;
    }

    const Vector3 anchor = player_controller_.GrappleAnchorPoint();
    const Vector3 forward = CameraForward();
    const Vector3 right = CameraRight(forward);
    Vector3 up = Vector3CrossProduct(right, forward);
    if (Vector3LengthSqr(up) < 0.0001f)
    {
        up = Vector3{0.0f, 1.0f, 0.0f};
    }
    else
    {
        up = Vector3Normalize(up);
    }

    const Vector3 rope_start = Vector3Add(
        camera_.position,
        Vector3Add(
            Vector3Scale(forward, 0.62f),
            Vector3Add(Vector3Scale(right, 0.12f), Vector3Scale(up, -0.07f))));
    const Vector3 rope_span = Vector3Subtract(anchor, rope_start);
    const float rope_length = Vector3Length(rope_span);
    if (rope_length <= 0.06f)
    {
        return;
    }

    const float rope_radius = 0.012f;
    DrawCylinderEx(rope_start, anchor, rope_radius, rope_radius, 8, Color{58, 74, 122, 255});
    DrawLine3D(rope_start, anchor, Color{185, 205, 255, 230});

    DrawSphere(rope_start, 0.030f, Color{86, 95, 125, 255});
    DrawSphere(anchor, 0.11f, Color{120, 140, 255, 230});
    DrawSphereWires(anchor, 0.11f, 8, 8, Fade(BLACK, 0.45f));
}

void FpsSandboxRuntime::DrawWeaponViewModel() const
{
    if (!player_controller_.IsActive())
    {
        return;
    }

    const WeaponSlot &weapon = CurrentWeapon();
    const float weapon_scale = current_weapon_index_ == 0 ? 0.92f : 1.05f;

    const Vector3 forward = CameraForward();
    const Vector3 right = CameraRight(forward);
    Vector3 up = Vector3CrossProduct(right, forward);
    if (Vector3LengthSqr(up) < 0.0001f)
    {
        up = Vector3{0.0f, 1.0f, 0.0f};
    }
    else
    {
        up = Vector3Normalize(up);
    }

    const Vector3 anchor = Vector3Add(
        camera_.position,
        Vector3Add(
            Vector3Scale(forward, 0.58f + 0.15f * aim_blend_),
            Vector3Add(
                Vector3Scale(right, 0.26f - 0.18f * aim_blend_),
                Vector3Scale(up, -0.25f + 0.12f * aim_blend_))));

    const Color body_color = Color{92, 96, 104, 255};
    const Color accent_color = weapon.accent;

    const Vector3 body_start = anchor;
    const Vector3 body_end = Vector3Add(anchor, Vector3Scale(forward, 0.30f * weapon_scale));
    DrawCylinderEx(body_start,
                   body_end,
                   0.055f * weapon_scale,
                   0.050f * weapon_scale,
                   10,
                   body_color);

    const Vector3 barrel_start = Vector3Add(body_end, Vector3Scale(forward, 0.02f * weapon_scale));
    const Vector3 barrel_end = Vector3Add(barrel_start, Vector3Scale(forward, 0.28f * weapon_scale));
    DrawCylinderEx(barrel_start,
                   barrel_end,
                   0.028f * weapon_scale,
                   0.022f * weapon_scale,
                   10,
                   body_color);

    const Vector3 rail_start = Vector3Add(
        body_start,
        Vector3Add(Vector3Scale(up, 0.032f * weapon_scale), Vector3Scale(right, 0.01f * weapon_scale)));
    const Vector3 rail_end = Vector3Add(rail_start, Vector3Scale(forward, 0.36f * weapon_scale));
    DrawCylinderEx(rail_start,
                   rail_end,
                   0.012f * weapon_scale,
                   0.012f * weapon_scale,
                   8,
                   accent_color);

    const Vector3 grip_top = Vector3Add(
        body_start,
        Vector3Add(Vector3Scale(forward, 0.08f * weapon_scale), Vector3Scale(up, -0.02f * weapon_scale)));
    const Vector3 grip_bottom = Vector3Add(
        grip_top,
        Vector3Add(Vector3Scale(up, -0.18f * weapon_scale), Vector3Scale(forward, -0.07f * weapon_scale)));
    DrawCylinderEx(grip_top,
                   grip_bottom,
                   0.032f * weapon_scale,
                   0.022f * weapon_scale,
                   8,
                   Color{68, 69, 74, 255});
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
        camera_.fovy = hip_fov_;
    }
}

void FpsSandboxRuntime::RespawnPlayer()
{
    const Vector3 spawn_position = spawn_points_[current_spawn_point_index_];
    const Vector3 look_direction = Vector3Subtract(
        spawn_look_targets_[current_spawn_point_index_],
        spawn_position);
    player_controller_.RespawnAt(spawn_position, look_direction);

    bullets_.clear();
    impact_decals_.clear();
    for (WeaponSlot &weapon : weapon_slots_)
    {
        weapon.reserve_ammo = std::max(weapon.reserve_ammo, weapon.magazine_capacity * 4);
        weapon.ammo_in_magazine = weapon.magazine_capacity;
    }
    current_weapon_index_ = 0;
    reloading_ = false;
    reload_remaining_seconds_ = 0.0f;
    fire_cooldown_seconds_ = 0.0f;
    hit_marker_remaining_seconds_ = 0.0f;
    recoil_offset_ = Vector2{0.0f, 0.0f};
    spread_bloom_ = 0.0f;
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

    if (IsKeyPressed(KEY_T))
    {
        RespawnPlayer();
    }
}

void FpsSandboxRuntime::UpdateWeaponState(float frame_delta_seconds)
{
    aiming_down_sights_ = IsMouseButtonDown(MOUSE_BUTTON_RIGHT);
    trigger_held_ = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    trigger_pressed_ = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    fire_cooldown_seconds_ = std::max(0.0f, fire_cooldown_seconds_ - frame_delta_seconds);

    if (IsKeyPressed(KEY_ONE))
    {
        SwitchWeapon(0);
    }
    if (IsKeyPressed(KEY_TWO))
    {
        SwitchWeapon(1);
    }

    const float wheel_move = GetMouseWheelMove();
    if (wheel_move != 0.0f)
    {
        int next = current_weapon_index_;
        if (wheel_move > 0.0f)
        {
            next -= 1;
        }
        else
        {
            next += 1;
        }

        const int count = static_cast<int>(weapon_slots_.size());
        if (count > 0)
        {
            while (next < 0)
            {
                next += count;
            }
            next %= count;
            SwitchWeapon(next);
        }
    }

    if (IsKeyPressed(KEY_R))
    {
        StartReload();
    }

    if (reloading_)
    {
        reload_remaining_seconds_ = std::max(
            0.0f,
            reload_remaining_seconds_ - frame_delta_seconds);
        if (reload_remaining_seconds_ <= 0.0f)
        {
            CompleteReload();
        }
    }
    else
    {
        const WeaponSlot &weapon = CurrentWeapon();
        if (weapon.ammo_in_magazine <= 0 && weapon.reserve_ammo > 0)
        {
            StartReload();
        }
    }

    const float target_blend = aiming_down_sights_ ? 1.0f : 0.0f;
    const float blend_step = 10.0f * frame_delta_seconds;
    if (aim_blend_ < target_blend)
    {
        aim_blend_ = std::min(target_blend, aim_blend_ + blend_step);
    }
    else
    {
        aim_blend_ = std::max(target_blend, aim_blend_ - blend_step);
    }
}

void FpsSandboxRuntime::UpdateRecoil(float frame_delta_seconds)
{
    const float recover_lerp = std::clamp(recoil_recover_speed_ * frame_delta_seconds,
                                          0.0f,
                                          1.0f);
    recoil_offset_.x = Lerp(recoil_offset_.x, 0.0f, recover_lerp);
    recoil_offset_.y = Lerp(recoil_offset_.y, 0.0f, recover_lerp);

    spread_bloom_ = std::max(
        0.0f,
        spread_bloom_ - spread_bloom_decay_ * frame_delta_seconds);
}

void FpsSandboxRuntime::ApplyRecoilToCamera()
{
    const Vector3 base_forward = CameraForward();
    const Vector3 right = CameraRight(base_forward);
    Vector3 up = Vector3CrossProduct(right, base_forward);
    if (Vector3LengthSqr(up) < 0.0001f)
    {
        up = Vector3{0.0f, 1.0f, 0.0f};
    }
    else
    {
        up = Vector3Normalize(up);
    }

    Vector3 recoil_forward = Vector3Add(
        base_forward,
        Vector3Add(
            Vector3Scale(right, recoil_offset_.x),
            Vector3Scale(up, recoil_offset_.y)));

    if (Vector3LengthSqr(recoil_forward) < 0.0001f)
    {
        recoil_forward = base_forward;
    }
    else
    {
        recoil_forward = Vector3Normalize(recoil_forward);
    }
    camera_.target = Vector3Add(camera_.position, recoil_forward);
}

void FpsSandboxRuntime::HandleWeaponFire()
{
    if (!player_controller_.IsActive() || reloading_ || fire_cooldown_seconds_ > 0.0f)
    {
        return;
    }

    WeaponSlot &weapon = CurrentWeapon();
    const bool wants_to_fire = weapon.automatic_fire ? trigger_held_ : trigger_pressed_;
    if (!wants_to_fire)
    {
        return;
    }

    if (weapon.ammo_in_magazine <= 0)
    {
        StartReload();
        return;
    }

    const Vector3 forward = CameraForward();
    const Vector3 right = CameraRight(forward);
    Vector3 up = Vector3CrossProduct(right, forward);
    if (Vector3LengthSqr(up) < 0.0001f)
    {
        up = Vector3{0.0f, 1.0f, 0.0f};
    }
    else
    {
        up = Vector3Normalize(up);
    }

    const float base_spread = aiming_down_sights_ ? weapon.ads_spread : weapon.hip_spread;
    const float spread = base_spread + spread_bloom_;
    const float spread_x = (static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f) * spread;
    const float spread_y = (static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f) * spread;

    Vector3 shot_direction = Vector3Add(
        forward,
        Vector3Add(Vector3Scale(right, spread_x), Vector3Scale(up, spread_y)));
    if (Vector3LengthSqr(shot_direction) < 0.0001f)
    {
        shot_direction = forward;
    }
    else
    {
        shot_direction = Vector3Normalize(shot_direction);
    }

    // Spawn from center reticle ray so trajectory visually matches the crosshair.
    const Vector3 spawn_position = Vector3Add(
        camera_.position,
        Vector3Scale(shot_direction, 0.45f));

    SpawnBullet(spawn_position, shot_direction, weapon.muzzle_speed);
    weapon.ammo_in_magazine = std::max(0, weapon.ammo_in_magazine - 1);
    fire_cooldown_seconds_ = weapon.fire_interval_seconds;

    const float weapon_kick_scale = weapon.automatic_fire ? 1.28f : 1.86f;
    const float ads_scale = aiming_down_sights_ ? 0.90f : 1.0f;
    const float bloom_ratio = spread_bloom_max_ > 0.0001f
        ? std::clamp(spread_bloom_ / spread_bloom_max_, 0.0f, 1.0f)
        : 0.0f;
    const float sustained_kick_scale =
        1.0f + bloom_ratio * (weapon.automatic_fire ? 1.25f : 0.72f);
    const float yaw_random =
        static_cast<float>(GetRandomValue(-1000, 1000)) / 1000.0f;

    recoil_offset_.y = std::clamp(
        recoil_offset_.y + recoil_pitch_kick_ * weapon_kick_scale * ads_scale * sustained_kick_scale,
        -recoil_max_pitch_,
        recoil_max_pitch_);
    recoil_offset_.x = std::clamp(
        recoil_offset_.x + yaw_random * recoil_yaw_kick_ * weapon_kick_scale * (0.92f + 0.56f * bloom_ratio),
        -recoil_max_yaw_,
        recoil_max_yaw_);

    const float bloom_scale = weapon.automatic_fire ? 1.45f : 1.00f;
    const float ads_bloom_scale = aiming_down_sights_ ? 0.88f : 1.0f;
    spread_bloom_ = std::min(
        spread_bloom_max_,
        spread_bloom_ + spread_bloom_per_shot_ * bloom_scale * ads_bloom_scale);

    if (weapon.ammo_in_magazine == 0 && weapon.reserve_ammo > 0)
    {
        StartReload();
    }
}

void FpsSandboxRuntime::StartReload()
{
    if (reloading_)
    {
        return;
    }

    WeaponSlot &weapon = CurrentWeapon();
    if (weapon.ammo_in_magazine >= weapon.magazine_capacity ||
        weapon.reserve_ammo <= 0)
    {
        return;
    }

    reloading_ = true;
    reload_remaining_seconds_ = weapon.reload_duration_seconds;
}

void FpsSandboxRuntime::CompleteReload()
{
    if (!reloading_)
    {
        return;
    }

    WeaponSlot &weapon = CurrentWeapon();
    const int needed = std::max(0, weapon.magazine_capacity - weapon.ammo_in_magazine);
    const int loaded = std::min(needed, weapon.reserve_ammo);
    weapon.ammo_in_magazine += loaded;
    weapon.reserve_ammo -= loaded;

    reloading_ = false;
    reload_remaining_seconds_ = 0.0f;
}

void FpsSandboxRuntime::SwitchWeapon(int weapon_index)
{
    if (weapon_index < 0 ||
        weapon_index >= static_cast<int>(weapon_slots_.size()) ||
        weapon_index == current_weapon_index_)
    {
        return;
    }

    current_weapon_index_ = weapon_index;
    reloading_ = false;
    reload_remaining_seconds_ = 0.0f;
    fire_cooldown_seconds_ = std::min(fire_cooldown_seconds_, 0.05f);
}

const FpsSandboxRuntime::WeaponSlot &FpsSandboxRuntime::CurrentWeapon() const
{
    return weapon_slots_[current_weapon_index_];
}

FpsSandboxRuntime::WeaponSlot &FpsSandboxRuntime::CurrentWeapon()
{
    return weapon_slots_[current_weapon_index_];
}

void FpsSandboxRuntime::SpawnBullet(const Vector3 &origin,
                                    const Vector3 &direction,
                                    float muzzle_speed)
{
    if (Vector3LengthSqr(direction) < 0.0001f)
    {
        return;
    }

    BallisticBullet bullet{};
    bullet.previous_position = origin;
    bullet.current_position = origin;
    bullet.velocity = Vector3Scale(Vector3Normalize(direction), muzzle_speed);
    bullet.remaining_life_seconds = bullet_life_seconds_;
    bullets_.push_back(bullet);
}

void FpsSandboxRuntime::SpawnImpactDecal(const Vector3 &position,
                                         const Vector3 &normal,
                                         float intensity_scale)
{
    ImpactDecal decal{};
    decal.position = position;
    if (Vector3LengthSqr(normal) < 0.0001f)
    {
        decal.normal = Vector3{0.0f, 1.0f, 0.0f};
    }
    else
    {
        decal.normal = Vector3Normalize(normal);
    }
    decal.position = Vector3Add(decal.position, Vector3Scale(decal.normal, 0.001f));

    const float clamped_intensity = std::clamp(intensity_scale, 0.70f, 1.55f);
    const float random_scale =
        static_cast<float>(GetRandomValue(80, 120)) / 100.0f;
    decal.radius = 0.021f * clamped_intensity * random_scale;
    decal.remaining_life_seconds = decal_life_seconds_;

    if (static_cast<int>(impact_decals_.size()) >= max_impact_decals_)
    {
        impact_decals_.erase(impact_decals_.begin());
    }
    impact_decals_.push_back(decal);
}

void FpsSandboxRuntime::UpdateBullets(float fixed_delta_seconds)
{
    for (auto it = bullets_.begin(); it != bullets_.end();)
    {
        bool should_remove = false;

        it->remaining_life_seconds -= fixed_delta_seconds;
        it->previous_position = it->current_position;
        it->velocity.y -= bullet_gravity_ * fixed_delta_seconds;
        const Vector3 predicted_position = Vector3Add(
            it->current_position,
            Vector3Scale(it->velocity, fixed_delta_seconds));

        const Vector3 travel = Vector3Subtract(predicted_position, it->previous_position);
        const float distance = Vector3Length(travel);
        if (distance > 0.0001f)
        {
            JPH::BodyID hit_body;
            Vector3 hit_point{};
            if (physics_world_.RayCast(
                    it->previous_position,
                    travel,
                    distance,
                    hit_body,
                    hit_point,
                    player_controller_.BodyId()))
            {
                Vector3 hit_normal = Vector3Negate(travel);
                if (Vector3LengthSqr(hit_normal) < 0.0001f)
                {
                    hit_normal = Vector3{0.0f, 1.0f, 0.0f};
                }
                else
                {
                    hit_normal = Vector3Normalize(hit_normal);
                }

                const float impact_scale = std::clamp(
                    Vector3Length(it->velocity) / 540.0f,
                    0.75f,
                    1.45f);
                SpawnImpactDecal(hit_point, hit_normal, impact_scale);

                bool headshot = false;
                if (ResolveRobotHit(hit_body, headshot))
                {
                    TriggerHitMarker(headshot);
                }

                it->current_position = hit_point;
                should_remove = true;
            }
            else
            {
                it->current_position = predicted_position;
            }
        }

        if (it->remaining_life_seconds <= 0.0f || it->current_position.y < -20.0f)
        {
            should_remove = true;
        }

        if (should_remove)
        {
            it = bullets_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

void FpsSandboxRuntime::UpdateImpactDecals(float frame_delta_seconds)
{
    for (auto it = impact_decals_.begin(); it != impact_decals_.end();)
    {
        it->remaining_life_seconds -= frame_delta_seconds;
        if (it->remaining_life_seconds <= 0.0f)
        {
            it = impact_decals_.erase(it);
        }
        else
        {
            ++it;
        }
    }
}

Vector3 FpsSandboxRuntime::CameraForward() const
{
    const Vector3 forward = Vector3Subtract(camera_.target, camera_.position);
    if (Vector3LengthSqr(forward) < 0.0001f)
    {
        return Vector3{0.0f, 0.0f, 1.0f};
    }
    return Vector3Normalize(forward);
}

Vector3 FpsSandboxRuntime::CameraRight(const Vector3 &forward) const
{
    Vector3 right = Vector3CrossProduct(forward, camera_.up);
    if (Vector3LengthSqr(right) < 0.0001f)
    {
        right = Vector3{1.0f, 0.0f, 0.0f};
    }
    else
    {
        right = Vector3Normalize(right);
    }
    return right;
}
} // namespace game::fps
