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

Vector3 NormalizeOr(const Vector3 &value, const Vector3 &fallback)
{
    if (Vector3LengthSqr(value) < 0.000001f)
    {
        return fallback;
    }
    return Vector3Normalize(value);
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
    grounded_ = false;
    wall_running_ = false;
    wall_run_elapsed_seconds_ = 0.0f;
    wall_normal_ = Vector3Zero();
    StopGrapple(false);
    grapple_cooldown_seconds_ = 0.0f;
    grapple_rope_length_ = 0.0f;

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
    wall_running_ = false;
    wall_run_elapsed_seconds_ = 0.0f;
    wall_normal_ = Vector3Zero();
    StopGrapple(false);
    grapple_cooldown_seconds_ = 0.0f;
    grapple_rope_length_ = 0.0f;
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
    grapple_pressed_ = false;
    wall_running_ = false;
    wall_run_elapsed_seconds_ = 0.0f;
    wall_normal_ = Vector3Zero();
    StopGrapple(false);
    grapple_rope_length_ = 0.0f;
}

bool FpsPlayerController::IsActive() const
{
    return active_;
}

bool FpsPlayerController::IsGrounded() const
{
    return grounded_;
}

bool FpsPlayerController::IsWallRunning() const
{
    return wall_running_;
}

bool FpsPlayerController::IsGrappling() const
{
    return grappling_;
}

Vector3 FpsPlayerController::GrappleAnchorPoint() const
{
    return grapple_anchor_;
}

JPH::BodyID FpsPlayerController::BodyId() const
{
    return body_id_;
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

    grapple_pressed_ = IsKeyPressed(KEY_Q);
}

