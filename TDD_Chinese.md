# CityFlow 简要技术设计文档

---

## 1. 架构概览

CityFlow 采用**阶段分离、组件化**的架构，将游戏流程拆分为**规划阶段**和**模拟阶段**，两者严格隔离，以降低实时耦合的复杂度。

### 核心管理类

核心管理类集中在 `GameMode` 或委派的 Manager 组件中：

| 管理器 | 类型 | 职责 |
|---|---|---|
| **GridManager** | `UWorldSubsystem` | 维护二维逻辑网格。提供网格吸附、放置验证、邻居查询、连接掩码计算和建筑接口注册。**管理共享道路预算** —— 玩家放置和 L-system 生成都从同一个 `RoadBudget` 池中消费。 |
| **LSystemManager** | `UWorldSubsystem` | **可选的**辅助毛细道路生成器。从死胡同和直路段提取分支起点，执行广度优先、吸引偏向的生长算法。与玩家**共享道路预算**。由玩家手动触发（UI 按钮或控制台命令）。 |
| **VehicleManager** | `UWorldSubsystem` + `FTickableGameObject` | 生成并管理所有车辆 Actor。提供道路图上的 **A\* 寻路**，将网格路径转换为世界空间航点运动计划，处理**拥堵检测**（每格车辆数统计）和**交叉口占用**（≥3 向路口的互斥锁）。每帧 Tick。 |
| **ScoringManager** | `UWorldSubsystem` | 在模拟阶段追踪到达数、到达分数和拥堵惩罚。使用定时器周期性扣除拥堵惩罚。结算时计算包含全连通奖励的最终分数。 |
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
     - 成功后：预览 Actor 转为 `EnterPlacedState()`，恢复原始材质并启用碰撞。单元格设为对应类型并计算 `ConnectedMask`。立即生成新的预览 Actor。
4. **删除** — 绑定到 `IA_RemoveItem`（鼠标右键）：
   - **Started：** 重置 `LastRemovedGridPos`，执行首次删除。
   - **Triggered：** 启用**拖拽连续删除**，同样通过 `LastRemovedGridPos` 去重。
   - **Completed：** 重置 `LastRemovedGridPos`。
   - 删除逻辑（`TryRemoveAtCursor`）：射线检测 → `WorldToGrid()` → 从网格中查找 `Cell.RoadActor`（而非碰撞命中）。若该格包含已放置的 `AGridPlaceableActor`，调用 `RemoveFromGrid()` + `Destroy()`。
5. **邻居刷新：** 遍历四个邻居；若任一为道路，重新计算其掩码。

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

- **Actor 缩放抵消：** 建筑 Actor 使用 `SetActorScale3D` 来视觉缩放。程序化顶点以世界单位计算，然后通过 `SetRelativeScale3D(1/S.X, 1/S.Y, 1)` 抵消父级缩放，防止双重变换。
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

---

### 2.5 L-System 分支生成

`ULSystemManager` 是一个 `UWorldSubsystem`，在规划阶段结束时触发。它使用**广度优先迭代队列**自动生成毛细道路网络，连接所有尚未接入道路网络的建筑。

#### 架构

`ULSystemManager` 通过 `GetWorld()->GetSubsystem<ULSystemManager>()` 访问。所有配置均通过蓝图可调用的 Setter 函数完成（不暴露 `UPROPERTY(EditAnywhere)`）。

#### 起点提取

从两个来源收集生长起点：

**A. 死胡同道路格：**
- 遍历所有道路格；对每个死胡同（`ConnectedMask` 仅 1 位置位），记录未连接方向上的生长点。

**B. 直路段（间隔采样）：**
- 识别直路路段（`ConnectedMask` 含 2 个对向位：Up+Down 或 Left+Right）。
- 沿两个轴向遍历该路段，收集完整连续段。
- 若段长 < 3 格，跳过（太短不适合分支）。
- 沿段每 `MinBranchSpacing + 1` 格在垂直方向采样分支点。
- 转角、T 型路口、十字路口**跳过**（已充分连接）。

**C. 未连接建筑出入口：**
- 遍历所有未连接建筑；对每个出入口连接点为 `Empty` 的，从建筑网格边缘格沿出入口朝向添加生长点。

#### 广度优先迭代生长

系统使用**迭代队列**而非递归回溯算法驱动生长：

1. `FLSystemGrowthPoint` 结构体持有网格位置和生长方向。
2. `ProcessGrowthStep()` 通过 `FTimerHandle` 以 `GrowthInterval`（默认 0.1s）间隔调用。
3. 每步从**队首**取出一个点，调用 `TryGrowAt()`。
4. 新的生长点插入**队尾**，在所有活跃分支之间产生广度优先交替。

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

