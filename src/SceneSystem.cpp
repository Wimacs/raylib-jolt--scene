#include "SceneSystem.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace
{
Vector3 QuaternionAxis(const Quaternion &rotation)
{
    Quaternion normalized = QuaternionNormalize(rotation);
    Vector3 axis{normalized.x, normalized.y, normalized.z};
    const float length = Vector3Length(axis);
    if (length < 0.0001f)
    {
        return Vector3{0.0f, 1.0f, 0.0f};
    }
    return Vector3Scale(axis, 1.0f / length);
}

float QuaternionAngle(const Quaternion &rotation)
{
    Quaternion normalized = QuaternionNormalize(rotation);
    const float clamped_w = std::max(-1.0f, std::min(1.0f, normalized.w));
    return 2.0f * std::acos(clamped_w);
}
} // namespace

SceneSystem::SceneSystem(PhysicsWorld &physics_world) : physics_world_(physics_world)
{
}

void SceneSystem::InitializeDefaultScene()
{
    objects_.clear();
    next_id_ = 1;
    drag_state_.reset();

    AddBox(Vector3{0.0f, -1.0f, 0.0f},
           Vector3{20.0f, 1.0f, 20.0f},
           false,
           Color{120, 120, 120, 255});

    AddBox(Vector3{0.0f, 2.5f, 0.0f},
           Vector3{0.6f, 0.6f, 0.6f},
           true,
           Color{80, 180, 255, 255});

    AddSphere(Vector3{2.5f, 3.0f, 0.0f}, 0.5f, true, Color{255, 150, 70, 255});
    AddBox(Vector3{-2.0f, 4.5f, -1.2f},
           Vector3{0.45f, 0.45f, 0.45f},
           true,
           Color{140, 255, 170, 255});
}

void SceneSystem::Step(float delta_time)
{
    physics_world_.Step(delta_time);
}

void SceneSystem::Draw(int highlighted_object_id) const
{
    for (const SceneObject &object : objects_)
    {
        if (!physics_world_.IsBodyAdded(object.body_id))
        {
            continue;
        }

        const Vector3 position = physics_world_.GetBodyPosition(object.body_id);
        const Quaternion rotation = physics_world_.GetBodyRotation(object.body_id);

        const bool highlighted = object.id == highlighted_object_id;
        const Color tint = highlighted ? YELLOW : object.color;

        if (object.shape == SceneShapeType::Sphere)
        {
            DrawSphere(position, object.radius, tint);
            DrawSphereWires(position,
                            object.radius,
                            12,
                            12,
                            highlighted ? GOLD : Fade(BLACK, 0.4f));
            continue;
        }

        if (object.shape == SceneShapeType::Box || object.shape == SceneShapeType::Ground)
        {
            const Vector3 full_size = Vector3Scale(object.half_extents, 2.0f);
            DrawCubeV(position, full_size, tint);
            DrawCubeWiresV(position, full_size, highlighted ? GOLD : Fade(BLACK, 0.5f));

            if (object.dynamic)
            {
                const Vector3 axis = QuaternionAxis(rotation);
                const float angle = QuaternionAngle(rotation);
                if (angle > 0.01f)
                {
                    const Vector3 top =
                        Vector3Add(position, Vector3{0.0f, object.half_extents.y, 0.0f});
                    const Vector3 dir =
                        Vector3RotateByAxisAngle(Vector3{0.0f, 1.0f, 0.0f}, axis, angle);
                    DrawLine3D(top, Vector3Add(top, Vector3Scale(dir, 0.8f)), RED);
                }
            }
        }
    }
}

int SceneSystem::AddBox(const Vector3 &position,
                        const Vector3 &half_extents,
                        bool dynamic,
                        Color color)
{
    if (color.a == 0)
    {
        color = RandomBrightColor();
    }

    const JPH::BodyID body_id = dynamic
                                    ? physics_world_.CreateDynamicBox(position, half_extents)
                                    : physics_world_.CreateStaticBox(position, half_extents);
    if (body_id.IsInvalid())
    {
        return 0;
    }

    SceneObject object{};
    object.id = next_id_++;
    object.shape = dynamic ? SceneShapeType::Box : SceneShapeType::Ground;
    object.dynamic = dynamic;
    object.half_extents = half_extents;
    object.radius = std::max({half_extents.x, half_extents.y, half_extents.z});
    object.color = color;
    object.body_id = body_id;
    objects_.push_back(object);
    return object.id;
}

int SceneSystem::AddSphere(const Vector3 &position,
                           float radius,
                           bool dynamic,
                           Color color)
{
    if (color.a == 0)
    {
        color = RandomBrightColor();
    }

    JPH::BodyID body_id;
    if (dynamic)
    {
        body_id = physics_world_.CreateDynamicSphere(position, radius);
    }
    else
    {
        body_id = physics_world_.CreateStaticBox(position, Vector3{radius, radius, radius});
    }

    if (body_id.IsInvalid())
    {
        return 0;
    }

    SceneObject object{};
    object.id = next_id_++;
    object.shape = SceneShapeType::Sphere;
    object.dynamic = dynamic;
    object.half_extents = Vector3{radius, radius, radius};
    object.radius = radius;
    object.color = color;
    object.body_id = body_id;
    objects_.push_back(object);
    return object.id;
}