void FpsPlayerController::FixedUpdate(float fixed_delta_seconds)
{
    if (!active_ || body_id_.IsInvalid() || !physics_world_.IsBodyAdded(body_id_))
    {
        return;
    }

    grapple_cooldown_seconds_ = std::max(0.0f, grapple_cooldown_seconds_ - fixed_delta_seconds);

    if (grapple_pressed_)
    {
        if (grappling_)
        {
            StopGrapple(true);
        }
        else
        {
            TryStartGrapple();
        }
    }

    grounded_ = ComputeGrounded();
    if (grounded_)
    {
        wall_running_ = false;
        wall_run_elapsed_seconds_ = 0.0f;
    }

    Vector3 move_local = Vector3{move_input_.x, 0.0f, move_input_.y};
    if (Vector3LengthSqr(move_local) > 1.0f)
    {
        move_local = Vector3Normalize(move_local);
    }

    const Vector3 forward = HorizontalForward();
    const Vector3 right =
        NormalizeOr(Vector3CrossProduct(forward, Vector3{0.0f, 1.0f, 0.0f}),
                    Vector3{1.0f, 0.0f, 0.0f});

    Vector3 move_world = Vector3Add(
        Vector3Scale(right, move_local.x),
        Vector3Scale(forward, move_local.z));
    if (Vector3LengthSqr(move_world) > 0.0f)
    {
        move_world = Vector3Normalize(move_world);
    }

    const float target_speed = sprinting_ ? sprint_speed_ : walk_speed_;
    const Vector3 current_velocity = physics_world_.GetBodyLinearVelocity(body_id_);
    Vector3 next_velocity = current_velocity;
    const Vector3 horizontal_current = Vector3{current_velocity.x, 0.0f, current_velocity.z};
    const Vector3 horizontal_target = Vector3Scale(move_world, target_speed);

    if (!grounded_ && !grappling_ && move_input_.y > 0.1f)
    {
        Vector3 detected_wall_normal{};
        if (FindRunnableWall(detected_wall_normal))
        {
            wall_running_ = true;
            wall_normal_ = detected_wall_normal;
            wall_run_elapsed_seconds_ += fixed_delta_seconds;
        }
        else
        {
            wall_running_ = false;
            wall_run_elapsed_seconds_ = 0.0f;
        }
    }
    else
    {
        wall_running_ = false;
        wall_run_elapsed_seconds_ = 0.0f;
    }

    if (wall_running_ && wall_run_elapsed_seconds_ > wall_run_max_seconds_)
    {
        wall_running_ = false;
        wall_run_elapsed_seconds_ = 0.0f;
    }

    if (wall_running_)
    {
        const float wall_forward_component = Vector3DotProduct(forward, wall_normal_);
        Vector3 wall_tangent = Vector3Subtract(
            forward,
            Vector3Scale(wall_normal_, wall_forward_component));
        wall_tangent = NormalizeOr(
            wall_tangent,
            NormalizeOr(Vector3CrossProduct(Vector3{0.0f, 1.0f, 0.0f}, wall_normal_),
                        forward));

        if (Vector3DotProduct(wall_tangent, forward) < 0.0f)
        {
            wall_tangent = Vector3Negate(wall_tangent);
        }

        const Vector3 side_tangent = NormalizeOr(
            Vector3CrossProduct(Vector3{0.0f, 1.0f, 0.0f}, wall_normal_),
            wall_tangent);
        Vector3 wall_direction = Vector3Add(
            wall_tangent,
            Vector3Scale(side_tangent, move_local.x * 0.35f));
        wall_direction = NormalizeOr(wall_direction, wall_tangent);

        next_velocity.x = wall_direction.x * wall_run_speed_ - wall_normal_.x * wall_run_stick_velocity_;
        next_velocity.z = wall_direction.z * wall_run_speed_ - wall_normal_.z * wall_run_stick_velocity_;
        next_velocity.y = std::max(current_velocity.y, -wall_run_fall_speed_);
    }
    else if (grappling_)
    {
        const Vector3 control_velocity = Vector3Scale(move_world, target_speed * 0.50f);
        const float control_lerp = std::clamp(6.5f * fixed_delta_seconds, 0.0f, 1.0f);
        next_velocity.x = Lerp(next_velocity.x, control_velocity.x, control_lerp);
        next_velocity.z = Lerp(next_velocity.z, control_velocity.z, control_lerp);
    }
    else if (grounded_)
    {
        const float ground_lerp = std::clamp(ground_acceleration_ * fixed_delta_seconds, 0.0f, 1.0f);
        next_velocity.x = Lerp(current_velocity.x, horizontal_target.x, ground_lerp);
        next_velocity.z = Lerp(current_velocity.z, horizontal_target.z, ground_lerp);
    }
    else
    {
        Vector3 delta = Vector3Subtract(horizontal_target, horizontal_current);
        const float delta_len = Vector3Length(delta);
        const float max_delta = air_acceleration_ * fixed_delta_seconds;
        if (delta_len > max_delta && delta_len > 0.0001f)
        {
            delta = Vector3Scale(delta, max_delta / delta_len);
        }
        const Vector3 horizontal_next = Vector3Add(horizontal_current, delta);
        next_velocity.x = horizontal_next.x;
        next_velocity.z = horizontal_next.z;
    }

    if (grappling_)
    {
        grapple_remaining_seconds_ -= fixed_delta_seconds;
        const Vector3 body_position = physics_world_.GetBodyPosition(body_id_);
        const Vector3 to_anchor = Vector3Subtract(grapple_anchor_, body_position);
        const float distance_to_anchor = Vector3Length(to_anchor);

        grapple_rope_length_ = std::max(
            grapple_detach_distance_,
            grapple_rope_length_ - grapple_retract_speed_ * fixed_delta_seconds);

        if (distance_to_anchor <= grapple_detach_distance_ ||
            grapple_remaining_seconds_ <= 0.0f)
        {
            if (distance_to_anchor > 0.0001f)
            {
                const Vector3 release_direction =
                    Vector3Scale(to_anchor, 1.0f / distance_to_anchor);
                next_velocity = Vector3Add(
                    next_velocity,
                    Vector3Scale(release_direction, grapple_release_boost_));
            }
            StopGrapple(true);
        }
        else
        {
            const Vector3 pull_direction = Vector3Scale(to_anchor, 1.0f / distance_to_anchor);
            const float distance_factor = std::clamp(
                distance_to_anchor / std::max(0.001f, grapple_rope_length_),
                0.85f,
                1.75f);
            const float pull_target_speed = std::min(
                grapple_max_speed_,
                grapple_retract_speed_ + distance_to_anchor * 1.25f);
            const Vector3 pull_target_velocity =
                Vector3Scale(pull_direction, pull_target_speed);

            const float steer_lerp = std::clamp(
                grapple_direction_steer_strength_ * fixed_delta_seconds,
                0.0f,
                1.0f);
            next_velocity = Vector3Lerp(next_velocity, pull_target_velocity, steer_lerp);

            next_velocity = Vector3Add(
                next_velocity,
                Vector3Scale(pull_direction,
                             grapple_pull_accel_ * distance_factor * fixed_delta_seconds));

            if (distance_to_anchor > grapple_rope_length_)
            {
                const float rope_excess = distance_to_anchor - grapple_rope_length_;
                next_velocity = Vector3Add(
                    next_velocity,
                    Vector3Scale(pull_direction,
                                 rope_excess * grapple_tension_strength_ * fixed_delta_seconds));
            }

            // Prevent moving away from the anchor while rope is under tension.
            const float pull_speed = Vector3DotProduct(next_velocity, pull_direction);
            if (pull_speed < 0.0f)
            {
                next_velocity = Vector3Subtract(
                    next_velocity,
                    Vector3Scale(pull_direction, pull_speed));
            }

            if (grapple_anchor_.y > body_position.y + 0.25f)
            {
                next_velocity.y += 8.0f * fixed_delta_seconds;
            }
            next_velocity.y = std::max(next_velocity.y, -grapple_max_fall_speed_);

            const float speed = Vector3Length(next_velocity);
            if (speed > grapple_max_speed_)
            {
                next_velocity = Vector3Scale(next_velocity, grapple_max_speed_ / speed);
            }
        }
    }

    if (jump_requested_)
    {
        if (grappling_)
        {
            StopGrapple(true);
            next_velocity.y = std::max(next_velocity.y, jump_velocity_ * 0.9f);
        }
        else if (wall_running_)
        {
            const Vector3 wall_jump = Vector3Add(
                Vector3Scale(Vector3Negate(wall_normal_), wall_jump_push_),
                Vector3Scale(forward, wall_jump_forward_boost_));
            next_velocity.x = wall_jump.x + current_velocity.x * 0.35f;
            next_velocity.z = wall_jump.z + current_velocity.z * 0.35f;
            next_velocity.y = std::max(jump_velocity_, current_velocity.y + 1.2f);
            wall_running_ = false;
            wall_run_elapsed_seconds_ = 0.0f;
            grounded_ = false;
        }
        else if (grounded_)
        {
            next_velocity.y = jump_velocity_;
            grounded_ = false;
        }
    }
    jump_requested_ = false;
    grapple_pressed_ = false;

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
    const Vector3 eye_position = Vector3Add(body_position, Vector3{0.0f, eye_offset_from_center_, 0.0f});
    const Vector3 forward = ViewForward();

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

Vector3 FpsPlayerController::ViewForward() const
{
    const float cos_pitch = cosf(pitch_);
    return Vector3{
        cos_pitch * sinf(yaw_),
        sinf(pitch_),
        cos_pitch * cosf(yaw_)};
}

Vector3 FpsPlayerController::EyePosition() const
{
    const Vector3 body_position = physics_world_.GetBodyPosition(body_id_);
    return Vector3Add(body_position, Vector3{0.0f, eye_offset_from_center_, 0.0f});
}

Vector3 FpsPlayerController::HorizontalForward() const
{
    const Vector3 forward = Vector3{sinf(yaw_), 0.0f, cosf(yaw_)};
    return NormalizeOr(forward, Vector3{0.0f, 0.0f, 1.0f});
}

bool FpsPlayerController::FindRunnableWall(Vector3 &out_wall_normal) const
{
    out_wall_normal = Vector3Zero();

    if (body_id_.IsInvalid() || !physics_world_.IsBodyAdded(body_id_))
    {
        return false;
    }

    const Vector3 origin = Vector3Add(EyePosition(), Vector3{0.0f, -0.22f, 0.0f});
    const Vector3 forward = HorizontalForward();
    const Vector3 right = NormalizeOr(Vector3CrossProduct(forward, Vector3{0.0f, 1.0f, 0.0f}),
                                      Vector3{1.0f, 0.0f, 0.0f});
    const float probe_distance = capsule_radius_ + wall_run_probe_distance_;

    struct WallHit
    {
        bool hit{false};
        Vector3 origin{0.0f, 0.0f, 0.0f};
        Vector3 point{0.0f, 0.0f, 0.0f};
        Vector3 ray_direction{0.0f, 0.0f, 0.0f};
        float distance{0.0f};
    };

    auto cast_wall_probe = [&](const Vector3 &probe_origin,
                               const Vector3 &direction) -> WallHit
    {
        WallHit result{};
        JPH::BodyID hit_body;
        Vector3 hit_point{};
        if (!physics_world_.RayCast(probe_origin,
                                    direction,
                                    probe_distance,
                                    hit_body,
                                    hit_point,
                                    body_id_))
        {
            return result;
        }

        result.hit = true;
        result.origin = probe_origin;
        result.point = hit_point;
        result.ray_direction = direction;
        result.distance = Vector3Distance(probe_origin, hit_point);
        return result;
    };

    WallHit selected{};
    const Vector3 height_offsets[3]{
        Vector3{0.0f, 0.35f, 0.0f},
        Vector3{0.0f, 0.0f, 0.0f},
        Vector3{0.0f, -0.35f, 0.0f}};
    for (const Vector3 &height_offset : height_offsets)
    {
        const Vector3 probe_origin = Vector3Add(origin, height_offset);
        const WallHit right_hit = cast_wall_probe(probe_origin, right);
        const WallHit left_hit = cast_wall_probe(probe_origin, Vector3Negate(right));

        if (right_hit.hit &&
            (!selected.hit || right_hit.distance < selected.distance))
        {
            selected = right_hit;
        }
        if (left_hit.hit &&
            (!selected.hit || left_hit.distance < selected.distance))
        {
            selected = left_hit;
        }
    }

    if (!selected.hit)
    {
        return false;
    }

    Vector3 wall_normal = Vector3Subtract(selected.origin, selected.point);
    if (Vector3LengthSqr(wall_normal) < 0.0001f)
    {
        wall_normal = Vector3Negate(selected.ray_direction);
    }
    wall_normal = NormalizeOr(wall_normal, Vector3Negate(selected.ray_direction));

    // Reject floor / ceiling hits: runnable wall should be close to vertical.
    if (fabsf(wall_normal.y) > 0.45f)
    {
        return false;
    }

    wall_normal.y = 0.0f;
    wall_normal = NormalizeOr(wall_normal, Vector3Negate(selected.ray_direction));
    out_wall_normal = wall_normal;
    return true;
}

void FpsPlayerController::TryStartGrapple()
{
    if (!grapple_pressed_ ||
        grappling_ ||
        grapple_cooldown_seconds_ > 0.0f ||
        body_id_.IsInvalid() ||
        !physics_world_.IsBodyAdded(body_id_))
    {
        return;
    }

    const Vector3 origin = EyePosition();
    const Vector3 direction = ViewForward();
    JPH::BodyID hit_body;
    Vector3 hit_point{};
    if (!physics_world_.RayCast(origin,
                                direction,
                                grapple_range_,
                                hit_body,
                                hit_point,
                                body_id_))
    {
        return;
    }

    grappling_ = true;
    grapple_anchor_ = hit_point;
    grapple_remaining_seconds_ = grapple_max_seconds_;
    const Vector3 body_position = physics_world_.GetBodyPosition(body_id_);
    grapple_rope_length_ = std::max(
        grapple_detach_distance_,
        Vector3Distance(body_position, hit_point));
    wall_running_ = false;
    wall_run_elapsed_seconds_ = 0.0f;
}

void FpsPlayerController::StopGrapple(bool start_cooldown)
{
    if (!grappling_)
    {
        return;
    }

    grappling_ = false;
    grapple_rope_length_ = 0.0f;
    grapple_remaining_seconds_ = 0.0f;
    if (start_cooldown)
    {
        grapple_cooldown_seconds_ = grapple_cooldown_after_use_;
    }
}
} // namespace game::fps
