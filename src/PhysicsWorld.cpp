#include "PhysicsWorld.h"

#include <Jolt/Core/Factory.h>
#include <Jolt/Core/JobSystemThreadPool.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Math/Quat.h>
#include <Jolt/Math/Vec3.h>
#include <Jolt/Physics/Body/BodyCreationSettings.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/BodyLock.h>
#include <Jolt/Physics/Body/BodyLockMulti.h>
#include <Jolt/Physics/Body/MotionProperties.h>
#include <Jolt/Physics/Body/MotionType.h>
#include <Jolt/Physics/Collision/CastResult.h>
#include <Jolt/Physics/Collision/NarrowPhaseQuery.h>
#include <Jolt/Physics/Collision/RayCast.h>
#include <Jolt/Physics/Collision/Shape/BoxShape.h>
#include <Jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <Jolt/Physics/Collision/Shape/CylinderShape.h>
#include <Jolt/Physics/Collision/Shape/SphereShape.h>
#include <Jolt/Physics/Constraints/Constraint.h>
#include <Jolt/Physics/Constraints/DistanceConstraint.h>
#include <Jolt/Physics/Constraints/FixedConstraint.h>
#include <Jolt/Physics/Constraints/HingeConstraint.h>
#include <Jolt/Physics/Constraints/PointConstraint.h>
#include <Jolt/Physics/Constraints/SliderConstraint.h>
#include <Jolt/Physics/EActivation.h>
#include <Jolt/RegisterTypes.h>

#include <raymath.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <iostream>
#include <thread>

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

Quaternion ToRaylibQuaternion(JPH::QuatArg rotation)
{
    return Quaternion{rotation.GetX(),
                      rotation.GetY(),
                      rotation.GetZ(),
                      rotation.GetW()};
}

Vector3 ClampMin(const Vector3 &value, float minimum)
{
    return Vector3{
        std::max(value.x, minimum),
        std::max(value.y, minimum),
        std::max(value.z, minimum)};
}

JPH::EMotionType ToMotionType(bool dynamic)
{
    return dynamic ? JPH::EMotionType::Dynamic : JPH::EMotionType::Static;
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

    constexpr JPH::uint kMaxBodies = 8192;
    constexpr JPH::uint kNumBodyMutexes = 0;
    constexpr JPH::uint kMaxBodyPairs = 8192;
    constexpr JPH::uint kMaxContactConstraints = 8192;

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

    ClearConstraints();

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
    debug_bodies_.clear();
    constraints_.clear();

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
    BodyPhysicsParams params{};
    return CreateBox(center, half_extents, false, params).body_id;
}

JPH::BodyID PhysicsWorld::CreateDynamicBox(const Vector3 &center,
                                           const Vector3 &half_extents)
{
    BodyPhysicsParams params{};
    return CreateBox(center, half_extents, true, params).body_id;
}

JPH::BodyID PhysicsWorld::CreateDynamicSphere(const Vector3 &center, float radius)
{
    BodyPhysicsParams params{};
    return CreateSphere(center, radius, true, params).body_id;
}

JPH::BodyID PhysicsWorld::CreateDynamicCapsule(const Vector3 &center,
                                               float half_height,
                                               float radius)
{
    BodyPhysicsParams params{};
    return CreateCapsule(center, half_height, radius, true, params).body_id;
}

JPH::BodyID PhysicsWorld::CreateDynamicCylinder(const Vector3 &center,
                                                float half_height,
                                                float radius)
{
    BodyPhysicsParams params{};
    return CreateCylinder(center, half_height, radius, true, params).body_id;
}

BodySpawnResult PhysicsWorld::CreateBox(const Vector3 &center,
                                        const Vector3 &half_extents,
                                        bool dynamic,
                                        const BodyPhysicsParams &params)
{
    const Vector3 safe_half_extents = ClampMin(half_extents, 0.02f);
    JPH::BodyCreationSettings settings(
        new JPH::BoxShape(ToVec3(safe_half_extents)),
        ToRVec3(center),
        JPH::Quat::sIdentity(),
        ToMotionType(dynamic),
        dynamic ? Layers::MOVING : Layers::NON_MOVING);
    ApplyBodyPhysicsParams(settings, params, dynamic);

    const PhysicsDebugBody debug{
        JPH::BodyID(),
        PhysicsShapeType::Box,
        safe_half_extents,
        0.0f,
        0.0f,
        dynamic};

    const JPH::BodyID body_id =
        CreateBodyWithSettings(settings, debug, dynamic && !params.is_sensor);
    BodySpawnResult result{};
    result.body_id = body_id;
    result.debug_body = debug;
    result.debug_body.body_id = body_id;
    return result;
}

