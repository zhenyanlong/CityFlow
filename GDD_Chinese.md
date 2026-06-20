# CityFlow 游戏设计文档

---

## 1. 核心机制

CityFlow 是一款以道路规划与交通模拟为核心的策略游戏，分为**规划阶段**和**模拟阶段**两个互不干扰的环节。

在规划阶段，玩家在一个网格化的城市地图上，使用有限的"主干预算"铺设城市骨架道路，将地图上随机生成的住宅（起点）和商业区（终点）连接起来。玩家可以自由决定主干走向、留出分支接口，并在确认主干后触发 L-system 自动生成支路网络——系统会从主干预留的接口和未被连接的建筑出发，以程序化生长的方式补全街区小路，试图将所有剩余建筑接入路网。玩家可以反复调整主干布局、重新生成支路，直到满意为止。

进入模拟阶段后，道路网络锁定，车辆开始从起点涌出，沿着路网驶向各自的终点。玩家无法再修改道路，只能观察交通流动。车辆会自动寻路、在路口让行、在拥堵路段排队等待。如果路网设计不合理，某些路段会出现明显拥堵，拉低得分；反之，高效的路网能让车辆顺畅到达，带来高分回报。

游戏的核心吸引力在于"规划主干以引导程序化生成"的策略深度——玩家无法直接控制支路的具体形态，但可以通过主干的位置、方向和预留接口，极大地影响 L-system 的生长结果，从而在有限预算下追求最高效率。

### 1.1 辅助毛细道路生成

L-system 是玩家规划的**辅助工具**，而不是替代玩家进行规划。玩家铺设的主干道决定主道路连通分量、可用分支锚点，以及连接各建筑所需的成本。触发生成后，系统会先从分配预算中预留连接计划所需成本，只有多余预算才能用于有机侧向生长。

面向玩家的生成规则如下：

- 只有当建筑的某个出入口与其他已连接建筑属于同一个连续道路分量时，该建筑才算真正接入路网；互不相连的门口短路不会被判定为成功城市网络。
- 系统会尽量复用已有道路，因此结构清晰的主干布局能留下更多预算用于毛细支路。
- 死胡同沿远离主干的方向继续生长，间距合适的直路段可以产生垂直分支。
- 分支偏向最近且有价值的建筑出入口，同时保留受控随机性。
- 旋转建筑使用其实际旋转后的出入口方向。
- 当所有建筑进入同一道路分量、分配预算耗尽或不存在合法连接时，生成结束。

在玩家操作的 Random Mode 中，生成器遵守共享道路预算中配置的 L-system 比例。标题界面的自动预览没有玩家规划环节，因此可以使用全部剩余预算，确保背景模拟从可用路网开始。

### 1.2 主菜单、教程、设置与可访问性

标题界面提供 **Random Mode**、**Tutorial**、**Settings** 和**退出**。菜单背景持续运行自动重新生成的城市对局，直观预览规划与交通系统。

- **Tutorial** 显示可配置的教程条目列表；选择条目后展示可本地化的说明文本和可选图片。
- **Settings** 提供主音量、SFX 音量以及英文/简体中文切换。
- 主菜单背景音乐通过项目 SoundClass 层级播放；SFX 音量影响路由到 SFX 类或其子类的声音。
- 运行时语言切换使用 Unreal Engine 原生 Culture/Localization 系统。C++ 中所有玩家可见文本均使用可本地化 `FText` 资源，教程文本保持资产驱动，便于 Localization Gather。

### 1.3 Random Mode 难度与交通压力

点击 **Random Mode** 后，系统会先显示难度选择界面，再生成本局城市。难度会同时调整四项玩家可见参数：建筑数量、车辆生成间隔、模拟运行时间和道路总预算。

界面使用一块共享参数详情区域：默认显示中等难度；悬停简单、中等或困难按钮时，会在玩家点击确认前预览对应配置的四项数值。

| 难度 | 建筑数量 | 车辆生成间隔 | 运行时间 | 道路预算 | 单建筑预算 |
|---|---:|---:|---:|---:|---:|
| 简单 | 8 | 0.90 秒 | 120 秒 | 220 | 27.5 |
| 中等 | 12 | 0.65 秒 | 180 秒 | 230 | 19.2 |
| 困难 | 16 | 0.45 秒 | 240 秒 | 240 | 15.0 |

