# CityFlow 简要技术设计文档

---

## 1. 架构概览

CityFlow 采用**阶段分离、组件化**的架构，将游戏流程拆分为**规划阶段**和**模拟阶段**，两者严格隔离，以降低实时耦合的复杂度。

### 核心管理类

核心管理类集中在 `GameMode` 或委派的 Manager 组件中：

| 管理器 | 类型 | 职责 |
|---|---|---|
| **GridManager** | `UWorldSubsystem` | 维护二维逻辑网格。提供网格吸附、放置验证、邻居查询、连接掩码计算和建筑接口注册。**管理共享道路预算** —— 玩家放置和 L-system 生成都从同一个 `RoadBudget` 池中消费。 |
| **LSystemManager** | `UWorldSubsystem` | **可选的**混合毛细道路生成器。它先预留门口到主道路连通分量的连接路径，再仅用未预留的分支预算执行去重、吸引力优先的有机生长。连通性按同一个共享道路分量验证，并保留现有蓝图控制接口。 |
| **VehicleManager** | `UWorldSubsystem` + `FTickableGameObject` | 生成并管理所有车辆 Actor。提供道路图上的 **A\* 寻路**，将网格路径转换为世界空间样条运动计划，处理**拥堵检测**（每格车辆数统计）。每帧 Tick，执行交叉口方向占用锁的定期清理。 |
| **ScoringManager** | `UWorldSubsystem` | 在模拟阶段追踪到达数、到达分数、死亡罚分和拥堵惩罚。向 HUD 广播带世界锚点的分数变化 popup 请求，使用定时器周期性扣除拥堵惩罚，并在结算时计算包含全连通奖励的最终分数。 |
| **CityFlowGameMode** | `AGameModeBase` | 拥有**状态机**（`ECityFlowGamePhase`：Planning → Simulating → Evaluation）。初始化网格、生成默认建筑、管理共享预算分配、触发阶段切换。为 UI 控制提供蓝图可调用 API。 |

各管理器通过**事件总线**（如 `OnRoadPlaced`、`OnVehicleArrived`）进行通信，避免硬引用。

### 游戏状态机

| 状态 | 描述 |
|---|---|
| **规划 (Planning)** | 玩家铺设主干道路；可手动触发 L-system 生长；在确认之前可反复迭代。 |
| **模拟 (Simulating)** | 车辆以固定频率从起点生成并驶向目的地；道路网络只读；持续检测拥堵。 |
| **结算 (Evaluation)** | 模拟时间结束或所有车辆已消失；显示最终得分和统计数据。 |

---

## 2. 关键系统

### 2.1 网格系统与自动道路放置

#### 逻辑网格

`GridManager` 是一个 `UWorldSubsystem`，持有 `TArray<TArray<FGridCell>>` 二维数组。`FGridCell` 结构体定义如下：

```cpp
USTRUCT(BlueprintType)
struct FGridCell
{
    ECellType Type;       // Empty, Road, Building
    uint8 ConnectedMask;  // bit0: 上, bit1: 下, bit2: 左, bit3: 右
    int32 BuildingID;     // 若为建筑单元格，引用所属建筑
    TObjectPtr<AActor> RoadActor; // 若为道路单元格，指向对应的道路 Actor
};
```

`EGridDirection` 位掩码枚举定义了四个方向（Up、Down、Left、Right）。工具结构体 `FGridVector` 提供轻量级二维整数坐标及算术运算符，`GridDirectionUtils` 提供方向到向量的映射。

所有世界坐标通过 `WorldToGrid(Location)` 映射到网格索引，确保放置时对齐到单元格中心。

#### 玩家放置流程（带预览）

1. `BeginPlay` 时，Controller 生成一个**预览 Actor** 并进入预览状态。
2. 每帧 `LineTrace` 检测地面跟踪光标位置；预览 Actor 吸附到网格，实时跟随鼠标。`CanPlaceAt()` 检查目标格是否合法；`SetPreviewPlacementValid()` 更新预览材质 —— 合法格显示 `PreviewMaterial`，被占用格显示 `InvalidPreviewMaterial`（均在蓝图中可配置）。
3. **放置** — 绑定到 `IA_PlaceItem`（鼠标左键）：
   - **Started：** 重置 `LastPlacedGridPos`，执行首次放置。
   - **Triggered**（按住期间每帧触发）：启用**拖拽连续放置**。每帧调用 `TryPlaceAtCursor()` 辅助函数；若网格坐标与 `LastPlacedGridPos` 相同则跳过（去重），否则在新单元格尝试放置。
   - **Completed**（松开时）：重置 `LastPlacedGridPos`。
   - 放置逻辑：
     - 将命中点转换为网格坐标 `(x, y)`。
     - **验证：** 目标单元格必须为 `Empty`。
     - 成功后：预览 Actor 转为 `EnterPlacedState()`，恢复原始材质并启用碰撞。单元格设为对应类型并计算 `ConnectedMask`。立即生成新的预览 Actor。在 `OnPlacedOnGrid()` 之后，Actor 播放**放置生长动画** — 从可配置的初始大小缩放到完整尺寸，使用 ease-out 缓动曲线。
4. **删除** — 绑定到 `IA_RemoveItem`（鼠标右键）：
   - **Started：** 重置 `LastRemovedGridPos`，执行首次删除。
   - **Triggered：** 启用**拖拽连续删除**，同样通过 `LastRemovedGridPos` 去重。
   - **Completed：** 重置 `LastRemovedGridPos`。
   - 删除逻辑（`TryRemoveAtCursor`）：射线检测 → `WorldToGrid()` → 从网格中查找 `Cell.RoadActor`（而非碰撞命中）。若该格包含已放置的 `AGridPlaceableActor`，调用 `RemoveFromGrid()` + `Destroy()`。
5. **邻居刷新：** 遍历四个邻居；若任一为道路，重新计算其掩码。

#### 世界退出阶段安全性

- `UGridManager::Deinitialize()` 在清空存储前先将网格标记为未初始化，再重置尺寸和道路预算，避免退出阶段的延迟查询读取过期元数据。
- `IsValidGridPos()` 按实际嵌套数组边界校验，`GetCellsOfType()` 只遍历仍然存在的数组，因此网格销毁后会安全失败并返回空结果。
- `UScoringManager::Deinitialize()` 会在调用 `StopScoring()` 前清除计分激活标记。退出时仍会解绑委托和清理定时器，但不会在 Grid 子系统销毁后计算最终分数。
- 正常游戏从模拟切换到 Evaluation 时，`StopScoring()` 仍在计分激活状态下执行，并生成完整结算报告。

### 2.2 网格可放置 Actor 层级

所有可放置在网格上的物品均继承自抽象基类 `AGridPlaceableActor`。

#### 类层级

```
AGridPlaceableActor  (Abstract)          ← 状态管理 + 统一 API
  └─ AMeshGridPlaceableActor (Abstract)  ← StaticMesh + 预览材质切换
       ├─ ATestGridPlaceableActor         ← 测试方块
       ├─ ARoadTile                       ← 基于连接掩码自动变形的道路
       └─ ABuilding                       ← 多单元格建筑，可配置出入口
```

#### AGridPlaceableActor（状态管理）

纯状态管理，不包含任何视觉逻辑。提供一个 `USceneComponent` 作为根组件来锚定所有子组件，支持视觉元素的相对位置管理（StaticMesh、景观附加子对象等）。

**类型分类：** `PlaceableType`（`EPlaceableType` 枚举：`Road`、`Building`、`Landscape`）标识此可放置物的类别。

**网格旋转：** `EGridRotation` 枚举（`Rot0`、`Rot90`、`Rot180`、`Rot270`）— 支持旋转的子类可使用。

| 功能 | API |
|---|---|
| 状态标志 | `IsPreview()` / `IsPlacedOnGrid()` / `IsPreviewPlacementValid()` |
| 进入预览 | `EnterPreviewState()` → 触发 `OnEnterPreview()`（BlueprintNativeEvent） |
| 进入放置 | `EnterPlacedState()` → 触发 `OnEnterPlaced()` |
| 预览有效性 | `SetPreviewPlacementValid(bool)` → 触发 `OnPreviewValidChanged(bool)`（BlueprintNativeEvent）。跟踪 `bPreviewPlacementValid` 标志。 |
| 网格操作 | `PlaceOnGrid()` / `RemoveFromGrid()` / `CanPlaceAt()` / `SnapToGridPosition()` |
| 放置回调 | `OnPlacedOnGrid()` / `OnRemovedFromGrid()`（BlueprintNativeEvent） |
| 网格反向查找 | `RegisterCells()` 将 `this` 作为 `RoadActor` 传入 `OccupyCell()`，实现从网格单元反向查找 Actor，用于右键删除 |
| 多格支持 | `BuildingSize`（`FVector2D`，蓝图可配置）定义占地区域；`GetBuildingSize()` 返回占地区域尺寸；`CalculateOccupiedCells()` 计算覆盖的网格单元格列表；`ValidatePlacement()` 供子类添加自定义放置规则 |
| 类型分类 | `GetPlaceableType()` 返回 `EPlaceableType`（Road / Building / Landscape） |
| 根组件 | `RootSceneComponent`（`USceneComponent`）作为根组件，管理子组件的相对位置 |

#### AMeshGridPlaceableActor（视觉层）

添加 `UStaticMeshComponent` 并实现自动材质切换。`MeshComponent` 挂载到 `RootSceneComponent`（由 `AGridPlaceableActor` 提供）下，允许额外的子组件独立放置。

| 功能 | 详情 |
|---|---|
| `MeshComponent` | `UStaticMeshComponent` 挂载到 `RootSceneComponent` |
| `PreviewMaterial` | 可在蓝图中配置；合法预览位置使用的半透明材质 |
| `InvalidPreviewMaterial` | 可在蓝图中配置；预览 Actor 悬浮在已占用格子上时显示的材质（如红色） |
| `OnEnterPreview` 覆写 | 保存所有原始材质 → 所有材质槽替换为 `PreviewMaterial` → 关闭碰撞 |
| `OnEnterPlaced` 覆写 | 按材质槽索引逐个恢复原始材质 → 开启碰撞 |
| `OnPreviewValidChanged` 覆写 | 处于预览状态时，合法则切换到 `PreviewMaterial`，不合法则切换到 `InvalidPreviewMaterial` |

#### 预览外观扩展 (AGridPlaceableActor)

在 `AGridPlaceableActor` 基类上定义了 `virtual void UpdatePreviewAppearance(const FGridVector& GridPos)` 方法（默认空实现）。Controller 每帧在 `SetPreviewPlacementValid()` 之后调用此方法，允许子类根据预测的网格位置更新预览视觉。

#### 放置生长动画（v0.9）

当 `GridPlaceableActor` 通过 `PlaceOnGrid()` 放置到网格上时，会播放**缩放入场动画**作为视觉反馈。动画通过 `FTimerHandle` 驱动（无每帧 Tick 开销），使用 ease-out 三次方缓动曲线。

**插入点：** 在 `PlaceOnGrid()` 末尾、`OnPlacedOnGrid()` 和 `OnGridPlaced` 广播之后 — 这确保子类（如 `ARoadTile::UpdateAppearance()`）已在动画捕获目标缩放之前应用了最终 `SetActorScale3D()`。

**流程：**
1. `PlaySpawnAnimation()` 捕获当前 `GetActorScale3D()` 作为 `TargetScale`。
2. 设置初始缩放为 `TargetScale × SpawnAnimationInitialScale`。
3. 启动 `FTimerHandle`，频率 ~60 Hz（0.016 秒间隔）。
4. `TickSpawnAnimation()` 递增已用时间，计算 `T = elapsed / Duration`，应用 `Alpha = 1 - (1-T)^3`（ease-out 三次方），设置 `ActorScale = TargetScale × Alpha`。
5. 当 `T ≥ 1.0` 时，精确设置到 `TargetScale` 并清除 Timer。

**蓝图可配置属性（均在 `AGridPlaceableActor` 上）：**

| 属性 | 默认值 | 描述 |
|---|---|---|
| `bPlaySpawnAnimation` | `true` | 总开关；设为 `false` 可禁用（如 L-system 批量放置） |
| `SpawnAnimationDuration` | `0.2` | 动画时长（秒） |
| `SpawnAnimationInitialScale` | `0.05` | 初始缩放比例（0.0–1.0，被钳位到 ≥ 0.01 以避免零缩放问题） |

**安全性：** `EndPlay()` 清除 `SpawnAnimTimer`，防止 Actor 销毁后回调悬空。

---

### 2.3 道路地块自动变形与样条管理

`ARoadTile` 根据 `ConnectedMask` 自动切换其视觉效果和行驶路径。

#### 模型切换

`ARoadTile` 继承自 `AMeshGridPlaceableActor`，通过 `FRoadMeshConfig` 结构体数组实现灵活的 Mask→模型映射。每种路口类型只需配置一个**标准朝向（CanonicalMask）**的模型，其余方向由 90° 顺时针旋转（Yaw）自动推导。

**EGridDirection 位掩码定义：** Up=1, Down=2, Left=4, Right=8

**CanonicalMask 与标准朝向约定：**

| 类型 | CanonicalMask | 标准朝向（开口方向） | 连接数 | 说明 |
|------|---------------|---------------------|--------|------|
| 死胡同 (DeadEnd) | **8** (Right) | 开口朝 +X | 1 | |
| 直路 (Straight) | **12** (Left+Right) | 水平，开口 -X / +X | 2 (对向) | 道路沿 X 轴；90° 旋转后变垂直 |
| 转角 (Corner) | **10** (Down+Right) | 开口 +X / -Y | 2 (垂直) | |
| T 型路口 (TJunction) | **14** (Down+Left+Right) | 缺失 Up | 3 | 开口 -X / +X / -Y |
| 十字路口 (Cross) | **15** (全方向) | — | 4 | |

**自动旋转查找：** `FindMeshConfig()` 遍历 `RoadMeshConfigs`，每次将 `CanonicalMask` 顺时针旋转 90°（最多 4 次），匹配实际 `ConnectedMask`。匹配后返回旋转角度 `Rot × 90°`。

**缩放策略：**
- `ReferenceCellSize`：模型设计参考尺寸，运行时 `BaseScale = CellSize / ReferenceCellSize`
- 每配置项含 `ScaleMultiplier`（FVector），逐轴独立缩放
- 最终 `ActorScale = ScaleMultiplier × BaseScale`
- 旋转 90° 或 270° 时自动 Swap(ScaleMultiplier.X, ScaleMultiplier.Y)，保证直路竖过来缩放正确

**邻居刷新：** `GridManager::OccupyCell` / `ClearCell` 调用 `UpdateNeighborMasks()` 重新计算四个邻居的 `ConnectedMask`，触发 `OnCellChanged` 广播。`ARoadTile` 监听该委托，自动调用 `UpdateAppearance()` 切换 Mesh / Rotation / Scale。

