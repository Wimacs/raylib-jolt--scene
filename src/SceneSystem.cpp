#include "SceneSystem.h"

#include <nlohmann/json.hpp>
#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <unordered_map>

namespace
{
using json = nlohmann::json;

json VecToJson(const Vector3 &value)
{
    return json{{"x", value.x}, {"y", value.y}, {"z", value.z}};
}

Vector3 JsonToVec(const json &value, const Vector3 &fallback)
{
    if (!value.is_object())
    {
        return fallback;
    }
    return Vector3{
        value.value("x", fallback.x),
        value.value("y", fallback.y),
        value.value("z", fallback.z)};
}

json ColorToJson(const Color &color)
{
    return json{{"r", color.r}, {"g", color.g}, {"b", color.b}, {"a", color.a}};
}

Color JsonToColor(const json &value, const Color &fallback)
{
    if (!value.is_object())
    {
        return fallback;
    }
    return Color{
        static_cast<unsigned char>(value.value("r", fallback.r)),
        static_cast<unsigned char>(value.value("g", fallback.g)),
        static_cast<unsigned char>(value.value("b", fallback.b)),
        static_cast<unsigned char>(value.value("a", fallback.a))};
}

const char *ShapeToString(SceneShapeType shape)
{
    switch (shape)
    {
    case SceneShapeType::Box:
        return "box";
    case SceneShapeType::Sphere:
        return "sphere";
    case SceneShapeType::Capsule:
        return "capsule";
    case SceneShapeType::Cylinder:
        return "cylinder";
    case SceneShapeType::Ground:
        return "ground";
    }
    return "box";
}

SceneShapeType StringToShape(const std::string &shape)
{
    if (shape == "sphere")
    {
        return SceneShapeType::Sphere;
    }
    if (shape == "capsule")
    {
        return SceneShapeType::Capsule;
    }
    if (shape == "cylinder")
    {
        return SceneShapeType::Cylinder;
    }
    if (shape == "ground")
    {
        return SceneShapeType::Ground;
    }
    return SceneShapeType::Box;
}

const char *ConstraintTypeToString(SceneConstraintType type)
{
    switch (type)
    {
    case SceneConstraintType::Fixed:
        return "fixed";
    case SceneConstraintType::Point:
        return "point";
    case SceneConstraintType::Distance:
        return "distance";
    case SceneConstraintType::Hinge:
        return "hinge";
    case SceneConstraintType::Slider:
        return "slider";
    }
    return "fixed";
}

SceneConstraintType StringToConstraintType(const std::string &type)
{
    if (type == "point")
    {
        return SceneConstraintType::Point;
    }
    if (type == "distance")
    {
        return SceneConstraintType::Distance;
    }
    if (type == "hinge")
    {
        return SceneConstraintType::Hinge;
    }
    if (type == "slider")
    {
        return SceneConstraintType::Slider;
    }
    return SceneConstraintType::Fixed;
}

const char *MotorModeToString(SceneMotorMode mode)
{
    switch (mode)
    {
    case SceneMotorMode::Off:
        return "off";
    case SceneMotorMode::Velocity:
        return "velocity";
    case SceneMotorMode::Position:
        return "position";
    }
    return "off";
}

SceneMotorMode StringToMotorMode(const std::string &mode)
{
    if (mode == "velocity")
    {
        return SceneMotorMode::Velocity;
    }
    if (mode == "position")
    {
        return SceneMotorMode::Position;
    }
    return SceneMotorMode::Off;
}

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
    physics_world_.ClearConstraints();
    for (const SceneObject &object : objects_)
    {
        physics_world_.DestroyBody(object.body_id);
    }

    objects_.clear();
    constraints_.clear();
    next_id_ = 1;
    drag_state_.reset();

    // Built-in fallback now mirrors an FPS-friendly blockout arena.
    AddBox(Vector3{0.0f, -1.0f, 0.0f},
           Vector3{40.0f, 1.0f, 40.0f},
           false,
           Color{110, 115, 120, 255},
           ScenePhysics{});

