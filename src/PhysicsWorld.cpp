#include "PhysicsWorld.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/RegisterTypes.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <iostream>
#include <thread>

#include <raymath.h>

namespace
{
static void TraceImpl(const char *in_format, ...)
{
    va_list list;
    va_start(list, in_format);
    char buffer[1024];
    vsnprintf(buffer, sizeof(buffer), in_format, list);
    va_end(list);
    std::cout << buffer << '\n';
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char *in_expression,
                             const char *in_message,
                             const char *in_file,
                             uint32_t in_line)
{
    std::cerr << in_file << ":" << in_line << " (" << in_expression
              << ") " << (in_message != nullptr ? in_message : "") << '\n';
    return true;
}
#endif

JPH::RVec3 ToRVec3(const Vector3 &value)
{
    return JPH::RVec3(value.x, value.y, value.z);
}

JPH::Vec3 ToVec3(const Vector3 &value)
{
    return JPH::Vec3(value.x, value.y, value.z);
}

JPH::Quat ToJoltQuat(const Quaternion &rotation)
{
    return JPH::Quat(rotation.x, rotation.y, rotation.z, rotation.w);
}

Vector3 ClampMin(const Vector3 &value, float minimum)
{
    return Vector3{
        std::max(value.x, minimum),
        std::max(value.y, minimum),
        std::max(value.z, minimum)};
}

Quaternion ToRaylibQuaternion(JPH::QuatArg rotation)
{
    return Quaternion{rotation.GetX(),
                      rotation.GetY(),
                      rotation.GetZ(),
                      rotation.GetW()};
}
} // namespace

PhysicsWorld::PhysicsWorld() = default;

PhysicsWorld::~PhysicsWorld()
{
    Shutdown();
}

bool PhysicsWorld::Initialize()
{
    if (initialized_)
    {
        return true;
    }

    JPH::RegisterDefaultAllocator();
    JPH::Trace = TraceImpl;
    JPH_IF_ENABLE_ASSERTS(AssertFailed = AssertFailedImpl;)

    JPH::Factory::sInstance = new JPH::Factory();
    JPH::RegisterTypes();

    constexpr JPH::uint kTempAllocatorSize = 10 * 1024 * 1024;
    temp_allocator_ = std::make_unique<JPH::TempAllocatorImpl>(kTempAllocatorSize);

    const int worker_count =
        std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1);
    job_system_ = std::make_unique<JPH::JobSystemThreadPool>(
        JPH::cMaxPhysicsJobs,
        JPH::cMaxPhysicsBarriers,
        worker_count);

    broad_phase_layer_interface_ = std::make_unique<BPLayerInterfaceImpl>();
    object_vs_broadphase_layer_filter_ =
        std::make_unique<ObjectVsBroadPhaseLayerFilterImpl>();
    object_vs_object_layer_filter_ =
        std::make_unique<ObjectLayerPairFilterImpl>();

    constexpr JPH::uint kMaxBodies = 4096;
    constexpr JPH::uint kNumBodyMutexes = 0;
    constexpr JPH::uint kMaxBodyPairs = 4096;
    constexpr JPH::uint kMaxContactConstraints = 4096;

    physics_system_ = std::make_unique<JPH::PhysicsSystem>();
    physics_system_->Init(kMaxBodies,
                          kNumBodyMutexes,
                          kMaxBodyPairs,
                          kMaxContactConstraints,
                          *broad_phase_layer_interface_,
                          *object_vs_broadphase_layer_filter_,
                          *object_vs_object_layer_filter_);

    physics_system_->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));
    initialized_ = true;
    return true;
}

void PhysicsWorld::Shutdown()
{
    if (!initialized_)
    {
        return;
    }

    if (physics_system_ != nullptr)
    {
        JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
        for (const JPH::BodyID body_id : created_bodies_)
        {
            if (body_id.IsInvalid())
            {
                continue;
            }

            if (body_interface.IsAdded(body_id))
            {
                body_interface.RemoveBody(body_id);
            }
            body_interface.DestroyBody(body_id);
        }
    }

    created_bodies_.clear();
    physics_system_.reset();
    job_system_.reset();
    temp_allocator_.reset();
    broad_phase_layer_interface_.reset();
    object_vs_broadphase_layer_filter_.reset();
    object_vs_object_layer_filter_.reset();

    JPH::UnregisterTypes();
    delete JPH::Factory::sInstance;
    JPH::Factory::sInstance = nullptr;

    initialized_ = false;
}