模型在 Blueprint 中通过 `RoadMeshConfigs` 数组配置，运行时通过 `SetStaticMesh` + `SetActorRotation` + `SetActorScale3D` 进行切换。

#### 预览外观

`ARoadTile` 覆写 `UpdatePreviewAppearance()`，在放置前预测未来 `ConnectedMask`。预览状态下每帧：

1. 调用 `GridManager::CalculateConnectedMask(GridPos)` 计算放置后该位置会得到的掩码。
2. 运行 `FindMeshConfig()` 查找匹配的 Mesh/Rotation/Scale。
3. 将相应 mesh、旋转、缩放应用到预览 Actor。
4. 根据放置有效性，用 `PreviewMaterial` 或 `InvalidPreviewMaterial` 覆盖所有材质槽。

玩家可以在点击前看到目标单元格将出现哪种道路 mesh 配置。

#### Mesh 材质缓存

`ARoadTile` 维护一个 `TMap<UStaticMesh*, TArray<UMaterialInterface*>> MeshMaterialCache`，用于在放置后可靠地恢复原始 mesh 材质。缓存通过 `EnsureMeshMaterialsCached()` 延迟填充，从 `UStaticMesh::GetStaticMaterials()` 读取默认材质。

- **预览时：** `UpdatePreviewAppearance` 在 `SetStaticMesh()` 之前调用 `EnsureMeshMaterialsCached()`，然后用预览/非法材质覆盖所有材质槽。
- **放置时：** `OnEnterPlaced()` 覆写父类实现，在父类的 `RestoreOriginalMaterials()` 之后从缓存恢复材质。`UpdateAppearance()` 在因邻居更新导致 mesh 变更时也会调用 `EnsureMeshMaterialsCached()` + `RestoreMeshMaterials()`。
- **`OnPreviewValidChanged`** 被覆写为空体，防止父类的材质切换逻辑干扰由 `UpdatePreviewAppearance` 管理的逐网格预览外观。

此方案避免了 `OnPreviewValidChanged`、`OnEnterPreview` 和 `UpdatePreviewAppearance` 之间材质随机翻转的竞态问题。

#### 内部样条管理（混合策略）— ⚠️ 需要重构

> **当前状态：** `ARoadTile::GetSplinePath()` 存在但**未被 VehicleManager 使用**。样条路径生成经过多次迭代调试后仍有已知问题——当前简化的路径构建方式见 2.6。本节记录预期的设计目标。

`ARoadTile::GetSplinePath(EntryDir, ExitDir)` 计算从入口边中点到出口边中点的平滑路径。路块上不保存样条组件——全部实时计算，无持久状态。

**路径计算（预期设计）：**
- **直行通过（对向）：** 返回 2 个世界空间点：`[入口边中点, 出口边中点]`。车辆自身的样条在两点间线性插值。
- **转弯（垂直或任意非对向）：** 在本地空间计算二次贝塞尔曲线：
  - P0 = 入口边中点
  - P1 = P0 + P2（两边外角，**非格子中心**）
  - P2 = 出口边中点
  - 采样 13 个世界空间点，生成平滑弧线。

**当前方案（v0.3，已重构）：**
- `BuildSplinePath()` 基于 A\* 原始路径（全部格子保留不合并）生成样条点和对应的切线方向。
- 返回等长的 `TArray<FVector>` 切线方向数组，由 `SetSplinePath` 用于设置精确的样条切线。
- 直道格子中心也包含在输出中 —— 作为连续弯道序列之间的分隔符。
- **转弯点偏移：** 每个转弯点替换为两个偏移点：
  - EntryOffset = `center - EntryDir * CellSize/2`（向来的方向回退半格），切线 = `EntryDir`
  - ExitOffset = `center + ExitDir * CellSize/2`（向下一格方向偏移半格），切线 = `ExitDir`
- **连续弯道处理：** 背靠背的弯道序列中，仅首个弯道添加完整的 entry+exit 点对；后续弯道仅添加 exit 偏移点。防止偏移点在格子边界处重复。
- 直道格输出 `cell_center`，切线 = 路径前进方向。
- `SetSplinePath(Points, TangentDirs)` 直接使用切线方向，将样条点切线设为 `TangentDir * CellSize/2`。
- `ARoadTile::GetSplinePath()` 从未被使用，保留供未来参考。

---

### 2.4 多单元格建筑与接口

`ABuilding` 继承自 `AMeshGridPlaceableActor`，提供多单元格建筑放置、可配置出入口以及在网格上旋转的功能。

#### ABuilding 概览

| 功能 | 详情 |
|---|---|
| `BuildingSize` (`FVector2D`) | 建筑网格占位尺寸（如 `(2,2)`、`(3,2)`），可在蓝图中配置 |
| `BuildingRotation` (`EGridRotation`) | 网格上的旋转角度（`Rot0`/`Rot90`/`Rot180`/`Rot270`）；影响实际占地区域、Mesh 旋转和出入口位置 |
| `MeshComponent` | 来自 `AMeshGridPlaceableActor` 的建筑 Mesh，可在蓝图中配置 |
| `ReferenceMeshSize` | Mesh 参考世界尺寸（默认 `100`），与 `CellSize` 一起用于计算缩放 |
| `bIsDestination` | 该建筑是目的地（商业）还是起点（住宅） |
| `Doorways` (`TArray<FBuildingDoorway>`) | 手动配置的出入口列表；每个出入口位置在**建筑本地网格坐标**中定义 |
| `AutoGenerateDoorways()` | 在每条边中点外侧一格自动生成出入口 |

#### 建筑放置与视觉

- **网格表示：** 建筑矩形覆盖的每个单元格设置为 `ECellType::Building`，`BuildingID` 引用 Actor 的 `GetUniqueID()`。所有单元格通过 `RoadActor` 指向同一个 `ABuilding`。
- **旋转：** `BuildingRotation` 控制建筑在网格上的朝向。`Rot0`/`Rot180` 保持原始 `W×H` 占地区域；`Rot90`/`Rot270` 则交换为 `H×W`（如 `2×3` 的建筑旋转 90° 后占 `3×2` 格）。`CalculateOccupiedCells()` 通过 `GetEffectiveBuildingSize()` 覆写以计算有效的占地区域。
- **视觉居中与旋转：** 放置后（`OnPlacedOnGrid`），Actor 从左上角网格锚点重定位到覆盖区域的几何中心，将 Mesh 缩放至填充有效占地区域，并应用 `SetActorRotation(Yaw = 旋转角度 × 90°)`。
- **预览：** `UpdatePreviewAppearance()` 实时应用相同的居中、缩放和旋转逻辑，让玩家在放置前看到建筑占地区域和朝向。
- **放置时邻居刷新：** `OnPlacedOnGrid_Implementation` 遍历建筑占地区域的所有邻居单元格，对每个 Road 单元格广播 `OnCellChanged`，触发道路地块重新计算 `ConnectedMask`。

#### FBuildingDoorway 结构体

```cpp
USTRUCT(BlueprintType)
struct FBuildingDoorway
{
    FGridVector RelativePosition;   // 相对于建筑左上角网格锚点的坐标（建筑本地坐标）
    EGridDirection FacingDirection; // 出入口朝向（同样为建筑本地坐标，跟随旋转）
};
```

- 出入口在**建筑本地**网格相对坐标中定义（如 `2×3` 建筑中的 `(1, 2)` 表示建筑占地第 1 列、第 2 行的格子）。
- 建筑旋转时，出入口位置**跟随**建筑一起变换，通过 `TransformLocalPosition()` 实现：
  - `Rot0`：`(lx, ly)` → `(lx, ly)`
  - `Rot90`：`(lx, ly)` → `(ly, W-1-lx)`
  - `Rot180`：`(lx, ly)` → `(W-1-lx, H-1-ly)`
  - `Rot270`：`(lx, ly)` → `(H-1-ly, lx)`
- `GetDoorwayWorldPositions()` 将每个出入口的本地位置变换为绝对网格坐标。
- `HasDoorwayAt(WorldPos)` 检查给定的绝对网格位置是否匹配任一出入口。
- 出入口完全可在蓝图中配置 — 可按具体格子位置添加、删除或重新定位。

#### 出入口-道路连接检测

`CalculateConnectedMask` 计算道路单元格的掩码时，现在同时检查 `Road` 和 `Building` 邻居类型：

- 若邻居为 `Road` → 设置对应方向位（与之前一致）。
- 若邻居为 `Building` → 将 `RoadActor` 强制转换为 `ABuilding` 并调用 `HasDoorwayAt(道路位置)`。若建筑在此道路单元格位置设有出入口（即道路正好放置在建筑的出入口点上），则设置对应方向位。

这使得道路地块能够将建筑出入口视为"连接的邻居"并相应更新其 Mesh 外观。预览模式同样受益：`ARoadTile::UpdatePreviewAppearance()` 调用 `CalculateConnectedMask`，因此预测的掩码也会包含出入口连接。

#### GridManager 随机放置（调试）

---

### 2.4b 建筑地基与人行道 (ProceduralMesh)

`UFoundationComponent` 是挂载到 `ABuilding` 的 `UActorComponent`，使用两个 `UProceduralMeshComponent` 实例程序化生成三维地基平台和环绕人行道。

#### 架构

```
ABuilding
  └─ UFoundationComponent (CreateDefaultSubobject)
       ├─ UProceduralMeshComponent (FoundationMesh)   ← 主体 + 侧墙
       └─ UProceduralMeshComponent (SidewalkMesh)     ← 边框人行道
```

**核心设计原则：**

- **Actor 缩放抵消：** 建筑 Actor 使用 `SetActorScale3D` 来视觉缩放。程序化顶点以世界单位计算，然后通过 `SetRelativeScale3D(1/S.X, 1/S.Y, 1)` 抵消父级缩放，防止双重变换。**v0.11 修复：** `BuildFoundation` 现在接收显式 `InOwnerScale` 参数（由 `RefreshFoundation` 计算 `TargetScale = EffSize × CellSize / ReferenceMeshSize` 并传入），而非在构建时读取 `Owner->GetActorScale3D()`，避免在 spawn 动画期间重建地基时读取到动画中间缩放值。
- **Z 轴惯例：** 地基落在地面（Z=0），向上挤出至 `FoundationHeight`。人行道落在地基顶部（`FoundationHeight` → `FoundationHeight + SidewalkHeight`）。底面也生成在 Z=0 以保证完整性。
- **统一缠绕顺序：** UE 使用左手坐标系，**顺时针 (CW)** 缠绕为正面。轮廓为逆时针 (CCW) 生成；所有顶面/墙面三角形的缠绕均转换为 CW 以朝外。

#### 蓝图可配置属性

| 属性 | 默认值 | 描述 |
|---|---|---|
| `FoundationHeight` | 50 | 地基挤出高度（Z=0 到 Z=50） |
| `Padding` | 50 | 从建筑边缘到地基边缘的内缩边距 |
| `CornerRadius` | 40 | 圆角半径（根据相邻边的 padding 自适应） |
| `SidewalkWidth` | 20 | 人行道环宽（从地基边缘向外） |
| `SidewalkHeight` | 10 | 人行道在地基顶部之上的挤出高度 |
| `FoundationMaterial` | — | 地基主体的材质 |
| `SidewalkMaterial` | — | 人行道环的材质 |
| `FoundationCollisionProfileName` | `None` | 地基 ProceduralMesh 的碰撞预设。通过 `GetCollisionProfileOptions()` 以下拉菜单形式暴露，列出所有引擎和项目碰撞预设。 |
| `SidewalkCollisionProfileName` | `None` | 人行道 ProceduralMesh 的碰撞预设。与地基具有相同的下拉菜单行为。 |

#### 按边连接处理

`BuildFoundation` 接收 4 个标志（`bTopConnected`/`bRightConnected`/`bBottomConnected`/`bLeftConnected`）。已连接边使用 `Padding = 0`（与建筑平齐），未连接边使用配置的 `Padding` 值。圆角半径动态适配——若相邻任一边的 `Padding = 0`，圆角半径被限制为 `min(CornerRadius, maxPad)`。

#### 网格生成细节

| 组件 | 几何体 | 缠绕顺序 |
|---|---|---|
| **顶面** | 从 Outline 顶点在 Z=`FoundationHeight` 进行 N 边形扇形三角剖分 | `(0, i+2, i+1)` CW |
| **底面** | 同样的 N 边形在 Z=0，法线=`(0,0,-1)` | `(0, i+1, i+2)`（从下方看 CCW→CW） |
| **墙面四边形** | 每段：`(A_Top, B_Top, B_Bot, A_Bot)`，法线=`(Edge.Y, -Edge.X, 0)`（朝外） | `(0,1,2), (0,2,3)` CW |
| **人行道外/内墙** | 环绕环形的 4 个外墙 + 4 个内墙四边形 | CW |
| **人行道顶面** | 4 个梯形组成环形顶面，Z=`FoundationHeight + SidewalkHeight` | `(Base, Base+2, Base+1)` CW |
| **人行道底面** | 4 个梯形在 Z=`FoundationHeight`，法线=`(0,0,-1)` | `(Base, Base+1, Base+2)`（翻转以朝下） |

#### 连接状态刷新

`ABuilding::OnDoorwayCellChanged` 监听 `GridManager::OnCellChanged`。当出入口连接点的类型变更为 `Road` 或从 `Road` 变走时，`DetermineEdgeConnections()` 重新评估 4 条边，然后 `RefreshFoundation()` 重建网格。

`RefreshFoundation()` 会把建筑预期的目标缩放显式传入 `BuildFoundation()`。`FoundationMesh` 和 `SidewalkMesh` 都使用这个显式缩放抵消父 Actor 缩放，而不是在重建时读取 `Owner->GetActorScale3D()`。这样即使刷新发生在放置缩放动画期间，或 Actor 处于临时缩放状态，人行道尺寸也会始终绑定到建筑 footprint。

---

### 2.5 L-System 分支生成

`ULSystemManager` 是一个在规划阶段运行的 `UWorldSubsystem`。它现在采用**连通性保底的混合生成器**：预留的最短连接路径在预算足够时保证建筑接入同一个道路连通分量，带吸引力偏向的有机生长只能使用未被预留的剩余预算。

#### 架构

`ULSystemManager` 通过 `GetWorld()->GetSubsystem<ULSystemManager>()` 访问。现有蓝图 Setter 与委托接口保持兼容。内部维护去重后的有机生长前沿、连接格计划、单次生成预算账本和带种子的 `FRandomStream`。

#### 统一连通分量规则

连通不再只是“每栋建筑门口存在一格道路”。`AreAllBuildingsConnected()` 会遍历道路连通分量，只有当每栋建筑至少有一个出入口位于**同一个**道路分量中时才返回成功。`CityFlowGameMode::AreAllBuildingsConnected()` 委托给这一权威检查，使自动流程和其他系统使用相同定义。

