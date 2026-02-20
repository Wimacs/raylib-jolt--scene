cmake_minimum_required(VERSION 3.20)

project(raylib_jolt_scene LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES OFF CACHE BOOL "" FORCE)

set(DOUBLE_PRECISION OFF CACHE BOOL "" FORCE)
set(GENERATE_DEBUG_SYMBOLS ON CACHE BOOL "" FORCE)
set(CROSS_PLATFORM_DETERMINISTIC OFF CACHE BOOL "" FORCE)
set(INTERPROCEDURAL_OPTIMIZATION OFF CACHE BOOL "" FORCE)
set(FLOATING_POINT_EXCEPTIONS_ENABLED OFF CACHE BOOL "" FORCE)
set(OBJECT_LAYER_BITS 16 CACHE STRING "" FORCE)
if(EMSCRIPTEN)
  set(PLATFORM
      Web
      CACHE STRING "" FORCE)
  set(USE_SSE4_1 OFF CACHE BOOL "" FORCE)
  set(USE_SSE4_2 OFF CACHE BOOL "" FORCE)
  set(USE_AVX OFF CACHE BOOL "" FORCE)
  set(USE_AVX2 OFF CACHE BOOL "" FORCE)
  set(USE_AVX512 OFF CACHE BOOL "" FORCE)
  set(USE_LZCNT OFF CACHE BOOL "" FORCE)
  set(USE_TZCNT OFF CACHE BOOL "" FORCE)
  set(USE_F16C OFF CACHE BOOL "" FORCE)
  set(USE_FMADD OFF CACHE BOOL "" FORCE)
else()
  set(USE_SSE4_1 ON CACHE BOOL "" FORCE)
  set(USE_SSE4_2 ON CACHE BOOL "" FORCE)
  set(USE_AVX ON CACHE BOOL "" FORCE)
  set(USE_AVX2 ON CACHE BOOL "" FORCE)
  set(USE_AVX512 OFF CACHE BOOL "" FORCE)
  set(USE_LZCNT ON CACHE BOOL "" FORCE)
  set(USE_TZCNT ON CACHE BOOL "" FORCE)
  set(USE_F16C ON CACHE BOOL "" FORCE)
  set(USE_FMADD ON CACHE BOOL "" FORCE)
endif()

FetchContent_Declare(
  raylib
  GIT_REPOSITORY https://github.com/raysan5/raylib.git
  GIT_TAG ae50bfa2cc569c0f8d5bc4315d39db64005b1b0)

FetchContent_Declare(
  JoltPhysics
  GIT_REPOSITORY https://github.com/jrouwe/JoltPhysics.git
  GIT_TAG cede24d2733a4a473c6d486650ca9b6d0481681a
  SOURCE_SUBDIR Build)

FetchContent_Declare(
  raygui
  GIT_REPOSITORY https://github.com/raysan5/raygui.git
  GIT_TAG 4.0)

FetchContent_MakeAvailable(raylib JoltPhysics)
FetchContent_GetProperties(raygui)
if(NOT raygui_POPULATED)
  FetchContent_Populate(raygui)
endif()

find_package(nlohmann_json CONFIG QUIET)

if(NOT TARGET nlohmann_json::nlohmann_json)
  find_path(NLOHMANN_JSON_INCLUDE_DIR nlohmann/json.hpp)
  if(NLOHMANN_JSON_INCLUDE_DIR)
    add_library(nlohmann_json::nlohmann_json INTERFACE IMPORTED)
    set_target_properties(
      nlohmann_json::nlohmann_json
      PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${NLOHMANN_JSON_INCLUDE_DIR}")
  else()
    FetchContent_Declare(
      nlohmann_json
      GIT_REPOSITORY https://github.com/nlohmann/json.git
      GIT_TAG v3.11.3)
    FetchContent_MakeAvailable(nlohmann_json)
  endif()
endif()

add_executable(
  raylib_jolt_scene
  src/main.cpp
  src/app/GameApp.cpp
  src/engine/core/FixedStepClock.cpp
  src/game/fps/FpsPlayerController.cpp
  src/game/fps/FpsSandboxRuntime.cpp
  src/PhysicsWorld.cpp
  src/SceneSystem.cpp)

target_include_directories(raylib_jolt_scene PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(raylib_jolt_scene PRIVATE ${JoltPhysics_SOURCE_DIR}/..)
target_include_directories(raylib_jolt_scene PRIVATE ${raygui_SOURCE_DIR}/src)
target_link_libraries(raylib_jolt_scene PRIVATE Jolt raylib nlohmann_json::nlohmann_json)
if(EMSCRIPTEN)
  target_link_options(
    raylib_jolt_scene PRIVATE "-sUSE_GLFW=3" "-sGL_ENABLE_GET_PROC_ADDRESS"
                              "-sASYNCIFY"
                              "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/scene_fps_sandbox.json@scene_fps_sandbox.json"
                              "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/scene_default_showcase.json@scene_default_showcase.json"
                              "SHELL:--preload-file ${CMAKE_SOURCE_DIR}/scene_constraints_motor_example.json@scene_constraints_motor_example.json")
endif()

if(MSVC)
  target_compile_options(raylib_jolt_scene PRIVATE /W4)
else()
  target_compile_options(
    raylib_jolt_scene PRIVATE -Wall -Wextra -Wpedantic
                               -Wno-missing-field-initializers
                               -Wno-deprecated-declarations
                               -Wno-unused-parameter
                               -Wno-unused-but-set-variable)
endif()
