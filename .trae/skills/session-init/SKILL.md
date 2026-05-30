---
name: "session-init"
description: "初始化开发会话，读取 GDD、TDD、DailyProgress 文档以快速同步当前开发进度。当用户说「开始工作」「初始化会话」「继续开发」「同步进度」或需要了解当前项目状态时调用。"
---

# 会话初始化与进度同步

此 skill 用于在开始新会话或切换到 CityFlow 项目时，快速读取核心设计文档和进度记录，帮助开发者了解项目全貌、当前开发阶段以及待完成的工作。

## 适用场景

- 用户说 "开始工作"、"初始化会话"、"继续开发"、"同步进度"
- 用户刚打开项目，需要快速了解当前开发状态
- 用户需要确认 GDD、TDD、DailyProgress 中英文版本是否一致

## 工作流程

### 1. 读取核心文档

按以下顺序并行读取所有 6 份核心文档：

| 文件 | 内容 | 作用 |
|------|------|------|
| `GDD.md` | 游戏设计文档（英文） | 了解游戏核心机制、胜负条件、美术风格 |
| `GDD_Chinese.md` | 游戏设计文档（中文） | GDD 中文对照 |
| `TDD.md` | 技术设计文档（英文） | 了解架构、关键系统、数据流 |
| `TDD_Chinese.md` | 技术设计文档（中文） | TDD 中文对照 |
| `DailyProgress.md` | 每日进度记录（英文） | 了解已完成的功能模块 |
| `DailyProgress_Chinese.md` | 每日进度记录（中文） | 进度中文对照 |

**重要**：以上 6 份文件内容可能较长，使用 `Read` 工具每次读取足够行数（建议 limit=250），或按章节分批读取。

### 2. 检查中英文同步状态

快速对比中英文版本：
- 检查 `GDD.md` 与 `GDD_Chinese.md` 的章节结构是否一致
- 检查 `TDD.md` 与 `TDD_Chinese.md` 的章节结构是否一致
- 检查 `DailyProgress.md` 与 `DailyProgress_Chinese.md` 的日期条目是否对应

如果发现某份文档的章节数量、表格结构或日期条目不一致，在汇报中提示用户是否需要同步。

### 3. 分析当前开发状态

基于读取的内容，分析并整理：

**设计层面（GDD）：**
- 游戏的核心玩法是否已在 GDD 中有清晰定义
- 哪些机制已在实现，哪些仍待设计/实现

**技术层面（TDD）：**
- 当前的架构设计覆盖了哪些模块
- 哪些系统已实现、哪些正在实现、哪些尚未开始

**进度层面（DailyProgress）：**
- 最近的开发日期和完成的功能
- 当前的工作焦点和下一步计划

### 4. 汇报摘要

向用户提供一份结构化的状态摘要，包含：

```
## 项目状态摘要

### 基本信息
- 项目：CityFlow
- 最近活跃日期：[从 DailyProgress 中提取]
- 技术栈：Unreal Engine (C++)

### 设计文档状态
- GDD 中英文：同步 / 不同步（需说明差异）
- TDD 中英文：同步 / 不同步（需说明差异）

### 已完成模块
- [从 DailyProgress 中提取的已完成功能列表]

### 当前进度
- [最近一天的工作内容和当前焦点]

### 建议下一步
- [基于 GDD/TDD 中尚未实现的部分，给出建议]
```

### 5. 提示后续操作

完成摘要后，主动询问用户接下来想要做什么，例如：
- "是否需要对某个不一致的文档进行同步？"
- "是否要继续实现某个未完成的功能？"
- "是否需要更新 GDD/TDD 以反映最新的设计/实现？"

## 重要规则

### 代码组织规范

项目遵循 UE 的 `Public/` 和 `Private/` 分离约定，在此之上按**功能模块**分层组织：

**模块顶层目录（在 `Public/` 和 `Private/` 下各有一份镜像）：**

| 目录 | 用途 | 示例 |
|---|---|---|
| `Grid/` | 网格系统（已存在，文件不动） | `GridManager`, `GridPlaceableActor` |
| `Player/` | 玩家（已存在，文件不动） | `CityFlowPawn`, `CityFlowPlayerController` |
| `Vehicle/` | 车辆系统（未来模块） | — |
| `LSystem/` | L-system 生成（未来模块） | — |
| `Scoring/` | 计分系统（未来模块） | — |

**每个新模块内部按文件类型分子目录：**

```
{Module}/
  ├─ Types/       ← UENUM、USTRUCT、纯数据结构定义
  ├─ Subsystem/   ← UWorldSubsystem / UGameInstanceSubsystem
  ├─ Actor/       ← AActor 及其子类
  └─ Comp/        ← UActorComponent / USceneComponent
```

| 子目录 | 存放内容 | 命名约定 |
|---|---|---|
| `Types/` | 枚举、结构体、纯数据定义 | `{Module}Types.h`、`{Module}Structs.h` |
| `Subsystem/` | WorldSubsystem 等管理类 | `{Module}Manager.h`（如 `VehicleManager`） |
| `Actor/` | 所有 Actor 子类 | `{Module}Actor.h` 或具体名 |
| `Comp/` | ActorComponent / SceneComponent | `{Module}Component.h` |

**规则：**
- 现有 `Grid/` 目录下的文件**不移动**，保持原样
- 任何**新增模块**（如 Vehicle、LSystem、Scoring）必须遵守上述分子目录规范
- `Public/` 和 `Private/` 下的目录结构保持镜像一致
- 测试代码放入 `Test/` 目录，不纳入正式模块

### 禁止自动提交（Commit）

**NEVER** 执行 `git commit` 操作，除非用户明确要求提交。用户偏好手动提交代码，任何自动 commit 都是不被允许的。即使工作完成、文档更新完毕，也只能提供 commit summary 供用户参考，绝不能直接执行 commit。
