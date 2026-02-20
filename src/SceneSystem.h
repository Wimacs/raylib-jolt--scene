#pragma once

#include "PhysicsWorld.h"

#include <Jolt/Physics/Body/BodyID.h>

#include <cstdint>
#include <cfloat>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

enum class SceneConstraintType : uint8_t
{
    Fixed,
    Point,
    Distance,
    Hinge,
    Slider,
};

enum class SceneMotorMode : uint8_t
{
    Off,
    Velocity,
    Position,
};

struct SceneMotor
{
    SceneMotorMode mode{SceneMotorMode::Off};
    float target_velocity{0.0f};
    float target_position{0.0f};
    float spring_frequency{2.0f};
    float spring_damping{1.0f};
    float max_force{FLT_MAX};
    float max_torque{FLT_MAX};
};

struct SceneConstraint
{
    std::string id;
    SceneConstraintType type{SceneConstraintType::Fixed};
    int body1_id{0};
    int body2_id{0};

    Vector3 point1{0.0f, 0.0f, 0.0f};
    Vector3 point2{0.0f, 0.0f, 0.0f};

    Vector3 axis1{1.0f, 0.0f, 0.0f};
    Vector3 axis2{1.0f, 0.0f, 0.0f};
    Vector3 normal1{0.0f, 1.0f, 0.0f};
    Vector3 normal2{0.0f, 1.0f, 0.0f};

    float min_limit{-FLT_MAX};
    float max_limit{FLT_MAX};
    float max_friction{0.0f};
    bool auto_detect_point{false};
    bool enabled{true};

    SceneMotor motor;
};

struct ScenePhysics
{
    float friction{0.2f};
    float restitution{0.0f};
    float linear_damping{0.05f};
    float angular_damping{0.05f};
    float gravity_factor{1.0f};
    float max_linear_velocity{500.0f};
    float max_angular_velocity{0.25f * JPH::JPH_PI * 60.0f};
    bool allow_sleeping{true};
    bool is_sensor{false};
    bool use_manifold_reduction{true};
    bool apply_gyroscopic_force{false};
    bool enhanced_internal_edge_removal{false};
    bool collide_kinematic_vs_non_dynamic{false};
    bool allow_dynamic_or_kinematic{false};
    bool use_custom_mass{false};
    float mass{1.0f};
    bool use_linear_cast{false};
};

enum class SceneShapeType : uint8_t
{
    Box,
    Sphere,
    Capsule,
    Cylinder,
    Ground,
};

struct SceneObject
{
    int id{0};
    SceneShapeType shape{SceneShapeType::Box};
    bool dynamic{true};
    Vector3 half_extents{0.5f, 0.5f, 0.5f};
    float radius{0.5f};
    float half_height{0.5f};
    Color color{WHITE};
    ScenePhysics physics{};
    JPH::BodyID body_id{};
};

class SceneSystem
{
public:
    explicit SceneSystem(PhysicsWorld &physics_world);

    void InitializeDefaultScene();
    void Step(float delta_time);
    void Draw(int highlighted_object_id = 0) const;

    int AddBox(const Vector3 &position,
               const Vector3 &half_extents,
               bool dynamic,
               Color color);
    int AddSphere(const Vector3 &position,
                  float radius,
                  bool dynamic,
                  Color color);
    int AddCapsule(const Vector3 &position,
                   float half_height,
                   float radius,
                   Color color);
    int AddCylinder(const Vector3 &position,
                    float half_height,
                    float radius,
                    Color color);

    int AddBox(const Vector3 &position,
               const Vector3 &half_extents,
               bool dynamic,
               Color color,
               const ScenePhysics &physics);
    int AddSphere(const Vector3 &position,
                  float radius,
                  bool dynamic,
                  Color color,
                  const ScenePhysics &physics);
    int AddCapsule(const Vector3 &position,
                   float half_height,
                   float radius,
                   Color color,
                   const ScenePhysics &physics);
    int AddCylinder(const Vector3 &position,
                    float half_height,
                    float radius,
                    Color color,
                    const ScenePhysics &physics);

    bool RemoveObject(int object_id);
    bool RemoveObjectByBody(JPH::BodyID body_id);

    std::optional<int> PickObject(const Camera3D &camera,
                                  const Vector2 &screen,
                                  bool dynamic_only = true) const;

    bool StartDrag(const Camera3D &camera, const Vector2 &screen);
    void UpdateDrag(const Camera3D &camera, const Vector2 &screen);
    void EndDrag();

    [[nodiscard]] const std::vector<SceneObject> &Objects() const;
    [[nodiscard]] bool IsDragging() const;
    [[nodiscard]] int DraggingObjectId() const;

    void DrawPhysicsDebug(bool show_sleeping = true,
                          JPH::BodyID ignore_body_id = JPH::BodyID()) const;

    bool SaveToJson(const std::string &path) const;
    bool LoadFromJson(const std::string &path, std::string &out_error);

    bool AddConstraint(const SceneConstraint &constraint);
    bool RemoveConstraint(const std::string &constraint_id);
    [[nodiscard]] const std::vector<SceneConstraint> &Constraints() const;

private:
    std::optional<size_t> FindObjectIndexById(int object_id);
    std::optional<size_t> FindObjectIndexById(int object_id) const;
    std::optional<size_t> FindObjectIndexByBody(JPH::BodyID body_id) const;

    static Color RandomBrightColor();

    static ScenePhysics DefaultDynamicPhysics();
    static PhysicsShapeType ToPhysicsShape(SceneShapeType shape);
    static ConstraintType ToPhysicsConstraintType(SceneConstraintType type);
    static MotorMode ToPhysicsMotorMode(SceneMotorMode mode);
    static SceneConstraintType FromPhysicsConstraintType(ConstraintType type);
    static SceneMotorMode FromPhysicsMotorMode(MotorMode mode);

    BodyPhysicsParams ToBodyParams(const ScenePhysics &physics) const;
    ConstraintDesc ToConstraintDesc(const SceneConstraint &constraint) const;
    bool ResolveBodyId(int scene_id, JPH::BodyID &out_body_id) const;
    bool RebuildPhysicsConstraints(std::string &out_error);

    PhysicsWorld &physics_world_;
    std::vector<SceneObject> objects_;
    std::vector<SceneConstraint> constraints_;
    int next_id_{1};

    struct DragState
    {
        int object_id{0};
        float target_distance{0.0f};
        Vector3 local_pick_offset{0.0f, 0.0f, 0.0f};
    };

    std::optional<DragState> drag_state_;
};