    AddBox(Vector3{0.0f, 2.0f, -20.0f},
           Vector3{20.0f, 3.0f, 1.0f},
           false,
           Color{90, 95, 105, 255},
           ScenePhysics{});
    AddBox(Vector3{0.0f, 2.0f, 20.0f},
           Vector3{20.0f, 3.0f, 1.0f},
           false,
           Color{90, 95, 105, 255},
           ScenePhysics{});
    AddBox(Vector3{-20.0f, 2.0f, 0.0f},
           Vector3{1.0f, 3.0f, 20.0f},
           false,
           Color{90, 95, 105, 255},
           ScenePhysics{});
    AddBox(Vector3{20.0f, 2.0f, 0.0f},
           Vector3{1.0f, 3.0f, 20.0f},
           false,
           Color{90, 95, 105, 255},
           ScenePhysics{});

    AddBox(Vector3{0.0f, 1.2f, -6.0f},
           Vector3{6.0f, 1.2f, 0.6f},
           false,
           Color{120, 120, 130, 255},
           ScenePhysics{});
    AddBox(Vector3{0.0f, 1.2f, 6.0f},
           Vector3{6.0f, 1.2f, 0.6f},
           false,
           Color{120, 120, 130, 255},
           ScenePhysics{});

    AddBox(Vector3{-7.0f, 1.5f, 0.0f},
           Vector3{2.5f, 1.5f, 2.5f},
           false,
           Color{100, 105, 115, 255},
           ScenePhysics{});
    AddBox(Vector3{7.0f, 1.5f, 0.0f},
           Vector3{2.5f, 1.5f, 2.5f},
           false,
           Color{100, 105, 115, 255},
           ScenePhysics{});

    AddBox(Vector3{0.0f, 2.5f, 0.0f},
           Vector3{1.0f, 2.5f, 1.0f},
           false,
           Color{115, 120, 130, 255},
           ScenePhysics{});

    AddBox(Vector3{-12.0f, 0.5f, -8.0f},
           Vector3{2.0f, 0.5f, 2.0f},
           false,
           Color{135, 140, 150, 255},
           ScenePhysics{});
    AddBox(Vector3{-12.0f, 1.0f, -11.0f},
           Vector3{2.0f, 0.5f, 2.0f},
           false,
           Color{135, 140, 150, 255},
           ScenePhysics{});
    AddBox(Vector3{-12.0f, 1.5f, -14.0f},
           Vector3{4.0f, 0.5f, 4.0f},
           false,
           Color{135, 140, 150, 255},
           ScenePhysics{});

    AddBox(Vector3{12.0f, 1.5f, 14.0f},
           Vector3{4.0f, 0.5f, 4.0f},
           false,
           Color{135, 140, 150, 255},
           ScenePhysics{});
    AddBox(Vector3{0.0f, 2.5f, 14.0f},
           Vector3{8.0f, 0.4f, 1.5f},
           false,
           Color{140, 145, 155, 255},
           ScenePhysics{});

    AddBox(Vector3{2.0f, 4.0f, 2.0f},
           Vector3{0.55f, 0.55f, 0.55f},
           true,
           Color{205, 150, 90, 255},
           DefaultDynamicPhysics());
    AddBox(Vector3{-3.0f, 5.0f, -2.0f},
           Vector3{0.55f, 0.55f, 0.55f},
           true,
           Color{205, 150, 90, 255},
           DefaultDynamicPhysics());
    AddSphere(Vector3{4.0f, 6.0f, -4.0f},
              0.45f,
              true,
              Color{90, 180, 235, 255},
              DefaultDynamicPhysics());
    AddSphere(Vector3{-5.0f, 4.0f, 5.0f},
              0.45f,
              true,
              Color{90, 180, 235, 255},
              DefaultDynamicPhysics());
    AddCylinder(Vector3{0.0f, 5.0f, 0.0f},
                0.50f,
                0.35f,
                Color{160, 215, 150, 255},
                DefaultDynamicPhysics());
    AddCapsule(Vector3{6.0f, 5.0f, 3.0f},
               0.55f,
               0.30f,
               Color{185, 155, 230, 255},
               DefaultDynamicPhysics());

