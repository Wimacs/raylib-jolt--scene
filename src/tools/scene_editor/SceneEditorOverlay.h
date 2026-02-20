#pragma once

#include "SceneSystem.h"
#include "game/fps/FpsSandboxRuntime.h"

#include <array>
#include <string>

namespace tools::scene_editor
{
class SceneEditorOverlay
{
public:
    SceneEditorOverlay(SceneSystem &scene, game::fps::FpsSandboxRuntime &runtime);

    void Initialize();
    void HandleShortcuts();

    void DrawPanel(bool visible);
    void DrawHelpOverlay() const;

private:
    enum class AddShapeType : int
    {
        Box = 0,
        Sphere = 1,
        Capsule = 2,
        Cylinder = 3,
    };

    [[nodiscard]] std::string ResolveScenePathForLoad(const char *path) const;
    [[nodiscard]] std::string ResolveScenePathForSave(const char *path) const;

    bool SaveScene(const char *path);
    bool LoadScene(const char *path);

    void AddShapeFromUi(AddShapeType shape);

    SceneSystem &scene_;
    game::fps::FpsSandboxRuntime &runtime_;

    int add_shape_index_{0};
    float add_size_x_{0.5f};
    float add_size_y_{0.5f};
    float add_size_z_{0.5f};
    float add_radius_{0.35f};
    float add_half_height_{0.50f};

    std::array<char, 256> scene_json_path_{};
    bool editing_scene_json_path_{false};
    std::string scene_io_status_{"Ready"};
    Color scene_io_status_color_{DARKGREEN};
};
} // namespace tools::scene_editor