高难度的绝对预算会略微增加，使更大的地图仍有完成可能；但建筑规模和交通压力增长得更快，因此每栋建筑可分配到的有效预算逐级降低。

车辆系统会根据难度维持一个目标在途车辆数。每次生成脉冲会在起点未被阻挡且存在有效路线时批量补充空缺，直到达到目标数量，同时受车辆硬上限保护。这样短途车辆抵达后，道路仍能保持明显车流，而不会无限生成车辆。

## 2. 胜负条件

游戏采用计分制，没有传统意义上的"胜利"或"失败"，而是以结算总分衡量玩家的规划水平。计分系统定位为**规划评价报告**，而不是单纯的实时街机分数。模拟过程中，HUD 只需要显示车辆抵达和车辆死亡的即时反馈；完整分数拆解在结算阶段展示。

### 2.1 最终分总览

最终分采用 1000 分基础量表：

```text
FinalScore = round(RawScore * MapDifficultyMultiplier)

RawScore =
  ConnectivityScore
+ TrafficOutcomeScore
+ TravelEfficiencyScore
+ BudgetEfficiencyScore
+ RuntimeScore
```

推荐分类权重：

| 分类 | 权重 | 目的 |
|---|---:|---|
| 连通性 | 300 | 奖励将建筑接入可用城市路网。 |
| 交通结果 | 250 | 奖励车辆抵达，并惩罚车辆死亡。 |
| 通行效率 | 200 | 奖励更顺畅的单格通行时间。 |
| 预算效率 | 150 | 奖励高效使用道路预算，但前提是路网确实可用。 |
| 运行时长 | 100 | 在交通需求被有效处理时，奖励提前完成。 |

### 2.2 连通性分

连通性是最核心的规划指标：

```text
ConnectedRatio = ConnectedBuildingCount / TotalBuildingCount
LargestComponentRatio = LargestConnectedBuildingComponent / TotalBuildingCount
AllConnected = ConnectedBuildingCount == TotalBuildingCount

ConnectivityScore =
  180 * ConnectedRatio^2
+  80 * LargestComponentRatio
+  40 * AllConnected
```

该分项同时评价建筑覆盖率，以及已连接建筑是否属于同一个可用路网。全连通奖励被刻意限制在 40 分，避免"全连上但很低效"的设计压过更干净、更高效的方案。

### 2.3 交通结果分

交通结果评价模拟阶段实际发生的情况：

```text
SpawnedVehicles = ArrivedVehicles + DeadVehicles + ActiveVehiclesAtEnd

ArrivalRate = ArrivedVehicles / max(SpawnedVehicles, 1)
DeathRate = DeadVehicles / max(SpawnedVehicles, 1)

TrafficOutcomeScore =
  180 * ArrivalRate
+  70 * (1 - DeathRate)^2
```

模拟结束时仍在场的车辆也计入分母，避免未完成的交通流虚高抵达率。

### 2.4 通行效率分

通行效率使用成功抵达车辆通过单个网格所需的平均时间：

```text
AverageCellTravelTime = TotalTravelTimeOfArrivedVehicles / TotalCellsTraversedByArrivedVehicles

IdealCellTime = CellSize / AverageVehicleMoveSpeed
AcceptableCellTime = IdealCellTime * 2.5

EfficiencyRatio =
  clamp(
    (AcceptableCellTime - AverageCellTravelTime)
    / (AcceptableCellTime - IdealCellTime),
    0,
    1
  )

TravelEfficiencyScore = 200 * EfficiencyRatio
```

如果没有任何车辆成功抵达，则该项得 0 分。若未来车辆速度差异显著，可以进一步按车辆类型分别计算理论通行时间。

### 2.5 预算效率分

预算效率将实际道路消耗与地图归一化后的最低道路需求估计值比较：

```text
EstimatedMinRoadNeed = MSTLengthBetweenBuildings
UsedBudget = TotalRoadBudget - RemainingBudget

BudgetWasteRatio =
  clamp(
    (UsedBudget - EstimatedMinRoadNeed)
    / max(TotalRoadBudget - EstimatedMinRoadNeed, 1),
    0,
    1
  )

BudgetEfficiencyRatio = 1 - BudgetWasteRatio

BudgetEfficiencyScore =
  150
* BudgetEfficiencyRatio
* ConnectedRatio
* sqrt(ArrivalRate)
```

