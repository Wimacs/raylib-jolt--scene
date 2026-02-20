#include "game/fps/FpsPlayerController.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace game::fps
{
namespace
{
float AxisFromKeys(KeyboardKey positive, KeyboardKey negative)
{
    float axis = 0.0f;
    if (IsKeyDown(positive))
    {
        axis += 1.0f;
    }
    if (IsKeyDown(negative))
    {
        axis -= 1.0f;
    }
    return axis;
}
} // namespace

FpsPlayerController::FpsPlayerController(PhysicsWorld &physics_world)
    : physics_world_(physics_world)
{
}

bool FpsPlayerController::EnsureSpawned(const Vector3 &spawn_position)
{
    if (!body_id_.IsInvalid() && physics_world_.IsBodyAdded(body_id_))
    {
        return true;
    }

    BodyPhysicsParams params{};
    params.friction = 0.0f;
    params.restitution = 0.0f;
    params.linear_damping = 0.0f;
    params.angular_damping = 0.98f;
    params.max_linear_velocity = 80.0f;
    params.max_angular_velocity = 50.0f;
    params.allow_sleeping = false;
    params.lock_rotation = true;
    params.use_linear_cast = true;
    params.use_custom_mass = true;
    params.mass = 82.0f;

    const BodySpawnResult spawned = physics_world_.CreateCapsule(
        spawn_position,
        capsule_half_height_,
        capsule_radius_,
        true,
        params);
    body_id_ = spawned.body_id;
    return !body_id_.IsInvalid();
}

bool FpsPlayerController::ActivateAt(const Vector3 &spawn_position,
                                     const Vector3 &look_direction)
{
    if (!EnsureSpawned(spawn_position))
    {
        return false;
    }

    physics_world_.SetBodyTransform(
        body_id_,
        spawn_position,
        QuaternionIdentity(),
        true);
    physics_world_.SetBodyVelocityZero(body_id_);
    SetViewDirection(look_direction);

    DisableCursor();
    active_ = true;
    return true;
}

bool FpsPlayerController::RespawnAt(const Vector3 &spawn_position,
                                    const Vector3 &look_direction)
{
    if (!EnsureSpawned(spawn_position))
    {
        return false;
    }

    physics_world_.SetBodyTransform(
        body_id_,
        spawn_position,
        QuaternionIdentity(),
        true);
    physics_world_.SetBodyVelocityZero(body_id_);
    SetViewDirection(look_direction);
    grounded_ = false;
    return true;
}

void FpsPlayerController::Deactivate()
{
    if (!active_)
    {
        return;
    }

    EnableCursor();
    active_ = false;
    sprinting_ = false;
    move_input_ = Vector2{0.0f, 0.0f};
    jump_requested_ = false;
}

bool FpsPlayerController::IsActive() const
{
    return active_;
}

bool FpsPlayerController::IsGrounded() const
{
    return grounded_;
}

void FpsPlayerController::GatherInput()
{
    if (!active_)
    {
        return;
    }

    const Vector2 mouse_delta = GetMouseDelta();
    yaw_ -= mouse_delta.x * mouse_sensitivity_;
    pitch_ -= mouse_delta.y * mouse_sensitivity_;

    const float kPitchLimit = 1.50f;
    pitch_ = std::clamp(pitch_, -kPitchLimit, kPitchLimit);

    move_input_.x = AxisFromKeys(KEY_D, KEY_A);
    move_input_.y = AxisFromKeys(KEY_W, KEY_S);

    sprinting_ = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    if (IsKeyPressed(KEY_SPACE))
    {
        jump_requested_ = true;
    }
}

void FpsPlayerController::FixedUpdate(float fixed_delta_seconds)
{
    (void)fixed_delta_seconds;

    if (!active_ || body_id_.IsInvalid() || !physics_world_.IsBodyAdded(body_id_))
    {
        return;
    }

    grounded_ = ComputeGrounded();

    Vector3 move_local = Vector3{move_input_.x, 0.0f, move_input_.y};
    if (Vector3LengthSqr(move_local) > 1.0f)
    {
        move_local = Vector3Normalize(move_local);
    }

    const Vector3 forward = Vector3{sinf(yaw_), 0.0f, cosf(yaw_)};
    const Vector3 right =
        Vector3Normalize(Vector3CrossProduct(forward, Vector3{0.0f, 1.0f, 0.0f}));

    Vector3 move_world = Vector3Add(
        Vector3Scale(right, move_local.x),
        Vector3Scale(forward, move_local.z));
    if (Vector3LengthSqr(move_world) > 0.0f)
    {
        move_world = Vector3Normalize(move_world);
    }

    const float target_speed = sprinting_ ? sprint_speed_ : walk_speed_;
    const Vector3 horizontal_velocity = Vector3Scale(move_world, target_speed);

    const Vector3 current_velocity = physics_world_.GetBodyLinearVelocity(body_id_);
    Vector3 next_velocity = Vector3{
        horizontal_velocity.x,
        current_velocity.y,
        horizontal_velocity.z};

    if (jump_requested_ && grounded_)
    {
        next_velocity.y = jump_velocity_;
        grounded_ = false;
    }
    jump_requested_ = false;

    physics_world_.SetBodyLinearVelocity(body_id_, next_velocity, true);
    physics_world_.SetBodyAngularVelocity(body_id_, Vector3Zero(), false);
}

void FpsPlayerController::UpdateCamera(Camera3D &camera) const
{
    if (!active_ || body_id_.IsInvalid() || !physics_world_.IsBodyAdded(body_id_))
    {
        return;
    }

    const Vector3 body_position = physics_world_.GetBodyPosition(body_id_);
    const Vector3 eye_position = Vector3Add(
        body_position,
        Vector3{0.0f, eye_offset_from_center_, 0.0f});

    const float cos_pitch = cosf(pitch_);
    const Vector3 forward = Vector3{
        cos_pitch * sinf(yaw_),
        sinf(pitch_),
        cos_pitch * cosf(yaw_)};

    camera.position = eye_position;
    camera.target = Vector3Add(eye_position, forward);
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = 75.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

void FpsPlayerController::SetViewDirection(const Vector3 &look_direction)
{
    Vector3 direction = look_direction;
    if (Vector3LengthSqr(direction) < 0.0001f)
    {
        direction = Vector3{0.0f, 0.0f, 1.0f};
    }
    else
    {
        direction = Vector3Normalize(direction);
    }

    yaw_ = atan2f(direction.x, direction.z);
    pitch_ = asinf(std::clamp(direction.y, -1.0f, 1.0f));
}

bool FpsPlayerController::ComputeGrounded() const
{
    if (body_id_.IsInvalid() || !physics_world_.IsBodyAdded(body_id_))
    {
        return false;
    }

    const Vector3 body_position = physics_world_.GetBodyPosition(body_id_);
    const Vector3 origin = Vector3Add(body_position, Vector3{0.0f, 0.10f, 0.0f});

    const float probe_distance =
        capsule_half_height_ + capsule_radius_ + ground_probe_padding_;

    JPH::BodyID hit_body;
    Vector3 hit_point{};
    return physics_world_.RayCast(origin,
                                  Vector3{0.0f, -1.0f, 0.0f},
                                  probe_distance,
                                  hit_body,
                                  hit_point,
                                  body_id_);
}
} // namespace game::fps