    auto add_static_ramp =
        [this](const Vector3 &position,
               const Vector3 &half_extents,
               float angle_radians,
               Color color)
    {
        const int object_id = AddBox(position,
                                     half_extents,
                                     false,
                                     color,
                                     ScenePhysics{});
        const auto index = FindObjectIndexById(object_id);
        if (!index.has_value())
        {
            return;
        }

        const Quaternion rotation =
            QuaternionFromAxisAngle(Vector3{1.0f, 0.0f, 0.0f}, angle_radians);
        physics_world_.SetBodyTransform(objects_[*index].body_id,
                                        position,
                                        rotation,
                                        false);
    };

    add_static_ramp(Vector3{-13.0f, 1.05f, -2.0f},
                    Vector3{2.0f, 0.20f, 4.0f},
                    -0.174533f,
                    Color{170, 140, 110, 255});
    add_static_ramp(Vector3{-8.0f, 1.05f, -2.0f},
                    Vector3{2.0f, 0.20f, 4.0f},
                    -0.349066f,
                    Color{180, 132, 98, 255});
    add_static_ramp(Vector3{-3.0f, 1.05f, -2.0f},
                    Vector3{2.0f, 0.20f, 4.0f},
                    -0.523599f,
                    Color{188, 123, 87, 255});
    add_static_ramp(Vector3{2.0f, 1.05f, -2.0f},
                    Vector3{2.0f, 0.20f, 4.0f},
                    -0.610865f,
                    Color{196, 114, 79, 255});
    add_static_ramp(Vector3{7.0f, 1.05f, -2.0f},
                    Vector3{2.0f, 0.20f, 4.0f},
                    -0.785398f,
                    Color{204, 104, 70, 255});

    for (int i = 0; i < 8; ++i)
    {
        AddBox(Vector3{12.0f, 0.08f + static_cast<float>(i) * 0.16f, -12.0f + static_cast<float>(i) * 0.55f},
               Vector3{1.4f, 0.08f, 0.28f},
               false,
               Color{132, 136, 142, 255},
               ScenePhysics{});
    }
    AddBox(Vector3{12.0f, 1.40f, -7.40f},
           Vector3{2.2f, 0.16f, 1.20f},
           false,
           Color{144, 148, 154, 255},
           ScenePhysics{});
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
    return AddBox(position,
                  half_extents,
                  dynamic,
                  color,
                  dynamic ? DefaultDynamicPhysics() : ScenePhysics{});
}

int SceneSystem::AddSphere(const Vector3 &position,
                           float radius,
                           bool dynamic,
                           Color color)
{
    return AddSphere(position,
                     radius,
                     dynamic,
                     color,
                     dynamic ? DefaultDynamicPhysics() : ScenePhysics{});
}

int SceneSystem::AddCapsule(const Vector3 &position,
                            float half_height,
                            float radius,
                            Color color)
{
    return AddCapsule(position, half_height, radius, color, DefaultDynamicPhysics());
}

int SceneSystem::AddCylinder(const Vector3 &position,
                             float half_height,
                             float radius,
                             Color color)
{
    return AddCylinder(position, half_height, radius, color, DefaultDynamicPhysics());
}

int SceneSystem::AddBox(const Vector3 &position,
                        const Vector3 &half_extents,
                        bool dynamic,
                        Color color,
                        const ScenePhysics &physics)
{
    if (color.a == 0)
    {
        color = RandomBrightColor();
    }

    const BodySpawnResult spawn = physics_world_.CreateBox(
        position,
        half_extents,
        dynamic,
        ToBodyParams(physics));
    if (spawn.body_id.IsInvalid())
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
    object.physics = physics;
    object.body_id = spawn.body_id;
    objects_.push_back(object);
    return object.id;
}

int SceneSystem::AddSphere(const Vector3 &position,
                           float radius,
                           bool dynamic,
                           Color color,
                           const ScenePhysics &physics)
{
    if (color.a == 0)
    {
        color = RandomBrightColor();
    }

    const BodySpawnResult spawn = physics_world_.CreateSphere(
        position,
        radius,
        dynamic,
        ToBodyParams(physics));
    if (spawn.body_id.IsInvalid())
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
    object.physics = physics;
    object.body_id = spawn.body_id;
    objects_.push_back(object);
    return object.id;
}