`GetPrimaryRoadComponent()` 优先选择服务建筑数量最多的道路分量，并以分量规模作为平局判定。未连接建筑和有机吸引力都以该主分量为目标。

#### 预留连接计划

`BuildConnectionPlan()` 在动画生长前执行：

1. 收集已有道路格并选择主道路分量。
2. 若当前没有道路，则在一个有效建筑出入口处建立种子格。
3. 对主分量之外的每栋建筑，从其所有有效出入口向当前网络执行多源网格搜索。
4. 选择需要新增道路格最少的路径，按“网络到建筑”的顺序加入计划，合并到模拟连通分量后继续处理下一栋建筑。
5. 有机生长开始消费预算前，先预留计划中所有空格的精确成本。

规划器将 Building 格视为障碍，并可零放置成本复用已有 Road 格。`ProcessConnectionPlanStep()` 每次放置一个预留路径格，保留可见生长动画；若有机生长提前覆盖计划格，实时预留成本会自动减少。

#### 起点提取

从两个来源收集生长起点：

**A. 死胡同道路格：**
- 每个死胡同只沿远离已连接邻居的方向继续；侧向分支随后由概率规则产生，不再立即生成三条手臂。

**B. 直路段（间隔采样）：**
- 识别直路路段（`ConnectedMask` 含 2 个对向位：Up+Down 或 Left+Right）。
- 仅穿过同一轴向上的直路格；转角、死胡同、T 型和十字路口不会被错误纳入路段或 visited 集合。
- 若段长 < 3 格，跳过（太短不适合分支）。
- 沿段每 `MinBranchSpacing + 1` 格在垂直方向采样分支点。
- 转角、T 型路口、十字路口**跳过**（已充分连接）。

**C. 未连接建筑出入口：**
- 每栋建筑只选择距离主道路分量最近的有效出入口，避免所有门同时产生浪费预算的分支。
- 通过“旋转后的连接点 - 旋转后的建筑边缘格”推导方向，使 `Rot90/Rot180/Rot270` 建筑始终向外生长。

#### 优先级前沿生长

系统使用迭代前沿而非递归改写驱动生长：

1. `FLSystemGrowthPoint` 结构体持有网格位置和生长方向。
2. `ProcessGrowthStep()` 通过 `FTimerHandle` 以 `GrowthInterval`（默认 0.1s）间隔调用。
3. 前沿按 `(Position, Direction)` 去重，并在执行前按吸引分数进行全局排序。
4. 某个候选被阻挡时只丢弃该候选；只要还有其他候选或预留连接格，生成就不会提前结束。
5. 仅当 `GenerationBudgetRemaining > PendingConnectionCost` 时允许有机放置，装饰性分支不能侵占连通性预算。

#### 多格直道延伸

`TryGrowAt()` 执行时不是只放一格。它会尝试在点方向上连续放置最多 `StraightExtendLength`（默认 3）格：

- 每格通过 `World::SpawnActor<ARoadTile>` + `PlaceOnGrid()` 放置。
- 若路径阻断、预算耗尽或放置失败则提前停止。
- 仅**最后**成功放置的格生成后续生长点。

#### 侧向分支验证

侧向分支（左/右转）添加前，`IsSideBranchValid()` 检查分支目标格两侧的相邻格（相对于分支方向）。若**任一**侧已是 Road，分支被**拒绝**——防止在平行道路之间填空。

#### 前进延续与概率分叉

对每个新放置格的有效邻居（排除来路方向）：

| 方向 | 行为 |
|---|---|
| 前进（同方向） | **总是**添加为延续点 |
| 左 / 右 | 以 `BranchProbability`（默认 0.6）概率添加 —— 仅在 `IsSideBranchValid()` 通过时 |

#### 吸引偏向排序

所有待处理生长点按吸引分数进行全局优先级排序：

```
Score = Lerp(DistScore, AlignScore, AttractionStrength)
  DistScore  = 1 / (1 + 到最近未连接出入口的欧几里得距离)
  AlignScore = dot(归一化出入口方向, 生长方向)，截断 ≥ 0
```

目标使用最近的 Doorway，而不是建筑左上角网格坐标。侧向分支概率由单次生成专属的 `FRandomStream` 提供，避免受无关全局随机调用影响。

#### 可配置参数（蓝图 Setter）

| Setter | 默认值 | 描述 |
|---|---|---|
| `SetRoadTileClass(TSubclassOf<ARoadTile>)` | — | 分支生成使用的道路地块类 |
| `SetBranchBudget(int32)` | 50 | 单次生成的硬上限；不再被 GridManager 的全部剩余预算覆盖 |
| `SetGrowthInterval(float)` | 0.1 | 每步生长间隔秒数（动画速度） |
| `SetBranchProbability(float)` | 0.6 | 每步产生侧向分支的概率 |
| `SetAttractionStrength(float)` | 0.7 | 吸引评分中方向对齐 vs. 距离的权重 |
| `SetStraightExtendLength(int32)` | 3 | 每前进步骤放置的格数（多格延伸） |
| `SetMinBranchSpacing(int32)` | 3 | 直路段上分支点之间的最小间距 |

#### 事件委托

| 委托 | 签名 | 触发时机 |
|---|---|---|
| `OnGenerationStarted` | `()` | 收集起始点后，第一步之前 |
| `OnGenerationStep` | `(int32 RemainingBudget)` | 每步生长后 |
| `OnGenerationFinished` | `(bool bAllBuildingsConnected)` | 完成、中止或终止时 |

#### 终止条件

- 单次生成预算或 GridManager 共享预算耗尽。
- 所有建筑属于同一个道路连通分量（提前成功）。
- 有机生长前沿和预留连接计划均为空。
- 所有候选均无法放置，且无法重新构建有效修复路径。

玩家手动触发时遵守 `LSystemBudgetShare`。标题界面的自动预览没有玩家规划阶段，因此 GameMode 会提供全部剩余网格预算，避免在建筑未连通时进入模拟。

#### 道路地块创建

`CreateRoadTile()` 通过 `World::SpawnActor` + `PlaceOnGrid()` 生成 `ARoadTile`。`PlaceOnGrid` 内部调用 `GridManager::OccupyCell`，触发邻居掩码更新和 `OnCellChanged` 广播 —— 因此 `ARoadTile` 放置后自动切换到正确的 mesh/rotation。

#### 生长动画

- `FTimerHandle` 每 `GrowthInterval` 秒调用 `ProcessGrowthStep()`，每步放置一批格（每批最多 `StraightExtendLength` 格）。
- （未来）未完成的道路使用半透明材质；完成后切换为不透明。
- （未来）分支连接建筑时触发建筑高亮。

---

### 2.6 车辆 AI：A\* 寻路与样条运动

#### 实现状态：✅ 已实现 — v0.4 双向车道 + 弯道感知切线缩放

`AVehicleActor` 是一个可蓝图化的 `AActor`，包含 `UStaticMeshComponent` 车身和 `USplineComponent`（`PathSpline`），后者存储从起点到终点的完整世界空间路径。`PathSpline` 使用绝对变换（未挂载到移动中的根组件），确保车辆移动时世界空间查询保持正确。

**基于样条的运动模型：** 车辆维护一个 `CurrentSplineDistance` 浮点数。每帧 `TickMovementSpline(DeltaTime)` 将此距离推进 `MoveSpeed * DeltaTime`，从样条查询世界空间位置（`GetLocationAtDistanceAlongSpline`）和方向（`GetDirectionAtDistanceAlongSpline`），并更新 Actor 的位置和旋转。当 `CurrentSplineDistance >= SplineLength` 时车辆到达。

**路径构建（`BuildSplinePath`）— v0.4 转弯偏移 + arrive/leave 分离：**
对 A\* 原始路径（所有格子均保留）做单次遍历，输出样条点、每点切线方向和**独立的 arrive/leave 切线长度乘数**：
- **首格：** 添加 `cell_center`，切线朝向下一格，两乘数 = 1.0。
- **转弯格：** 将每个弯道替换为偏移点：
  - 入口偏移：`cell_center - EntryDir * CellSize/2`，切线 = `EntryDir`；非连续弯道时 arrive=1.0，leave=TurnMult。
  - 出口偏移：`cell_center + ExitDir * CellSize/2`，切线 = `ExitDir`；arrive=TurnMult，leave=1.0（可被后续连续弯道覆盖）。
  - 连续弯道：跳过入口偏移点，改为将上一个出口点的 leave 乘数覆盖为当前弯道的 TurnMult。
- **直道格：** 添加 `cell_center`，切线=前进方向，两乘数=1.0（重置弯道序列）。
- **末格：** 添加 `cell_center`，两乘数=1.0。

**弯道方向检测与缩放（v0.4）：**
- 使用叉积 Z 分量：`CrossZ = EntryDir.X × ExitDir.Y - EntryDir.Y × ExitDir.X`
  - `CrossZ > 0` → 右转；`CrossZ < 0` → 左转。
- `bRightHand == bIsRightTurn` → 外侧（大弯）→ `TurnMult = 1.0 + LaneOffsetFactor`
- `bRightHand != bIsRightTurn` → 内侧（小弯）→ `TurnMult = 1.0 - LaneOffsetFactor`

**`SetSplinePath` 切线控制 — 手柄打断方案（v0.4）：**
1. 旧方式建样条：`AddSplineWorldPoint` 全点，`SetTangentAtSplinePoint` 统一切线长度为 `CellSize`。
2. 打断手柄联动：通过 `SetSplinePointType(i, CurveCustomTangent, false)` 将全点设为 `CurveCustomTangent`，然后 `UpdateSpline()`。
3. 按段覆盖：通过 `SplineCurves.Position.Points[i]`，对每个 `LeaveTangentLengths[i] ≠ 1.0` 的段 `(i, i+1)`：
   - `Points[i].LeaveTangent = TangentDir[i] * CellSize * LMult`
   - `Points[i+1].ArriveTangent = TangentDir[i+1] * CellSize * LMult`
   - 两端手柄共用同一乘数，保证曲线段变形对称。
4. 最后 `UpdateSpline()` 应用。

**双向车道偏移（v0.4）：**
- 生成所有样条点和切线方向后，`BuildSplinePath` 对每个点施加垂直偏移。
- 偏移距离 = `CellSize × LaneOffsetFactor`（可配置，默认 0.2）。
- 右垂直方向 = `(TangentDir.Y, -TangentDir.X, 0.0)` — 切线在 XY 平面顺时针旋转 90°。
- 右舵（`ECityFlowDrivingSide::RightHand`）：点偏移 `+RightPerp × Offset`
- 左舵（`ECityFlowDrivingSide::LeftHand`）：点偏移 `−RightPerp × Offset`
- 配置：`ACityFlowGameMode::DrivingSide` 和 `LaneOffsetFactor` 在模拟开始时通过 `SetDrivingSide()` / `SetLaneOffsetFactor()` 传递给 `UVehicleManager`。

**车辆生成：** `UVehicleManager::SpawnVehicle(Origin, Destination)` 选取出入口连接点，通过 `BuildPath()` 运行 A\*，调用 `BuildSplinePath()` 生成世界空间点数组，生成车辆，吸附到第一个样条点，调用 `Vehicle->SetSplinePath(Points)`。

**A\* 寻路：**
- 节点 = 道路格（`ECellType::Road`）；边 = `ConnectedMask` 方向位。
- 代价 = 每步 1（均匀）；启发式 = 曼哈顿距离。

**交叉口占用（v0.6 — ✅ 方向占用 + 轮转调度）：**

**物理触发盒：**
- 每个 `ConnectedMask` ≥ 3（十字 / T 型路口）的 `ARoadTile` 启用一个 `UBoxComponent`（`IntersectionBox`），尺寸恰好为一个网格单元（抵消 Actor 缩放后：`BoxExtent = CellSize / ActorScale / 2`）。
- Box 使用 `ObjectType = ECC_Vehicle`，对 Vehicle 通道设为 `ECR_Overlap`，使 `VehicleMesh`（QueryVehicle 预设，Vehicle→Vehicle = Overlap）生成 `OnBeginOverlap` / `OnEndOverlap` 事件来驱动锁生命周期。
- Box 同时 Block `ECC_GameTraceChannel2`（Intersection），用于前向探测扫描。

**方向占用表（ARoadTile）：**
```
TMap<EGridDirection, TSet<TWeakObjectPtr<AVehicleActor>>> DirectionOccupants    // 物理在 Box 内的车辆
TMap<EGridDirection, TSet<TWeakObjectPtr<AVehicleActor>>> PendingReservations   // 探测授予但尚未进入 Box
TMap<TWeakObjectPtr<AVehicleActor>, EGridDirection>        VehicleEntryDirs      // 反向查找
TMap<TWeakObjectPtr<AVehicleActor>, float>                 PendingReservationTimestamps  // 超时过期用
```

**核心锁协议：**
1. **前向探测扫描（Channel2）：** `PerformForwardProbe()` 用 `ECC_GameTraceChannel2` 扫描，从 `GridDirectionUtils::DirectionFromWorldVector(Velocity)` 推导入口方向，调用 `RoadTile->TryAcquireIntersectionLock(this, EntryDir)`。
2. **冲突检查：** 收集 `OccupiedDirs = DirectionOccupants.Keys ∪ PendingReservations.Keys`。
   - 空 → 授予。
   - 仅含 `EntryDir` → 同向车流，授予（如未超出 `MaxConsecutiveGrants`）。
   - 含其他方向 → 交叉冲突。仅当 `ServingDirection == EntryDir && ServedCount < MaxConsecutiveGrants` 时授予。
3. **Pending→Occupants 转移：** `OnBeginOverlap` 将车辆从 `PendingReservations[EntryDir]` 移至 `DirectionOccupants[EntryDir]`。
4. **锁释放：** `OnEndOverlap` 从两个表中移除车辆。**无需显式释放 API**——Overlap 事件驱动整个生命周期。

**方向轮转调度（v0.6）：**
- `ServingDirection`：当前服务方向；`ServedCount`：本轮已放行车辆数；`WaitingDirs`：竞争方向集合；`MaxConsecutiveGrants = 1`：每方向每轮最多放行数。
- `EndOverlap` 检测到 Box 变空时：peek（不 remove）`WaitingDirs` 中第一个方向作为新 `ServingDirection`。该方向仅在 `TryAcquireIntersectionLock` 实际放行一辆车后才从 `WaitingDirs` 中移除。
- Box 被占用期间的交叉方向请求：仅当 `EntryDir == ServingDirection && ServedCount < MaxConsecutiveGrants` 时授予；否则拒绝。当 `ServedCount` 达上限时该方向**不重新入队**——本轮已让出。
- 单方向车流：无轮转干预（行为与简单方向占用模型完全一致）。

