#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/Core.h>
#include <Jolt/Core/IssueReporting.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Real.h>
#include <Jolt/Physics/Body/Body.h>
#include <Jolt/Physics/Collision/BroadPhase/BroadPhaseLayer.h>
#include <Jolt/Physics/Collision/ObjectLayer.h>
#include <Jolt/Physics/PhysicsSystem.h>

#include <raylib.h>

#include <memory>
#include <vector>

enum class PhysicsShapeType : uint8_t
{
    Box,
    Sphere,
    Capsule,
    Cylinder,
};

struct PhysicsDebugBody
{
    JPH::BodyID body_id{};
    PhysicsShapeType shape{PhysicsShapeType::Box};
    Vector3 half_extents{0.5f, 0.5f, 0.5f};
    float radius{0.5f};
    float half_height{0.5f};
    bool dynamic{true};
};

namespace Layers
{
static constexpr JPH::ObjectLayer NON_MOVING{0};
static constexpr JPH::ObjectLayer MOVING{1};
static constexpr JPH::ObjectLayer NUM_LAYERS{2};
} // namespace Layers

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
    [[nodiscard]] bool ShouldCollide(JPH::ObjectLayer object_1,
                                     JPH::ObjectLayer object_2) const override
    {
        switch (object_1)
        {
        case Layers::NON_MOVING:
            return object_2 == Layers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

namespace BroadPhaseLayers
{
static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
static constexpr JPH::BroadPhaseLayer MOVING(1);
static constexpr JPH::uint NUM_LAYERS(2);
} // namespace BroadPhaseLayers

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
    BPLayerInterfaceImpl()
    {
        object_to_broad_phase_[Layers::NON_MOVING] =
            BroadPhaseLayers::NON_MOVING;
        object_to_broad_phase_[Layers::MOVING] = BroadPhaseLayers::MOVING;
    }

    [[nodiscard]] JPH::uint GetNumBroadPhaseLayers() const override
    {
        return BroadPhaseLayers::NUM_LAYERS;
    }

    [[nodiscard]] JPH::BroadPhaseLayer GetBroadPhaseLayer(
        JPH::ObjectLayer layer) const override
    {
        JPH_ASSERT(layer < Layers::NUM_LAYERS);
        return object_to_broad_phase_[layer];
    }

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
    [[nodiscard]] const char *GetBroadPhaseLayerName(
        JPH::BroadPhaseLayer layer) const override
    {
        switch ((JPH::BroadPhaseLayer::Type)layer)
        {
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:
            return "NON_MOVING";
        case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:
            return "MOVING";
        default:
            JPH_ASSERT(false);
            return "INVALID";
        }
    }
#endif

private:
    JPH::BroadPhaseLayer object_to_broad_phase_[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl
    : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
    [[nodiscard]] bool ShouldCollide(
        JPH::ObjectLayer object_layer,
        JPH::BroadPhaseLayer broad_phase_layer) const override
    {
        switch (object_layer)
        {
        case Layers::NON_MOVING:
            return broad_phase_layer == BroadPhaseLayers::MOVING;
        case Layers::MOVING:
            return true;
        default:
            JPH_ASSERT(false);
            return false;
        }
    }
};

class PhysicsWorld
{
public:
    PhysicsWorld();
    ~PhysicsWorld();

    PhysicsWorld(const PhysicsWorld &) = delete;
    PhysicsWorld &operator=(const PhysicsWorld &) = delete;
    PhysicsWorld(PhysicsWorld &&) = delete;
    PhysicsWorld &operator=(PhysicsWorld &&) = delete;

    bool Initialize();
    void Shutdown();
    void Step(float delta_time);

    JPH::BodyID CreateStaticBox(const Vector3 &center,
                                const Vector3 &half_extents);
    JPH::BodyID CreateDynamicBox(const Vector3 &center,
                                 const Vector3 &half_extents);
    JPH::BodyID CreateDynamicSphere(const Vector3 &center, float radius);
    JPH::BodyID CreateDynamicCapsule(const Vector3 &center,
                                     float half_height,
                                     float radius);
    JPH::BodyID CreateDynamicCylinder(const Vector3 &center,
                                      float half_height,
                                      float radius);

    void DestroyBody(JPH::BodyID body_id);

    [[nodiscard]] Vector3 GetBodyPosition(JPH::BodyID body_id) const;
    [[nodiscard]] Quaternion GetBodyRotation(JPH::BodyID body_id) const;

    void SetBodyTransform(JPH::BodyID body_id,
                          const Vector3 &position,
                          const Quaternion &rotation,
                          bool activate);
    void SetBodyVelocityZero(JPH::BodyID body_id);

    [[nodiscard]] bool RayCast(const Vector3 &origin,
                               const Vector3 &direction,
                               float max_distance,
                               JPH::BodyID &out_body_id,
                               Vector3 &out_hit_point) const;

    [[nodiscard]] bool IsBodyAdded(JPH::BodyID body_id) const;

    [[nodiscard]] std::vector<PhysicsDebugBody> DebugBodies() const;

private:
    void RegisterDebugBody(const PhysicsDebugBody &debug_body);

    std::unique_ptr<JPH::PhysicsSystem> physics_system_;
    std::unique_ptr<JPH::TempAllocatorImpl> temp_allocator_;
    std::unique_ptr<JPH::JobSystemThreadPool> job_system_;
    std::unique_ptr<BPLayerInterfaceImpl> broad_phase_layer_interface_;
    std::unique_ptr<ObjectVsBroadPhaseLayerFilterImpl>
        object_vs_broadphase_layer_filter_;
    std::unique_ptr<ObjectLayerPairFilterImpl> object_vs_object_layer_filter_;
    std::vector<JPH::BodyID> created_bodies_;
    std::vector<PhysicsDebugBody> debug_bodies_;
    bool initialized_{false};
};
