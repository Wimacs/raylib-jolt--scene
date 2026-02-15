#include "SceneSystem.h"

#include <raygui.h>
#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
void SetupCamera(Camera3D &camera)
{
    camera.position = Vector3{8.0f, 7.0f, 10.0f};
    camera.target = Vector3{0.0f, 1.5f, 0.0f};
    camera.up = Vector3{0.0f, 1.0f, 0.0f};
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
}

Vector3 SpawnPositionFromCamera(const Camera3D &camera)
{
    const Vector3 direction =
        Vector3Normalize(Vector3Subtract(camera.target, camera.position));
    Vector3 spawn = Vector3Add(camera.position, Vector3Scale(direction, 6.0f));
    spawn.y = std::max(spawn.y, 1.2f);
    return spawn;
}

enum class AddShapeType : int
{
    Box = 0,
    Sphere = 1,
    Capsule = 2,
    Cylinder = 3,
};

void AddShapeFromUi(SceneSystem &scene,
                    AddShapeType shape,
                    const Vector3 &spawn,
                    float size_x,
                    float size_y,
                    float size_z,
                    float radius,
                    float half_height)
{
    switch (shape)
    {
    case AddShapeType::Box:
        scene.AddBox(spawn,
                     Vector3{size_x, size_y, size_z},
                     true,
                     Color{0, 0, 0, 0});
        break;
    case AddShapeType::Sphere:
        scene.AddSphere(spawn, radius, true, Color{0, 0, 0, 0});
        break;
    case AddShapeType::Capsule:
        scene.AddCapsule(spawn, half_height, radius, Color{0, 0, 0, 0});
        break;
    case AddShapeType::Cylinder:
        scene.AddCylinder(spawn, half_height, radius, Color{0, 0, 0, 0});
        break;
    }
}

std::string ResolveScenePathForLoad(const char *path)
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

std::string ResolveScenePathForSave(const char *path)
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

bool SaveScene(SceneSystem &scene,
               const char *path,
               std::string &status,
               Color &status_color)
{
    const std::string resolved_path = ResolveScenePathForSave(path);
    const bool ok = scene.SaveToJson(resolved_path);
    if (ok)
    {
        status = std::string("Saved scene to: ") + resolved_path;
        status_color = DARKGREEN;
    }
    else
    {
        status = std::string("Save failed: ") + resolved_path;
        status_color = MAROON;
    }
    return ok;
}

bool LoadScene(SceneSystem &scene,
               const char *path,
               std::string &status,
               Color &status_color,
               int &hovered_object_id,
               int &selected_object_id)
{
    const std::string resolved_path = ResolveScenePathForLoad(path);

    std::string error;
    const bool ok = scene.LoadFromJson(resolved_path, error);
    if (ok)
    {
        scene.EndDrag();
        hovered_object_id = 0;
        selected_object_id = 0;
        status = std::string("Loaded scene from: ") + resolved_path;
        status_color = DARKGREEN;
    }
    else
    {
        status = std::string("Load failed: ") + error;
        status_color = MAROON;
    }
    return ok;
}
} // namespace

