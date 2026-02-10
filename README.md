# raylib-jolt-scene

基于 [raylib](https://github.com/raysan5/raylib) + [Jolt Physics](https://github.com/jrouwe/JoltPhysics) 的可交互场景系统示例（参考 [rodneylab/jolt-raylib-hello-world](https://github.com/rodneylab/jolt-raylib-hello-world) 的集成方式）。

## 功能

- 场景内对象统一由 Jolt 刚体驱动（静态地面 + 动态方块/球体）
- 运行时新增对象
  - `1`：新增动态 Box
  - `2`：新增动态 Sphere
- 删除对象
  - 鼠标右键：删除当前悬停对象
  - `Delete` / `Backspace`：删除当前选中对象
- 拖拽对象
  - 鼠标左键按下动态对象开始拖拽，松开结束
- 自由相机（raylib 内置）
  - `W/A/S/D` + 鼠标

## 构建

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

## 运行

```bash
./build/raylib_jolt_scene
```

## 项目结构

- `CMakeLists.txt`：依赖拉取与构建配置（raylib + Jolt）
- `src/PhysicsWorld.h`, `src/PhysicsWorld.cpp`：Jolt 世界封装（初始化、步进、射线拾取、创建/删除刚体）
- `src/SceneSystem.h`, `src/SceneSystem.cpp`：场景对象管理、绘制、拾取与拖拽
- `src/main.cpp`：输入处理与主循环