int SceneSystem::AddCapsule(const Vector3 &position,
                            float half_height,
                            float radius,
                            Color color,
                            const ScenePhysics &physics)
{
    if (color.a == 0)
    {
        color = RandomBrightColor();
    }

    const BodySpawnResult spawn = physics_world_.CreateCapsule(
        position,
        half_height,
        radius,
        true,
        ToBodyParams(physics));
    if (spawn.body_id.IsInvalid())
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
    object.physics = physics;
    object.body_id = spawn.body_id;
    objects_.push_back(object);
    return object.id;
}

int SceneSystem::AddCylinder(const Vector3 &position,
                             float half_height,
                             float radius,
                             Color color,
                             const ScenePhysics &physics)
{
    if (color.a == 0)
    {
        color = RandomBrightColor();
    }

    const BodySpawnResult spawn = physics_world_.CreateCylinder(
        position,
        half_height,
        radius,
        true,
        ToBodyParams(physics));
    if (spawn.body_id.IsInvalid())
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
    object.physics = physics;
    object.body_id = spawn.body_id;
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

    const JPH::BodyID body_id = objects_[*index].body_id;
    physics_world_.DestroyBody(body_id);
    objects_.erase(objects_.begin() + static_cast<long>(*index));

    for (auto it = constraints_.begin(); it != constraints_.end();)
    {
        if (it->body1_id == object_id || it->body2_id == object_id)
        {
            it = constraints_.erase(it);
        }
        else
        {
            ++it;
        }
    }

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

void SceneSystem::DrawPhysicsDebug(bool show_sleeping,
                                   JPH::BodyID ignore_body_id) const
{
    const auto debug_bodies = physics_world_.DebugBodies();
    for (const PhysicsDebugBody &debug_body : debug_bodies)
    {
        if (!ignore_body_id.IsInvalid() && debug_body.body_id == ignore_body_id)
        {
            continue;
        }

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

bool SceneSystem::SaveToJson(const std::string &path) const
{
    json root;
    root["version"] = 2;
    root["units"] = json{
        {"length", "meter"},
        {"mass", "kilogram"},
        {"time", "second"},
        {"force", "newton"},
        {"torque", "newton_meter"},
        {"angle", "radian"},
        {"angular_velocity", "radian_per_second"}};
    root["objects"] = json::array();
    root["constraints"] = json::array();

    for (const SceneObject &object : objects_)
    {
        json item;
        item["id"] = object.id;
        item["shape"] = ShapeToString(object.shape);
        item["dynamic"] = object.dynamic;
        item["position"] = VecToJson(physics_world_.GetBodyPosition(object.body_id));
        item["rotation"] = json{
            {"x", physics_world_.GetBodyRotation(object.body_id).x},
            {"y", physics_world_.GetBodyRotation(object.body_id).y},
            {"z", physics_world_.GetBodyRotation(object.body_id).z},
            {"w", physics_world_.GetBodyRotation(object.body_id).w}};
        item["half_extents"] = VecToJson(object.half_extents);
        item["radius"] = object.radius;
        item["half_height"] = object.half_height;
        item["color"] = ColorToJson(object.color);

        json phys;
        phys["friction"] = object.physics.friction;
        phys["restitution"] = object.physics.restitution;
        phys["linear_damping"] = object.physics.linear_damping;
        phys["angular_damping"] = object.physics.angular_damping;
        phys["gravity_factor"] = object.physics.gravity_factor;
        phys["max_linear_velocity"] = object.physics.max_linear_velocity;
        phys["max_angular_velocity"] = object.physics.max_angular_velocity;
        phys["allow_sleeping"] = object.physics.allow_sleeping;
        phys["is_sensor"] = object.physics.is_sensor;
        phys["use_manifold_reduction"] = object.physics.use_manifold_reduction;
        phys["apply_gyroscopic_force"] = object.physics.apply_gyroscopic_force;
        phys["enhanced_internal_edge_removal"] = object.physics.enhanced_internal_edge_removal;
        phys["collide_kinematic_vs_non_dynamic"] = object.physics.collide_kinematic_vs_non_dynamic;
        phys["allow_dynamic_or_kinematic"] = object.physics.allow_dynamic_or_kinematic;
        phys["use_custom_mass"] = object.physics.use_custom_mass;
        phys["mass"] = object.physics.mass;
        phys["use_linear_cast"] = object.physics.use_linear_cast;
        phys["motion_quality"] = object.physics.use_linear_cast ? "linear_cast" : "discrete";
        item["physics"] = phys;

        root["objects"].push_back(item);
    }

    for (const SceneConstraint &constraint : constraints_)
    {
        json item;
        item["id"] = constraint.id;
        item["type"] = ConstraintTypeToString(constraint.type);
        item["body1"] = constraint.body1_id;
        item["body2"] = constraint.body2_id;
        item["point1"] = VecToJson(constraint.point1);
        item["point2"] = VecToJson(constraint.point2);
        item["axis1"] = VecToJson(constraint.axis1);
        item["axis2"] = VecToJson(constraint.axis2);
        item["normal1"] = VecToJson(constraint.normal1);
        item["normal2"] = VecToJson(constraint.normal2);
        item["min_limit"] = constraint.min_limit;
        item["max_limit"] = constraint.max_limit;
        item["max_friction"] = constraint.max_friction;
        item["auto_detect_point"] = constraint.auto_detect_point;
        item["enabled"] = constraint.enabled;
        item["motor"] = json{
            {"mode", MotorModeToString(constraint.motor.mode)},
            {"target_velocity", constraint.motor.target_velocity},
            {"target_position", constraint.motor.target_position},
            {"spring_frequency", constraint.motor.spring_frequency},
            {"spring_damping", constraint.motor.spring_damping},
            {"max_force", constraint.motor.max_force},
            {"max_torque", constraint.motor.max_torque}};
        root["constraints"].push_back(item);
    }

    std::ofstream out(path);
    if (!out.is_open())
    {
        return false;
    }

    out << root.dump(2);
    return true;
}

bool SceneSystem::LoadFromJson(const std::string &path, std::string &out_error)
{
    std::ifstream in(path);
    if (!in.is_open())
    {
        out_error = "Failed to open file: " + path;
        return false;
    }

    json root;
    try
    {
        in >> root;
    }
    catch (const std::exception &ex)
    {
        out_error = std::string("Invalid JSON: ") + ex.what();
        return false;
    }

    if (!root.contains("objects") || !root["objects"].is_array())
    {
        out_error = "JSON must contain an objects array";
        return false;
    }

    physics_world_.ClearConstraints();
    for (const SceneObject &object : objects_)
    {
        physics_world_.DestroyBody(object.body_id);
    }

    objects_.clear();
    constraints_.clear();
    next_id_ = 1;
    drag_state_.reset();

    std::vector<json> object_json = root["objects"].get<std::vector<json>>();
    for (const json &item : object_json)
    {
        const int object_id = item.value("id", next_id_++);
        const SceneShapeType shape = StringToShape(item.value("shape", std::string("box")));
        const bool dynamic = item.value("dynamic", shape != SceneShapeType::Ground);
        const Vector3 position = JsonToVec(item.value("position", json::object()), Vector3Zero());
        const Quaternion rotation = Quaternion{
            item.value("rotation", json::object()).value("x", 0.0f),
            item.value("rotation", json::object()).value("y", 0.0f),
            item.value("rotation", json::object()).value("z", 0.0f),
            item.value("rotation", json::object()).value("w", 1.0f)};
        const Vector3 half_extents = JsonToVec(item.value("half_extents", json::object()), Vector3{0.5f, 0.5f, 0.5f});
        const float radius = item.value("radius", 0.5f);
        const float half_height = item.value("half_height", 0.5f);
        const Color color = JsonToColor(item.value("color", json::object()), WHITE);

        ScenePhysics physics{};
        if (item.contains("physics") && item["physics"].is_object())
        {
            const json &phys = item["physics"];
            physics.friction = phys.value("friction", physics.friction);
            physics.restitution = phys.value("restitution", physics.restitution);
            physics.linear_damping = phys.value("linear_damping", physics.linear_damping);
            physics.angular_damping = phys.value("angular_damping", physics.angular_damping);
            physics.gravity_factor = phys.value("gravity_factor", physics.gravity_factor);
            physics.max_linear_velocity = phys.value("max_linear_velocity", physics.max_linear_velocity);
            physics.max_angular_velocity = phys.value("max_angular_velocity", physics.max_angular_velocity);
            physics.allow_sleeping = phys.value("allow_sleeping", physics.allow_sleeping);
            physics.is_sensor = phys.value("is_sensor", physics.is_sensor);
            physics.use_manifold_reduction = phys.value("use_manifold_reduction", physics.use_manifold_reduction);
            physics.apply_gyroscopic_force = phys.value("apply_gyroscopic_force", physics.apply_gyroscopic_force);
            physics.enhanced_internal_edge_removal = phys.value("enhanced_internal_edge_removal", physics.enhanced_internal_edge_removal);
            physics.collide_kinematic_vs_non_dynamic = phys.value("collide_kinematic_vs_non_dynamic", physics.collide_kinematic_vs_non_dynamic);
            physics.allow_dynamic_or_kinematic = phys.value("allow_dynamic_or_kinematic", physics.allow_dynamic_or_kinematic);
            physics.use_custom_mass = phys.value("use_custom_mass", physics.use_custom_mass);
            physics.mass = phys.value("mass", physics.mass);
            physics.use_linear_cast = phys.value("use_linear_cast", physics.use_linear_cast);
            const std::string motion_quality = phys.value("motion_quality", std::string(""));
            if (!motion_quality.empty())
            {
                physics.use_linear_cast = motion_quality == "linear_cast";
            }
        }

        int created = 0;
        switch (shape)
        {
        case SceneShapeType::Sphere:
            created = AddSphere(position, radius, dynamic, color, physics);
            break;
        case SceneShapeType::Capsule:
            created = AddCapsule(position, half_height, radius, color, physics);
            break;
        case SceneShapeType::Cylinder:
            created = AddCylinder(position, half_height, radius, color, physics);
            break;
        case SceneShapeType::Ground:
        case SceneShapeType::Box:
            created = AddBox(position, half_extents, dynamic, color, physics);
            break;
        }

        if (created == 0)
        {
            out_error = "Failed to create object from JSON";
            return false;
        }

        auto object_index = FindObjectIndexById(created);
        if (!object_index.has_value())
        {
            out_error = "Internal error while assigning object id";
            return false;
        }

        objects_[*object_index].id = object_id;
        next_id_ = std::max(next_id_, object_id + 1);
        physics_world_.SetBodyTransform(objects_[*object_index].body_id,
                                        position,
                                        rotation,
                                        false);
    }

    if (root.contains("constraints") && root["constraints"].is_array())
    {
        for (const json &item : root["constraints"])
        {
            SceneConstraint constraint{};
            constraint.id = item.value("id", std::string("constraint_" + std::to_string(constraints_.size())));
            constraint.type = StringToConstraintType(item.value("type", std::string("fixed")));
            constraint.body1_id = item.value("body1", 0);
            constraint.body2_id = item.value("body2", 0);
            constraint.point1 = JsonToVec(item.value("point1", json::object()), Vector3Zero());
            constraint.point2 = JsonToVec(item.value("point2", json::object()), Vector3Zero());
            constraint.axis1 = JsonToVec(item.value("axis1", json::object()), Vector3{1.0f, 0.0f, 0.0f});
            constraint.axis2 = JsonToVec(item.value("axis2", json::object()), Vector3{1.0f, 0.0f, 0.0f});
            constraint.normal1 = JsonToVec(item.value("normal1", json::object()), Vector3{0.0f, 1.0f, 0.0f});
            constraint.normal2 = JsonToVec(item.value("normal2", json::object()), Vector3{0.0f, 1.0f, 0.0f});
            constraint.min_limit = item.value("min_limit", constraint.min_limit);
            constraint.max_limit = item.value("max_limit", constraint.max_limit);
            constraint.max_friction = item.value("max_friction", constraint.max_friction);
            constraint.auto_detect_point = item.value("auto_detect_point", constraint.auto_detect_point);
            constraint.enabled = item.value("enabled", constraint.enabled);

            if (item.contains("motor") && item["motor"].is_object())
            {
                const json &motor = item["motor"];
                constraint.motor.mode = StringToMotorMode(motor.value("mode", std::string("off")));
                constraint.motor.target_velocity = motor.value("target_velocity", constraint.motor.target_velocity);
                constraint.motor.target_position = motor.value("target_position", constraint.motor.target_position);
                constraint.motor.spring_frequency = motor.value("spring_frequency", constraint.motor.spring_frequency);
                constraint.motor.spring_damping = motor.value("spring_damping", constraint.motor.spring_damping);
                constraint.motor.max_force = motor.value("max_force", constraint.motor.max_force);
                constraint.motor.max_torque = motor.value("max_torque", constraint.motor.max_torque);
            }

            constraints_.push_back(constraint);
        }
    }

    if (!RebuildPhysicsConstraints(out_error))
    {
        return false;
    }

    return true;
}

bool SceneSystem::AddConstraint(const SceneConstraint &constraint)
{
    if (constraint.id.empty())
    {
        return false;
    }

    const ConstraintDesc desc = ToConstraintDesc(constraint);
    if (!physics_world_.AddConstraint(desc))
    {
        return false;
    }

    constraints_.push_back(constraint);
    return true;
}

bool SceneSystem::RemoveConstraint(const std::string &constraint_id)
{
    if (!physics_world_.RemoveConstraintById(constraint_id))
    {
        return false;
    }

    const auto it = std::remove_if(
        constraints_.begin(),
        constraints_.end(),
        [&constraint_id](const SceneConstraint &constraint)
        {
            return constraint.id == constraint_id;
        });
    constraints_.erase(it, constraints_.end());
    return true;
}

const std::vector<SceneConstraint> &SceneSystem::Constraints() const
{
    return constraints_;
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

ScenePhysics SceneSystem::DefaultDynamicPhysics()
{
    ScenePhysics physics{};
    physics.friction = 0.45f;
    physics.restitution = 0.15f;
    physics.linear_damping = 0.05f;
    physics.angular_damping = 0.05f;
    physics.gravity_factor = 1.0f;
    physics.allow_sleeping = true;
    physics.use_linear_cast = false;
    return physics;
}

PhysicsShapeType SceneSystem::ToPhysicsShape(SceneShapeType shape)
{
    switch (shape)
    {
    case SceneShapeType::Sphere:
        return PhysicsShapeType::Sphere;
    case SceneShapeType::Capsule:
        return PhysicsShapeType::Capsule;
    case SceneShapeType::Cylinder:
        return PhysicsShapeType::Cylinder;
    case SceneShapeType::Ground:
    case SceneShapeType::Box:
    default:
        return PhysicsShapeType::Box;
    }
}

ConstraintType SceneSystem::ToPhysicsConstraintType(SceneConstraintType type)
{
    switch (type)
    {
    case SceneConstraintType::Point:
        return ConstraintType::Point;
    case SceneConstraintType::Distance:
        return ConstraintType::Distance;
    case SceneConstraintType::Hinge:
        return ConstraintType::Hinge;
    case SceneConstraintType::Slider:
        return ConstraintType::Slider;
    case SceneConstraintType::Fixed:
    default:
        return ConstraintType::Fixed;
    }
}

MotorMode SceneSystem::ToPhysicsMotorMode(SceneMotorMode mode)
{
    switch (mode)
    {
    case SceneMotorMode::Velocity:
        return MotorMode::Velocity;
    case SceneMotorMode::Position:
        return MotorMode::Position;
    case SceneMotorMode::Off:
    default:
        return MotorMode::Off;
    }
}

SceneConstraintType SceneSystem::FromPhysicsConstraintType(ConstraintType type)
{
    switch (type)
    {
    case ConstraintType::Point:
        return SceneConstraintType::Point;
    case ConstraintType::Distance:
        return SceneConstraintType::Distance;
    case ConstraintType::Hinge:
        return SceneConstraintType::Hinge;
    case ConstraintType::Slider:
        return SceneConstraintType::Slider;
    case ConstraintType::Fixed:
    default:
        return SceneConstraintType::Fixed;
    }
}

SceneMotorMode SceneSystem::FromPhysicsMotorMode(MotorMode mode)
{
    switch (mode)
    {
    case MotorMode::Velocity:
        return SceneMotorMode::Velocity;
    case MotorMode::Position:
        return SceneMotorMode::Position;
    case MotorMode::Off:
    default:
        return SceneMotorMode::Off;
    }
}

BodyPhysicsParams SceneSystem::ToBodyParams(const ScenePhysics &physics) const
{
    BodyPhysicsParams params{};
    params.friction = physics.friction;
    params.restitution = physics.restitution;
    params.linear_damping = physics.linear_damping;
    params.angular_damping = physics.angular_damping;
    params.gravity_factor = physics.gravity_factor;
    params.max_linear_velocity = physics.max_linear_velocity;
    params.max_angular_velocity = physics.max_angular_velocity;
    params.allow_sleeping = physics.allow_sleeping;
    params.is_sensor = physics.is_sensor;
    params.use_manifold_reduction = physics.use_manifold_reduction;
    params.apply_gyroscopic_force = physics.apply_gyroscopic_force;
    params.enhanced_internal_edge_removal = physics.enhanced_internal_edge_removal;
    params.collide_kinematic_vs_non_dynamic = physics.collide_kinematic_vs_non_dynamic;
    params.allow_dynamic_or_kinematic = physics.allow_dynamic_or_kinematic;
    params.use_custom_mass = physics.use_custom_mass;
    params.mass = physics.mass;
    params.use_linear_cast = physics.use_linear_cast;
    return params;
}

ConstraintDesc SceneSystem::ToConstraintDesc(const SceneConstraint &constraint) const
{
    ConstraintDesc desc{};
    desc.id = constraint.id;
    desc.type = ToPhysicsConstraintType(constraint.type);
    desc.point1 = constraint.point1;
    desc.point2 = constraint.point2;
    desc.axis1 = constraint.axis1;
    desc.axis2 = constraint.axis2;
    desc.normal1 = constraint.normal1;
    desc.normal2 = constraint.normal2;
    desc.min_limit = constraint.min_limit;
    desc.max_limit = constraint.max_limit;
    desc.max_friction = constraint.max_friction;
    desc.auto_detect_point = constraint.auto_detect_point;
    desc.enabled = constraint.enabled;
    desc.motor.mode = ToPhysicsMotorMode(constraint.motor.mode);
    desc.motor.target_velocity = constraint.motor.target_velocity;
    desc.motor.target_position = constraint.motor.target_position;
    desc.motor.spring_frequency = constraint.motor.spring_frequency;
    desc.motor.spring_damping = constraint.motor.spring_damping;
    desc.motor.max_force = constraint.motor.max_force;
    desc.motor.max_torque = constraint.motor.max_torque;

    ResolveBodyId(constraint.body1_id, desc.body1);
    ResolveBodyId(constraint.body2_id, desc.body2);
    return desc;
}

bool SceneSystem::ResolveBodyId(int scene_id, JPH::BodyID &out_body_id) const
{
    if (scene_id == 0)
    {
        out_body_id = JPH::BodyID();
        return true;
    }

    const auto index = FindObjectIndexById(scene_id);
    if (!index.has_value())
    {
        return false;
    }

    out_body_id = objects_[*index].body_id;
    return true;
}

bool SceneSystem::RebuildPhysicsConstraints(std::string &out_error)
{
    physics_world_.ClearConstraints();
    for (const SceneConstraint &constraint : constraints_)
    {
        ConstraintDesc desc = ToConstraintDesc(constraint);
        if (constraint.body1_id != 0 && desc.body1.IsInvalid())
        {
            out_error = "Constraint body1 is invalid: " + constraint.id;
            return false;
        }

        if (desc.body2.IsInvalid())
        {
            out_error = "Constraint body2 is invalid: " + constraint.id;
            return false;
        }

        if (!physics_world_.AddConstraint(desc))
        {
            out_error = "Failed adding constraint: " + constraint.id;
            return false;
        }
    }
    return true;
}