`EstimatedMinRoadNeed` 可用建筑中心或出入口连接点之间的曼哈顿距离最小生成树近似。预算分会乘以连通性和抵达表现，避免玩家因为单纯少修路而在城市不可用时获得高分。

### 2.6 运行时长分

运行时长只在模拟处理了足够交通需求时奖励提前完成：

```text
ExpectedSpawnedVehicles = SimulationDuration / VehicleSpawnInterval
CompletionRatio = ArrivalRate * (1 - DeathRate)

If SpawnedVehicles < ExpectedSpawnedVehicles * 0.5:
    RuntimeScore = 0
Else:
    TimeRatio = clamp(1 - ElapsedSimulationTime / SimulationDuration, 0, 1)
    RuntimeScore = 100 * TimeRatio * CompletionRatio
```

这可以避免某局因为车辆生成过少或路网无法持续承载需求而过早结束，却反而拿到高时间分。

### 2.7 Random Mode 可比性

Random Mode 中，不同建筑数量、建筑密集程度、道路预算和地图跨度应尽量可比较。因此最终分使用一个小幅地图难度倍率：

```text
MapDifficultyMultiplier = clamp(
  1.0
+ BuildingCountDifficulty
+ SpreadDifficulty
+ BudgetPressureDifficulty,
  0.85,
  1.20
)

BuildingCountDifficulty =
  clamp((TotalBuildingCount - 8) / 8, -0.25, 0.35) * 0.10

SpreadRatio = EstimatedMinRoadNeed / max(TotalBuildingCount, 1)

SpreadDifficulty =
  clamp((SpreadRatio - ReferenceSpreadRatio) / ReferenceSpreadRatio, -0.5, 0.8) * 0.15

BudgetPressure = EstimatedMinRoadNeed / max(TotalRoadBudget, 1)

BudgetPressureDifficulty =
  clamp((BudgetPressure - 0.45) / 0.45, -0.4, 0.8) * 0.15
```

推荐预设：

| 参数 | 数值 |
|---|---:|
| ReferenceBuildingCount | 8 |
| ReferenceSpreadRatio | 6.0 |
| TargetBudgetPressure | 0.45 |
| MapDifficultyMultiplier 范围 | 0.85 - 1.20 |
| AcceptableCellTimeMultiplier | 2.5 |

倍率范围刻意保持较窄，让玩家的规划质量始终比随机地图难度更重要。

### 2.8 结算报告

结算面板应以易读的报告形式展示最终得分：

```text
Final Score

Planning
- Connected Buildings
- Largest Connected Network
- Budget Used
- Estimated Minimum Road Need

Traffic
- Arrivals
- Deaths
- Arrival Rate
- Average Cell Travel Time

Breakdown
- Connectivity
- Traffic Outcome
- Travel Efficiency
- Budget Efficiency
- Runtime
- Map Difficulty Multiplier
```

玩家可以通过反复挑战，优化主干设计与支路生成策略，冲击更高分数和更干净的规划报告。

## 3. 叙事背景（可选）

CityFlow 设定在一个快速扩张的现代城市。市政规划者（玩家）负责设计主干道网络，而城市周边的社区和小型商业区则依靠自主生长的毛细血管路网接入城市动脉。每一次游戏都是一座新城市的诞生，玩家在秩序（主干规划）与涌现（L-system 生长）之间寻找平衡，目标是打造一座交通流畅、人人可达的高效都市。

## 4. 美术风格

采用低多边形风格，道路以简洁的灰色模块呈现，建筑为颜色区分的方形体块（住宅=蓝色系，商业=暖色系）。车辆为小型长方体，色彩鲜艳，便于在路网上追踪流动。整体视觉参考《Mini Motorway》的干净抽象风格，辅以柔和的俯视光照和浅色地面，使路网结构和交通流动成为画面的绝对焦点。选择这一风格的原因在于：低多边形资源制作成本低、视觉可读性高，且能在一月开发周期内达到统一、精美的完成度。