bool SceneSystem::RemoveObject(int object_id)
{
    auto index = FindObjectIndexById(object_id);
    if (!index.has_value())
    {
        return false;
    }

    physics_world_.DestroyBody(objects_[*index].body_id);
    objects_.erase(objects_.begin() + static_cast<long>(*index));

    if (drag_state_.has_value() && drag_state_->object_id == object_id)
    {
        drag_state_.reset();
    }
    return true;
}

bool SceneSystem::RemoveObjectByBody(JPH::BodyID body_id)
{
    const auto index = FindObjectIndexByBody(body_id);
    if (!index.has_value())
    {
        return false;
    }

    return RemoveObject(objects_[*index].id);
}

std::optional<int> SceneSystem::PickObject(const Camera3D &camera,
                                           const Vector2 &screen,
                                           bool dynamic_only) const
{
    const Ray ray = GetMouseRay(screen, camera);
    JPH::BodyID hit_body;
    Vector3 hit_point{};
    const bool has_hit =
        physics_world_.RayCast(ray.position, ray.direction, 500.0f, hit_body, hit_point);
    if (!has_hit)
    {
        return std::nullopt;
    }

    const auto index = FindObjectIndexByBody(hit_body);
    if (!index.has_value())
    {
        return std::nullopt;
    }

    const SceneObject &object = objects_[*index];
    if (dynamic_only && !object.dynamic)
    {
        return std::nullopt;
    }

    return object.id;
}

bool SceneSystem::StartDrag(const Camera3D &camera, const Vector2 &screen)
{
    const Ray ray = GetMouseRay(screen, camera);
    JPH::BodyID hit_body;
    Vector3 hit_point{};
    if (!physics_world_.RayCast(ray.position, ray.direction, 500.0f, hit_body, hit_point))
    {
        return false;
    }

    const auto index = FindObjectIndexByBody(hit_body);
    if (!index.has_value())
    {
        return false;
    }

    SceneObject &object = objects_[*index];
    if (!object.dynamic)
    {
        return false;
    }

    const Vector3 object_position = physics_world_.GetBodyPosition(object.body_id);
    const float distance = Vector3Distance(camera.position, hit_point);

    drag_state_ = DragState{object.id, distance, Vector3Subtract(hit_point, object_position)};
    physics_world_.SetBodyVelocityZero(object.body_id);
    return true;
}

void SceneSystem::UpdateDrag(const Camera3D &camera, const Vector2 &screen)
{
    if (!drag_state_.has_value())
    {
        return;
    }

    const auto index = FindObjectIndexById(drag_state_->object_id);
    if (!index.has_value())
    {
        drag_state_.reset();
        return;
    }

    SceneObject &object = objects_[*index];
    const Ray ray = GetMouseRay(screen, camera);

    const Vector3 direction = Vector3Normalize(ray.direction);
    const Vector3 target_point = Vector3Add(
        camera.position,
        Vector3Scale(direction, drag_state_->target_distance));
    const Vector3 new_position = Vector3Subtract(target_point, drag_state_->local_pick_offset);

    const Quaternion current_rotation = physics_world_.GetBodyRotation(object.body_id);
    physics_world_.SetBodyTransform(object.body_id, new_position, current_rotation, true);
    physics_world_.SetBodyVelocityZero(object.body_id);
}

void SceneSystem::EndDrag()
{
    drag_state_.reset();
}

const std::vector<SceneObject> &SceneSystem::Objects() const
{
    return objects_;
}

bool SceneSystem::IsDragging() const
{
    return drag_state_.has_value();
}

int SceneSystem::DraggingObjectId() const
{
    if (!drag_state_.has_value())
    {
        return 0;
    }
    return drag_state_->object_id;
}

std::optional<size_t> SceneSystem::FindObjectIndexById(int object_id)
{
    for (size_t i = 0; i < objects_.size(); ++i)
    {
        if (objects_[i].id == object_id)
        {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<size_t> SceneSystem::FindObjectIndexById(int object_id) const
{
    for (size_t i = 0; i < objects_.size(); ++i)
    {
        if (objects_[i].id == object_id)
        {
            return i;
        }
    }
    return std::nullopt;
}

std::optional<size_t> SceneSystem::FindObjectIndexByBody(JPH::BodyID body_id) const
{
    for (size_t i = 0; i < objects_.size(); ++i)
    {
        if (objects_[i].body_id == body_id)
        {
            return i;
        }
    }
    return std::nullopt;
}

Color SceneSystem::RandomBrightColor()
{
    const unsigned char red = static_cast<unsigned char>(GetRandomValue(90, 255));
    const unsigned char green = static_cast<unsigned char>(GetRandomValue(90, 255));
    const unsigned char blue = static_cast<unsigned char>(GetRandomValue(90, 255));
    return Color{red, green, blue, 255};
}
