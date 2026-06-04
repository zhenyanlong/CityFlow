# 车辆拥堵机制重构 — 实现规格文档

## 概述

重构现有的车辆拥堵机制（`AVehicleActor` + `UVehicleManager`），在**保持当前双向车道 T 字/十字路口交叉口锁机制不变**的前提下，增加四项新能力：建筑生成门控、碰撞检测跟车停止、启停速度过渡、同路片速度匹配。

---

## 1. 建筑生成门控 — Req 1

### 需求

如果一辆车尚未完全驶出其出发建筑所在的网格单元，则该建筑不应被选为下一辆车的起点（Origin）。

### 当前行为分析

- `UVehicleManager::SpawnVehicle()` 随机选取 origin building
- 车辆的样条路径以 `StartBuildingCell`（建筑内部格）为起点，第二个点为道路格
- 车辆生成后直接进入 `Moving` 状态并沿样条前进

### 细化方案

在 `CollectOriginDestinations()` 或 `SpawnVehicle()` 的 origin 选取阶段，增加一个过滤：

```cpp
bool IsBuildingBlocked(ABuilding* Building) const;
```

**检查逻辑：**
遍历所有 `ActiveVehicles`，若满足以下条件则建筑被"阻塞"：
- `Vehicle->Origin == Building`
- 车辆当前世界位置经 `GridManager::WorldToGrid()` 后的网格坐标**仍属于该建筑占地范围**

**判断"属于建筑占地范围"：**
```cpp
const FGridVector BuildingAnchor = Building->GetGridPosition();
const FVector2D BSize = Building->GetEffectiveBuildingSize();
const FGridVector LocalPos = VehicleGrid - BuildingAnchor;
// 若 0 <= LocalPos.X < BSize.X && 0 <= LocalPos.Y < BSize.Y，则车辆尚未驶出建筑
```

**影响范围：**
- `UVehicleManager::Tick()` 中选取 origin 时，跳过被阻塞的建筑
- 若所有 origin 均被阻塞，本帧跳过生成（自然排队等待）

### 边界情况

| 情况 | 行为 |
|---|---|
| 建筑有多个 doorway，车辆从 doorway A 出发 | 只要车辆还在建筑占地范围内，整栋建筑都不生成新车 |
| 建筑只有一个 doorway | 同上 |
| 车辆已驶出建筑格但仍在 doorway 道路格上 | 建筑解除阻塞，正常生成 |

---

## 2. 前车碰撞检测 — Req 2 & Req 4（合并设计）

### 需求

- Req 2: 前车停下时，后车应在离前车一定距离的位置停下
- Req 4: 同路片上，后车比前车快（或距离过近）时，通过调整速度与前车保持安全距离

### 统一方案：前向碰撞探测 + 安全距离模型

每个 `Tick`，车辆沿行驶方向执行**球体扫描（SphereSweep）**，检测前方同向车辆。如果探测到前车且距离小于安全距离，则调整目标速度。

**这样做的好处：** Req 2 和 Req 4 使用同一套物理探测机制，区别仅在前车速度是否为 0。Req 2 是 Req 4 的特殊情况。

### 2.1 碰撞通道配置

#### 当前状态

```cpp
// VehicleActor.cpp 构造函数
VehicleMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
```

当前 VehicleMesh **完全禁用碰撞**，需要改为启用查询。

#### 修改方案

**a) VehicleMesh 碰撞设置（C++ 构造函数中修改）：**

```cpp
VehicleMesh->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
VehicleMesh->SetCollisionObjectType(ECC_GameTraceChannel1);  // "Vehicle" Object Channel
VehicleMesh->SetCollisionResponseToAllChannels(ECR_Ignore);
VehicleMesh->SetCollisionResponseToChannel(ECC_GameTraceChannel1, ECR_Overlap);  // Overlap with other Vehicles
```

**b) 蓝图侧配置（假设项目已有 "Vehicle" Object Channel + "Vehicle" Trace Channel 预设）：**