**保险机制：**
| 机制 | 触发方式 | 目的 |
|------|---------|------|
| 已入驻全放行 | `TryAcquire` —— 车辆已在 `VehicleEntryDirs` 中 | 消除样条弯道上方向推导偏差导致的误拦 |
| 物理重叠校验 | `VehicleManager::SanitizeAllIntersectionLocks()` 每 2 秒通过 `IsOverlappingActor` | 清理 `EndOverlap` 事件丢失导致的僵尸 `DirectionOccupants` 记录 |
| 预占超时过期 | 同一 Timer，`ExpirePendingReservations(5.0f)` | 清除拥堵中车辆预占但永不进入 Box 的记录 |
| 已过路口追踪 | `AVehicleActor::PassedIntersections` —— `EndOverlap` 时 `MarkIntersectionPassed()`；`TryAcquire` 中检查 | 防止自回绕：刚驶出的车辆前向探测仍能扫到 Box |

**碰撞通道：**
| 通道 | 用途 |
|------|------|
| `ECC_GameTraceChannel1`（Vehicle） | VehicleMesh 车身 → 前向探测物理车辆检测 |
| `ECC_GameTraceChannel2`（Intersection） | IntersectionBox → 前向探测交叉口预留 |

**已移除（v0.5 遗留）：**
- `VehicleManager::IntersectionLocks` TMap、`AcquireIntersectionLock()`、`IsIntersectionLockedByOther()`、`UpdateIntersectionLocks()`
- `VehicleManager::CachedIntersections`、`bIntersectionsDirty`
- `VehicleActor::PathIntersectionCells`、`SetPathIntersections()`、`SetWaitingForIntersection()`
- `CityFlowVehicleTypes.h`：`FIntersectionLock`、`FIntersectionOccupant` 结构体

**运动状态机（v0.6）：**
| 状态 | 描述 |
|------|------|
| `Idle` | 初始或错误状态 |
| `Moving` | 沿样条路径移动；每帧执行前向探测（两次扫描：Ch1 车辆 + Ch2 交叉口） |
| `WaitingCongestion` | 前车或锁定交叉口导致停车；累积 `CongestionWaitTime` 用于死锁检测；障碍清除后自动恢复 |
| `Arrived` | 到达终点；清除所有预留交叉口，广播事件 |

**v0.11 运动流控：**
```
TickMovementSpline:
  1. PerformForwardProbe():
     a. Sweep Ch1 → 找到最近物理车辆
     b. Sweep Ch2 → 对每个命中的 IntersectionBox:
        - TryAcquireIntersectionLock(this, EntryDir) → 授予: 记录到 ReservedIntersections
        - 拒绝 → 视为虚拟障碍，取最小距离
     c. ClosestDist = min(车辆距离, 交叉口距离)
     d. bFrontVehicleTooClose = 最近距离 ≤ 安全距离（车辆）或任意虚拟障碍
  2. 若 bFrontVehicleTooClose：
     a. 状态 → WaitingCongestion，累加 CongestionWaitTime
     b. 若 CongestionWaitTime >= DeadlockTimeout → 释放所有路口锁，清空 PassedIntersections，重置计时器
     c. 减速 → return
  3. 若已解除 → Moving → 重置 CongestionWaitTime → 加速 → 推进样条
```

**拥堵检测（v0.11 — ✅ 已修复）：**
- `UpdateCongestion()` 每帧遍历 `ActiveVehicles` 统计每格车辆数，使用 `WorldToGrid()` 构建临时 `TMap<FGridVector, int32>`。
- 车辆数 `> CongestionThreshold`（默认 3）的格被标记为拥堵。
- 拥堵数据可通过 `GetCongestedCells()` 查询，并通过 `OnCongestionUpdated` 广播。
- **v0.11 修复：** 旧版使用持久化 `TMap<FGridVector, AVehicleActor*>`（`VehicleGridMap`），每格最多存 1 辆车（TMap 重复 key 会断言）。新版逐帧计数的方案既正确又简洁，同时删除了 `UpdateVehicleGridOccupancy()` 和 `IsOccupiedByVehicle()` 两份死代码。

**死锁超时释放（v0.11 — ✅ 新增）：**
- 当相邻两个路口互锁时（A 车占路口 1 等路口 2，B 车占路口 2 等路口 1），两车陷入"持锁并等待"死锁。
- `AVehicleActor` 处于 `WaitingCongestion` 时持续累加 `CongestionWaitTime`。超过 `DeadlockTimeout`（默认 3.0 秒，蓝图可配置）后，车辆强制通过 `ReleaseVehicleFromAllTables()` 释放所有路口占用，清空 `ReservedIntersections` 和 `PassedIntersections`，重置计时器。
- 释放后路口重新空闲，另一车辆获取锁后通行，打破死锁。
- 退出 `WaitingCongestion` 或接收新样条路径时自动重置 `CongestionWaitTime`。

**前向探测 — 零距离命中修复（v0.11）：**
- `PerformForwardProbe()` 之前用 `ProjDist > 0.0f`（车辆扫描）和 `InterDist <= 0.0f`（路口扫描）过滤命中，当探测体积起始就与碰撞体重叠时，命中被错误跳过。
- 分别修正为 `ProjDist >= 0.0f` 和 `InterDist < 0.0f`，使占用位置就在路口内的车辆（如从建筑门口即为路口处生成）能正确获取路口锁。

**前向探测 — Spline 采样段（v0.15）：**
- `PerformForwardProbe()` 现在通过 `BuildForwardProbeSegment()` 构建扫描段，不再使用 `GetActorLocation() + VelocityDirection`。
- 探测起点从当前 spline 的 `CurrentSplineDistance + SelfAvoidOffset` 处采样；终点从 `StartDistance + ForwardProbeDistance` 处采样，并钳位到 spline 长度内。
- 扫描方向由采样出的 spline 线段推导，车辆检测（Channel1）和路口预占（Channel2）使用同一组 spline 采样起终点。
- `PerformRamKill()` 复用同一个辅助函数，使暴走车辆沿实际 spline 路径击杀，而不是依赖可能滞后的 Actor transform 方向。

#### 路口占用指示器（v0.13 — ✅ 已实现）

每个启用了 `IntersectionBox` 的 `ARoadTile`（十字 / T 型路口）在路口上方悬浮一个平面指示器，一目了然地展示占用状态。

**架构：**
- `UStaticMeshComponent IndicatorPlane` — 挂载到 `RootSceneComponent` 的 Plane mesh，碰撞禁用。
- 使用引擎内置 Plane（`/Engine/BasicShapes/Plane`），缩放到 `CellSize × IndicatorSize / 100.0f`（Plane 默认为 100×100 单位）。
- `UMaterialInstanceDynamic` 在首次 `UpdateIndicator()` 时从 `IndicatorMaterial` 惰性创建。
- 材质需暴露名为 `"Color"` 的 `VectorParameter` 并接入 emissive；半透明/无光照混合模式。

**状态刷新触发点：**
| 触发条件 | 位置 |
|---|---|
| IntersectionBox 启用/禁用 | `UpdateIntersectionBox()` → `UpdateIndicator()` |
| 车辆进入 Box | `OnIntersectionBoxBeginOverlap()` → `UpdateIndicatorState()` |
| 车辆离开 Box | `OnIntersectionBoxEndOverlap()` → `UpdateIndicatorState()` |
| 定期物理校验 | `SanitizeOccupants()` → `UpdateIndicatorState()` |
| 预占用过期 | `ExpirePendingReservations()` → `UpdateIndicatorState()` |

**颜色逻辑：**
- `IsAnyDirectionOccupied()` 返回 `false` → `IndicatorFreeColor`（默认绿色）
- `IsAnyDirectionOccupied()` 返回 `true` → `IndicatorOccupiedColor`（默认红色）

**蓝图可配置属性（均在 ARoadTile 上）：**

| 属性 | 默认值 | 描述 |
|---|---|---|
| `IndicatorMaterial` | — | DMI 基础材质（必须有 "Color" VectorParameter） |
| `IndicatorSize` | `0.4` | 平面相对单元格大小的比例（0.0–1.0） |
| `IndicatorZOffset` | `80.0` | IntersectionBox 顶部上方的 Z 偏移 |
| `IndicatorFreeColor` | `(0,1,0)` | 绿色 — 路口空闲 |
| `IndicatorOccupiedColor` | `(1,0,0)` | 红色 — 路口被占用 |

**材质设置（需在编辑器中创建）：**
- Domain: Surface, Blend Mode: Translucent, Shading Model: Unlit, Two Sided: true
- 节点图：`TexCoord → ComponentMask(RG) → Add(-0.5,-0.5) → Length → OneMinus → SmoothStep(Min,Max) → Multiply(VectorParameter"Color") → Emissive Color`
- SmoothStep Min/Max 根据期望圆半径调整（推荐 `0.48`/`0.52` 可获得完整内切圆）

#### 车辆死亡与停车闪烁系统（v0.12 — ✅ 已实现）

**概述：** 当车辆进入 `WaitingCongestion` 状态（因拥堵而车速归零），基类 `AVehicleActor` 会累加 `TotalStopTime` 并暴露一套模块化、基于 virtual 方法的停车/死亡管线。基类行为是车辆材质以递增频率闪烁红光，直到超过 `DeathTimeout` 后触发超时行为 virtual 方法。基类实现通过 `HandleVehicleDeath()` 触发死亡；子类可覆写以实现替代行为。

##### 架构设计 — Virtual 方法钩子

系统通过 5 个 `protected virtual` 方法实现子类可扩展性：

| Virtual 方法 | 基类行为 | 子类覆写用途 |
|---|---|---|
| `OnVehicleStopped(DeltaTime)` | 累加 `TotalStopTime`；驱动材质红光闪烁 | 附加停车效果（如警报声） |
| `OnVehicleResumed()` | 重置材质 emissive 为 0 | 停止附加效果 |
| `HandleVehicleDeath()` | 播放 VFX/SFX/CameraShake → 广播 `OnVehicleDeath` → `Destroy()` | 自定义死亡行为（无爆炸、不同视觉） |
| `HandleWaitTimeout()` | 调用 `HandleVehicleDeath()` → 销毁 | **v0.14:** 自定义超时行为（如进入狂暴冲撞模式而非死亡） |
| `ShouldResetStopTime()` | 返回 `false`（持续累计 → 一定死亡） | 返回 `true` 则永不超时死亡 |

##### 运动循环集成

```
TickMovementSpline:
  if bFrontVehicleTooClose:
    TotalStopTime += DeltaTime
    OnVehicleStopped(DeltaTime)           ← virtual: 材质闪烁
    if TotalStopTime >= DeathTimeout:
      HandleWaitTimeout()                 ← virtual: 基类→死亡, 子类→狂暴
      if IsActorBeingDestroyed(): return
      // else: 落入移动逻辑（狂暴模式）
    else:
      减速 → return
  if bBerserk:
    PerformRamKill()                      ← 扫荡击杀前方车辆
  if 恢复通行 (之前是 WaitingCongestion):
    OnVehicleResumed()                    ← virtual: 重置材质
    TotalStopTime = ShouldResetStopTime() ? 0 : TotalStopTime
```

`TotalStopTime` 与 `CongestionWaitTime`（仍用于死锁超时释放路口锁）**互相独立**，二者在 `WaitingCongestion` 期间并行累加。

##### 材质红光闪烁

首次停车时从 `VehicleMesh` 的材质（slot 0）惰性创建 `UMaterialInstanceDynamic`。之后每帧在 `WaitingCongestion` 中：

- `Progress = TotalStopTime / DeathTimeout`（钳位 0→1）
- `频率 = Lerp(0.5 Hz, 4.0 Hz, Progress)` — 越接近死亡越快
- `强度 = (sin(TotalStopTime × 频率 × 2π) + 1) / 2` — 平滑正弦脉冲 0→1
- `MID->SetScalarParameterValue("FlashIntensity", 强度)`

材质需暴露 `ScalarParameter` 名为 `FlashIntensity`，连接到 emissive 通道（乘以红色，如 `(5,0,0)` 配合 Bloom 实现可见辉光）。恢复行驶时 `FlashIntensity` 置为 0。

##### 死亡序列

`HandleVehicleDeath()` 基类实现：

1. 释放所有路口占用（与到达/EndPlay 相同的清理逻辑）
2. **VFX：** `UNiagaraFunctionLibrary::SpawnSystemAtLocation()` 使用 `ENCPoolMethod::None` + `bAutoDestroy=true`。生成后通过 `SetVariableFloat(ExplosionVFXScaleParamName, ExplosionVFXScale)` 将缩放浮点值直接推入 Niagara User Parameter（如 `"Scale"`）。Niagara 系统的 `LoopBehavior` 必须设置为 `Once`，`bAutoDestroy` 才会触发。
3. **SFX：** `UGameplayStatics::PlaySoundAtLocation()`
4. **Camera Shake：** 距离衰减 — `强度 = Clamp(1.0 - 相机距离/DeathShakeMaxDistance, 0, 1)`。相机离爆炸越近震感越强；俯视远距离不震。
5. **委托：** `OnVehicleDeath.Broadcast(this)` — VehicleManager 和 ScoringManager 均监听
6. `Destroy()`

##### 蓝图可配置属性（均在 AVehicleActor 上）

| 属性 | 类型 | 默认值 | 分类 | 描述 |
|---|---|---|---|---|
| `DeathTimeout` | `float` | `5.0` | `Vehicle\|Death` | 超时触发 `HandleWaitTimeout()` 的秒数 |
| `ExplosionVFX` | `UNiagaraSystem*` | — | `Vehicle\|Death\|VFX` | 爆炸 Niagara 系统资产 |
| `ExplosionVFXScale` | `float` | `1.0` | `Vehicle\|Death\|VFX` | 发送至 Niagara User Parameter 的浮点值（由 `ExplosionVFXScaleParamName` 指定参数名） |
| `ExplosionVFXScaleParamName` | `FName` | `"Scale"` | `Vehicle\|Death\|VFX` | Niagara 中接收缩放值的 User Parameter 名称 |
| `ExplosionSFX` | `USoundBase*` | — | `Vehicle\|Death\|SFX` | 爆炸音效资产 |
| `DeathCameraShake` | `TSubclassOf<UCameraShakeBase>` | — | `Vehicle\|Death\|Camera` | 震屏类 |
| `DeathShakeMaxDistance` | `float` | `3000.0` | `Vehicle\|Death\|Camera` | 震屏衰减最大距离（cm） |
| `FlashIntensity` 参数 | 材质 ScalarParam | — | （材质） | 需接入 emissive × 红色漫反射 |

##### 跨管理器事件流