BodySpawnResult PhysicsWorld::CreateSphere(const Vector3 &center,
                                           float radius,
                                           bool dynamic,
                                           const BodyPhysicsParams &params)
{
    const float safe_radius = std::max(radius, 0.05f);
    JPH::BodyCreationSettings settings(
        new JPH::SphereShape(safe_radius),
        ToRVec3(center),
        JPH::Quat::sIdentity(),
        ToMotionType(dynamic),
        dynamic ? Layers::MOVING : Layers::NON_MOVING);
    ApplyBodyPhysicsParams(settings, params, dynamic);

    const PhysicsDebugBody debug{
        JPH::BodyID(),
        PhysicsShapeType::Sphere,
        Vector3{safe_radius, safe_radius, safe_radius},
        safe_radius,
        0.0f,
        dynamic};

    const JPH::BodyID body_id =
        CreateBodyWithSettings(settings, debug, dynamic && !params.is_sensor);
    BodySpawnResult result{};
    result.body_id = body_id;
    result.debug_body = debug;
    result.debug_body.body_id = body_id;
    return result;
}

BodySpawnResult PhysicsWorld::CreateCapsule(const Vector3 &center,
                                            float half_height,
                                            float radius,
                                            bool dynamic,
                                            const BodyPhysicsParams &params)
{
    const float safe_radius = std::max(radius, 0.05f);
    const float safe_half_height = std::max(half_height, 0.05f);
    JPH::BodyCreationSettings settings(
        new JPH::CapsuleShape(safe_half_height, safe_radius),
        ToRVec3(center),
        JPH::Quat::sIdentity(),
        ToMotionType(dynamic),
        dynamic ? Layers::MOVING : Layers::NON_MOVING);
    ApplyBodyPhysicsParams(settings, params, dynamic);

    const PhysicsDebugBody debug{
        JPH::BodyID(),
        PhysicsShapeType::Capsule,
        Vector3{safe_radius, safe_half_height + safe_radius, safe_radius},
        safe_radius,
        safe_half_height,
        dynamic};

    const JPH::BodyID body_id =
        CreateBodyWithSettings(settings, debug, dynamic && !params.is_sensor);
    BodySpawnResult result{};
    result.body_id = body_id;
    result.debug_body = debug;
    result.debug_body.body_id = body_id;
    return result;
}

BodySpawnResult PhysicsWorld::CreateCylinder(const Vector3 &center,
                                             float half_height,
                                             float radius,
                                             bool dynamic,
                                             const BodyPhysicsParams &params)
{
    const float safe_radius = std::max(radius, 0.05f);
    const float safe_half_height = std::max(half_height, 0.05f);
    JPH::BodyCreationSettings settings(
        new JPH::CylinderShape(safe_half_height, safe_radius),
        ToRVec3(center),
        JPH::Quat::sIdentity(),
        ToMotionType(dynamic),
        dynamic ? Layers::MOVING : Layers::NON_MOVING);
    ApplyBodyPhysicsParams(settings, params, dynamic);

    const PhysicsDebugBody debug{
        JPH::BodyID(),
        PhysicsShapeType::Cylinder,
        Vector3{safe_radius, safe_half_height, safe_radius},
        safe_radius,
        safe_half_height,
        dynamic};

    const JPH::BodyID body_id =
        CreateBodyWithSettings(settings, debug, dynamic && !params.is_sensor);
    BodySpawnResult result{};
    result.body_id = body_id;
    result.debug_body = debug;
    result.debug_body.body_id = body_id;
    return result;
}

