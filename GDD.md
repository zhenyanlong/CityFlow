# Game Design Document: CityFlow

---

## 1. Core Mechanics

CityFlow is a strategy game centered on road planning and traffic simulation, divided into two independent phases: the **Planning Phase** and the **Simulation Phase**.

During the Planning Phase, the player works on a grid-based city map, using a limited "arterial budget" to lay down the city's skeleton road network, connecting randomly generated residential zones (origins) and commercial zones (destinations). The player freely decides the direction of arterial roads, leaves branch connection points, and after confirming the arterial layout, triggers the L-system to automatically generate the capillary road network — the system starts from the reserved arterial connection points and any unconnected buildings, procedurally growing side streets to fill in the blocks and attempting to connect all remaining buildings to the network. The player may iteratively adjust the arterial layout and re-generate the branch network until satisfied.

Once the Simulation Phase begins, the road network is locked, and vehicles begin pouring out from their origins, traveling along the network toward their respective destinations. The player can no longer modify roads and can only observe traffic flow. Vehicles automatically find paths, yield at intersections, and queue up on congested segments. If the road network is poorly designed, certain segments will experience visible congestion, lowering the score; conversely, an efficient network allows vehicles to arrive smoothly, yielding high score rewards.

The core appeal of the game lies in the strategic depth of "planning arterial roads to guide procedural generation" — the player cannot directly control the specific shape of branch roads, but can greatly influence the L-system's growth results through the position, direction, and reserved connection points of arterial roads, striving for maximum efficiency under a limited budget.

## 2. Win / Lose Conditions

The game uses a scoring system. There is no traditional "victory" or "defeat"; instead, the final total score measures the player's planning ability. The score is designed as a **planning evaluation report** rather than a simple real-time arcade counter. During simulation, the HUD only needs to surface immediate feedback for vehicle arrivals and deaths; the full score breakdown appears at evaluation time.

### 2.1 Final Score Overview

The final score uses a 1000-point base scale:

```text
FinalScore = round(RawScore * MapDifficultyMultiplier)

RawScore =
  ConnectivityScore
+ TrafficOutcomeScore
+ TravelEfficiencyScore
+ BudgetEfficiencyScore
+ RuntimeScore
```

Recommended category weights:

| Category | Weight | Purpose |
|---|---:|---|
| Connectivity | 300 | Rewards connecting buildings into a usable city network. |
| Traffic Outcome | 250 | Rewards vehicles arriving and penalizes vehicle deaths. |
| Travel Efficiency | 200 | Rewards smooth per-cell travel time. |
| Budget Efficiency | 150 | Rewards efficient road spending, but only when the network works. |
| Runtime | 100 | Rewards early completion when traffic demand is actually handled. |

### 2.2 Connectivity Score

Connectivity is the primary planning metric:

```text
ConnectedRatio = ConnectedBuildingCount / TotalBuildingCount
LargestComponentRatio = LargestConnectedBuildingComponent / TotalBuildingCount
AllConnected = ConnectedBuildingCount == TotalBuildingCount

ConnectivityScore =
  180 * ConnectedRatio^2
+  80 * LargestComponentRatio
+  40 * AllConnected
```

This rewards both broad building coverage and whether connected buildings belong to the same usable road network. The full-connectivity reward is intentionally limited so that a fully connected but inefficient network can still be outscored by a cleaner plan.

### 2.3 Traffic Outcome Score

Traffic outcome evaluates what actually happened during the Simulation Phase:

```text
SpawnedVehicles = ArrivedVehicles + DeadVehicles + ActiveVehiclesAtEnd

ArrivalRate = ArrivedVehicles / max(SpawnedVehicles, 1)
DeathRate = DeadVehicles / max(SpawnedVehicles, 1)

TrafficOutcomeScore =
  180 * ArrivalRate
+  70 * (1 - DeathRate)^2
```

Vehicles still active at the end of simulation count in the denominator, preventing unfinished traffic from inflating the arrival rate.

### 2.4 Travel Efficiency Score

Travel efficiency uses the average time required for successful vehicles to pass through one grid cell:

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

If no vehicle arrives, this category scores 0. Future versions may compute the ideal time per vehicle type if vehicle speeds differ significantly.

### 2.5 Budget Efficiency Score

Budget efficiency compares actual road spending against a map-normalized estimated minimum road need:

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

`EstimatedMinRoadNeed` can be approximated with a Manhattan-distance minimum spanning tree between building centers or doorway connection points. Budget score is multiplied by connectivity and arrival performance so that simply using fewer roads does not produce a high score when the city is not functional.

### 2.6 Runtime Score

Runtime rewards fast completion only when the simulation processed enough traffic demand:

```text
ExpectedSpawnedVehicles = SimulationDuration / VehicleSpawnInterval
CompletionRatio = ArrivalRate * (1 - DeathRate)

If SpawnedVehicles < ExpectedSpawnedVehicles * 0.5:
    RuntimeScore = 0
Else:
    TimeRatio = clamp(1 - ElapsedSimulationTime / SimulationDuration, 0, 1)
    RuntimeScore = 100 * TimeRatio * CompletionRatio
```

This avoids rewarding a short simulation that ended early only because too few vehicles were spawned or the network failed to sustain demand.

### 2.7 Random Mode Comparability

Random Mode maps should remain comparable across different building counts, building density, road budget, and map spread. The score therefore uses a small map difficulty multiplier:

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

Recommended presets:

| Parameter | Value |
|---|---:|
| ReferenceBuildingCount | 8 |
| ReferenceSpreadRatio | 6.0 |
| TargetBudgetPressure | 0.45 |
| MapDifficultyMultiplier clamp | 0.85 - 1.20 |
| AcceptableCellTimeMultiplier | 2.5 |

The multiplier range is deliberately narrow so that player planning quality remains more important than random map difficulty.

### 2.8 Evaluation Report

The evaluation panel should present the final score as a readable report:

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

Players may replay repeatedly, optimizing arterial design and branch generation strategies to chase higher scores and cleaner planning reports.

## 3. Narrative (Optional)

CityFlow is set in a rapidly expanding modern city. The municipal planner (the player) is responsible for designing the arterial road network, while surrounding neighborhoods and small commercial districts rely on self-growing capillary road networks to connect into the urban arteries. Each playthrough is the birth of a new city, where the player balances order (arterial planning) with emergence (L-system growth), aiming to build an efficient city with smooth traffic and universal accessibility.

## 4. Art Style

The game adopts a low-poly style. Roads are presented as clean gray modules, buildings are color-coded cuboid blocks (residential = blue tones, commercial = warm tones). Vehicles are small rectangular prisms in vibrant colors, making it easy to track their flow across the network. The overall visual approach references the clean, abstract style of *Mini Motorway*, complemented by soft top-down lighting and light-colored ground, keeping the road network structure and traffic flow as the absolute visual focus. The rationale for this style choice: low-poly asset production is inexpensive, visual readability is high, and it can achieve a cohesive, polished level of finish within a one-month development cycle.
