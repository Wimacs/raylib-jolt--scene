#include "SceneSystem.h"

#include <raylib.h>
#include <raymath.h>

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
} // namespace

int main()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT);
    InitWindow(1440, 900, "Raylib + Jolt Scene System");
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

    while (!WindowShouldClose())
    {
        const float dt = GetFrameTime();

        UpdateCamera(&camera, CAMERA_FREE);

        const Vector2 mouse = GetMousePosition();
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

        if (IsKeyPressed(KEY_ONE))
        {
            const Vector3 spawn = SpawnPositionFromCamera(camera);
            scene.AddBox(spawn,
                         Vector3{0.5f, 0.5f, 0.5f},
                         true,
                         Color{0, 0, 0, 0});
        }

        if (IsKeyPressed(KEY_TWO))
        {
            const Vector3 spawn = SpawnPositionFromCamera(camera);
            scene.AddSphere(spawn, 0.45f, true, Color{0, 0, 0, 0});
        }

        if (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE))
        {
            if (selected_object_id != 0)
            {
                const bool removed = scene.RemoveObject(selected_object_id);
                if (removed)
                {
                    selected_object_id = 0;
                }
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT))
        {
            if (hovered_object_id != 0)
            {
                if (scene.RemoveObject(hovered_object_id) &&
                    hovered_object_id == selected_object_id)
                {
                    selected_object_id = 0;
                }
            }
        }

        scene.Step(dt);

        BeginDrawing();
        ClearBackground(Color{235, 238, 245, 255});

        BeginMode3D(camera);
        DrawGrid(50, 1.0f);

        const int highlighted_id =
            scene.IsDragging() ? scene.DraggingObjectId() : hovered_object_id;
        scene.Draw(highlighted_id != 0 ? highlighted_id : selected_object_id);

        EndMode3D();

        DrawRectangle(14, 14, 590, 144, Fade(RAYWHITE, 0.88f));
        DrawRectangleLines(14, 14, 590, 144, Fade(DARKGRAY, 0.7f));

        DrawText("Raylib + Jolt Scene System", 28, 28, 26, BLACK);
        DrawText("WASD + Mouse: 移动相机 (raylib free camera)", 28, 62, 20, DARKGRAY);
        DrawText("1: 增加 Box    2: 增加 Sphere", 28, 86, 20, DARKGRAY);
        DrawText("左键拖拽物体  右键删除悬停物体  Del/Backspace 删除选中", 28, 110, 20, DARKGRAY);

        const std::string object_count_text = "Objects: " +
                                              std::to_string(scene.Objects().size());
        DrawText(object_count_text.c_str(), 28, 132, 18, GRAY);

        if (selected_object_id != 0)
        {
            const std::string selected_text = "Selected ID: " +
                                              std::to_string(selected_object_id);
            DrawText(selected_text.c_str(), 460, 132, 18, MAROON);
        }

        EndDrawing();
    }

    physics_world.Shutdown();
    CloseWindow();
    return 0;
}