```
AVehicleActor::HandleVehicleDeath()
  │
  ├─► VFX / SFX / CameraShake（本地）
  ├─► OnVehicleDeath.Broadcast(this)
  │     │
  │     ├─► UVehicleManager::OnVehicleDeathHandler(Vehicle)
  │     │     ├─ ActiveVehicles.Remove(Vehicle)
  │     │     └─ OnVehicleDied.Broadcast(Vehicle)
  │     │           │
  │     │           └─► UScoringManager::OnVehicleDeathHandler(Vehicle)
  │     │                 ├─ DeathCount++
  │     │                 ├─ DeathPenaltyTotal += Settings->DeathPenalty（默认 50）
  │     │                 ├─ TotalScore = ArrivalScoreTotal - CongestionPenaltyTotal - DeathPenaltyTotal
  │     │                 └─ OnScoreChanged.Broadcast(TotalScore)
  │     │
  │     └─（蓝图通过 OnVehicleDeath 委托监听）
  │
  └─► Destroy()
```

##### 计分公式更新（v0.12）

```
TotalScore = ArrivalScoreTotal - CongestionPenaltyTotal - DeathPenaltyTotal
```

`DeveloperSettings::DeathPenalty`（新增，默认 `50`）控制每死亡扣分。生命周期：死亡罚分在模拟期间实时扣除；`FullConnectivityBonus` 仍在结算时添加。

##### 死亡车辆清理

- `VehicleManager::Tick()` 只从 `ActiveVehicles` 中移除 `Arrived` 和 `Idle` 状态的车辆。`HandleVehicleDeath()` 在广播 `OnVehicleDeath` 后立即调用 `Destroy()`，且死亡处理器在销毁前从 `ActiveVehicles` 中移除车辆，因此不存在悬空指针。
- `EndPlay()`（在 `Destroy()` 期间调用）释放所有剩余路口占用，与之前一致。

#### 狂暴车辆（v0.14 — ✅ 已实现）

`ARampageVehicle` 是 `AVehicleActor` 的子类，等待超时时不死亡，而是进入**狂暴冲撞模式**：忽略所有前向探测，以更高速度行驶，撞死沿途所有车辆。

##### 类层级

```
AVehicleActor
  └─ ARampageVehicle   ← 狂暴超时行为 + 冲撞击杀
```

##### 狂暴超时行为

当 `TotalStopTime >= DeathTimeout` 时，调用 `ARampageVehicle::HandleWaitTimeout()` 而非死亡：

1. 设置 `bBerserk = true` — 车辆进入冲撞模式。
2. 释放所有路口占用（`ReservedIntersections`、`PassedIntersections`）。
3. 重置 `TotalStopTime`、`CongestionWaitTime` 和闪烁材质。
4. 将 `MovementState` 设置为 `Moving`。
5. 调用者（`TickMovementSpline`）检测到 `!IsActorBeingDestroyed()` 后在同一帧落入移动逻辑。

##### 冲撞模式行为

`bBerserk` 为 true 时，`TickMovementSpline` 每帧：

| 行为 | 详情 |
|---|---|
| **跳过前向探测** | 不调用 `PerformForwardProbe()` — 无车辆或路口能阻挡冲撞车辆 |
| **速度倍率** | `有效速度 = MoveSpeed × GetBerserkSpeedMultiplier()`（默认 1.2×） |
| **冲撞击杀** | `PerformRamKill()` 通过 `ECC_GameTraceChannel1` 扫荡前方，对每辆活跃车辆调用 `HandleVehicleDeath()` |
| **急促闪烁** | `Tick()` 在 `bBerserk` 为 true 时持续用高频正弦写入材质 `FlashIntensity` |

##### PerformRamKill

使用与 `PerformForwardProbe` 相同的扫荡参数（半径、距离、偏移），但将命中视为击杀目标：

- 通过 `SweepMultiByChannel`（ECC_GameTraceChannel1）扫荡，忽略自身。
- 对前方（投影距离在 [0, ForwardProbeDistance] 内）的每个命中 `AVehicleActor`：
  - 跳过已在销毁中的目标。
  - 跳过状态为 `Arrived` 或 `Idle` 的目标。
  - 调用 `OtherVehicle->HandleVehicleDeath()` — 完整爆炸 VFX/SFX 序列。
- 被击杀车辆正常广播 `OnVehicleDeath` → VehicleManager 移除 → ScoringManager 计死亡罚分。

##### 蓝图可配置属性（在 ARampageVehicle 上）

| 属性 | 类型 | 默认值 | 分类 | 描述 |
|---|---|---|---|---|
| `RampageSpeedMultiplier` | `float` | `1.2` | `Vehicle\|Berserk` | 冲撞期间的速度倍率 |
| `RampageFlashFrequency` | `float` | `18.0` | `Vehicle\|Berserk` | 暴走模式下红色材质闪烁频率 |

##### 基类狂暴基础设施（AVehicleActor）

为支持冲撞模式，`AVehicleActor` 新增以下 protected 成员：

| 成员 | 类型 | 描述 |
|---|---|---|
| `bBerserk` | `bool` | 由子类 `HandleWaitTimeout()` 设置；为 true 时跳过前向探测，每帧调用 `PerformRamKill` |
| `GetBerserkSpeedMultiplier()` | `virtual float` | 基类返回 `1.0`；`ARampageVehicle` 在 `bBerserk` 时返回 `RampageSpeedMultiplier` |
| `PerformRamKill()` | `void` | 扫荡击杀前方车辆 |
| `HandleWaitTimeout()` | `virtual void` | **v0.14:** 替换已删除的 `bEnableTimeoutDeath` 布尔值；基类调用 `HandleVehicleDeath()`，子类可覆写 |

#### 瞬移车辆（v0.15 — ✅ 已实现）

`ATeleportVehicle` 是 `AVehicleActor` 的子类，等待超时时不死亡，而是沿当前 spline 路径朝目的地方向向前瞬移一段距离。如果瞬移落点与其他活跃车辆重叠，则通过标准 `HandleVehicleDeath()` 管线击杀这些车辆。

##### 类层级

```
AVehicleActor
  └─ ATeleportVehicle   ← 超时瞬移 + 重叠死亡
```

##### 超时瞬移行为

当 `TotalStopTime >= DeathTimeout` 时，调用 `ATeleportVehicle::HandleWaitTimeout()`：

1. 若配置了 `TeleportBeforeVFX`，先在当前 Actor 位置生成特效，并将 `TeleportVFXScale` 写入 `TeleportVFXScaleParamName` 指定的 Niagara User Parameter。
2. 在 `[TeleportMinDistance, TeleportMaxDistance]` 中随机选取向前瞬移距离（默认 1200-3000 cm）。
3. 将 `CurrentSplineDistance` 向前移动该距离，并钳位到 `[0, SplineLength]`。
4. 在新的 spline 距离采样位置，并将 Actor 移动到 `NewPos + VehicleZOffset`。
5. 更新 `VelocityDirection`、Actor 旋转和 `PreviousGridPosition`。
6. 若配置了 `TeleportAfterVFX`，在新的 Actor 位置生成特效，并使用同一套缩放参数写入。
7. 释放所有已预占/已通过的路口引用，因为车辆发生了瞬时位移。
8. 重置 `TotalStopTime`、`CongestionWaitTime`、`bFrontVehicleTooClose`、闪烁材质强度，并回到 `Moving`。
9. 瞬移后立即调用 `KillOverlappingVehicles(TeleportOverlapRadius)`。

##### 重叠死亡

`AVehicleActor::KillOverlappingVehicles()` 在当前 Actor 位置通过 `ECC_GameTraceChannel1` 执行 `OverlapMultiByChannel`：

- 使用子类提供的可配置球体半径。
- 忽略自身、已经销毁中的车辆，以及 `Arrived` / `Idle` 状态车辆。
- 对每辆唯一的重叠活跃车辆调用 `OtherVehicle->HandleVehicleDeath()`。
- 死亡事件继续走现有 VehicleManager 和 ScoringManager 管线，因此死亡罚分和清理逻辑保持不变。

##### 蓝图可配置属性（在 ATeleportVehicle 上）

| 属性 | 类型 | 默认值 | 分类 | 描述 |
|---|---|---|---|---|
| `TeleportMinDistance` | `float` | `1200.0` | `Vehicle\|Teleport` | 超时时沿当前 spline 向前移动的最小距离（cm） |
| `TeleportMaxDistance` | `float` | `3000.0` | `Vehicle\|Teleport` | 超时时沿当前 spline 向前移动的最大距离（cm） |
| `TeleportOverlapRadius` | `float` | `120.0` | `Vehicle\|Teleport` | 瞬移后用于检测待击杀车辆的球体半径 |
| `TeleportBeforeVFX` | `UNiagaraSystem*` | — | `Vehicle\|Teleport\|VFX` | 瞬移前在旧位置生成的特效 |
| `TeleportAfterVFX` | `UNiagaraSystem*` | — | `Vehicle\|Teleport\|VFX` | 瞬移后在新位置生成的特效 |
| `TeleportVFXScale` | `float` | `1.0` | `Vehicle\|Teleport\|VFX` | 写入两个瞬移 Niagara 系统 User Parameter 的缩放浮点值 |
| `TeleportVFXScaleParamName` | `FName` | `"Scale"` | `Vehicle\|Teleport\|VFX` | 接收 `TeleportVFXScale` 的 Niagara User Parameter 名称 |

#### 车辆 Hover 目的地方向指示器（v0.16 — ✅ 已实现）

模拟阶段中，当鼠标悬停在车辆上时，该车辆会被高亮描边，并在车辆上方显示一个指向目的地的方向箭头。

##### PlayerController Hover 检测

`ACityFlowPlayerController::Tick()` 每帧调用 `UpdateVehicleHover()`：

1. `IsSimulationPhaseActive()` 检查 `ACityFlowGameMode::GetCurrentPhase() == ECityFlowGamePhase::Simulating`。
2. 若不在模拟阶段，`ClearHoveredVehicle()` 会关闭上一辆车的 hover 状态。
3. 模拟阶段中，Controller 先使用 `ECC_GameTraceChannel1`（Vehicle 通道）执行鼠标下方 trace，若未命中车辆则回退到 `ECC_Visibility`。
4. 当命中的 Actor 发生变化时，上一辆 `HoveredVehicle` 接收 `SetHovered(false)`，新的 `AVehicleActor` 接收 `SetHovered(true)`。

这样规划阶段的放置预览逻辑与车辆查看逻辑保持独立，并避免模拟阶段之外出现 hover 效果。

##### 车辆 Hover 渲染

`AVehicleActor::SetHovered()` 切换两个效果：

- **描边 Mask：** `ApplyHoverRenderState()` 遍历车辆 Actor 下所有 `UPrimitiveComponent` 子组件，并统一应用 `SetRenderCustomDepth()` 与 `SetCustomDepthStencilValue(HoverStencilValue)`。这会覆盖蓝图中额外添加的子 Mesh，而不只处理基础 `VehicleMesh`。
- **目的地箭头：** `DestinationArrowWidget` 随 hover 状态显示/隐藏。该组件会从 CustomDepth 遍历中排除，因此箭头本身不会被纳入车辆描边 Mask。

`PathSpline` 同样会从 CustomDepth 写入中排除。

##### 目的地箭头朝向

`DestinationArrowWidget` 是挂在 `VehicleRoot` 下的世界空间 `UWidgetComponent`。车辆处于 hover 状态时，`UpdateDestinationArrow()` 会执行：

- 计算车辆当前位置到 `Destination->GetActorLocation()` 的水平向量。
- 将 Widget 旋转到该方向，并叠加 `DestinationArrowRotationOffset`。
- Widget 保持作为车辆子组件跟随车辆移动；高度由 `DestinationArrowHeight` 控制，并在 `OnConstruction()` 和 `BeginPlay()` 中通过 `RefreshDestinationArrowOffset()` 写入组件相对 Z。

##### 蓝图 / 编辑器要求

车辆蓝图需要为 `DestinationArrowWidget` 指定 Widget Class（例如一个只包含箭头图片的简单 Widget）。项目还需要开启 `Custom Depth-Stencil Pass`，并提供一个读取 `CustomStencil == HoverStencilValue`（默认 252）的后处理描边材质。

##### 已知限制

由于描边由后处理材质绘制，在某些渲染顺序下可能会视觉上盖到世界空间 3D Widget 上方。当前接受此限制，不进行修复；该实现保留世界空间箭头，避免引入额外的屏幕空间 Widget 通信逻辑。

---

### 2.7 起点 / 目的地生成与计分

#### 实现状态：✅ 已实现 — v0.10 DataAsset 驱动生成 + doorway 验证

#### 车辆生成数据资产

`UVehicleDataAsset::VehicleEntries` 是 `FVehicleSpawnEntry` 的数组。车辆生成使用两级优先级：若 `ACityFlowGameMode::VehicleDataAsset`（`TObjectPtr<UVehicleDataAsset>`）已配置，则传给 `UVehicleManager::SetVehicleDataAsset()` 直接使用；否则 `CacheSpawnEntries()` 回退到 `DeveloperSettings::DefaultVehicleDataAsset`。`PickRandomVehicleClass()` 从加载的数据源按加权随机选择。

每个 `AVehicleActor` 子类在蓝图 Class Defaults 中自行配置 Mesh/MoveSpeed/DebugColor——无需 DataAsset 驱动属性覆盖。

#### 建筑生成

`CityFlowGameMode::InitializeDefaultScene()` 现在支持两种路径：

**主路径：`UBuildingDataAsset`**（v0.10 新增）——一个 `UPrimaryDataAsset`，包含单个 `BuildingEntries` 数组，每项为 `FBuildingDataEntry`（`TSubclassOf<ABuilding>` + `float SpawnWeight`）。

**v0.13 更新：** 所有建筑现在同时作为起点和终点。`CollectOriginDestinations()` 将每个建筑同时加入起点和终点数组。`ABuilding` 上的 `bIsDestination` 标记不再用于生成逻辑。生成守卫条件要求至少 2 个建筑（`StartSpawning()` 中检查），以保证不同的起点/终点。已有的生成循环去重（`Dest == Origin` 跳过）确保同一个建筑不会被同时选为起点和终点。

生成数量使用**最大余数法**确定性分配：每个条目获得 `floor(weight / totalWeight × DefaultBuildingCount)`，剩余名额按小数部分从大到小分配。

**回退：** 若 `BuildingDataAsset` 未设置，则使用旧版 `OriginBuildingClass` / `DestinationBuildingClass` 单类配置（各 50%）。

建筑随后通过 `GridManager::TryPlaceBuildingsRandom()` 以随机位置和旋转放置。

#### 建筑 Doorway 放置验证（v0.10）

`ABuilding::ValidatePlacement()` 现在验证每个 doorway 的连接点（建筑占地往外一格）满足两个条件：
- **边界内：** `IsValidGridPos(ConnPt)` — doorway 格必须在网格边界内。
- **未被其他建筑占据：** `GetCell(ConnPt).Type != ECellType::Building` — 防止 doorway 格与其他建筑占地重叠。

若任一 doorway 验证失败，`CanPlaceAt()` 返回 `false`，`TryPlaceBuildingRandom()` 自动尝试下一个候选位置/旋转。

