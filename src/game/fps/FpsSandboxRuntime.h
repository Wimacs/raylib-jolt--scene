#pragma once

#include "SceneSystem.h"
#include "engine/core/FixedStepClock.h"
#include "game/fps/FpsPlayerController.h"

#include <array>
#include <raylib.h>
#include <vector>

namespace game::fps
{
class FpsSandboxRuntime
{
public:
    FpsSandboxRuntime(SceneSystem &scene, PhysicsWorld &physics_world);

    void Initialize();
    void Update(float frame_delta_seconds);

    void DrawWorld() const;
    void DrawOverlay() const;

private:
    struct BallisticBullet
    {
        Vector3 previous_position{0.0f, 0.0f, 0.0f};
        Vector3 current_position{0.0f, 0.0f, 0.0f};
        Vector3 velocity{0.0f, 0.0f, 0.0f};
        float remaining_life_seconds{0.0f};
    };

    struct ImpactDecal
    {
        Vector3 position{0.0f, 0.0f, 0.0f};
        Vector3 normal{0.0f, 1.0f, 0.0f};
        float radius{0.03f};
        float remaining_life_seconds{0.0f};
    };

    struct WeaponSlot
    {
        const char *display_name{"Weapon"};
        int magazine_capacity{0};
        int ammo_in_magazine{0};
        int reserve_ammo{0};
        bool automatic_fire{false};
        float fire_interval_seconds{0.15f};
        float reload_duration_seconds{1.5f};
        float hip_spread{0.015f};
        float ads_spread{0.005f};
        float muzzle_speed{85.0f};
        Color accent{WHITE};
    };

    void SetupDefaultCamera();
    void DrawGameplayMarkers() const;
    void DrawBullets() const;
    void DrawImpactDecals() const;
    void DrawGrappleVisual() const;
    void DrawWeaponViewModel() const;
    void DrawWeaponHud() const;
    void EnterPlayerMode();
    void RespawnPlayer();
    void HandleHotkeys();
    void UpdateWeaponState(float frame_delta_seconds);
    void UpdateRecoil(float frame_delta_seconds);
    void ApplyRecoilToCamera();
    void HandleWeaponFire();
    void StartReload();
    void CompleteReload();
    void SwitchWeapon(int weapon_index);
    [[nodiscard]] const WeaponSlot &CurrentWeapon() const;
    [[nodiscard]] WeaponSlot &CurrentWeapon();
    void SpawnBullet(const Vector3 &origin, const Vector3 &direction, float muzzle_speed);
    void SpawnImpactDecal(const Vector3 &position,
                          const Vector3 &normal,
                          float intensity_scale);
    void UpdateBullets(float fixed_delta_seconds);
    void UpdateImpactDecals(float frame_delta_seconds);
    [[nodiscard]] Vector3 CameraForward() const;
    [[nodiscard]] Vector3 CameraRight(const Vector3 &forward) const;

    SceneSystem &scene_;
    PhysicsWorld &physics_world_;
    Camera3D camera_{};
    FpsPlayerController player_controller_;
    engine::core::FixedStepClock fixed_step_clock_;

    bool show_physics_debug_{true};
    bool show_help_overlay_{true};
    bool aiming_down_sights_{false};
    bool trigger_held_{false};
    bool trigger_pressed_{false};
    bool reloading_{false};
    float aim_blend_{0.0f};
    float fire_cooldown_seconds_{0.0f};
    float reload_remaining_seconds_{0.0f};
    Vector2 recoil_offset_{0.0f, 0.0f};
    float spread_bloom_{0.0f};

    float hip_fov_{75.0f};
    float ads_fov_{42.0f};
    float bullet_life_seconds_{4.0f};
    float bullet_gravity_{9.81f};
    float recoil_recover_speed_{6.2f};
    float recoil_pitch_kick_{0.0155f};
    float recoil_yaw_kick_{0.0088f};
    float recoil_max_pitch_{0.30f};
    float recoil_max_yaw_{0.20f};
    float spread_bloom_per_shot_{0.0029f};
    float spread_bloom_decay_{0.031f};
    float spread_bloom_max_{0.032f};
    float decal_life_seconds_{8.5f};
    int max_impact_decals_{180};

    std::array<WeaponSlot, 2> weapon_slots_{
        WeaponSlot{
            "Pistol",
            12,
            12,
            72,
            false,
            0.28f,
            1.45f,
            0.010f,
            0.0030f,
            460.0f,
            Color{228, 189, 118, 255}},
        WeaponSlot{
            "AK-47",
            20,
            20,
            160,
            true,
            0.092f,
            2.20f,
            0.020f,
            0.0070f,
            430.0f,
            Color{201, 110, 82, 255}}};
    int current_weapon_index_{0};

    std::vector<BallisticBullet> bullets_;
    std::vector<ImpactDecal> impact_decals_;

    std::array<Vector3, 4> spawn_points_{
        Vector3{-14.0f, 1.10f, -14.0f},
        Vector3{14.0f, 1.10f, -14.0f},
        Vector3{-14.0f, 1.10f, 14.0f},
        Vector3{14.0f, 1.10f, 14.0f}};
    std::array<Vector3, 4> spawn_look_targets_{
        Vector3{-6.0f, 1.5f, -6.0f},
        Vector3{6.0f, 1.5f, -6.0f},
        Vector3{-6.0f, 1.5f, 6.0f},
        Vector3{6.0f, 1.5f, 6.0f}};
    std::array<Vector3, 5> pickup_points_{
        Vector3{0.0f, 0.2f, 0.0f},
        Vector3{-8.0f, 0.2f, 0.0f},
        Vector3{8.0f, 0.2f, 0.0f},
        Vector3{0.0f, 0.2f, -10.0f},
        Vector3{0.0f, 0.2f, 10.0f}};

    int current_spawn_point_index_{0};
};
} // namespace game::fps
