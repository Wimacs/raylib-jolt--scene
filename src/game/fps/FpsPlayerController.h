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

    void GatherInput();
    void FixedUpdate(float fixed_delta_seconds);
    void UpdateCamera(Camera3D &camera) const;

private:
    void SetViewDirection(const Vector3 &look_direction);
    [[nodiscard]] bool ComputeGrounded() const;

    PhysicsWorld &physics_world_;
    JPH::BodyID body_id_{};

    bool active_{false};
    bool grounded_{false};

    float yaw_{0.0f};
    float pitch_{0.0f};

    Vector2 move_input_{0.0f, 0.0f};
    bool sprinting_{false};
    bool jump_requested_{false};

    float mouse_sensitivity_{0.0022f};
    float walk_speed_{6.5f};
    float sprint_speed_{9.5f};
    float jump_velocity_{6.3f};

    float capsule_radius_{0.35f};
    float capsule_half_height_{0.55f};
    float eye_offset_from_center_{0.55f};
    float ground_probe_padding_{0.16f};
};
} // namespace game::fps