新生长点在插入队列前按吸引分数排序：

```
Score = Lerp(DistScore, AlignScore, AttractionStrength)
  DistScore  = 1 / (1 + 到最近未连接建筑的欧几里得距离)
  AlignScore = dot(归一化建筑方向, 生长方向)，截断 ≥ 0
```

分数高的点在同一批次内优先执行。这在不破坏广度优先保证的前提下将分支引向未连接建筑。

#### 可配置参数（蓝图 Setter）

| Setter | 默认值 | 描述 |
|---|---|---|
| `SetRoadTileClass(TSubclassOf<ARoadTile>)` | — | 分支生成使用的道路地块类 |
| `SetBranchBudget(int32)` | 50 | 最大可放置道路格总数 |
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

- 预算耗尽。
- 所有建筑已连接（提前成功）。
- 队列为空 —— 无更多合法生长方向。

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

**交叉口占用（双向，v0.4）：**
- 交叉口 = 任意 ≥ 3 个连接方向的道路格。
- `TMap<FGridVector, TMap<TObjectPtr<AVehicleActor>, EGridDirection>> IntersectionLocks`：每个交叉口格映射到一组 `(Vehicle, EntryGridDirection)` 对。
- **方向感知冲突检查：** 进入交叉口之前，车辆的进入网格方向（通过 `GridDirectionUtils::DirectionFromWorldVector(VelocityDirection)` 获得）与已有占用者比较：
  - 同向 → 不冲突（同车道同向）
  - 反向 → 不冲突（对向双向车道）
  - 垂直方向 → **冲突**（交叉路径）；车辆进入 `WaitingIntersection`
- **锁获取：** 移动至交叉口格后，车辆注册 `(this, EntryDir)` 到该交叉口的占用者集合。
- **锁释放：** `UpdateIntersectionLocks()` 移除已离开交叉口格或不再活跃的车辆。

**拥堵检测：**
- 每帧 `UpdateCongestion()` 将世界位置映射到网格格。
- 超过 `CongestionThreshold`（默认 3）辆车的格标记为拥堵。
- 拥堵数据可通过 `GetCongestedCells()` 查询。

#### 车辆生成表

`UVehicleDataAsset`（`UPrimaryDataAsset`）作为**车辆类注册表**：

```cpp
USTRUCT()
struct FVehicleSpawnEntry
{
    TSubclassOf<AVehicleActor> VehicleClass;   // AVehicleActor 的蓝图子类
    float SpawnWeight = 1.0f;                  // 相对生成概率
};
```

`UVehicleDataAsset::VehicleEntries` 是一个 `FVehicleSpawnEntry` 数组。`UVehicleManager::CacheSpawnEntries()` 在模拟开始时加载 `DeveloperSettings::DefaultVehicleDataAsset` 引用的 DataAsset，然后每次生成时由 `PickRandomVehicleClass()` 执行加权随机选取。

每个 `AVehicleActor` 子类（如 `BP_Car`、`BP_Truck`）在其蓝图默认值中直接配置自身的 `VehicleMesh`、`MoveSpeed`、`DebugColor` 等属性——无需 DataAsset 驱动属性覆盖。

| 状态 | 描述 |
|---|---|
| `Idle` | 初始或错误状态 |
| `Moving` | 沿样条路径移动 |
| `WaitingCongestion` | 前方车辆过近 |
| `WaitingIntersection` | 等待交叉口锁；超时自动重试 |
| `Arrived` | 到达终点 |

---

### 2.7 起点 / 目的地生成与计分

#### 实现状态：✅ 已实现

#### 建筑生成

`CityFlowGameMode::InitializeDefaultScene()` 委托给 `GridManager::TryPlaceBuildingsRandom()`，使用可配置的 `OriginBuildingClass` / `DestinationBuildingClass` 数量。建筑随机放置带随机旋转，确保不重叠。

#### 车辆生成

`UVehicleManager::Tick()` 以 `SpawnInterval`（默认 5s）间隔生成车辆。每帧随机选取起点和终点，调用 `SpawnVehicle()` 计算 A\* 路径并生成 Actor。

#### 计分机制（UScoringManager）

| 项目 | 规则 |
|---|---|
| **基础到达分** | 每辆车到达 +ArrivalScore（默认 100，可通过 DeveloperSettings 配置） |
| **拥堵惩罚** | 每秒扣除 CongestionPenaltyPerSecond × 拥堵格数（默认 5/格/秒） |
| **全连通奖励** | 结算时所有建筑连通，+FullConnectivityBonus（默认 500） |
| **效率奖励** | 剩余道路预算（未来：按比例奖励） |