- Collision Preset 设为 `Vehicle-QueryOnly`（用户已提及）
- Object Type: `Vehicle`
- 对 Vehicle 通道: Overlap
- 对其他通道: Ignore

> **注意：** 此项配置最终在蓝图中确认。C++ 构造函数中设置默认值，蓝图可覆盖。

### 2.2 前向探测

#### 探测参数

在 `AVehicleActor` 上新增 Blueprint 可配置参数：

```cpp
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
float ForwardProbeRadius = 50.0f;      // 球体扫描半径

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
float ForwardProbeDistance = 500.0f;   // 最大探测距离

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
float SelfAvoidOffset = 150.0f;        // 扫描起点前移距离（避免探测到自己）
```

**扫描起点：**
```
ProbeStart = VehicleWorldLocation + ForwardDirection * SelfAvoidOffset
ProbeEnd   = ProbeStart + ForwardDirection * ForwardProbeDistance
```

**扫描类型：** `SphereSweepSingleByChannel`，通道 = ECC_GameTraceChannel1 (Vehicle)

#### 探测流程（在 `TickMovementSpline` 中）

```
每帧执行：

1. 执行前向球体扫描
2. 若 HitResult.bBlockingHit == true 或 FHitResult 返回了第一个 Hit Actor:
   - HitActor 不为 nullptr 且是 AVehicleActor 类型
   - HitActor != this（防止自探测）
   - HitDistance = HitResult.Distance（从 ProbeStart 到碰撞点的距离）
3. 计算安全距离:
   SafeDist = Max(SafeDistanceMin, CurrentSpeed * SafeDistanceSeconds)
   // SafeDistanceMin: 最小静态安全距离（如同 100.0f）
   // SafeDistanceSeconds: 速度比例因子（如 0.5s，即 1.5m 对应 3m/s 速度）
4. 若 HitDistance <= SafeDist:
   → 进入 "速度适配" 逻辑（见第 3 节）
5. 若未命中或 HitDistance > SafeDist:
   → 正常行驶（使用当前目标速度）
```

### 2.3 安全距离模型

```cpp
// AVehicleActor 新增属性
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
float SafeDistanceMin = 100.0f;          // 静态最小安全距离（cm）

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle|Collision")
float SafeDistanceSeconds = 0.5f;        // 速度×秒数 = 动态安全距离

// 运行时计算
float GetSafeDistance() const
{
    return FMath::Max(SafeDistanceMin, CurrentSpeed * SafeDistanceSeconds);
}
```

**示例：** `CurrentSpeed = 600 cm/s`，`SafeDistanceSeconds = 0.5` → 安全距离 = `max(100, 600*0.5=300) = 300 cm`

### 2.4 同路片判断（简化处理）

完整判断"两车是否在同一路径段"需要对比 spline 拓扑，实现复杂。这里采用**简化策略：**

> **如果前向探测命中了一辆车，即认为两车在同一路段上。** 原因：探测方向 = 车辆行驶方向，射线/球体沿路径切线射出。如果前车不在同一路段/方向上，探测不会命中。

此简化可覆盖所有实际场景（直道跟车、弯道跟车），无需额外的路径拓扑比对。

### 2.5 与交叉口锁机制的共存

```
车辆 Tick 执行顺序（TickMovementSpline 内部）：

1. 前向碰撞探测 → 计算安全距离 → 若过近则降低目标速度
2. 交叉口锁前瞻检测（已有逻辑，不变）
   - 若交叉口被占用 → WaitingIntersection
   - 速度过渡逻辑在所有状态下均生效（见第 3 节）
```

**交叉口锁优先级：** 如果同时满足"前车过近"和"交叉口被占用"，以交叉口锁为准（`WaitingIntersection` 状态）。

---

## 3. 启停速度过渡 — Req 3

### 需求

所有车辆的启停都应有**快速但平滑**的速度变化，不只是出发点和目的地。

### 当前行为

```cpp
// 当前仅有的加速逻辑（TickMovementSpline）
const float TargetSpeed = MoveSpeed * min(remaining/DecelerationDistance, 1.0);
CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime, Acceleration);
```