void PhysicsWorld::Step(float delta_time)
{
    if (!initialized_ || physics_system_ == nullptr || delta_time <= 0.0f)
    {
        return;
    }

    const int collision_steps = std::max(1, static_cast<int>(std::ceil(delta_time * 60.0f)));
    physics_system_->Update(
        delta_time,
        collision_steps,
        temp_allocator_.get(),
        job_system_.get());
}

JPH::BodyID PhysicsWorld::CreateStaticBox(const Vector3 &center,
                                          const Vector3 &half_extents)
{
    if (!initialized_)
    {
        return JPH::BodyID();
    }

    const Vector3 safe_half_extents = ClampMin(half_extents, 0.02f);
    const JPH::BodyCreationSettings settings(
        new JPH::BoxShape(ToVec3(safe_half_extents)),
        ToRVec3(center),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Static,
        Layers::NON_MOVING);

    JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    const JPH::BodyID body_id =
        body_interface.CreateAndAddBody(settings, JPH::EActivation::DontActivate);
    if (body_id.IsInvalid())
    {
        return body_id;
    }

    created_bodies_.push_back(body_id);
    RegisterDebugBody(PhysicsDebugBody{
        body_id,
        PhysicsShapeType::Box,
        safe_half_extents,
        0.0f,
        0.0f,
        false});
    return body_id;
}

JPH::BodyID PhysicsWorld::CreateDynamicBox(const Vector3 &center,
                                           const Vector3 &half_extents)
{
    if (!initialized_)
    {
        return JPH::BodyID();
    }

    const Vector3 safe_half_extents = ClampMin(half_extents, 0.02f);
    const JPH::BodyCreationSettings settings(
        new JPH::BoxShape(ToVec3(safe_half_extents)),
        ToRVec3(center),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        Layers::MOVING);

    JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    const JPH::BodyID body_id =
        body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate);
    if (body_id.IsInvalid())
    {
        return body_id;
    }

    created_bodies_.push_back(body_id);
    RegisterDebugBody(PhysicsDebugBody{
        body_id,
        PhysicsShapeType::Box,
        safe_half_extents,
        0.0f,
        0.0f,
        true});
    return body_id;
}

JPH::BodyID PhysicsWorld::CreateDynamicSphere(const Vector3 &center, float radius)
{
    if (!initialized_)
    {
        return JPH::BodyID();
    }

    const float safe_radius = std::max(radius, 0.05f);
    const JPH::BodyCreationSettings settings(
        new JPH::SphereShape(safe_radius),
        ToRVec3(center),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        Layers::MOVING);

    JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    const JPH::BodyID body_id =
        body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate);
    if (body_id.IsInvalid())
    {
        return body_id;
    }

    created_bodies_.push_back(body_id);
    RegisterDebugBody(PhysicsDebugBody{
        body_id,
        PhysicsShapeType::Sphere,
        Vector3{safe_radius, safe_radius, safe_radius},
        safe_radius,
        0.0f,
        true});
    return body_id;
}

JPH::BodyID PhysicsWorld::CreateDynamicCapsule(const Vector3 &center,
                                               float half_height,
                                               float radius)
{
    if (!initialized_)
    {
        return JPH::BodyID();
    }

    const float safe_radius = std::max(radius, 0.05f);
    const float safe_half_height = std::max(half_height, 0.05f);
    const JPH::BodyCreationSettings settings(
        new JPH::CapsuleShape(safe_half_height, safe_radius),
        ToRVec3(center),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        Layers::MOVING);

    JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    const JPH::BodyID body_id =
        body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate);
    if (body_id.IsInvalid())
    {
        return body_id;
    }

    created_bodies_.push_back(body_id);
    RegisterDebugBody(PhysicsDebugBody{
        body_id,
        PhysicsShapeType::Capsule,
        Vector3{safe_radius, safe_half_height + safe_radius, safe_radius},
        safe_radius,
        safe_half_height,
        true});
    return body_id;
}

JPH::BodyID PhysicsWorld::CreateDynamicCylinder(const Vector3 &center,
                                                float half_height,
                                                float radius)
{
    if (!initialized_)
    {
        return JPH::BodyID();
    }

    const float safe_radius = std::max(radius, 0.05f);
    const float safe_half_height = std::max(half_height, 0.05f);
    const JPH::BodyCreationSettings settings(
        new JPH::CylinderShape(safe_half_height, safe_radius),
        ToRVec3(center),
        JPH::Quat::sIdentity(),
        JPH::EMotionType::Dynamic,
        Layers::MOVING);

    JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    const JPH::BodyID body_id =
        body_interface.CreateAndAddBody(settings, JPH::EActivation::Activate);
    if (body_id.IsInvalid())
    {
        return body_id;
    }

    created_bodies_.push_back(body_id);
    RegisterDebugBody(PhysicsDebugBody{
        body_id,
        PhysicsShapeType::Cylinder,
        Vector3{safe_radius, safe_half_height, safe_radius},
        safe_radius,
        safe_half_height,
        true});
    return body_id;
}