void PhysicsWorld::DestroyBody(JPH::BodyID body_id)
{
    if (!initialized_ || body_id.IsInvalid())
    {
        return;
    }

    RemoveConstraintsForBody(body_id);

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

bool PhysicsWorld::SetBodyPhysicsParams(JPH::BodyID body_id,
                                        const BodyPhysicsParams &params,
                                        bool activate)
{
    if (!initialized_ || body_id.IsInvalid() || !IsBodyAdded(body_id))
    {
        return false;
    }

    JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    body_interface.SetFriction(body_id, params.friction);
    body_interface.SetRestitution(body_id, params.restitution);
    body_interface.SetGravityFactor(body_id, params.gravity_factor);
    body_interface.SetUseManifoldReduction(body_id, params.use_manifold_reduction);

    const JPH::BodyLockInterface &lock_interface = physics_system_->GetBodyLockInterface();
    JPH::BodyLockWrite lock(lock_interface, body_id);
    if (!lock.Succeeded())
    {
        return false;
    }

    JPH::Body &body = lock.GetBody();
    body.SetIsSensor(params.is_sensor);
    body.SetCollideKinematicVsNonDynamic(params.collide_kinematic_vs_non_dynamic);
    body.SetApplyGyroscopicForce(params.apply_gyroscopic_force);
    body.SetEnhancedInternalEdgeRemoval(params.enhanced_internal_edge_removal);
    body.SetAllowSleeping(params.allow_sleeping);

    if (JPH::MotionProperties *motion = body.GetMotionPropertiesUnchecked(); motion != nullptr)
    {
        motion->SetLinearDamping(params.linear_damping);
        motion->SetAngularDamping(params.angular_damping);
        motion->SetMaxLinearVelocity(params.max_linear_velocity);
        motion->SetMaxAngularVelocity(params.max_angular_velocity);
    }

    if (activate)
    {
        body_interface.ActivateBody(body_id);
    }
    return true;
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

bool PhysicsWorld::AddConstraint(const ConstraintDesc &desc)
{
    if (!initialized_ || physics_system_ == nullptr || desc.body2.IsInvalid())
    {
        return false;
    }

    if (!IsBodyAdded(desc.body2))
    {
        return false;
    }

    if (!desc.body1.IsInvalid() && !IsBodyAdded(desc.body1))
    {
        return false;
    }

    if (!desc.body1.IsInvalid() && desc.body1 == desc.body2)
    {
        return false;
    }

    ConstraintDesc normalized = desc;
    if (normalized.min_limit > normalized.max_limit)
    {
        std::swap(normalized.min_limit, normalized.max_limit);
    }

    JPH::Ref<JPH::Constraint> constraint;

    auto create_constraint = [&](JPH::Body &body1, JPH::Body &body2) -> bool
    {
        switch (normalized.type)
        {
        case ConstraintType::Fixed:
        {
            JPH::FixedConstraintSettings settings;
            settings.mSpace = JPH::EConstraintSpace::WorldSpace;
            settings.mAutoDetectPoint = normalized.auto_detect_point;
            settings.mPoint1 = ToRVec3(normalized.point1);
            settings.mPoint2 = ToRVec3(normalized.point2);
            settings.mAxisX1 = ToVec3(normalized.axis1);
            settings.mAxisY1 = ToVec3(normalized.normal1);
            settings.mAxisX2 = ToVec3(normalized.axis2);
            settings.mAxisY2 = ToVec3(normalized.normal2);
            settings.mEnabled = normalized.enabled;
            constraint = settings.Create(body1, body2);
            return constraint != nullptr;
        }
        case ConstraintType::Point:
        {
            JPH::PointConstraintSettings settings;
            settings.mSpace = JPH::EConstraintSpace::WorldSpace;
            settings.mPoint1 = ToRVec3(normalized.point1);
            settings.mPoint2 = ToRVec3(normalized.point2);
            settings.mEnabled = normalized.enabled;
            constraint = settings.Create(body1, body2);
            return constraint != nullptr;
        }
        case ConstraintType::Distance:
        {
            JPH::DistanceConstraintSettings settings;
            settings.mSpace = JPH::EConstraintSpace::WorldSpace;
            settings.mPoint1 = ToRVec3(normalized.point1);
            settings.mPoint2 = ToRVec3(normalized.point2);
            settings.mMinDistance = normalized.min_limit;
            settings.mMaxDistance = normalized.max_limit;
            settings.mEnabled = normalized.enabled;
            constraint = settings.Create(body1, body2);
            return constraint != nullptr;
        }
        case ConstraintType::Hinge:
        {
            JPH::HingeConstraintSettings settings;
            settings.mSpace = JPH::EConstraintSpace::WorldSpace;
            settings.mPoint1 = ToRVec3(normalized.point1);
            settings.mPoint2 = ToRVec3(normalized.point2);
            settings.mHingeAxis1 = ToVec3(normalized.axis1);
            settings.mHingeAxis2 = ToVec3(normalized.axis2);
            settings.mNormalAxis1 = ToVec3(normalized.normal1);
            settings.mNormalAxis2 = ToVec3(normalized.normal2);
            settings.mLimitsMin = normalized.min_limit;
            settings.mLimitsMax = normalized.max_limit;
            settings.mMaxFrictionTorque = normalized.max_friction;
            settings.mMotorSettings.mSpringSettings.mFrequency = normalized.motor.spring_frequency;
            settings.mMotorSettings.mSpringSettings.mDamping = normalized.motor.spring_damping;
            settings.mMotorSettings.SetForceLimit(normalized.motor.max_force);
            settings.mMotorSettings.SetTorqueLimit(normalized.motor.max_torque);
            settings.mEnabled = normalized.enabled;

            JPH::Constraint *raw = settings.Create(body1, body2);
            if (raw == nullptr)
            {
                return false;
            }
            auto *hinge = static_cast<JPH::HingeConstraint *>(raw);
            hinge->SetMotorState(ToJoltMotorState(normalized.motor.mode));
            hinge->SetTargetAngularVelocity(normalized.motor.target_velocity);
            hinge->SetTargetAngle(normalized.motor.target_position);
            constraint = hinge;
            return true;
        }
        case ConstraintType::Slider:
        {
            JPH::SliderConstraintSettings settings;
            settings.mSpace = JPH::EConstraintSpace::WorldSpace;
            settings.mAutoDetectPoint = normalized.auto_detect_point;
            settings.mPoint1 = ToRVec3(normalized.point1);
            settings.mPoint2 = ToRVec3(normalized.point2);
            settings.mSliderAxis1 = ToVec3(normalized.axis1);
            settings.mSliderAxis2 = ToVec3(normalized.axis2);
            settings.mNormalAxis1 = ToVec3(normalized.normal1);
            settings.mNormalAxis2 = ToVec3(normalized.normal2);
            settings.mLimitsMin = normalized.min_limit;
            settings.mLimitsMax = normalized.max_limit;
            settings.mMaxFrictionForce = normalized.max_friction;
            settings.mMotorSettings.mSpringSettings.mFrequency = normalized.motor.spring_frequency;
            settings.mMotorSettings.mSpringSettings.mDamping = normalized.motor.spring_damping;
            settings.mMotorSettings.SetForceLimit(normalized.motor.max_force);
            settings.mMotorSettings.SetTorqueLimit(normalized.motor.max_torque);
            settings.mEnabled = normalized.enabled;

            JPH::Constraint *raw = settings.Create(body1, body2);
            if (raw == nullptr)
            {
                return false;
            }
            auto *slider = static_cast<JPH::SliderConstraint *>(raw);
            slider->SetMotorState(ToJoltMotorState(normalized.motor.mode));
            slider->SetTargetVelocity(normalized.motor.target_velocity);
            slider->SetTargetPosition(normalized.motor.target_position);
            constraint = slider;
            return true;
        }
        }
        return false;
    };

    if (desc.body1.IsInvalid())
    {
        const JPH::BodyLockInterface &lock_interface = physics_system_->GetBodyLockInterface();
        JPH::BodyLockWrite lock(lock_interface, desc.body2);
        if (!lock.Succeeded())
        {
            return false;
        }

        JPH::Body &body2 = lock.GetBody();
        if (!create_constraint(JPH::Body::sFixedToWorld, body2))
        {
            return false;
        }
    }
    else
    {
        const JPH::BodyID ids[2] = {desc.body1, desc.body2};
        const JPH::BodyLockInterface &lock_interface = physics_system_->GetBodyLockInterface();
        JPH::BodyLockMultiWrite lock(lock_interface, ids, 2);

        JPH::Body *body1 = lock.GetBody(0);
        JPH::Body *body2 = lock.GetBody(1);
        if (body1 == nullptr || body2 == nullptr)
        {
            return false;
        }

        if (!create_constraint(*body1, *body2))
        {
            return false;
        }
    }

    if (constraint == nullptr)
    {
        return false;
    }

    physics_system_->AddConstraint(constraint.GetPtr());

    ConstraintHandle handle{};
    handle.id = normalized.id;
    handle.type = normalized.type;
    handle.constraint = constraint;
    handle.body1 = normalized.body1;
    handle.body2 = normalized.body2;
    handle.desc = normalized;
    constraints_.push_back(handle);
    return true;
}
bool PhysicsWorld::RemoveConstraintById(const std::string &id)
{
    const auto it = std::find_if(
        constraints_.begin(),
        constraints_.end(),
        [&id](const ConstraintHandle &handle)
        {
            return handle.id == id;
        });
    if (it == constraints_.end())
    {
        return false;
    }

    physics_system_->RemoveConstraint(it->constraint.GetPtr());
    constraints_.erase(it);
    return true;
}

bool PhysicsWorld::RemoveConstraintsForBody(JPH::BodyID body_id)
{
    if (body_id.IsInvalid())
    {
        return false;
    }

    bool removed_any = false;
    for (auto it = constraints_.begin(); it != constraints_.end();)
    {
        if (it->body1 == body_id || it->body2 == body_id)
        {
            physics_system_->RemoveConstraint(it->constraint.GetPtr());
            it = constraints_.erase(it);
            removed_any = true;
        }
        else
        {
            ++it;
        }
    }
    return removed_any;
}

void PhysicsWorld::ClearConstraints()
{
    if (!initialized_ || physics_system_ == nullptr)
    {
        constraints_.clear();
        return;
    }

    for (const ConstraintHandle &handle : constraints_)
    {
        physics_system_->RemoveConstraint(handle.constraint.GetPtr());
    }
    constraints_.clear();
}

const std::vector<ConstraintHandle> &PhysicsWorld::Constraints() const
{
    return constraints_;
}

JPH::BodyID PhysicsWorld::CreateBodyWithSettings(const JPH::BodyCreationSettings &settings,
                                                 const PhysicsDebugBody &debug_body,
                                                 bool activate)
{
    if (!initialized_)
    {
        return JPH::BodyID();
    }

    JPH::BodyInterface &body_interface = physics_system_->GetBodyInterface();
    const JPH::BodyID body_id = body_interface.CreateAndAddBody(
        settings,
        activate ? JPH::EActivation::Activate : JPH::EActivation::DontActivate);
    if (body_id.IsInvalid())
    {
        return body_id;
    }

    created_bodies_.push_back(body_id);
    PhysicsDebugBody stored = debug_body;
    stored.body_id = body_id;
    RegisterDebugBody(stored);
    return body_id;
}

void PhysicsWorld::ApplyBodyPhysicsParams(JPH::BodyCreationSettings &settings,
                                          const BodyPhysicsParams &params,
                                          bool dynamic) const
{
    settings.mFriction = params.friction;
    settings.mRestitution = params.restitution;
    settings.mLinearDamping = params.linear_damping;
    settings.mAngularDamping = params.angular_damping;
    settings.mGravityFactor = params.gravity_factor;
    settings.mMaxLinearVelocity = params.max_linear_velocity;
    settings.mMaxAngularVelocity = params.max_angular_velocity;
    settings.mAllowSleeping = params.allow_sleeping;
    settings.mIsSensor = params.is_sensor;
    settings.mUseManifoldReduction = params.use_manifold_reduction;
    settings.mApplyGyroscopicForce = params.apply_gyroscopic_force;
    settings.mEnhancedInternalEdgeRemoval = params.enhanced_internal_edge_removal;
    settings.mCollideKinematicVsNonDynamic = params.collide_kinematic_vs_non_dynamic;
    settings.mAllowDynamicOrKinematic = params.allow_dynamic_or_kinematic;
    settings.mMotionQuality = params.use_linear_cast
                                  ? JPH::EMotionQuality::LinearCast
                                  : JPH::EMotionQuality::Discrete;

    if (dynamic && params.use_custom_mass)
    {
        settings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
        settings.mMassPropertiesOverride.mMass = std::max(params.mass, 0.01f);
    }
}

JPH::EMotorState PhysicsWorld::ToJoltMotorState(MotorMode mode)
{
    switch (mode)
    {
    case MotorMode::Velocity:
        return JPH::EMotorState::Velocity;
    case MotorMode::Position:
        return JPH::EMotorState::Position;
    case MotorMode::Off:
    default:
        return JPH::EMotorState::Off;
    }
}

void PhysicsWorld::RegisterDebugBody(const PhysicsDebugBody &debug_body)
{
    debug_bodies_.push_back(debug_body);
}
