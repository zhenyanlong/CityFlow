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
| **LSystemManager** | 在规划阶段结束时运行。从主干网络中提取分支起点，执行概率化网格 L-system，自动生成毛细道路网络来连接剩余建筑。包含生长动画控制器。 |
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
       └─ ATestGridPlaceableActor         ← 测试方块
```

#### AGridPlaceableActor（状态管理）

纯状态管理，不包含任何视觉逻辑。提供：

| 功能 | API |
|---|---|
| 状态标志 | `IsPreview()` / `IsPlacedOnGrid()` / `IsPreviewPlacementValid()` |
| 进入预览 | `EnterPreviewState()` → 触发 `OnEnterPreview()`（BlueprintNativeEvent） |
| 进入放置 | `EnterPlacedState()` → 触发 `OnEnterPlaced()` |
| 预览有效性 | `SetPreviewPlacementValid(bool)` → 触发 `OnPreviewValidChanged(bool)`（BlueprintNativeEvent）。跟踪 `bPreviewPlacementValid` 标志。 |
| 网格操作 | `PlaceOnGrid()` / `RemoveFromGrid()` / `CanPlaceAt()` / `SnapToGridPosition()` |
| 放置回调 | `OnPlacedOnGrid()` / `OnRemovedFromGrid()`（BlueprintNativeEvent） |
| 网格反向查找 | `RegisterCells()` 将 `this` 作为 `RoadActor` 传入 `OccupyCell()`，实现从网格单元反向查找 Actor，用于右键删除 |

#### AMeshGridPlaceableActor（视觉层）

添加 `UStaticMeshComponent` 并实现自动材质切换：

| 功能 | 详情 |
|---|---|
| `MeshComponent` | `UStaticMeshComponent` 作为 RootComponent |
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

- 建筑可占据矩形区域，如 **2×2** 或 **2×3**。它们在网格中以多个 `Building` 单元格表示，均属于同一个 `ABuilding` Actor。
- 建筑自动在每条边的中点外侧生成潜在道路接口（**出入口**）；接口坐标位于建筑矩形**向外一格**的位置，方向朝外。

  > **示例：** 对于一个 2×2 的建筑，左侧接口位于 `(MinX-1, CenterY)`，方向朝**右**。

- 每个接口记录其所属建筑和连接状态。
- 当道路单元格恰好放置在接口位置时，建筑自动标记为**已连接**。

---

### 2.5 L-System 分支生成

在规划阶段，铺设主干道路后，玩家可触发 L-system 自动生成**毛细网络**，连接所有尚未接入道路网络的建筑。

#### 起点提取

- 遍历所有主干道路单元格。对于 `ConnectedMask` 中**未置位**且邻居为 `Empty` 的每个方向，将该单元格和方向记录为**"分支生长点"**。
- 此外，所有未连接的建筑接口也作为独立的起始点。

#### 概率化网格 L-System 解释器

递归函数 `GrowBranch(Position, Direction, Budget)` 模拟类似 L-system 的树状扩展。核心逻辑：

1. 向前尝试走一格；检查合法性（边界内、未被占用）。成功后放置道路并消耗 1 单位预算。
2. 计算到最近未连接建筑的**吸引方向**，用于加权分支概率：朝向吸引方向转弯的概率增大。
3. 以概率 `p_branch`（如 `0.6`）产生分支，通过调用自身进行左转和/或右转探索，实现类似 `F → F[+F]F[-F]F` 的效果，但受到吸引目标的引导。
4. 若前进失败，函数回溯并自动尝试其他分支方向。

**终止条件：**

- 预算耗尽。
- 所有建筑已连接。
- 无合法扩展方向。

#### 生长动画

- `FTimerHandle` 每 **0.1 秒**执行一次扩展步骤，逐格展示毛细道路的生成。
- 未完成的道路使用**半透明材质**，完成后切换为不透明。
- 分支连接建筑时触发建筑高亮。

#### 集成

每个生成的道路单元格调用 `GridManager::OccupyCell` 和 `RoadManager::CreateRoadTile`，更新道路图，确保后续车辆寻路正常工作。

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
