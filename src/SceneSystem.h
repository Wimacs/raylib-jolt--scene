#pragma once

#include "PhysicsWorld.h"

#include <Jolt/Physics/Body/BodyID.h>

#include <cstdint>
#include <optional>
#include <vector>

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

    void DrawPhysicsDebug(bool show_sleeping = true) const;

private:
    std::optional<size_t> FindObjectIndexById(int object_id);
    std::optional<size_t> FindObjectIndexById(int object_id) const;
    std::optional<size_t> FindObjectIndexByBody(JPH::BodyID body_id) const;

    static Color RandomBrightColor();

    PhysicsWorld &physics_world_;
    std::vector<SceneObject> objects_;
    int next_id_{1};

    struct DragState
    {
        int object_id{0};
        float target_distance{0.0f};
        Vector3 local_pick_offset{0.0f, 0.0f, 0.0f};
    };

    std::optional<DragState> drag_state_;
};