只有到终点时才减速，且总是从 0 加速。其他状态切换（等待→恢复）无过渡。

### 细化方案

引入**统一速度目标（TargetSpeed）**模型，所有速度变化都通过 `FInterpConstantTo` 插值：

```cpp
// AVehicleActor 新增属性
UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
float StartAcceleration = 2000.0f;   // 启停过渡加速度（比普通加速更快）

UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Vehicle")
float StartDeceleration = 2500.0f;   // 启停过渡减速度（比加速略快，刹车感）
```

#### 速度状态机

| 触发条件 | TargetSpeed | InterpSpeed | 说明 |
|---|---|---|---|
| 正常行驶、无前车 | `MoveSpeed × SpeedRatio` | `Acceleration`（已有） | 保持现有逻辑，终点减速 |
| 前车距离 < 安全距离 | `FMath::Min(FrontVehicleSpeed, MoveSpeed × SpeedRatio)` | `StartAcceleration` | 快速匹配前车速度 |
| 进入 WaitingIntersection | 0 | `StartDeceleration` | 快速刹车到 0 |
| 进入 WaitingCongestion | 0 | `StartDeceleration` | 快速刹车到 0 |
| 从 Waiting* 恢复到 Moving | `MoveSpeed × SpeedRatio` | `StartAcceleration` | 快速起步 |
| 到达终点 | 0（随 SpeedRatio → 0） | `Acceleration` | 保持现有逻辑 |

#### 实现方式

将当前分散的速度计算逻辑统一到一个函数：

```cpp
float AVehicleActor::ComputeTargetSpeed() const
{
    // 基础速度（含终点减速）
    const float RemainingDist = PathSpline->GetSplineLength() - CurrentSplineDistance;
    const float SpeedRatio = (DecelerationDistance > 0.0f)
        ? FMath::Min(RemainingDist / DecelerationDistance, 1.0f)
        : 1.0f;
    const float BaseTarget = MoveSpeed * SpeedRatio;

    // 前车速度匹配
    if (bFrontVehicleTooClose && FrontVehicle.IsValid())
    {
        return FMath::Min(FrontVehicle->GetCurrentSpeed(), BaseTarget);
    }

    return BaseTarget;
}

float AVehicleActor::GetSpeedInterpRate() const
{
    switch (MovementState)
    {
    case EVehicleMovementState::WaitingIntersection:
    case EVehicleMovementState::WaitingCongestion:
        return StartDeceleration;
    case EVehicleMovementState::Moving:
        // 如果 TargetSpeed > CurrentSpeed 且差距大 → 起步加速
        // 否则用正常 Acceleration
        return (TargetSpeed > CurrentSpeed + 100.0f) ? StartAcceleration : Acceleration;
    default:
        return Acceleration;
    }
}
```

然后 `TickMovementSpline` 统一调用：

```cpp
const float TargetSpeed = ComputeTargetSpeed();
const float InterpRate = GetSpeedInterpRate();
CurrentSpeed = FMath::FInterpConstantTo(CurrentSpeed, TargetSpeed, DeltaTime, InterpRate);
```

---

## 4. 修改清单

### 4.1 AVehicleActor 变更

| 变更项 | 类型 | 说明 |
|---|---|---|
| `VehicleMesh` 碰撞启用 | 修改 | 构造函数 `SetCollisionEnabled(QueryOnly)`，设置 Vehicle Object Channel |
| `ForwardProbeRadius` | 新增属性 | 前向探测球体半径，默认 50 |
| `ForwardProbeDistance` | 新增属性 | 最大探测距离，默认 500 |
| `SelfAvoidOffset` | 新增属性 | 自回避偏移，默认 150 |
| `SafeDistanceMin` | 新增属性 | 最小安全距离，默认 100 |
| `SafeDistanceSeconds` | 新增属性 | 速度比例因子，默认 0.5 |
| `StartAcceleration` | 新增属性 | 启停加速度，默认 2000 |
| `StartDeceleration` | 新增属性 | 启停减速度，默认 2500 |
| `FrontVehicle` | 新增成员 | 当前探测到的最近前车弱引用 |
| `bFrontVehicleTooClose` | 新增成员 | 前车过近标志 |
| `GetCurrentSpeed()` | 新增方法 | 公开当前实际速度（供后车读取） |
| `ComputeTargetSpeed()` | 新增方法 | 统一速度目标计算 |
| `GetSpeedInterpRate()` | 新增方法 | 根据状态返回插值速率 |
| `PerformForwardProbe()` | 新增方法 | 执行前向碰撞探测 |
| `TickMovementSpline()` | 重构 | 整合碰撞探测 + 统一速度模型 |

