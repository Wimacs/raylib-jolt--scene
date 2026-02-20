#include "app/GameApp.h"

#include "PhysicsWorld.h"
#include "SceneSystem.h"
#include "game/fps/FpsSandboxRuntime.h"

#include <raylib.h>

#include <filesystem>
#include <string>
#include <vector>

namespace app
{
namespace
{
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
} // namespace

int GameApp::Run()
{
    SetConfigFlags(FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(1600, 960, "Raylib + Jolt Scene System");
    SetWindowMinSize(1120, 720);
    SetTargetFPS(120);

    PhysicsWorld physics_world;
    if (!physics_world.Initialize())
    {
        CloseWindow();
        return 1;
    }

    SceneSystem scene(physics_world);
    scene.InitializeDefaultScene();

    std::string load_error;
    scene.LoadFromJson(
        ResolveScenePathForLoad("scene_fps_sandbox.json"),
        load_error);

    game::fps::FpsSandboxRuntime runtime(scene, physics_world);
    runtime.Initialize();

    while (!WindowShouldClose())
    {
        const float frame_dt = GetFrameTime();
        runtime.Update(frame_dt);

        BeginDrawing();
        ClearBackground(Color{236, 239, 246, 255});

        runtime.DrawWorld();
        runtime.DrawOverlay();

        EndDrawing();
    }

    physics_world.Shutdown();
    CloseWindow();
    return 0;
}
} // namespace app