模拟开始时 `StartScoring()`，结算时 `StopScoring()`。

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
| Alt + 鼠标视角 | `IA_Alt` + `IA_Look` —— 按住 Alt 时设 `bAltHeld = true`，切换输入模式为 `FInputModeGameOnly()`（捕获鼠标），通过鼠标 delta 驱动 `AddControllerYawInput()`（C++ 仅控制 yaw；pitch 在蓝图中处理）。松开 Alt 恢复 `FInputModeGameAndUI` + 鼠标光标 |
| 滚轮缩放 | `IA_Zoom` 调整 `TargetSpringArmLength`（Clamp 到 [Min, Max]）。蓝图每帧读取此变量以驱动 spring arm 长度插值 |
| 蓝图可配置 | `MoveSpeed`、`LookSensitivity`、`ZoomSpeed`、`MinSpringArmLength`、`MaxSpringArmLength`、`DefaultCameraPitch`、`MinCameraPitch`、`MaxCameraPitch` |
| 摄像机设置 | 在蓝图中处理：`USpringArmComponent` + `UCameraComponent` 作为子组件；spring arm 使用 `bUsePawnControlRotation = true`；角色自动接管 |

**C++ 维护供蓝图使用的关键变量：**

| 变量 | 默认值 | 说明 |
|---|---|---|
| `CameraYaw` | 0 | 当前朝向 yaw —— 蓝图从摄像机更新；`Move()` 从此计算移动方向 |
| `TargetSpringArmLength` | 10000 | Spring arm 目标长度 —— 蓝图读取并向此值插值 |
| `DefaultCameraPitch` | -60 | BeginPlay 时通过 `SetControlRotation` 设置的初始摄像机俯仰角 |
| `MinCameraPitch` | -80 | 最小俯仰角（最俯视） |
| `MaxCameraPitch` | -30 | 最大俯仰角（最平视） |

#### CityFlowPlayerController

| 功能 | 实现 |
|---|---|
| 光标 | `bShowMouseCursor = true`（由 Pawn 管理：Alt 期间隐藏，松开恢复） |
| 预览系统 | `BeginPlay` 时生成预览 Actor；通过 `Tick()` → `GetHitResultUnderCursor()` → `SnapToGrid()` 跟随光标；每帧通过 `SetPreviewPlacementValid()` 更新有效性，然后调用 `UpdatePreviewAppearance()` 让 `ARoadTile` 在预览中显示预测的 mesh |
| 放置 | `IA_PlaceItem`（鼠标左键）→ `Started`/`Triggered`/`Completed` 事件 → `TryPlaceAtCursor()` 辅助函数，通过 `LastPlacedGridPos` 去重实现拖拽连续放置 |
| 删除 | `IA_RemoveItem`（鼠标右键）→ `Started`/`Triggered`/`Completed` 事件 → `TryRemoveAtCursor()` 辅助函数，通过 `LastRemovedGridPos` 去重实现拖拽连续删除。从网格 `Cell.RoadActor` 查找 Actor，不依赖碰撞命中。 |
| 蓝图可配置 | `PlaceableActorClass`（任意 `AGridPlaceableActor` 子类）；`IA_PlaceItem`、`IA_RemoveItem` |

#### 放置开关

`ACityFlowPlayerController` 提供放置开关，用于与其他系统（L-system、模拟）协调：

| API | 描述 |
|---|---|
| `EnablePlacement()` | 恢复光标采样、生成新预览 Actor、显示鼠标光标 |
| `DisablePlacement()` | 停止光标采样、销毁预览 Actor |
| `IsPlacementEnabled()` | 查询当前放置开关状态 |

放置关闭时，`Tick()` 跳过 `UpdatePreviewPosition()`，`TryPlaceAtCursor()` / `TryRemoveAtCursor()` 均为空操作。进入模拟时自动关闭放置，重新规划时自动恢复。

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

---

### 2.11 GameMode 状态机

#### 实现状态：✅ 已实现

`ACityFlowGameMode` 通过 `ECityFlowGamePhase` 管理游戏生命周期：

| 阶段 | 转换 | 动作 |
|---|---|---|
| **None** → **Planning** | `BeginPlay()` | 初始化网格、生成默认建筑、创建 GameWidget、设置预算 |
| **Planning** → **Simulating** | `StartSimulationPhase()` | 锁定道路放置、启动 VehicleManager 生成 + ScoringManager、启动模拟计时器 |
| **Simulating** → **Evaluation** | 计时器到期或 `EndSimulationPhase()` | 停止生成、结算分数、广播事件 |
| **Evaluation** → **Planning** | `RestartPlanningPhase()` | 清除车辆、重置预算、重新开放放置 |