int main()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1600, 960, "Raylib + Jolt Scene System");
    SetWindowMinSize(1120, 720);
    SetTargetFPS(120);

    Camera3D camera{};
    SetupCamera(camera);

    PhysicsWorld physics_world;
    if (!physics_world.Initialize())
    {
        CloseWindow();
        return 1;
    }

    SceneSystem scene(physics_world);
    scene.InitializeDefaultScene();

    int hovered_object_id = 0;
    int selected_object_id = 0;

    bool show_ui = true;
    bool show_help = true;
    bool show_physics_debug = true;

    int add_shape_index = 0;
    float add_size_x = 0.5f;
    float add_size_y = 0.5f;
    float add_size_z = 0.5f;
    float add_radius = 0.35f;
    float add_half_height = 0.50f;

    std::array<char, 256> scene_json_path{};
    std::snprintf(scene_json_path.data(), scene_json_path.size(), "%s", "scene_default_showcase.json");
    bool editing_scene_json_path = false;
    std::string scene_io_status = "Ready";
    Color scene_io_status_color = DARKGREEN;

    if (!LoadScene(scene,
                   scene_json_path.data(),
                   scene_io_status,
                   scene_io_status_color,
                   hovered_object_id,
                   selected_object_id))
    {
        scene_io_status = "Default showcase not found. Using built-in scene.";
        scene_io_status_color = ORANGE;
    }

    while (!WindowShouldClose())
    {
        const float dt = GetFrameTime();
        const Vector2 mouse = GetMousePosition();

        if (IsKeyPressed(KEY_F1))
        {
            show_ui = !show_ui;
        }
        if (IsKeyPressed(KEY_F2))
        {
            show_physics_debug = !show_physics_debug;
        }
        if (IsKeyPressed(KEY_F3))
        {
            show_help = !show_help;
        }
        if (IsKeyPressed(KEY_F5))
        {
            SaveScene(scene,
                      scene_json_path.data(),
                      scene_io_status,
                      scene_io_status_color);
        }
        if (IsKeyPressed(KEY_F9))
        {
            LoadScene(scene,
                      scene_json_path.data(),
                      scene_io_status,
                      scene_io_status_color,
                      hovered_object_id,
                      selected_object_id);
        }

        UpdateCamera(&camera, CAMERA_FREE);

        const auto hovered = scene.PickObject(camera, mouse, false);
        hovered_object_id = hovered.has_value() ? *hovered : 0;

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        {
            if (scene.StartDrag(camera, mouse))
            {
                selected_object_id = scene.DraggingObjectId();
            }
            else
            {
                selected_object_id = hovered_object_id;
            }
        }

        if (scene.IsDragging())
        {
            if (IsMouseButtonDown(MOUSE_BUTTON_LEFT))
            {
                scene.UpdateDrag(camera, mouse);
            }
            else
            {
                scene.EndDrag();
            }
        }

        if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE))
        {
            if (selected_object_id != 0 && scene.RemoveObject(selected_object_id))
            {
                selected_object_id = 0;
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && hovered_object_id != 0)
        {
            if (scene.RemoveObject(hovered_object_id) && hovered_object_id == selected_object_id)
            {
                selected_object_id = 0;
            }
        }

        if (IsKeyPressed(KEY_ONE))
        {
            const Vector3 spawn = SpawnPositionFromCamera(camera);
            scene.AddBox(spawn, Vector3{0.5f, 0.5f, 0.5f}, true, Color{0, 0, 0, 0});
        }
        if (IsKeyPressed(KEY_TWO))
        {
            const Vector3 spawn = SpawnPositionFromCamera(camera);
            scene.AddSphere(spawn, 0.45f, true, Color{0, 0, 0, 0});
        }
        if (IsKeyPressed(KEY_THREE))
        {
            const Vector3 spawn = SpawnPositionFromCamera(camera);
            scene.AddCapsule(spawn, 0.50f, 0.30f, Color{0, 0, 0, 0});
        }
        if (IsKeyPressed(KEY_FOUR))
        {
            const Vector3 spawn = SpawnPositionFromCamera(camera);
            scene.AddCylinder(spawn, 0.50f, 0.35f, Color{0, 0, 0, 0});
        }

        scene.Step(dt);

        BeginDrawing();
        ClearBackground(Color{236, 239, 246, 255});

        BeginMode3D(camera);
        DrawGrid(60, 1.0f);

        const int highlighted_id =
            scene.IsDragging() ? scene.DraggingObjectId() : hovered_object_id;
        scene.Draw(highlighted_id != 0 ? highlighted_id : selected_object_id);

        if (show_physics_debug)
        {
            scene.DrawPhysicsDebug(true);
        }

        EndMode3D();

        if (show_ui)
        {
            const float panel_x = 16.0f;
            const float panel_y = 16.0f;
            const float panel_w = 400.0f;
            const float panel_h = std::max(700.0f, static_cast<float>(GetScreenHeight()) - 32.0f);
            const Rectangle panel = Rectangle{panel_x, panel_y, panel_w, panel_h};
            GuiPanel(panel, "Scene Controls");

            float y = panel.y + 36.0f;
            const float x = panel.x + 16.0f;

            GuiLabel(Rectangle{x, y, 220, 22}, "Spawn Shape");
            y += 26.0f;

            GuiToggleGroup(Rectangle{x, y, 360, 28},
                           "Box;Sphere;Capsule;Cylinder",
                           &add_shape_index);
            y += 44.0f;

            GuiLabel(Rectangle{x, y, 180, 22}, "Box Half Extents (m)");
            y += 24.0f;
            GuiSliderBar(Rectangle{x, y, 360, 20}, "X", TextFormat("%.2f", add_size_x), &add_size_x, 0.10f, 2.00f);
            y += 26.0f;
            GuiSliderBar(Rectangle{x, y, 360, 20}, "Y", TextFormat("%.2f", add_size_y), &add_size_y, 0.10f, 2.00f);
            y += 26.0f;
            GuiSliderBar(Rectangle{x, y, 360, 20}, "Z", TextFormat("%.2f", add_size_z), &add_size_z, 0.10f, 2.00f);
            y += 34.0f;

            GuiLabel(Rectangle{x, y, 180, 22}, "Round Shapes (m)");
            y += 24.0f;
            GuiSliderBar(Rectangle{x, y, 360, 20}, "Radius", TextFormat("%.2f", add_radius), &add_radius, 0.05f, 1.50f);
            y += 26.0f;
            GuiSliderBar(Rectangle{x, y, 360, 20}, "Half Height", TextFormat("%.2f", add_half_height), &add_half_height, 0.05f, 1.50f);
            y += 40.0f;

            if (GuiButton(Rectangle{x, y, 176, 30}, "Add At Camera"))
            {
                const Vector3 spawn = SpawnPositionFromCamera(camera);
                AddShapeFromUi(scene,
                               static_cast<AddShapeType>(add_shape_index),
                               spawn,
                               add_size_x,
                               add_size_y,
                               add_size_z,
                               add_radius,
                               add_half_height);
            }

            if (GuiButton(Rectangle{x + 184, y, 176, 30}, "Delete Selected"))
            {
                if (selected_object_id != 0 && scene.RemoveObject(selected_object_id))
                {
                    selected_object_id = 0;
                }
            }
            y += 42.0f;

            GuiLabel(Rectangle{x, y, 220, 22}, "Scene JSON Path");
            y += 24.0f;
            if (GuiTextBox(Rectangle{x, y, 360, 30},
                           scene_json_path.data(),
                           static_cast<int>(scene_json_path.size()),
                           editing_scene_json_path))
            {
                editing_scene_json_path = !editing_scene_json_path;
            }
            y += 40.0f;

            if (GuiButton(Rectangle{x, y, 176, 30}, "Save Scene JSON"))
            {
                SaveScene(scene,
                          scene_json_path.data(),
                          scene_io_status,
                          scene_io_status_color);
            }

            if (GuiButton(Rectangle{x + 184, y, 176, 30}, "Load Scene JSON"))
            {
                LoadScene(scene,
                          scene_json_path.data(),
                          scene_io_status,
                          scene_io_status_color,
                          hovered_object_id,
                          selected_object_id);
            }
            y += 40.0f;

            DrawText(scene_io_status.c_str(), static_cast<int>(x), static_cast<int>(y), 16, scene_io_status_color);
            y += 28.0f;

            GuiCheckBox(Rectangle{x, y, 20, 20}, "Show Physics Debug", &show_physics_debug);
            y += 26.0f;
            GuiCheckBox(Rectangle{x, y, 20, 20}, "Show Help Overlay", &show_help);
            y += 28.0f;

            GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Object Count: %i", static_cast<int>(scene.Objects().size())));
            y += 22.0f;
            GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Constraint Count: %i", static_cast<int>(scene.Constraints().size())));
            y += 22.0f;
            GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Hovered ID: %i", hovered_object_id));
            y += 22.0f;
            GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Selected ID: %i", selected_object_id));
            y += 22.0f;
            GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Dragging: %s", scene.IsDragging() ? "Yes" : "No"));
            y += 22.0f;
            GuiLabel(Rectangle{x, y, 360, 22}, TextFormat("Window: %d x %d", GetScreenWidth(), GetScreenHeight()));
        }

        if (show_help)
        {
            const int margin = 16;
            const int help_w = std::min(760, std::max(460, GetScreenWidth() - 460));
            const int help_h = 128;
            const int help_x = GetScreenWidth() - help_w - margin;
            const int help_y = margin;

            DrawRectangle(help_x, help_y, help_w, help_h, Fade(RAYWHITE, 0.92f));
            DrawRectangleLines(help_x, help_y, help_w, help_h, Fade(DARKGRAY, 0.60f));

            DrawText("Controls", help_x + 14, help_y + 12, 22, BLACK);
            DrawText("WASD + mouse: move camera (free mode)", help_x + 14, help_y + 40, 18, DARKGRAY);
            DrawText("Left drag: move dynamic body | Right click: delete hovered", help_x + 14, help_y + 62, 18, DARKGRAY);
            DrawText("1/2/3/4 spawn | F1 UI | F2 debug | F3 help | F5 save | F9 load", help_x + 14, help_y + 84, 18, DARKGRAY);
            DrawText("Units: meter, kilogram, second, Newton, N·m, radian", help_x + 14, help_y + 106, 17, DARKGRAY);
        }

        EndDrawing();
    }

    physics_world.Shutdown();
    CloseWindow();
    return 0;
}
