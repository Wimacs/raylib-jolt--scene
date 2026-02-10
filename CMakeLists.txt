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
set(USE_SSE4_1 ON CACHE BOOL "" FORCE)
set(USE_SSE4_2 ON CACHE BOOL "" FORCE)
set(USE_AVX ON CACHE BOOL "" FORCE)
set(USE_AVX2 ON CACHE BOOL "" FORCE)
set(USE_AVX512 OFF CACHE BOOL "" FORCE)
set(USE_LZCNT ON CACHE BOOL "" FORCE)
set(USE_TZCNT ON CACHE BOOL "" FORCE)
set(USE_F16C ON CACHE BOOL "" FORCE)
set(USE_FMADD ON CACHE BOOL "" FORCE)

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

add_executable(
  raylib_jolt_scene
  src/main.cpp
  src/PhysicsWorld.cpp
  src/SceneSystem.cpp
  src/RayGuiImpl.cpp)

target_include_directories(raylib_jolt_scene PRIVATE ${JoltPhysics_SOURCE_DIR}/..)
target_include_directories(raylib_jolt_scene PRIVATE ${raygui_SOURCE_DIR}/src)
target_link_libraries(raylib_jolt_scene PRIVATE Jolt raylib)

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