新增辅助函数 `GetDoorwayConnectionPointForPosition(Doorway, BasePos)`，在 `GridPosition` 未设置前计算 doorway 对任意候选位置的连接点，支持放置前验证。

#### 车辆生成

`UVehicleManager::Tick()` 以 `SpawnInterval`（默认 5s）间隔生成车辆。每帧随机选取起点和终点，调用 `SpawnVehicle()` 计算 A\* 路径并生成 Actor。

#### 计分机制（UScoringManager）

| 项目 | 规则 |
|---|---|
| **基础到达分** | 每辆车到达 +ArrivalScore（默认 100，可通过 DeveloperSettings 配置） |
| **死亡罚分** | 每辆车死亡 -DeathPenalty（默认 50）。`ScoringManager` 直接绑定每辆新生成车辆的 `OnVehicleDeath`，同时监听 `VehicleManager::OnVehicleDied`；`ScoredDeathVehicles` 用于对两条通知路径去重。 |
| **拥堵惩罚** | 每秒扣除 CongestionPenaltyPerSecond × 拥堵格数（默认 5/格/秒） |
| **全连通奖励** | 结算时所有建筑连通，+FullConnectivityBonus（默认 500） |
| **效率奖励** | 剩余道路预算（未来：按比例奖励） |

模拟开始时 `StartScoring()`，结算时 `StopScoring()`。模拟阶段中，`TotalScore` 是 HUD 上显示的实时分数：

```text
LiveScore = ArrivalScoreTotal - CongestionPenaltyTotal - DeathPenaltyTotal
```

`StartScoring()` 会广播 `OnScoreChanged(0)`；每次车辆到达、车辆死亡、拥堵罚分都会重新计算实时分数并广播 `OnScoreChanged`，使 `UCityFlowGameWidget::Txt_Score` 实时刷新。最终报告分则在 `ComputeFinalScore()` 中单独计算。

`OnScorePopupRequested(FVector WorldLocation, int32 DeltaScore)` 在到达和死亡分数变化时广播。计分层不生成 UI Actor，只向 HUD 层报告世界锚点和带符号的分数变化。

**v0.18 最终分更新：** 上方“到达分 - 罚分”实时累计模型继续作为模拟阶段 HUD 分数和即时 popup 反馈模型。最终结算改为 GDD 中定义的报告式评分模型，并统一存储在 `FCityFlowScoreBreakdown`（`Public/Scoring/Types/ScoringTypes.h`）中。

| 分项 | 实现 |
|---|---|
| **连通性** | 统计总建筑数、已连通建筑数，计算道路连通分量和最大已连通建筑分量，并应用 `180 * ConnectedRatio^2 + 80 * LargestComponentRatio + 40 * AllConnected`。 |
| **交通结果** | 统计生成、到达、死亡和结算时仍在场车辆；未完成车辆仍计入分母，避免虚高到达率。 |
| **通行效率** | `AVehicleActor` 记录 `TravelTime` 和 `PathCellCount`；到达车辆汇总为 `TotalTravelTimeOfArrivedVehicles / TotalCellsTraversedByArrivedVehicles`。 |
| **预算效率** | 使用道路格数量作为 `UsedBudget`，用建筑网格位置之间的曼哈顿 MST 估算最低道路需求，再乘以连通性和 `sqrt(ArrivalRate)`。 |
| **运行时长** | 记录模拟阶段已运行时间，同时按 GDD 将 runtime score 作为提前完成奖励计算；Evaluation UI 单独显示已运行时间。 |
| **地图难度** | 从 `UCityFlowDeveloperSettings` 读取 `ReferenceBuildingCount`、`ReferenceSpreadRatio`、`TargetBudgetPressure`、`AcceptableCellTimeMultiplier` 和地图难度倍率 clamp 参数。 |

`StopScoring()` 现在调用 `ComputeFinalScore()`，填充 `FCityFlowScoreBreakdown`，更新 `TotalScore` 并广播 `OnScoreChanged`。`CF_ShowScoreStats` 会打印最终分、原始分、各分项、规划统计、交通统计和地图难度倍率。

---

### 2.8 玩家与摄像机系统

#### CityFlowPawn

基于 `ACharacter` 的子类，配置为俯视角自由飞行控制，移动方向基于朝向：

| 功能 | 实现 |
|---|---|
| 移动模式 | `MOVE_Flying`（无重力，全轴向移动） |
| 输入 | `Enhanced Input` → `IA_Move`（Axis2D）、`IA_Look`（Axis2D）、`IA_Zoom`（Axis1D）、`IA_Alt`（Digital） |
| 移动方向 | 从 `CameraYaw` 推导（由蓝图从摄像机朝向更新）—— WASD 相对于玩家朝向移动，而非实时摄像机视角 |
| 摄像机朝向 | `CameraYaw`（BlueprintReadWrite, float）—— 蓝图每帧从摄像机 yaw 更新此变量；`Move()` 构建 `FRotator(0, CameraYaw, 0)` 计算前/右向量 |
| 视角复位 | `ResetToInitialViewState(bool bResetLocation)` 恢复 BeginPlay 时记录的 Controller pitch/yaw，可选将 Pawn 传送回初始 Transform，同时更新 `CameraYaw` 并清除当前移动速度。HUD 在返回主菜单时以 `bResetLocation=true` 调用，在标题界面进入游戏时以 `bResetLocation=false` 调用，避免标题界面旋转 yaw 泄漏到游戏镜头。 |
| 移动停止 | `StopCameraMovement()` 清除 Alt 视角状态，并对角色移动组件调用 `StopMovementImmediately()`。HUD 在进入 Pause、Evaluation 等 UI-only 状态时调用。 |
| Alt + 鼠标视角 | `IA_Alt` + `IA_Look` —— 按住 Alt 时设 `bAltHeld = true`，切换输入模式为 `FInputModeGameOnly()`（捕获鼠标），通过鼠标 delta 驱动 `AddControllerYawInput()`（C++ 仅控制 yaw；pitch 在蓝图中处理），并**关闭放置功能**（`DisablePlacement()`），避免光标和预览 Actor 干扰摄像机旋转。松开 Alt 恢复 `FInputModeGameAndUI` + 鼠标光标，且**仅在当前阶段为 `Planning` 时恢复放置**（避免在 Simulation 阶段误开启放置）。 |
| 主菜单摄像机 yaw | `SetMainMenuCameraYawRotationEnabled(bool)` 控制 `Tick()` 内的标题界面 yaw 旋转分支，在标题菜单可见时缓慢增加 Controller yaw。HUD 在 `ShowStartWidget()` 时开启，进入游戏或结算时关闭。Pawn Tick 保持开启，使蓝图中的摄像机 pitch/zoom 插值在离开主菜单后仍能继续运行。 |
| 滚轮缩放 | `IA_Zoom` 调整 `TargetSpringArmLength`（Clamp 到 [Min, Max]）。蓝图每帧读取此变量以驱动 spring arm 长度插值 |
| 蓝图可配置 | `MoveSpeed`、`LookSensitivity`、`MainMenuCameraYawSpeed`、`ZoomSpeed`、`MinSpringArmLength`、`MaxSpringArmLength`、`DefaultCameraPitch`、`MinCameraPitch`、`MaxCameraPitch` |
| 摄像机设置 | 在蓝图中处理：`USpringArmComponent` + `UCameraComponent` 作为子组件；spring arm 使用 `bUsePawnControlRotation = true`；角色自动接管 |

**C++ 维护供蓝图使用的关键变量：**

| 变量 | 默认值 | 说明 |
|---|---|---|
| `CameraYaw` | 0 | 当前朝向 yaw —— 蓝图从摄像机更新；`Move()` 从此计算移动方向 |
| `MainMenuCameraYawSpeed` | 4 | 标题界面 yaw 旋转速度，单位为度/秒 |
| `TargetSpringArmLength` | 10000 | Spring arm 目标长度 —— 蓝图读取并向此值插值 |
| `DefaultCameraPitch` | -60 | BeginPlay 时通过 `SetControlRotation` 设置的初始摄像机俯仰角 |
| `MinCameraPitch` | -80 | 最小俯仰角（最俯视） |
| `MaxCameraPitch` | -30 | 最大俯仰角（最平视） |

#### CityFlowPlayerController

| 功能 | 实现 |
|---|---|
| 光标 | `bShowMouseCursor = true`（由 Pawn 管理：Alt 期间隐藏，松开恢复） |
| 预览系统 | 放置功能默认关闭，因此主菜单和结算界面不会显示预览 Actor。`EnablePlacement()` 生成预览 Actor；开启后通过 `Tick()` → `GetHitResultUnderCursor()` → `SnapToGrid()` 跟随光标；每帧通过 `SetPreviewPlacementValid()` 更新有效性，然后调用 `UpdatePreviewAppearance()` 让 `ARoadTile` 在预览中显示预测的 mesh |
| 放置 | `IA_PlaceItem`（鼠标左键）→ `Started`/`Triggered`/`Completed` 事件 → `TryPlaceAtCursor()` 辅助函数，通过 `LastPlacedGridPos` 去重实现拖拽连续放置 |
| 删除 | `IA_RemoveItem`（鼠标右键）→ `Started`/`Triggered`/`Completed` 事件 → `TryRemoveAtCursor()` 辅助函数，通过 `LastRemovedGridPos` 去重实现拖拽连续删除。从网格 `Cell.RoadActor` 查找 Actor，不依赖碰撞命中。 |
| 蓝图可配置 | `PlaceableActorClass`（任意 `AGridPlaceableActor` 子类）；`IA_PlaceItem`、`IA_RemoveItem`、`IA_Pause` |
| 暂停 | `IA_Pause` → `OnPausePressed` → `HUD::TogglePause()` — 切换暂停覆盖层和 `SetGamePaused` |

#### 摄像机 / 输入状态安全

HUD 状态切换会显式清理从玩法状态遗留到 UI 状态的输入：

| 切换 | 安全处理 |
|---|---|
| 返回主菜单 | 关闭放置，复位 Pawn 位置和初始视角，刷新已按下按键，忽略移动输入，切到 `FInputModeUIOnly`，再开启标题界面 yaw 旋转 |
| 主菜单 → 游戏 | 关闭标题 yaw 旋转，只复位视角 yaw/pitch 而不移动 Pawn，刷新已按下按键，重置移动输入忽略状态，切到 `FInputModeGameAndUI`，若当前为 Planning 则开启放置 |
| 游戏 → Evaluation / Pause | 按需关闭放置，立即停止 Pawn 移动，刷新已按下按键，忽略移动输入，并切到 `FInputModeUIOnly` |

这避免了两个标题/结算流程回归：主菜单旋转 yaw 带入游戏镜头，以及按住 WASD 进入 Evaluation 后 Pawn 继续向某个方向漂移。

#### 放置开关

`ACityFlowPlayerController` 提供放置开关，用于与其他系统（L-system、模拟）协调：

| API | 描述 |
|---|---|
| `EnablePlacement()` | 恢复光标采样、生成新预览 Actor、显示鼠标光标 |
| `DisablePlacement()` | 停止光标采样、销毁预览 Actor |
| `IsPlacementEnabled()` | 查询当前放置开关状态 |

放置关闭时，`Tick()` 跳过 `UpdatePreviewPosition()`，`TryPlaceAtCursor()` / `TryRemoveAtCursor()` 均为空操作。进入模拟时自动关闭放置，重新规划时自动恢复。

放置开关现在也由标题流程协调：`ShowStartWidget()` 和 `ShowEvaluationWidget()` 会关闭放置；普通开始游戏和 Random Mode 仅在玩家进入 Planning 对局后开启放置。

---

### 2.9 网格可视化

提供两个 Actor 类，用于在规划阶段运行时渲染网格参考线，便于调试和视觉参考。

#### AGridPlaneVisualizer（主方案）

使用单个 Plane mesh 配合半透明世界对齐材质，实现高效的单次 DrawCall 网格线渲染。

| 功能 | 实现 |
|---|---|
| 渲染方式 | 单个 `UStaticMeshComponent`（Plane mesh），缩放至完整网格尺寸 |
| 材质 | 运行时创建 `UMaterialInstanceDynamic`，从 `GridManager` 传递 `CellSize`、`LineWidth`、`LineColor`、`OriginX`、`OriginY` 材质参数 |
| 网格对齐 | Plane 的大小和位置在 `SetupPlane()` 中根据 `GridManager` 的宽/高/单元格大小/原点计算 |
| 可视化配置 | `LineColor`、`LineWidth`、`ZOffset` 均可通过蓝图配置 |

材质使用 `M_PrototypeGrid`，`Blend Mode = Translucent`，材质图中基于世界坐标进行简单数学运算。Plane 通过 `ZOffset` 放置在网格原点上方的微偏移处，避免 Z-fighting。

**蓝图 API：**

| 函数 | 说明 |
|---|---|
| `SetupPlane()` | 从 `GridManager` 读取网格参数，配置 Plane 的位置、缩放和动态材质 |
| `UpdateMaterialParams()` | 从当前 `GridManager` 状态和属性值刷新所有材质参数 |
| `SetGridVisible(bool)` | 切换 Plane 可见性 |

#### AGridVisualizer

使用 `ULineBatchComponent::DrawLine()` 逐条绘制网格线。出于性能考虑已被 `AGridPlaneVisualizer` 替代，但仍保留作为备选渲染器。

---

### 2.10 道路预算系统

#### 实现状态：✅ 已实现

`GridManager::RoadBudget` 追踪**共享道路预算**池。玩家放置和 L-system 生长都从同一池中消费。

**预算流程：**
1. `CityFlowGameMode::BeginPlay()` 调用 `GridManager::SetRoadBudget(TotalRoadBudget)`。
2. `GridManager::OccupyCell()` 对 `ECellType::Road` 递减 `RoadBudget`；预算耗尽时返回 `false`。
3. `LSystemManager::StartGenerate()` 从 `GridManager::GetRemainingBudget()` 读取当前预算；每次 `ProcessGrowthStep()` 重新同步。
4. GameMode 对外暴露 `PlayerBudget` / `LSystemBudget` 用于 UI 展示。

| 方法 | 描述 |
|---|---|
| `SetRoadBudget(int32)` | 设置绝对预算值 |
| `GetRemainingBudget()` | 返回当前剩余 |
| `ConsumeRoadBudget(int32)` | 尝试扣除 |
| `AddRoadBudget(int32)` | 增加预算（调试/作弊） |

**ClearCell 返还：** `ClearCell()` 在清理的格子为 `ECellType::Road` 时返还 +1 到 `RoadBudget`，与 `OccupyCell` 保持预算对称。这意味着删除路块会返还预算，L-system 耗尽的预算可通过移除道路来补充。

---

### 2.11 GameMode 状态机

#### 实现状态：✅ 已实现 — v0.17 标题预览与随机规划流程

