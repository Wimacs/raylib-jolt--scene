#include "SceneSystem.h"

#include <raygui.h>
#include <raylib.h>
#include <raymath.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

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
} // namespace

int main()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1600, 960, "Raylib + Jolt Scene System");
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
            const Rectangle panel = Rectangle{16, 16, 380, 420};
            GuiPanel(panel, "Scene Controls");

            float y = panel.y + 36;
            const float x = panel.x + 16;

            GuiLabel(Rectangle{x, y, 150, 22}, "Spawn Shape");
            y += 26;

            GuiToggleGroup(Rectangle{x, y, 350, 28},
                           "Box;Sphere;Capsule;Cylinder",
                           &add_shape_index);
            y += 44;

            GuiLabel(Rectangle{x, y, 140, 22}, "Box Half Extents");
            y += 24;
            GuiSliderBar(Rectangle{x, y, 350, 20}, "X", TextFormat("%.2f", add_size_x), &add_size_x, 0.10f, 2.00f);
            y += 26;
            GuiSliderBar(Rectangle{x, y, 350, 20}, "Y", TextFormat("%.2f", add_size_y), &add_size_y, 0.10f, 2.00f);
            y += 26;
            GuiSliderBar(Rectangle{x, y, 350, 20}, "Z", TextFormat("%.2f", add_size_z), &add_size_z, 0.10f, 2.00f);
            y += 34;

            GuiLabel(Rectangle{x, y, 140, 22}, "Round Shapes");
            y += 24;
            GuiSliderBar(Rectangle{x, y, 350, 20}, "Radius", TextFormat("%.2f", add_radius), &add_radius, 0.05f, 1.50f);
            y += 26;
            GuiSliderBar(Rectangle{x, y, 350, 20}, "Half Height", TextFormat("%.2f", add_half_height), &add_half_height, 0.05f, 1.50f);
            y += 40;

            if (GuiButton(Rectangle{x, y, 170, 30}, "Add At Camera"))
            {
                const Vector3 spawn = SpawnPositionFromCamera(camera);
                const AddShapeType shape = static_cast<AddShapeType>(add_shape_index);

                switch (shape)
                {
                case AddShapeType::Box:
                    scene.AddBox(spawn,
                                 Vector3{add_size_x, add_size_y, add_size_z},
                                 true,
                                 Color{0, 0, 0, 0});
                    break;
                case AddShapeType::Sphere:
                    scene.AddSphere(spawn, add_radius, true, Color{0, 0, 0, 0});
                    break;
                case AddShapeType::Capsule:
                    scene.AddCapsule(spawn, add_half_height, add_radius, Color{0, 0, 0, 0});
                    break;
                case AddShapeType::Cylinder:
                    scene.AddCylinder(spawn, add_half_height, add_radius, Color{0, 0, 0, 0});
                    break;
                }
            }

            if (GuiButton(Rectangle{x + 180, y, 170, 30}, "Delete Selected"))
            {
                if (selected_object_id != 0 && scene.RemoveObject(selected_object_id))
                {
                    selected_object_id = 0;
                }
            }
            y += 44;

            GuiCheckBox(Rectangle{x, y, 20, 20}, "Show Physics Debug", &show_physics_debug);
            y += 26;
            GuiCheckBox(Rectangle{x, y, 20, 20}, "Show Help Overlay", &show_help);
            y += 30;

            GuiLabel(Rectangle{x, y, 350, 22}, TextFormat("Object Count: %i", static_cast<int>(scene.Objects().size())));
            y += 22;
            GuiLabel(Rectangle{x, y, 350, 22}, TextFormat("Hovered ID: %i", hovered_object_id));
            y += 22;
            GuiLabel(Rectangle{x, y, 350, 22}, TextFormat("Selected ID: %i", selected_object_id));
            y += 22;
            GuiLabel(Rectangle{x, y, 350, 22}, TextFormat("Dragging: %s", scene.IsDragging() ? "Yes" : "No"));
        }

        if (show_help)
        {
            DrawRectangle(420, 16, 520, 106, Fade(RAYWHITE, 0.92f));
            DrawRectangleLines(420, 16, 520, 106, Fade(DARKGRAY, 0.60f));

            DrawText("Controls", 434, 28, 22, BLACK);
            DrawText("WASD + Mouse: move camera (free mode)", 434, 56, 19, DARKGRAY);
            DrawText("Left drag: move dynamic body | Right click: delete hovered",
                     434,
                     80,
                     19,
                     DARKGRAY);
            DrawText("1/2/3/4: spawn Box/Sphere/Capsule/Cylinder | F1 UI | F2 Physics Debug | F3 Help",
                     434,
                     102,
                     17,
                     DARKGRAY);
        }

        EndDrawing();
    }

    physics_world.Shutdown();
    CloseWindow();
    return 0;
}
