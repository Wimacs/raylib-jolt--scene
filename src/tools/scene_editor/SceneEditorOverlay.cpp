#include "tools/scene_editor/SceneEditorOverlay.h"

#include <raygui.h>
#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace tools::scene_editor
{
SceneEditorOverlay::SceneEditorOverlay(SceneSystem &scene,
                                       game::fps::FpsSandboxRuntime &runtime)
    : scene_(scene), runtime_(runtime)
{
}

void SceneEditorOverlay::Initialize()
{
    std::snprintf(scene_json_path_.data(),
                  scene_json_path_.size(),
                  "%s",
                  "scene_fps_sandbox.json");

    if (!LoadScene(scene_json_path_.data()))
    {
        scene_io_status_ = "Default FPS sandbox not found. Using built-in scene.";
        scene_io_status_color_ = ORANGE;
    }
}

void SceneEditorOverlay::HandleShortcuts()
{
    if (IsKeyPressed(KEY_F5))
    {
        SaveScene(scene_json_path_.data());
    }
    if (IsKeyPressed(KEY_F9))
    {
        LoadScene(scene_json_path_.data());
    }
}

void SceneEditorOverlay::DrawPanel(bool visible)
{
    if (!visible)
    {
        return;
    }

    const float panel_x = 16.0f;
    const float panel_y = 16.0f;
    const float panel_w = 400.0f;
    const float panel_h = std::max(700.0f,
                                   static_cast<float>(GetScreenHeight()) - 32.0f);
    const Rectangle panel = Rectangle{panel_x, panel_y, panel_w, panel_h};
    GuiPanel(panel, "Scene Controls");

    float y = panel.y + 36.0f;
    const float x = panel.x + 16.0f;

    GuiLabel(Rectangle{x, y, 220, 22}, "Spawn Shape");
    y += 26.0f;

    GuiToggleGroup(Rectangle{x, y, 360, 28},
                   "Box;Sphere;Capsule;Cylinder",
                   &add_shape_index_);
    y += 44.0f;

    GuiLabel(Rectangle{x, y, 180, 22}, "Box Half Extents (m)");
    y += 24.0f;
    GuiSliderBar(Rectangle{x, y, 360, 20}, "X", TextFormat("%.2f", add_size_x_), &add_size_x_, 0.10f, 2.00f);
    y += 26.0f;
    GuiSliderBar(Rectangle{x, y, 360, 20}, "Y", TextFormat("%.2f", add_size_y_), &add_size_y_, 0.10f, 2.00f);
    y += 26.0f;
    GuiSliderBar(Rectangle{x, y, 360, 20}, "Z", TextFormat("%.2f", add_size_z_), &add_size_z_, 0.10f, 2.00f);
    y += 34.0f;

    GuiLabel(Rectangle{x, y, 180, 22}, "Round Shapes (m)");
    y += 24.0f;
    GuiSliderBar(Rectangle{x, y, 360, 20}, "Radius", TextFormat("%.2f", add_radius_), &add_radius_, 0.05f, 1.50f);
    y += 26.0f;
    GuiSliderBar(Rectangle{x, y, 360, 20}, "Half Height", TextFormat("%.2f", add_half_height_), &add_half_height_, 0.05f, 1.50f);
    y += 40.0f;

    if (GuiButton(Rectangle{x, y, 176, 30}, "Add At Camera"))
    {
        AddShapeFromUi(static_cast<AddShapeType>(add_shape_index_));
    }

    if (GuiButton(Rectangle{x + 184, y, 176, 30}, "Delete Selected"))
    {
        runtime_.DeleteSelectedObject();
    }
    y += 42.0f;

    GuiLabel(Rectangle{x, y, 220, 22}, "Scene JSON Path");
    y += 24.0f;
    if (GuiTextBox(Rectangle{x, y, 360, 30},
                   scene_json_path_.data(),
                   static_cast<int>(scene_json_path_.size()),
                   editing_scene_json_path_))
    {
        editing_scene_json_path_ = !editing_scene_json_path_;
    }
    y += 40.0f;

    if (GuiButton(Rectangle{x, y, 176, 30}, "Save Scene JSON"))
    {
        SaveScene(scene_json_path_.data());
    }

    if (GuiButton(Rectangle{x + 184, y, 176, 30}, "Load Scene JSON"))
    {
        LoadScene(scene_json_path_.data());
    }
    y += 40.0f;

    DrawText(scene_io_status_.c_str(),
             static_cast<int>(x),
             static_cast<int>(y),
             16,
             scene_io_status_color_);
    y += 28.0f;

    bool show_debug = runtime_.ShowPhysicsDebug();
    GuiCheckBox(Rectangle{x, y, 20, 20}, "Show Physics Debug", &show_debug);
    runtime_.SetShowPhysicsDebug(show_debug);
    y += 26.0f;

    bool show_help = runtime_.ShowHelpOverlay();
    GuiCheckBox(Rectangle{x, y, 20, 20}, "Show Help Overlay", &show_help);
    runtime_.SetShowHelpOverlay(show_help);
    y += 28.0f;

    GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Object Count: %i", static_cast<int>(scene_.Objects().size())));
    y += 22.0f;
    GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Constraint Count: %i", static_cast<int>(scene_.Constraints().size())));
    y += 22.0f;
    GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Hovered ID: %i", runtime_.HoveredObjectId()));
    y += 22.0f;
    GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Selected ID: %i", runtime_.SelectedObjectId()));
    y += 22.0f;
    GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Dragging: %s", scene_.IsDragging() ? "Yes" : "No"));
    y += 22.0f;
    GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Control Mode: %s", runtime_.IsPlayerMode() ? "FPS Player" : "Editor Free"));
    y += 22.0f;
    GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Spawn Point: %d / %d", runtime_.CurrentSpawnPointIndex() + 1, runtime_.SpawnPointCount()));
    y += 22.0f;
    GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Player Grounded: %s", runtime_.PlayerGrounded() ? "Yes" : "No"));
    y += 22.0f;
    GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Window: %d x %d", GetScreenWidth(), GetScreenHeight()));
}

void SceneEditorOverlay::DrawHelpOverlay() const
{
    if (!runtime_.ShowHelpOverlay())
    {
        return;
    }

    const int margin = 16;
    const int help_w = std::min(760, std::max(460, GetScreenWidth() - 460));
    const int help_h = 128;
    const int help_x = GetScreenWidth() - help_w - margin;
    const int help_y = margin;

    DrawRectangle(help_x, help_y, help_w, help_h, Fade(RAYWHITE, 0.92f));
    DrawRectangleLines(help_x, help_y, help_w, help_h, Fade(DARKGRAY, 0.60f));

    DrawText("Controls", help_x + 14, help_y + 12, 22, BLACK);
    DrawText("F4 mode switch | F6 next spawn | R respawn (FPS mode)", help_x + 14, help_y + 40, 18, DARKGRAY);
    DrawText("Editor: left drag body | right click delete | 1/2/3/4 spawn", help_x + 14, help_y + 62, 18, DARKGRAY);
    DrawText("FPS: WASD move | Shift sprint | Space jump | mouse look", help_x + 14, help_y + 84, 18, DARKGRAY);
    DrawText("Units: meter, kilogram, second, Newton, N.m, radian", help_x + 14, help_y + 106, 17, DARKGRAY);
}

std::string SceneEditorOverlay::ResolveScenePathForLoad(const char *path) const
{
    namespace fs = std::filesystem;

    if (path == nullptr || path[0] == '\0')
    {
        return {};
    }

    const fs::path input(path);
    std::vector<fs::path> candidates;
    candidates.push_back(input);

    if (input.is_relative())
    {
        const char *app_dir_raw = GetApplicationDirectory();
        if (app_dir_raw != nullptr && app_dir_raw[0] != '\0')
        {
            const fs::path app_dir(app_dir_raw);
            candidates.push_back(app_dir / input);
            candidates.push_back(app_dir / ".." / input);
            candidates.push_back(app_dir / ".." / ".." / input);
        }
    }

    for (const fs::path &candidate : candidates)
    {
        std::error_code ec;
        if (fs::exists(candidate, ec) && fs::is_regular_file(candidate, ec))
        {
            return candidate.lexically_normal().string();
        }
    }

    return input.string();
}

std::string SceneEditorOverlay::ResolveScenePathForSave(const char *path) const
{
    namespace fs = std::filesystem;

    if (path == nullptr || path[0] == '\0')
    {
        return {};
    }

    const fs::path input(path);
    if (!input.is_relative())
    {
        return input.string();
    }

    const std::string existing = ResolveScenePathForLoad(path);
    if (!existing.empty() && existing != input.string())
    {
        return existing;
    }

    const char *app_dir_raw = GetApplicationDirectory();
    if (app_dir_raw != nullptr && app_dir_raw[0] != '\0')
    {
        const fs::path target = fs::path(app_dir_raw) / ".." / input;
        return target.lexically_normal().string();
    }

    return input.string();
}

bool SceneEditorOverlay::SaveScene(const char *path)
{
    const std::string resolved_path = ResolveScenePathForSave(path);
    const bool ok = scene_.SaveToJson(resolved_path);
    if (ok)
    {
        scene_io_status_ = std::string("Saved scene to: ") + resolved_path;
        scene_io_status_color_ = DARKGREEN;
    }
    else
    {
        scene_io_status_ = std::string("Save failed: ") + resolved_path;
        scene_io_status_color_ = MAROON;
    }
    return ok;
}

bool SceneEditorOverlay::LoadScene(const char *path)
{
    const std::string resolved_path = ResolveScenePathForLoad(path);

    std::string error;
    const bool ok = scene_.LoadFromJson(resolved_path, error);
    if (ok)
    {
        runtime_.ResetInteractionState();
        scene_io_status_ = std::string("Loaded scene from: ") + resolved_path;
        scene_io_status_color_ = DARKGREEN;
    }
    else
    {
        scene_io_status_ = std::string("Load failed: ") + error;
        scene_io_status_color_ = MAROON;
    }
    return ok;
}

void SceneEditorOverlay::AddShapeFromUi(AddShapeType shape)
{
    const Vector3 spawn = runtime_.SpawnPositionFromCamera();
    switch (shape)
    {
    case AddShapeType::Box:
        scene_.AddBox(spawn,
                      Vector3{add_size_x_, add_size_y_, add_size_z_},
                      true,
                      Color{0, 0, 0, 0});
        break;
    case AddShapeType::Sphere:
        scene_.AddSphere(spawn, add_radius_, true, Color{0, 0, 0, 0});
        break;
    case AddShapeType::Capsule:
        scene_.AddCapsule(spawn, add_half_height_, add_radius_, Color{0, 0, 0, 0});
        break;
    case AddShapeType::Cylinder:
        scene_.AddCylinder(spawn, add_half_height_, add_radius_, Color{0, 0, 0, 0});
        break;
    }
}
} // namespace tools::scene_editor