### 4.2 UVehicleManager 变更

| 变更项 | 类型 | 说明 |
|---|---|---|
| `IsBuildingBlocked(ABuilding*)` | 新增方法 | 检查建筑是否被未驶出车辆阻塞 |
| `CollectOriginDestinations()` | 修改 | 跳过被阻塞的 origin 建筑 |
| `Tick()` spawn 逻辑 | 修改 | origin 选取时同步过滤 |

### 4.3 CityFlowVehicleTypes 变更

| 变更项 | 类型 | 说明 |
|---|---|---|
| `WaitingCongestion` 状态逻辑 | 修改 | 碰撞探测触发进入，启停过渡启用 |

无需新增枚举值。

---

## 5. 不变部分（保障）

以下现有机制**不做任何修改：**

- `USplineComponent` PathSpline 的路径构建（`BuildSplinePath`、`SetSplinePath`）
- 双向车道偏移逻辑（`LaneOffsetFactor`、`DrivingSide`）
- A* 寻路（`FindRoadPath`）
- 交叉口锁系统（`IntersectionLocks`、`IsIntersectionLockedByOther`、`AcquireIntersectionLock`）
- 交叉口锁的方向感知冲突检测（`ArePerpendicular`）
- 拥堵统计（`UpdateCongestion`、`CongestedCells`）
- `WaitingIntersection` 的 `IntersectionWaitTimer` 超时重试
- 车辆生成/销毁生命周期

---

## 6. 风险评估

| 风险 | 等级 | 缓解措施 |
|---|---|---|
| VehicleMesh 碰撞启用后与道路/建筑等产生误碰撞 | 中 | Collision Response 仅对 Vehicle 通道设 Overlap，其他通道 Ignore |
| 球体扫描对性能的影响 | 低 | 每车辆每帧 1 次扫描，车辆数 < 50，开销可控 |
| 碰撞探测可能命中对向车道车辆 | 低 | 对向车辆与当前车辆方向相反，不在探测球体范围内（探测方向 = 行驶方向） |
| 建筑生成门控导致无 origin 可用 | 低 | 合法场景：所有建筑都被当前车辆占用几帧后恢复；非法场景：地图无建筑 → 已有检查 |
| `StartAcceleration`/`StartDeceleration` 过渡太快产生抖动 | 低 | 默认值合理（2000/2500），蓝图可微调；`FInterpConstantTo` 保证无过冲 |

---

## 7. 待确认事项

1. **碰撞通道配置：** 项目中 "Vehicle" Object Channel 和 "Vehicle-QueryOnly" Collision Preset 是否已存在？还是需要新建？请确认后在 C++ 中使用对应的 `ECC_GameTraceChannelX`。

2. **安全距离参数默认值：** `SafeDistanceMin=100`（约 1 个格子）、`SafeDistanceSeconds=0.5` 是否合理？方格 `CellSize` 默认是多少（100? 200?）？我根据 CellSize 调整默认值。

3. **同路片判断简化策略：** 是否接受"前向探测命中即认为同路片"的简化？如需精确比对两车是否在相同 spline segment 上，需要额外实现 spline 分段索引追踪，增加约 30% 复杂度。

4. **`StartAcceleration`/`StartDeceleration` 默认值：** 2000/2500 是否合适？当前 `Acceleration=800`，启停过渡应为正常加速的 2-3 倍才产生"快速但平滑"的效果。

请逐项确认或提出修改意见。
