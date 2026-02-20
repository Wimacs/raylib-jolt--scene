# FPS 架构设计（raylib + Jolt）

本文档用于指导后续 agent 在本仓库内持续演进，把当前场景编辑 Demo 逐步建设为可复用 FPS 项目。

## 1. 目标

- 将“引擎能力”和“具体游戏玩法”解耦。
- 保持当前 `raylib + jolt` 运行能力与场景 JSON 数据驱动能力。
- 提供清晰目录结构，支持未来复用到其他 3D 游戏（不只 FPS）。
- 采用固定步长物理更新，提升 FPS 玩法一致性与可调试性。

## 2. 架构分层

### 2.1 app 层（应用编排）

职责：窗口生命周期、主循环编排、模块装配。

- `src/app/GameApp.*`

约束：

- 不直接实现具体玩法规则。
- 不包含大型 UI/业务逻辑细节。

### 2.2 engine 层（可复用引擎能力）

职责：与具体游戏无关的公共能力。

- `src/engine/core/FixedStepClock.*`：固定步长时间累积器。
- 后续可扩展：`EventBus`、`Asset`、`ECS`、`DebugDraw` 等。

约束：

- engine 层可以被多个 game 模块复用。
- 尽量避免 engine 依赖某个具体玩法语义（如“武器”、“敌人”）。

### 2.3 game 层（具体游戏模块）

职责：FPS 玩法、输入到行为的映射、游戏状态。

- `src/game/fps/FpsSandboxRuntime.*`
- `src/game/fps/FpsPlayerController.*`

当前阶段包含：

- FPS-only 运行模式（不再提供编辑器自由相机模式）。
- 物理固定步长更新。
- 3D 场景渲染入口。
- FPS 玩家原型：胶囊体、鼠标视角、WASD、冲刺、跳跃、地面检测。
- 对战沙盒辅助：固定出生点、出生点切换、快速重生、拾取点占位标记。

后续拆分方向：

- `PlayerInputSystem`
- `CharacterMotorSystem`
- `WeaponSystem`
- `HitScanSystem`
- `HUDSystem`

### 2.4 tools 层（开发工具，可选）

职责：编辑器与开发期工具，不直接耦合核心 runtime。

- `src/tools/scene_editor/SceneEditorOverlay.*`（保留源码，但默认运行路径不启用）

当前阶段包含：

- raygui 面板
- Scene JSON 路径解析与 Save/Load
- 调试开关（physics debug/help）

## 3. 依赖方向（必须遵守）

允许：

- `app -> engine`
- `app -> game`
- `app -> tools`
- `game -> engine`
- `tools -> game`
- `tools -> engine`

禁止：

- `engine -> game`
- `engine -> tools`
- `game <-> tools` 循环依赖（单向使用即可）

说明：

- 当前主运行路径仅 `app + game + engine`，`tools` 作为可选离线开发工具保留。

## 4. 固定步长策略

- 目标步长：`1 / 120s`。
- 每帧先累积 `frame_dt`，再执行 `N` 次固定步长更新。
- 每帧最大物理子步数有限制（防止极端卡顿导致“螺旋死亡”）。

收益：

- FPS 角色控制和碰撞反馈更稳定。
- 回放、调参与问题复现更容易。

## 5. 当前代码迁移映射

来自旧 `main.cpp`：

- 窗口与主循环 -> `GameApp`
- FPS 交互输入、相机、出生/重生、帮助覆盖层 -> `FpsSandboxRuntime`
- FPS 玩家控制 -> `FpsPlayerController`

已保留：

- `PhysicsWorld`（Jolt 封装）
- `SceneSystem`（场景对象与约束）

后续建议（第二阶段）：

- 将 `SceneSystem` 内的 JSON 逻辑拆到 `SceneSerializer`。
- 将 `SceneSystem` 内的绘制逻辑拆到 `SceneRenderer`。
- 将拖拽/拾取拆为 `InteractionSystem`。

## 6. 后续 Agent 任务建议（优先级）

1. 引入 `IPhysicsWorld` 抽象，隔离 Jolt 实现。
2. 增加 `Player` 组件与 `CharacterController`（胶囊体 + 地面检测）。
3. 加入 `Weapon` 数据驱动（JSON 配置射速、伤害、后坐力）。
4. 拆分 runtime 为 system 管线，减少单类膨胀。
5. 建立 `data/levels` 与 `data/prefabs` 目录，并提供版本化 schema。

## 7. 代码风格约束

- 新功能尽量在对应层内新增文件，不把逻辑重新塞回 `main.cpp`。
- 公共能力先考虑放 `engine/`，玩法逻辑放 `game/`。
- 所有新增模块优先保持“高内聚、低耦合、可替换”。
- 当类长度明显增长时，优先拆分到子模块而非继续追加。

## 8. 验收标准（本阶段）

- 工程可编译运行。
- `main.cpp` 仅保留最小入口。
- 现有交互功能（拖拽、删除、场景存取、快捷生成、UI）保持可用。
- 新结构对后续 FPS 玩法扩展具备清晰入口。