void PhysicsWorld::DestroyBody(JPH::BodyID body_id)
{
    if (!initialized_ || body_id.IsInvalid())
    {
        return;
    }

    auto it = std::find(created_bodies_.begin(), created_bodies_.end(), body_id);
    if (it == created_bodies_.end())
    {
        return;
    }

    JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    if (body_interface.IsAdded(body_id))
    {
        body_interface.RemoveBody(body_id);
    }
    body_interface.DestroyBody(body_id);
    created_bodies_.erase(it);

    const auto debug_it = std::remove_if(
        debug_bodies_.begin(),
        debug_bodies_.end(),
        [body_id](const PhysicsDebugBody &debug_body)
        {
            return debug_body.body_id == body_id;
        });
    debug_bodies_.erase(debug_it, debug_bodies_.end());
}

Vector3 PhysicsWorld::GetBodyPosition(JPH::BodyID body_id) const
{
    if (!initialized_ || body_id.IsInvalid() || !IsBodyAdded(body_id))
    {
        return Vector3Zero();
    }

    const JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    const JPH::RVec3 position = body_interface.GetCenterOfMassPosition(body_id);
    return Vector3{position.GetX(), position.GetY(), position.GetZ()};
}

Quaternion PhysicsWorld::GetBodyRotation(JPH::BodyID body_id) const
{
    if (!initialized_ || body_id.IsInvalid() || !IsBodyAdded(body_id))
    {
        return QuaternionIdentity();
    }

    const JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    return ToRaylibQuaternion(body_interface.GetRotation(body_id));
}

void PhysicsWorld::SetBodyTransform(JPH::BodyID body_id,
                                    const Vector3 &position,
                                    const Quaternion &rotation,
                                    bool activate)
{
    if (!initialized_ || body_id.IsInvalid() || !IsBodyAdded(body_id))
    {
        return;
    }

    JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    body_interface.SetPositionAndRotation(
        body_id,
        ToRVec3(position),
        ToJoltQuat(rotation),
        activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
}

void PhysicsWorld::SetBodyVelocityZero(JPH::BodyID body_id)
{
    if (!initialized_ || body_id.IsInvalid() || !IsBodyAdded(body_id))
    {
        return;
    }

    JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    body_interface.SetLinearVelocity(body_id, JPH::Vec3::sZero());
    body_interface.SetAngularVelocity(body_id, JPH::Vec3::sZero());
}

bool PhysicsWorld::RayCast(const Vector3 &origin,
                          const Vector3 &direction,
                          float max_distance,
                          JPH::BodyID &out_body_id,
                          Vector3 &out_hit_point) const
{
    out_body_id = JPH::BodyID();
    out_hit_point = Vector3Zero();

    if (!initialized_ || max_distance <= 0.0f)
    {
        return false;
    }

    const Vector3 normalized_direction = Vector3Normalize(direction);
    const Vector3 scaled_direction =
        Vector3Scale(normalized_direction, max_distance);

    const JPH::RRayCast ray(ToRVec3(origin), ToVec3(scaled_direction));
    JPH::RayCastResult hit;
    const bool has_hit = physics_system_->GetNarrowPhaseQuery().CastRay(ray, hit);
    if (!has_hit)
    {
        return false;
    }

    out_body_id = hit.mBodyID;
    const JPH::RVec3 hit_position = ray.GetPointOnRay(hit.mFraction);
    out_hit_point = Vector3{
        hit_position.GetX(),
        hit_position.GetY(),
        hit_position.GetZ()};
    return true;
}

bool PhysicsWorld::IsBodyAdded(JPH::BodyID body_id) const
{
    if (!initialized_ || body_id.IsInvalid())
    {
        return false;
    }

    const JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    return body_interface.IsAdded(body_id);
}

std::vector<PhysicsDebugBody> PhysicsWorld::DebugBodies() const
{
    std::vector<PhysicsDebugBody> active_bodies;
    active_bodies.reserve(debug_bodies_.size());

    for (const PhysicsDebugBody &debug_body : debug_bodies_)
    {
        if (IsBodyAdded(debug_body.body_id))
        {
            active_bodies.push_back(debug_body);
        }
    }
    return active_bodies;
}

void PhysicsWorld::RegisterDebugBody(const PhysicsDebugBody &debug_body)
{
    debug_bodies_.push_back(debug_body);
}