**蓝图可配置属性：**
- `OriginBuildingClass` / `DestinationBuildingClass` — 建筑蓝图类
- `RoadTileClass` — 道路地块蓝图类
- `VehicleClass` — 车辆蓝图类（未来覆写）
- `GameWidgetClass` / `EvaluationWidgetClass` — UMG Widget 类
- `TotalRoadBudget`、`LSystemBudgetShare` — 预算分配
- `SimulationDuration`、`DefaultBuildingCount`、`DefaultGridWidth/Height/CellSize`
- `DrivingSide` — `ECityFlowDrivingSide`（右舵/左舵），控制双向车道行驶侧
- `LaneOffsetFactor` — float（0.0~0.45，默认 0.2），样条路径距道路中心的偏移比例

**事件：** `OnGamePhaseChanged`、`OnPlanningPhaseEnd`、`OnSimulationPhaseEnd`

---

### 2.12 UI 系统

#### 实现状态：✅ 已实现

**CityFlowHUD**（`ACityFlowHUD`）：
- 管理 `GameWidget`（规划/模拟覆盖层）和 `EvaluationWidget`（结算界面）。
- `ShowGameWidget()` / `ShowEvaluationWidget()` 切换可见 Widget。

**CityFlowGameWidget**（`UUserWidget` C++ 基类）：
- 使用 `BindWidget` 元标记自动绑定 UMG 控件——蓝图子类只需放置同名控件，无需手动绑定。
- **绑定的控件：**
  - `Btn_TriggerLSystem`（`UButton`）— 触发 L-system 毛细道路生成
  - `Btn_StartSimulation`（`UButton`）— 启动模拟阶段
  - `Btn_RestartPlanning`（`UButton`）— 返回规划阶段（仅在结算阶段可见）
  - `Txt_Phase`（`UTextBlock`）— 显示当前游戏阶段
  - `Txt_Budget`（`UTextBlock`）— 显示剩余道路预算
  - `Txt_Score`（`UTextBlock`）— 显示当前分数
- **按钮自动绑定：** `NativeConstruct()` 自动绑定所有按钮的 `OnClicked` 事件；`NativeDestruct()` 通过 `RemoveAll` 清理。
- **按钮显隐状态：** `UpdateButtonStates(Phase)` 管理按钮可见性：
  - 规划阶段：`Btn_TriggerLSystem` + `Btn_StartSimulation` 可见，`Btn_RestartPlanning` 隐藏
  - 结算阶段：`Btn_RestartPlanning` 可见，操作按钮隐藏
- **阶段感知的放置开关：** `StartSimulation()` 调用 `PC->DisablePlacement()` 停止放置预览；`RestartPlanning()` 调用 `PC->EnablePlacement()` 恢复。
- **自动更新文本：** `HandleGamePhaseChanged()`、`HandleScoreChanged()`、`HandleLSystemStep()` 在 C++ 中直接更新 `Txt_Phase` / `Txt_Score` / `Txt_Budget`，无需蓝图参与。
- 暴露 `BlueprintImplementableEvent` 回调：`OnPhaseChanged_BP`、`OnScoreChanged_BP`、`OnBudgetChanged_BP`、`OnLSystemStep_BP`、`OnLSystemFinished_BP`、`OnEvaluation_BP`。
- 在 `NativeConstruct()` 中绑定 GameMode/ScoringManager/LSystemManager 委托。

蓝图子类只需放置指定名称的 UMG 控件——所有绑定和逻辑在 C++ 中处理。

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
| `CF_SetBudget N` | 设置绝对预算 |
| `CF_AddBudget N` | 增加预算 |
| `CF_ShowGridStats` | 打印网格统计信息 |
| `CF_ShowVehicleStats` | 打印车辆列表和状态 |
| `CF_ShowScoreStats` | 打印分数明细 |
| `CF_SetSimulationSpeed X` | 设置时间膨胀 |

#### 可视化调试（DeveloperSettings 开关）
- `bDebugDrawPaths` — 绘制车辆路径线 + 航点
- `bDebugDrawCongestion` — 在拥堵格上绘制红色框
- `bDebugDrawIntersections` — 在交叉口上绘制橙/红色框

#### DeveloperSettings（Config=Game）
`UCityFlowDeveloperSettings` 默认所有游戏参数，通过 项目设置 → CityFlow 进行编辑器内配置。

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
