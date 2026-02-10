#include "SceneSystem.h"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace
{
Matrix MatrixFromTransform(const Vector3 &position,
                           const Quaternion &rotation,
                           const Vector3 &scale)
{
    const Matrix translation = MatrixTranslate(position.x, position.y, position.z);
    const Matrix rotation_m = QuaternionToMatrix(QuaternionNormalize(rotation));
    const Matrix scaling = MatrixScale(scale.x, scale.y, scale.z);
    return MatrixMultiply(MatrixMultiply(scaling, rotation_m), translation);
}

void DrawOrientedBox(const Vector3 &position,
                     const Quaternion &rotation,
                     const Vector3 &half_extents,
                     Color fill,
                     Color wire)
{
    const Mesh cube = GenMeshCube(1.0f, 1.0f, 1.0f);
    Material default_material = LoadMaterialDefault();
    default_material.maps[MATERIAL_MAP_DIFFUSE].color = fill;

    const Matrix transform =
        MatrixFromTransform(position, rotation, Vector3Scale(half_extents, 2.0f));
    DrawMesh(cube, default_material, transform);

    const Vector3 corners[8] = {
        Vector3{-half_extents.x, -half_extents.y, -half_extents.z},
        Vector3{half_extents.x, -half_extents.y, -half_extents.z},
        Vector3{half_extents.x, half_extents.y, -half_extents.z},
        Vector3{-half_extents.x, half_extents.y, -half_extents.z},
        Vector3{-half_extents.x, -half_extents.y, half_extents.z},
        Vector3{half_extents.x, -half_extents.y, half_extents.z},
        Vector3{half_extents.x, half_extents.y, half_extents.z},
        Vector3{-half_extents.x, half_extents.y, half_extents.z}};

    Vector3 world[8]{};
    for (int i = 0; i < 8; ++i)
    {
        const Vector3 rotated = Vector3RotateByQuaternion(corners[i], rotation);
        world[i] = Vector3Add(position, rotated);
    }

    const int edges[12][2] = {
        {0, 1}, {1, 2}, {2, 3}, {3, 0},
        {4, 5}, {5, 6}, {6, 7}, {7, 4},
        {0, 4}, {1, 5}, {2, 6}, {3, 7}};
    for (const auto &edge : edges)
    {
        DrawLine3D(world[edge[0]], world[edge[1]], wire);
    }

    UnloadMesh(cube);
    UnloadMaterial(default_material);
}

void DrawOrientedSphere(const Vector3 &position, float radius, Color fill, Color wire)
{
    DrawSphere(position, radius, fill);
    DrawSphereWires(position, radius, 14, 14, wire);
}

void DrawOrientedCapsule(const Vector3 &position,
                        const Quaternion &rotation,
                        float half_height,
                        float radius,
                        Color fill,
                        Color wire)
{
    const Vector3 local_bottom{0.0f, -half_height, 0.0f};
    const Vector3 local_top{0.0f, half_height, 0.0f};
    const Vector3 bottom =
        Vector3Add(position, Vector3RotateByQuaternion(local_bottom, rotation));
    const Vector3 top = Vector3Add(position, Vector3RotateByQuaternion(local_top, rotation));

    DrawCapsule(bottom, top, radius, 10, 12, fill);
    DrawCapsuleWires(bottom, top, radius, 10, 12, wire);
}

void DrawOrientedCylinder(const Vector3 &position,
                         const Quaternion &rotation,
                         float half_height,
                         float radius,
                         Color fill,
                         Color wire)
{
    const Vector3 local_bottom{0.0f, -half_height, 0.0f};
    const Vector3 local_top{0.0f, half_height, 0.0f};
    const Vector3 bottom =
        Vector3Add(position, Vector3RotateByQuaternion(local_bottom, rotation));
    const Vector3 top = Vector3Add(position, Vector3RotateByQuaternion(local_top, rotation));

    DrawCylinderEx(bottom, top, radius, radius, 16, fill);
    DrawCylinderWiresEx(bottom, top, radius, radius, 16, wire);
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
    AddCapsule(Vector3{-1.8f, 3.4f, 0.8f}, 0.5f, 0.28f, Color{130, 230, 170, 255});
    AddCylinder(Vector3{1.4f, 4.0f, -1.0f}, 0.55f, 0.30f, Color{200, 170, 255, 255});
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
        const Color fill = highlighted ? Fade(YELLOW, 0.75f) : object.color;
        const Color wire = highlighted ? GOLD : Fade(BLACK, 0.45f);

        switch (object.shape)
        {
        case SceneShapeType::Sphere:
            DrawOrientedSphere(position, object.radius, fill, wire);
            break;
        case SceneShapeType::Capsule:
            DrawOrientedCapsule(position,
                                rotation,
                                object.half_height,
                                object.radius,
                                fill,
                                wire);
            break;
        case SceneShapeType::Cylinder:
            DrawOrientedCylinder(position,
                                 rotation,
                                 object.half_height,
                                 object.radius,
                                 fill,
                                 wire);
            break;
        case SceneShapeType::Ground:
        case SceneShapeType::Box:
            DrawOrientedBox(position, rotation, object.half_extents, fill, wire);
            break;
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
    object.half_height = half_extents.y;
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
    object.half_height = radius;
    object.color = color;
    object.body_id = body_id;
    objects_.push_back(object);
    return object.id;
}

int SceneSystem::AddCapsule(const Vector3 &position,
                            float half_height,
                            float radius,
                            Color color)
{
    if (color.a == 0)
    {
        color = RandomBrightColor();
    }

    const JPH::BodyID body_id =
        physics_world_.CreateDynamicCapsule(position, half_height, radius);
    if (body_id.IsInvalid())
    {
        return 0;
    }

    SceneObject object{};
    object.id = next_id_++;
    object.shape = SceneShapeType::Capsule;
    object.dynamic = true;
    object.half_extents = Vector3{radius, half_height + radius, radius};
    object.radius = radius;
    object.half_height = half_height;
    object.color = color;
    object.body_id = body_id;
    objects_.push_back(object);
    return object.id;
}

int SceneSystem::AddCylinder(const Vector3 &position,
                             float half_height,
                             float radius,
                             Color color)
{
    if (color.a == 0)
    {
        color = RandomBrightColor();
    }

    const JPH::BodyID body_id =
        physics_world_.CreateDynamicCylinder(position, half_height, radius);
    if (body_id.IsInvalid())
    {
        return 0;
    }

    SceneObject object{};
    object.id = next_id_++;
    object.shape = SceneShapeType::Cylinder;
    object.dynamic = true;
    object.half_extents = Vector3{radius, half_height, radius};
    object.radius = radius;
    object.half_height = half_height;
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

void SceneSystem::DrawPhysicsDebug(bool show_sleeping) const
{
    const auto debug_bodies = physics_world_.DebugBodies();
    for (const PhysicsDebugBody &debug_body : debug_bodies)
    {
        const Vector3 position = physics_world_.GetBodyPosition(debug_body.body_id);
        const Quaternion rotation = physics_world_.GetBodyRotation(debug_body.body_id);
        const Color wire = debug_body.dynamic ? GREEN : DARKGREEN;

        switch (debug_body.shape)
        {
        case PhysicsShapeType::Sphere:
            DrawOrientedSphere(position, debug_body.radius, BLANK, wire);
            break;
        case PhysicsShapeType::Capsule:
            DrawOrientedCapsule(position,
                                rotation,
                                debug_body.half_height,
                                debug_body.radius,
                                BLANK,
                                wire);
            break;
        case PhysicsShapeType::Cylinder:
            DrawOrientedCylinder(position,
                                 rotation,
                                 debug_body.half_height,
                                 debug_body.radius,
                                 BLANK,
                                 wire);
            break;
        case PhysicsShapeType::Box:
            DrawOrientedBox(position,
                            rotation,
                            debug_body.half_extents,
                            BLANK,
                            wire);
            break;
        }

        if (show_sleeping && debug_body.dynamic)
        {
            DrawSphere(position, 0.03f, SKYBLUE);
        }
    }
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
