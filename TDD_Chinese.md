# CityFlow 简要技术设计文档

---

## 1. 架构概览

CityFlow 采用**阶段分离、组件化**的架构，将游戏流程拆分为**规划阶段**和**模拟阶段**，两者严格隔离，以降低实时耦合的复杂度。

### 核心管理类

核心管理类集中在 `GameMode` 或委派的 Manager 组件中：

| 管理器 | 职责 |
|---|---|
| **GridManager** | 维护二维逻辑网格，存储每个单元格的类型（`Empty`、`Road`、`Building`）和连接掩码。提供网格吸附、放置验证、邻居查询和建筑接口注册功能。 |
| **RoadManager** | 处理道路 Actor 的创建、销毁和变形。接收来自玩家或 L-system 的放置请求，通过 `GridManager` 更新网格，并触发邻居刷新。 |
| **LSystemManager** | `UWorldSubsystem`，在规划阶段结束时运行。从死胡同和直干路段中提取选择性的分支起点，然后执行广度优先、吸引偏向的迭代生长算法，生成毛细道路网络连接剩余建筑。通过 `FTimerHandle` 以可配置的步长间隔进行生长动画。 |
| **VehicleManager** | 在模拟阶段管理所有车辆 Actor 的生成和生命周期。提供道路图上的 A* 寻路、基于样条的运动、拥堵等待和交叉口占用管理。 |
| **ScoringManager** | 独立计算分数，监听车辆到达事件，定期检测拥堵，统计最终总分。 |

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

#### 内部样条管理（混合策略）

为避免预生成大量样条组件的性能开销，采用**按需生成**的方式：

- **直道 / 转角路段** — 使用单个双向 `USplineComponent`。车辆根据行驶方向选择参数化的起点和终点。
- **复杂交叉口（T 型路口 / 十字路口）** — 不预置样条组件。取而代之的是存储一个 `TMap<FPathKey, TArray<FVector>>` 缓存。当车辆请求转弯路径（如从**下方**进入、从**左侧**离开）时：
  - 若缓存未命中，则实时计算贝塞尔曲线点：
    - 直行通过 → 2 个点
    - 转弯 → 3 个点
  - 结果缓存后返回点数组。

车辆沿点数组插值移动（或按需创建临时轻量样条），离开后即销毁。

> 该策略将样条组件总数从 **O(地块数 × 方向组合数)** 降低到 **O(车辆数)**，大幅减少内存和生成开销。

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

### 2.6 车辆 AI：基于样条的寻路与交通处理

#### 全局寻路

- `GridManager` 根据当前网格构建道路图（每个道路单元格为一个节点；连接方向匹配的相邻单元格之间存在边）。
- **A\* 算法**计算从起点建筑接口单元格到目的地建筑接口单元格的最短节点序列。
- 路径在车辆生成时**计算一次**；若规划阶段道路网络发生变更，受影响的车辆路径会重新计算。

#### 路径转样条运动计划

A\* 节点序列被转换为连续的运动计划：

```
(Tile1, 入口=无, 出口=右) → (Tile2, 入口=左, 出口=上) → …
```

当车辆进入新的 Tile 时，获取其 `(入口, 出口)` 对应的行驶路径：

- 直道路段直接提供样条。
- 交叉口返回点数组。

……随后沿路径开始移动。

#### 拥堵处理

- 车辆使用 `SphereOverlapActors` 检测同路径上的前方车辆。
- 若距离小于 `MinFollowDistance`，车辆减速或停止，保持**安全车距**。

#### 交叉口占用

对于连接数 **≥ 3** 的交叉口 Tile，采用简单的资源锁机制：

- 交叉口维护一个 `bOccupied` 布尔值和**请求队列**。
- 车辆进入前需请求权限：
  - 若 `!bOccupied`，授予权限并将 `bOccupied` 设为 `true`。
  - 车辆完全离开后释放。
- 等待车辆在入口前停止，**不阻塞其他方向**。

---

### 2.7 起点 / 目的地生成与计分

#### 建筑生成

- 游戏开始时，若干多单元格建筑随机放置在地图上，确保不重叠且间距充足：
  - **住宅** = 起点 (Origin)
  - **商业 / 办公** = 目的地 (Destination)
- 额外的建筑对可按间隔（如每 **60 秒**）出现，增加挑战性。

#### 车辆生成

- 模拟阶段中，每个起点建筑以固定频率（如每 **5 秒**）生成一辆车。
- 随机选择一个当前存在的目的地，触发 A\* 寻路。
- 若**无合法路径**存在，车辆不生成，并向玩家发出提示。

#### 计分机制

采用**"累计到达分数 + 拥堵惩罚 + 效率奖励"**模型，鼓励高效的网络设计：

| 项目 | 规则 |
|---|---|
| **基础到达分** | 每辆车到达目的地奖励 **+100** |
| **拥堵惩罚** | 每秒检查每个道路地块；若包含 **> 2 辆车**则计为一个拥堵点。每秒 **−= 5 × 拥堵点数量**。 |
| **效率奖励** | 剩余主干和毛细预算之和 × 系数，加入最终得分。 |
| **全连通奖励** | 模拟结束时若**所有建筑已连接**，额外 **+500** |

---

### 2.8 玩家与摄像机系统

#### CityFlowPawn

基于 `ACharacter` 的子类，配置为俯视角自由飞行控制：

| 功能 | 实现 |
|---|---|
| 移动模式 | `MOVE_Flying`（无重力，全轴向移动） |
| 输入 | `Enhanced Input` → `IA_Move`（Action，ValueType Axis2D） |
| 移动方向 | 通过 `GetControlRotation()` 从摄像机朝向推导 —— WASD 相对于当前镜头视角移动 |
| 蓝图可配置 | `MoveSpeed` |
| 摄像机设置 | 完全在蓝图中处理：添加 `USpringArmComponent` + `UCameraComponent` 作为子组件；角色自动接管并成为视角目标 |

#### CityFlowPlayerController

| 功能 | 实现 |
|---|---|
| 光标 | `bShowMouseCursor = true` |
| 预览系统 | `BeginPlay` 时生成预览 Actor；通过 `Tick()` → `GetHitResultUnderCursor()` → `SnapToGrid()` 跟随光标；每帧通过 `SetPreviewPlacementValid()` 更新有效性，然后调用 `UpdatePreviewAppearance()` 让 `ARoadTile` 在预览中显示预测的 mesh |
| 放置 | `IA_PlaceItem`（鼠标左键）→ `Started`/`Triggered`/`Completed` 事件 → `TryPlaceAtCursor()` 辅助函数，通过 `LastPlacedGridPos` 去重实现拖拽连续放置 |
| 删除 | `IA_RemoveItem`（鼠标右键）→ `Started`/`Triggered`/`Completed` 事件 → `TryRemoveAtCursor()` 辅助函数，通过 `LastRemovedGridPos` 去重实现拖拽连续删除。从网格 `Cell.RoadActor` 查找 Actor，不依赖碰撞命中。 |
| 蓝图可配置 | `PlaceableActorClass`（任意 `AGridPlaceableActor` 子类）；`IA_PlaceItem`、`IA_RemoveItem` |

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