`ACityFlowGameMode` 通过 `ECityFlowGamePhase` 管理游戏生命周期：

| 阶段 | 转换 | 动作 |
|---|---|---|
| **None** → **Planning** | `StartNewGame()`（普通开始）或 `StartRandomPlanningGame()`（Random Mode / 结算重开） | 初始化网格、生成建筑、设置预算；随机规划局还会随机化 seed、网格尺寸、建筑数量和道路预算 |
| **Planning** → **Simulating** | `StartSimulationPhase()`（UI/Cheat） | 锁定道路放置、启动 VehicleManager 生成 + ScoringManager、启动模拟计时器 |
| **Simulating** → **Evaluation** | 计时器到期或 `EndSimulationPhase()` | 停止生成、结算分数、广播事件 |
| **Evaluation** → **Planning** | `RestartPlanningPhase()` | 清除车辆、重置预算、重新开放放置 |
| **任意** → **None** | `ReturnToMainMenu()`（HUD） | 停止计时器、销毁所有已放置 Actor、重置网格、中止 L-system、清空阶段 |

`BeginPlay()` 现在仅设置预算——场景创建**推迟**到 `StartNewGame()`，由 HUD 在玩家点击主菜单 "开始游戏" 时触发。

#### 自动标题预览对局

`StartAutomatedRandomMatch(bool bAsMenuPreview)` 用于创建标题界面的自动背景对局。它复用正式游戏的场景初始化路径，随机化场景参数，生成河流/景观/建筑，触发 L-system 道路生成，并在 L-system 完成后自动进入模拟阶段。如果该局被标记为主菜单预览，HUD 会在模拟结束后跳过结算界面并立即启动下一局自动预览。

#### 随机规划开局

`StartRandomPlanningGame()` 是玩家点击 Random Mode 时使用的流程。它使用与自动预览相同的随机场景参数 helper，但只生成景观和建筑，切换到 Planning 后由玩家自行铺设道路、触发 L-system 和开始模拟。

**新增 API：**

| 方法 | 描述 |
|---|---|
| `StartNewGame()` | 初始化默认场景并切换到 Planning。守卫：仅允许从 `None` 阶段调用。 |
| `StartAutomatedRandomMatch(bool bAsMenuPreview)` | 创建标题界面背景用随机自动对局；自动生成道路，并在 L-system 完成后启动模拟。 |
| `StartRandomPlanningGame()` | 创建玩家可操作的随机 Planning 开局；只生成景观/建筑，不自动生成道路或启动模拟。 |
| `ReturnToMainMenu()` | 完全清理：停止生成/车辆/计分/计时器，通过 `TActorIterator` 销毁所有 `AGridPlaceableActor`，重新初始化网格，调用 `LSystemManager::AbortGeneration()`，回到 `None` 阶段。 |

**已从 GameMode 移除：** `GameWidgetClass` 和 `GameWidgetInstance` — HUD 是唯一 Widget 生命周期所有者。

**蓝图可配置属性：**
- `BuildingDataAsset` — `UBuildingDataAsset*` 用于加权建筑生成（主路径；回退：`OriginBuildingClass`/`DestinationBuildingClass`）
- `VehicleDataAsset` — `UVehicleDataAsset*` 用于加权车辆生成（主路径；回退：`DeveloperSettings::DefaultVehicleDataAsset`）
- `OriginBuildingClass` / `DestinationBuildingClass` — 建筑蓝图类（旧版回退）
- `RoadTileClass` — 道路地块蓝图类
- `TotalRoadBudget`、`LSystemBudgetShare` — 预算分配
- `SimulationDuration`、`DefaultBuildingCount`、`DefaultGridWidth/Height/CellSize`
- `DrivingSide` — `ECityFlowDrivingSide`（右舵/左舵）
- `LaneOffsetFactor` — float（0.0~0.45，默认 0.2）
- `bRandomizeAutoMatchParameters`、`AutoMatchGridWidthRange`、`AutoMatchGridHeightRange`、`AutoMatchBuildingCountRange`、`AutoMatchRoadBudgetRange` — 标题预览对局和 Random Mode 规划开局共用的随机场景参数范围

**事件：** `OnGamePhaseChanged`、`OnPlanningPhaseEnd`、`OnSimulationPhaseEnd`

---

### 2.12 UI 系统

#### 实现状态：✅ 已实现 — 主菜单子页面、音频设置、本地化、结算与倒计时

CityFlow 的 UI 由 **ACityFlowHUD** 作为唯一 Widget 生命周期所有者集中管理。Widget 遵循主菜单优先的四状态流程。

#### Widget 生命周期

```
StartWidget (主菜单)
  ├─ ShowStartWidget → GameMode::StartAutomatedRandomMatch(true) 作为动态标题背景
  ├─ Btn_RandomMode → HUD::ShowGameWidgetRandom() → GameMode::StartRandomPlanningGame() + EnablePlacement
  ├─ Btn_Tutorial → TutorialWidget → Btn_Back → StartWidget
  ├─ Btn_Settings → SettingsWidget → Btn_Back → StartWidget
  ├─ Btn_QuitGame → 退出
  ↓
GameWidget (规划/模拟 HUD 覆盖层)
  ├─ [Planning] Btn_TriggerLSystem / Btn_StartSimulation
  ├─ [Simulating] Btn_RestartPlanning (回到 Planning)
  ├─ Esc → HUD::TogglePause()
  ├─ Txt_Countdown: MM:SS 倒计时（模拟阶段）
  ↓
PauseWidget (Overlay, ZOrder=100)
  ├─ Btn_Resume → HUD::HidePauseOverlay()
  └─ Btn_ReturnToMain → HUD::ReturnToMainMenu() → StartWidget
  ↓
EvaluationWidget (结算)
  ├─ Btn_BackToMain → HUD::HandleEvaluationReturn() → StartWidget
  └─ Btn_Restart → HUD::HandleRestartClicked() → ShowGameWidgetRandom() → 随机 Planning 开局
```

**ACityFlowHUD** — 中央 Widget 管理器：
- `BeginPlay()` 显示 `StartWidget`（主菜单）；监听 `GameMode::OnSimulationPhaseEnd` 自动显示 `EvaluationWidget`。
- `ShowStartWidget()` 关闭放置，开启主菜单摄像机 yaw 旋转，并在 `bEnableMainMenuPreviewMatch` 为 true 时启动自动随机预览对局。
- 如果主菜单预览模拟结束，`HandleSimulationEnded()` 会启动下一局预览，而不是显示结算 Widget。
- `ShowGameWidgetRandom()` 通过 `StartRandomPlanningGame()` 启动随机 Planning 对局，关闭标题摄像机旋转，并开启放置。
- `ShowTutorialWidget()` / `ShowSettingsWidget()` 会替换 StartWidget，但不会销毁正在运行的菜单预览；两者通过共享返回处理器回到标题界面。
- HUD 启动时先创建配置好的 Settings Widget，在显示菜单前调用 `LoadAndApplySettings()`，随后通过显式 SoundClass Override 播放配置好的循环背景音乐。
- `TogglePause()` / `ShowPauseOverlay()` / `HidePauseOverlay()` — 暂停用 `FInputModeUIOnly`，恢复用 `FInputModeGameAndUI`。
- `ReturnToMainMenu()` — 蓝图可调用；清理后回到 `StartWidget`。
- `HandleReturnToMainClicked()` — 暂停 → `GameMode::ReturnToMainMenu()` → `ShowStartWidget()`。
- `HandleEvaluationReturn()` — 结算 → `GameMode::ReturnToMainMenu()` → `ShowStartWidget()`。

**蓝图可配置 Widget 类（在 HUD 上）：**

| 属性 | 类型 | 用途 |
|---|---|---|
| `StartWidgetClass` | `TSubclassOf<UCityFlowStartWidget>` | 主菜单 Widget |
| `GameWidgetClass` | `TSubclassOf<UCityFlowGameWidget>` | 规划/模拟 HUD 覆盖层 |
| `PauseWidgetClass` | `TSubclassOf<UCityFlowPauseWidget>` | 暂停菜单覆盖层 |
| `EvaluationWidgetClass` | `TSubclassOf<UCityFlowEvaluationWidget>` | 结算界面 |
| `TutorialWidgetClass` | `TSubclassOf<UCityFlowTutorialWidget>` | 教程条目浏览器 |
| `SettingsWidgetClass` | `TSubclassOf<UCityFlowSettingsWidget>` | 音频/语言设置 |
| `bEnableMainMenuPreviewMatch` | `bool` | 是否在标题界面启用自动随机背景模拟 |
| `BackgroundMusic` | `USoundBase*` | 循环播放的标题/游戏背景音乐 |
| `BackgroundMusicSoundClass` | `USoundClass*` | 显式音乐路由，通常为主类下的 `SC_Music` |
| `BackgroundMusicVolumeMultiplier` | `float` | 主音量控制前的单曲增益 |

#### CityFlowEvaluationWidget

`UCityFlowEvaluationWidget` 是模拟结束后显示的结算界面，展示所有关键统计数据并提供两个操作。

**展示数据（从 `ScoringManager` + `GameMode` 获取）：**

| 字段 | 数据来源 | 描述 |
|---|---|---|
| 总分 | `ScoringManager::GetTotalScore()` | 到达分 - 拥堵罚分 + 全连通奖励 |
| 到达数 | `ScoringManager::GetArrivalCount()` | 成功到达终点的车辆数 |
| 拥堵罚分 | `ScoringManager::GetCongestionPenalty()` | 总罚分 |
| 最高分 | 静态 `GlobalHighScore` | 本进程内所有局最高分 |
| 模拟时间 | `SimulationDuration - GameMode::GetSimulationTimeRemaining()` | 已用时间，`MM:SS` 格式 |

**BindWidget 控件：**
- `Txt_TotalScore` — 总分展示
- `Txt_Arrivals`、`Txt_Penalty`、`Txt_HighScore`、`Txt_SimulationTime` — BindWidgetOptional 详细行
- `Btn_BackToMain` → 广播 `OnBackToMainClicked`（HUD → `HandleEvaluationReturn` → 主菜单）
- `Btn_Restart` → 广播 `OnRestartClicked`（HUD → `HandleRestartClicked` → `ShowGameWidgetRandom()` → 随机 Planning 开局）

**公开 API：**
| 方法 | 描述 |
|---|---|
| `Populate(TotalScore, Arrivals, Penalty, ElapsedTime)` | 设置全部数据并刷新 UI；自动更新 `GlobalHighScore` |

HUD 的 `ShowEvaluationWidget()` 从 `ScoringManager` 和 `GameMode` 读取数据后调用 `Populate()`。

**v0.18 结算报告 UI 更新：** HUD 现在调用 `PopulateFromBreakdown(ScoringManager::GetScoreBreakdown())`，使结算界面直接读取完整的 `FCityFlowScoreBreakdown`。

**新增可选控件与自动生成行：**
- `Txt_TotalScore` 改为 `BindWidgetOptional`；如果蓝图未绑定该控件，最终分行会被跳过，不会报错。
- `ScoreReportPanel` 是可选 `VerticalBox`。存在时，C++ 会自动生成左右两列的报告行：左侧 description `TextBlock` 左对齐，右侧 value `TextBlock` 右对齐。
- 自动生成的文本会加黑色 outline，提高可读性。
- 仍支持手动绑定细分行：`Txt_RawScore`、`Txt_ConnectedBuildings`、`Txt_LargestConnectedNetwork`、`Txt_BudgetUsed`、`Txt_EstimatedMinimumRoadNeed`、`Txt_Deaths`、`Txt_ArrivalRate`、`Txt_AverageCellTravelTime`、`Txt_ConnectivityScore`、`Txt_TrafficOutcomeScore`、`Txt_TravelEfficiencyScore`、`Txt_BudgetEfficiencyScore`、`Txt_RuntimeScore`、`Txt_MapDifficultyMultiplier`。
- `Txt_RuntimeScore` 显示模拟阶段已运行时间（`MM:SS`），不是 GDD 的 runtime score 分项。

**逐项数字动画：**
- `BuildAnimatedScoreLines()` 构建报告行队列。
- `NativeTick()` 每次只推进当前行；当前数字到达目标值后，下一行才显示并开始滚动。
- `NumberRollDuration`、`LineRevealDelay` 和 `bAnimateScoreReport` 可在 Widget 上配置。

**基于图片的星级评级：**
- `FilledStarTexture` 使用玩家提供的点亮星星图片。
- `EmptyStarTexture` 可选；未设置时，空星使用同一张图片并套用 `EmptyStarOpacity`。
- `StarRatingPanel` 是可选 `HorizontalBox`。如果未绑定但存在 `ScoreReportPanel`，C++ 会自动生成 `Rating:` 行和 3 个 `UImage`。
- `CalculateStarRating()` 当前按最终分阈值计算：350 / 600 / 800 分分别对应一星 / 二星 / 三星。

#### CityFlowStartWidget

主菜单 Widget，使用 `BindWidget` 控件：
- `Btn_RandomMode` → 广播 `OnRandomModeClicked`（HUD 监听 → `HandleRandomModeClicked` → `ShowGameWidgetRandom()` — 只生成随机景观/建筑并进入 Planning，然后开启放置功能）
- `Btn_Tutorial` → 广播 `OnTutorialClicked`（HUD → `ShowTutorialWidget()`）
- `Btn_Settings` → 广播 `OnSettingsClicked`（HUD → `ShowSettingsWidget()`）
- `Btn_QuitGame` → 广播 `OnQuitGameClicked`
- `Txt_Title`、`Txt_Version`（BindWidgetOptional）— 展示文本

旧 `Btn_StartGame` 已不属于当前流程；原生基类会为向后兼容自动折叠同名旧控件。`ShowGameWidgetRandom()` 使用 `FInputModeGameAndUI` 并配置 `SetHideCursorDuringCapture(false)`，防止拖拽放置时光标消失。

#### Tutorial 数据与 Widget

- `UCityFlowTutorialDataAsset` 按顺序保存 `FCityFlowTutorialEntry`：稳定 `Id`、可本地化 `FText Title`、可多行且可本地化的 `FText Body`，以及可选软引用 `UTexture2D` 图片。
- `UCityFlowTutorialWidget` 可选绑定 `TutorialList`、`Txt_TutorialTitle`、`Txt_TutorialBody`、`Img_Tutorial` 和 `Btn_Back`。
- `bBuildDefaultEntryButtons=true` 时，C++ 自动创建左侧按钮与选择代理；蓝图可关闭该选项并实现 `OnTutorialListRebuilt`，自行创建调用 `SelectTutorial(Index)` 的自定义条目。
- 选择条目后更新右侧标题/正文并同步加载可选图片；未配置图片时自动折叠图片控件。

#### 设置、音频路由与持久化

