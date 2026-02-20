#pragma once

#include "PhysicsWorld.h"

#include <raylib.h>

namespace game::fps
{
class FpsPlayerController
{
public:
    explicit FpsPlayerController(PhysicsWorld &physics_world);

    bool EnsureSpawned(const Vector3 &spawn_position);
    bool ActivateAt(const Vector3 &spawn_position, const Vector3 &look_direction);
    bool RespawnAt(const Vector3 &spawn_position, const Vector3 &look_direction);
    void Deactivate();

    [[nodiscard]] bool IsActive() const;
    [[nodiscard]] bool IsGrounded() const;
    [[nodiscard]] bool IsWallRunning() const;
    [[nodiscard]] bool IsGrappling() const;
    [[nodiscard]] Vector3 GrappleAnchorPoint() const;
    [[nodiscard]] JPH::BodyID BodyId() const;

    void GatherInput();
    void FixedUpdate(float fixed_delta_seconds);
    void UpdateCamera(Camera3D &camera) const;

private:
    void SetViewDirection(const Vector3 &look_direction);
    [[nodiscard]] bool ComputeGrounded() const;
    [[nodiscard]] Vector3 ViewForward() const;
    [[nodiscard]] Vector3 EyePosition() const;
    [[nodiscard]] Vector3 HorizontalForward() const;
    [[nodiscard]] bool FindRunnableWall(Vector3 &out_wall_normal) const;
    void TryStartGrapple();
    void StopGrapple(bool start_cooldown);

    PhysicsWorld &physics_world_;
    JPH::BodyID body_id_{};

    bool active_{false};
    bool grounded_{false};
    bool wall_running_{false};
    bool grappling_{false};

    float yaw_{0.0f};
    float pitch_{0.0f};

    Vector2 move_input_{0.0f, 0.0f};
    bool sprinting_{false};
    bool jump_requested_{false};
    bool grapple_pressed_{false};

    float mouse_sensitivity_{0.0022f};
    float walk_speed_{6.5f};
    float sprint_speed_{10.8f};
    float jump_velocity_{6.3f};
    float ground_acceleration_{28.0f};
    float air_acceleration_{10.0f};
    float wall_run_speed_{14.5f};
    float wall_run_fall_speed_{0.35f};
    float wall_run_stick_velocity_{3.4f};
    float wall_run_max_seconds_{1.80f};
    float wall_run_probe_distance_{0.72f};
    float wall_jump_push_{8.6f};
    float wall_jump_forward_boost_{4.0f};

    Vector3 wall_normal_{0.0f, 0.0f, 0.0f};
    float wall_run_elapsed_seconds_{0.0f};

    Vector3 grapple_anchor_{0.0f, 0.0f, 0.0f};
    float grapple_range_{38.0f};
    float grapple_pull_accel_{96.0f};
    float grapple_max_speed_{36.0f};
    float grapple_max_seconds_{1.20f};
    float grapple_detach_distance_{1.15f};
    float grapple_max_fall_speed_{3.5f};
    float grapple_release_boost_{4.2f};
    float grapple_rope_length_{0.0f};
    float grapple_retract_speed_{54.0f};
    float grapple_direction_steer_strength_{18.0f};
    float grapple_tension_strength_{120.0f};
    float grapple_remaining_seconds_{0.0f};
    float grapple_cooldown_seconds_{0.0f};
    float grapple_cooldown_after_use_{0.5f};

    float capsule_radius_{0.35f};
    float capsule_half_height_{0.55f};
    float eye_offset_from_center_{0.55f};
    float ground_probe_padding_{0.16f};
};
} // namespace game::fps