- `UCityFlowSettingsWidget` 可选绑定 `Sld_SoundVolume`、`Sld_SFXVolume`、`Cmb_Language` 和 `Btn_Back`。
- 设置保存在 `UCityFlowMenuSettingsSaveGame` 的 `CityFlowMenuSettings` 存档槽中，并在主菜单显示前加载应用。
- 运行时音频使用 `UGameplayStatics::PushSoundMixModifier` 和 `SetSoundMixClassOverride`：主音量 Slider 控制配置的主 `SoundClass`，SFX Slider 控制 `SFXSoundClass` 及其子类。
- 音频资产必须配置到项目层级（`SC_Master` → `SC_Music`、`SC_SFX` 及可选子类）；声音的播放位置或 API 本身不会自动把它分类为 SFX。
- 语言选择通过 `UKismetInternationalizationLibrary::SetCurrentCulture(CultureCode, true)` 切换可配置的 `en` / `zh-Hans` Culture，并在切换后重建语言选项。

#### 原生文本本地化

- `.cpp` 中玩家可见静态文本使用 `LOCTEXT`，头文件默认值使用 `NSLOCTEXT`。
- 动态数值使用 `FText::Format` 和支持 Culture 的 `FText::AsNumber`；玩家 UI 不使用 `FText::FromString`。
- Tutorial 标题/正文保持资产 `FText`，由资产本地化步骤收集。
- `TEXT()` 仅保留给不可本地化的标识符、资产/配置键、Culture Code、格式参数名和开发日志。
- 独立源码 GatherText 验证可以无命名空间/Key 冲突地提取全部条目。

#### CityFlowPauseWidget

暂停覆盖 Widget，使用 `BindWidget` 控件：
- `Btn_Resume` → 广播 `OnResumeClicked`（HUD 监听 → `HidePauseOverlay()`）
- `Btn_ReturnToMain` → 广播 `OnReturnToMainClicked`（HUD 监听 → `ReturnToMainMenu()`）

#### CityFlowGameWidget（规划 / 模拟覆盖层）

`UUserWidget` C++ 基类，使用 `BindWidget` 绑定 UMG 控件：
- **绑定控件：** `Btn_TriggerLSystem`、`Btn_StartSimulation`、`Btn_RestartPlanning`、`Txt_Phase`、`Txt_Budget`、`Txt_Score`、`Txt_Countdown`（BindWidgetOptional）、`PopupLayer`（BindWidgetOptional `CanvasPanel`）、`BuildingMarkerLayer`（BindWidgetOptional `CanvasPanel`）
- **按钮自动绑定：** `NativeConstruct()` 通过 `AddDynamic` 绑定 `OnClicked`（回调必须有 `UFUNCTION()` 宏，`BindUFunction` 需要）；`NativeDestruct()` 清理。
- **按钮显隐：** `UpdateButtonStates(Phase)`：
  - Planning：`Btn_TriggerLSystem` + `Btn_StartSimulation` 可见
  - Simulating：`Btn_RestartPlanning` 可见
  - 其他：全部隐藏
- **放置开关：** `OnStartSimulationClicked` 调用 `PC->DisablePlacement()`；`OnRestartPlanningClicked` 调用 `PC->EnablePlacement()`。
- **自动更新文本：** `HandleGamePhaseChanged`、`HandleScoreChanged`、`HandleLSystemStep` 在 C++ 中直接更新 `Txt_*`。预算通过 `HandleCellChanged` 绑定到 `OnCellChanged`，从 `GridManager::GetRemainingBudget()` 读取，确保每次放置/删除实时刷新。
- **倒计时：** 阶段切换到 `Simulating` 时，`StartCountdown()` 读取 `SimulationDuration` 并启动每秒循环的 `TickCountdown()`（标记为 `UFUNCTION()`）。每次 Tick 递减 `CountdownSeconds` 并以 `MM:SS` 格式更新 `Txt_Countdown`。倒计时归零或离开 Simulating 阶段时停止。
- **BlueprintImplementableEvents：** `OnPhaseChanged_BP`、`OnScoreChanged_BP`、`OnBudgetChanged_BP`、`OnSimulationTick_BP`、`OnEvaluation_BP`、`OnLSystemStep_BP`、`OnLSystemFinished_BP`。
- **委托绑定（`NativeConstruct()` 中）：** `GameMode::OnGamePhaseChanged`、`ScoringManager::OnScoreChanged`、`ScoringManager::OnScorePopupRequested`、`LSystemManager::OnGenerationStep`、`LSystemManager::OnGenerationFinished`、`GridManager::OnCellChanged`。

#### 建筑位置 Marker 反馈

建筑位置标记使用由 `UCityFlowGameWidget` 管理的**屏幕空间 UMG** 实现。

- `UCityFlowGameWidget` 通过 `GridManager::GetCellsOfType(ECellType::Building)` 收集已放置的 `ABuilding`，并用 `BuildingID` 对多格建筑去重。
- Marker 创建使用 `BuildingMarkerWidgetClass`；如果未指定蓝图类，则原生 `UBuildingMarkerWidget` 会显示简单文本 fallback。
- 如果存在可选的 `BuildingMarkerLayer`，Marker 会加入该 CanvasPanel；否则回退到 `AddToViewport(15)`。
- `OnCellChanged` 和阶段切换只会把 Marker 列表标记为 dirty；实际重建在 `NativeTick()` 中合并执行，避免随机场景连续放置大量建筑格时反复重建 UI。
- 每帧将 `Building->GetActorLocation() + BuildingMarkerWorldOffset` 通过 `ProjectWorldLocationToWidgetPosition()` 投影到屏幕。屏幕内建筑在投影位置显示普通 marker。
- 屏幕外或位于摄像机背后的建筑，会把 marker 钉到距离最近的屏幕边缘内侧（由 `BuildingMarkerEdgePadding` 控制）；marker 切换为屏幕外状态，并旋转指向建筑方向。
- 显示时机由 `bShowBuildingMarkers`、`bShowBuildingMarkersInPlanning` 和 `bShowBuildingMarkersInSimulation` 控制。

#### 分数 Popup 反馈

分数变化反馈使用**屏幕空间 UMG**，不再使用世界空间 `WidgetComponent`。

- 当 `ScoringManager::OnScorePopupRequested(WorldLocation, DeltaScore)` 触发时，`UCityFlowGameWidget` 创建 `UScorePopupWidget`。
- 若可选 `PopupLayer`（`CanvasPanel`）存在，popup 加入该层；否则回退到 `AddToViewport(20)`。
- `UScorePopupWidget` 保存世界锚点，并在生命周期内每帧调用 `ProjectWorldLocationToWidgetPosition()`，让 popup 在视觉上贴近车辆/死亡位置，同时在 UI 层渲染，避免场景遮挡和朝向摄像机问题。
- Widget 执行屏幕空间上升偏移、透明度淡出和轻微缩放回落，结束后从父级移除。
- `ScorePopupWidgetClass`、`PositivePopupColor`、`NegativePopupColor` 在 `UCityFlowGameWidget` 上可配置；未指定蓝图 Widget 时，原生 `STextBlock` fallback 仍可显示。

#### 所有权

HUD 是**唯一的 Widget 生命周期所有者** — GameMode 不再创建 Widget。GameMode 的 `GameWidgetClass` 和 `GameWidgetInstance` 已被移除。BP GameMode 原来的 "Game Widget Class" 值应迁移到 BP HUD 的 `GameWidgetClass`。

---

### 2.13 调试基础设施

#### 控制台命令（CityFlowCheatExtension）

所有命令以 `CF_` 为前缀，通过控制台（~）访问：

| 命令 | 描述 |
|---|---|
| `CF_StartSimulation` | 触发模拟阶段 |
| `CF_EndSimulation` | 提前结束模拟 |
| `CF_RestartPlanning` | 返回规划阶段 |
| `CF_TriggerLSystem` | 手动触发 L-system 生长 |
| `CF_SpawnVehicle` | 生成单辆测试车辆 |
| `CF_ClearVehicles` | 销毁所有车辆 |
| `CF_TogglePathDebug` | 切换路径线绘制 |
| `CF_ToggleIntersectionDebug` | 切换交叉口框绘制 |
| `CF_ToggleCongestionDebug` | 切换拥堵框绘制 |
| `CF_ToggleVehicleAbilityDebug` | 切换暴走/瞬移车辆能力 screen debug message |
| `CF_SetBudget N` | 设置绝对预算 |
| `CF_AddBudget N` | 增加预算 |
| `CF_ShowGridStats` | 打印网格统计信息 |
| `CF_ShowVehicleStats` | 打印车辆列表和状态 |
| `CF_ShowScoreStats` | 打印分数明细 |
| `CF_SetSimulationSpeed X` | 设置时间膨胀 |

#### 可视化调试（DeveloperSettings 开关）
- `bDebugDrawPaths` — 绘制车辆路径线 + 航点
- `bDebugDrawCongestion` — 在拥堵格上绘制红色框
- `bDebugDrawIntersections` — 在交叉口上绘制橙/红色框，并控制交叉口锁定/进入/离开的 screen debug message
- `bDebugVehicleAbilities` — 控制暴走和瞬移车辆能力触发时的 screen debug message；默认 false

#### DeveloperSettings（Config=Game）
`UCityFlowDeveloperSettings` 默认所有游戏参数，通过 项目设置 → CityFlow 进行编辑器内配置。

---

### 2.14 环境景观装饰与草地覆盖

`UCityFlowLandscapeDecorationManager` 是一个 `UWorldSubsystem`，负责在规划阶段初始化时生成运行时环境景观。它拥有一个在编辑器中标记为 `CityFlowLandscapeDecorations` 的临时根 Actor，并通过 `UHierarchicalInstancedStaticMeshComponent` 生成装饰网格，使树木、岩石和草实例可以批量渲染，而不需要为每个装饰物生成独立 Actor。实际 UObject 名称交由 Unreal 自动生成唯一名，避免 Random Mode 或标题预览快速重生成时，旧的 pending-destroy Actor 仍占用固定名称而导致崩溃。

#### 装饰生命周期

- `ACityFlowGameMode::InitializeDefaultScene()` 在网格初始化和河流 mask 生成之后、默认建筑生成之前触发景观装饰生成。
- `ClearDecorations()` 在返回主菜单或重新生成场景时销毁临时根 Actor，并清空所有按格登记的实例记录。
- 道路和建筑放置通过网格事件（`OnCellChanged`、`OnGridPlaced`）被景观管理器感知，使被占用格能够清理对应的景观实例。
- 实例清理使用逻辑实例记录（`InstanceId`、`TWeakObjectPtr<UHierarchicalInstancedStaticMeshComponent>`、`bAlive`），而不是硬引用 Actor。被移除的 HISM 实例通过把 transform scale 设为 0 隐藏，避免 HISM index 重排，也避免一个大型实例跨多个格子时被重复删除。

#### 草地覆盖采样

草地覆盖通过 `UCityFlowLandscapeDecorationSettings::GrassCoverage` 配置：

| 属性 | 用途 |
|---|---|
| `GroundColorTexture` | CPU 侧采样用颜色贴图，预期与 Landscape 草地材质使用的颜色贴图一致。 |
| `MaterialTile` / `MaterialOffset` | 世界坐标到 UV 的转换参数，预期与 Landscape 材质的纹理平铺参数一致。 |
| `DensityPerCell` | 每个合格网格内的随机候选采样次数。 |
| `GreenRatioMin` | 硬阈值；`G/R < GreenRatioMin` 的采样点绝不生成草。 |
| `GreenRatioPivot` | 满密度目标比例；`GreenRatioMin` 到该值之间使用较陡的概率曲线。 |
| `DryGrassRatio` | 非硬剔除干地区域的可选最低概率；需要硬剔除干地时通常保持为 `0`。 |

每个合格空格内会随机采样候选位置。管理器用下列公式把世界坐标转换为贴图 UV：

```cpp
U = WorldLocation.X * MaterialTile.X + MaterialOffset.X;
V = WorldLocation.Y * MaterialTile.Y + MaterialOffset.Y;
```

采样像素的 `G/R` 比例驱动生成概率。运行日志会输出 `RatioObserved=(Min, Avg, Max)`、`BelowMin`、`Transition` 和 `Full` 计数，用于在 PIE 输出中诊断材质平铺和阈值调参是否有效。

#### 当前开放问题

草地稀疏度的视觉区分目前仍不明显，即使采样日志已经显示颜色比例存在区分。已观察到的 PIE 诊断示例为 `RatioObserved=(0.674, 0.981, 1.202)`，且 `BelowMin` 与 `Full` 计数存在明显差异，这说明 CPU 侧贴图采样路径确实读取到了不同地表颜色。剩余问题更可能位于视觉密度映射阶段，而不是基础颜色采样阶段。后续候选排查方向：

- 草模型缩放（`UniformScaleRange`）可能过大，导致低密度区域在视觉上仍被填满。
- `DensityPerCell` 相对每个草模型的可见覆盖面积可能仍然偏高。
- 按格独立随机采样可能在干地/过渡边缘分布出足够多的实例，使最终 HISM 结果冲淡了预期的密度对比。
- 后续修复可能需要改为按格累计密度、按 cluster 级别拒绝，或改用 Landscape Grass / foliage 风格的密度图，而不是逐采样点独立生成。

---

## 3. 性能考量

| 关注点 | 策略 |
|---|---|
| **网格规模控制** | 地图大小限制在 **20×20 至 30×30**；道路地块总数有界，避免逻辑遍历开销。 |
| **样条组件优化** | 如 [2.3](#23-道路地块自动变形与样条管理) 所述，复杂交叉口采用实时路径计算，样条组件数量与**车辆数**而非地块数挂钩。 |
| **车辆数量** | 同时存在的车辆控制在 **50 辆以下**，通过生成频率和建筑数量调节。 |
| **A\* 缓存** | 道路图仅在规划阶段变更时重建；模拟期间只读，路径结果可按车辆**缓存**。 |

---

## 4. 使用的库 / 工具

| 工具 / 库 | 用途 |
|---|---|
| **Unreal Engine 5.6** | 核心逻辑以 C++ 实现。 |
| **Enhanced Input** | WASD 移动与放置操作绑定，通过 `UInputAction` + `UInputMappingContext`。 |
| **`USplineComponent`** | 用于简单路段的车辆运动路径。 |
| **`UStaticMeshComponent`** | 道路和建筑视觉；素材来源于引擎内置或免费低多边形资源包。 |
| **`UWorldSubsystem`** | `GridManager` 作为全局可访问的单例子系统。 |
| **自定义 A\*** (Blueprint 实现) | 网格图上的全局路径规划。 |
| **`LineTraceByChannel`** | 基于鼠标的放置交互与预览跟踪。 |
| **Timer / Event 系统** | 生长动画、车辆生成、拥堵检测周期。 |
| **无第三方中间件** | 所有功能均在引擎内构建，最小化依赖风险。 |
