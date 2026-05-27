# Short Technical Design Document: CityFlow

---

## 1. Architecture Overview

CityFlow adopts a **phase-separated, component-based** architecture, splitting gameplay into a **Planning Phase** and a **Simulation Phase** that are strictly isolated to reduce real-time coupling complexity.

### Core Management Classes

Core management classes are gathered in the `GameMode` or delegated Manager components:

| Manager | Responsibility |
|---|---|
| **GridManager** | Maintains a 2D logical grid, storing each cell's type (`Empty`, `Road`, `Building`) and connection mask. Provides grid snapping, placement validation, neighbour queries, and building interface registration. |
| **RoadManager** | Handles creation, destruction, and morphing of road Actors. Receives placement requests from the player or the L-system, updates the grid via `GridManager`, and triggers neighbour refreshes. |
| **LSystemManager** | Runs at the end of the Planning Phase. Extracts branch starting points from the arterial network and executes a probabilistic grid L-system to automatically grow a capillary road network that connects any remaining buildings. Includes a growth animation controller. |
| **VehicleManager** | Manages spawning and lifetime of all vehicle Actors during the Simulation Phase. Provides A* pathfinding on the road graph, spline-based movement, congestion waiting, and intersection occupation. |
| **ScoringManager** | Independently calculates the score, listens for vehicle-arrival events, periodically detects congestion, and tallies the final total. |

Managers communicate through an **event bus** (e.g., `OnRoadPlaced`, `OnVehicleArrived`) to avoid hard references.

### Game State Machine

| State | Description |
|---|---|
| **Planning** | Player lays arterial roads; can manually trigger L-system growth; may iterate until confirming. |
| **Simulating** | Vehicles spawn at a fixed frequency from origins and drive to destinations; road network is read-only; congestion is continuously detected. |
| **Evaluation** | Simulation time ends or all vehicles have despawned; final score and statistics are displayed. |

---

## 2. Key Systems

### 2.1 Grid System & Automatic Road Placement

#### Logical Grid

`GridManager` uses a `TArray<TArray<FGridCell>>` 2D array. The `FGridCell` structure:

```cpp
struct FGridCell
{
    ECellType Type;       // Empty, Road, Building
    uint8 ConnectedMask;  // bit0: Up, bit1: Down, bit2: Left, bit3: Right
    int32 BuildingID;     // If a building cell, references the owning building
    ARoadTile* RoadActor; // If a road cell, points to the corresponding Actor
};
```

All world coordinates are mapped to grid indices through `WorldToGrid(Location)`, guaranteeing snapping to cell centres during placement.

#### Player Placement Flow

1. Mouse click → **LineTrace** against ground → get world position → convert to grid coordinate `(x, y)`.
2. **Validation rules:**
   - Target cell must be `Empty`.
   - Must be adjacent to at least one existing road cell or building interface (initial placement may start from the map edge).
3. On success:
   - Set the cell to `Road`, compute its `ConnectedMask` (check the four neighbours — up/down/left/right — for roads or building interfaces).
   - Spawn the corresponding `ARoadTile` Actor, pass the current mask so it can automatically select the appropriate model (straight, corner, T-junction, cross) and build internal driving splines.
4. **Neighbour refresh:** Iterate over the four neighbours; if any is a road, recalculate its mask and call `RoadRef->UpdateConnections(NewMask)`, so that adjacent tiles automatically morph when their connectivity changes.

---

### 2.2 Road Tile Automatic Morphing & Spline Management

`ARoadTile` switches its visual and driving path automatically based on `ConnectedMask`.

#### Model Switching

According to the number and relative orientation of set bits in the mask, a preset `UStaticMesh` array is mapped:

| Connections | Orientation | Model |
|---|---|---|
| 1 | — | Dead End |
| 2 | Opposite | Straight |
| 2 | Perpendicular | Corner |
| 3 | — | T-Junction |
| 4 | — | Cross |

Meshes are pre-placed in the Blueprint and switched via `SetStaticMesh`.

#### Internal Spline Management (Hybrid Strategy)

To avoid the performance cost of pre-generating large numbers of spline components, an **on-demand** approach is used:

- **Straight / Corner segments** — Use a single, bidirectional `USplineComponent`. Vehicles choose the parameterised start and end points based on their travel direction.
- **Complex intersections (T-Junction / Cross)** — No spline component is pre-placed. Instead, a `TMap<FPathKey, TArray<FVector>>` cache is stored. When a vehicle requests a turning path (e.g., enter from **Down**, exit from **Left**):
  - If the cache misses, Bézier curve points are calculated on the fly:
    - Straight-through → 2 points
    - Turning → 3 points
  - Result is cached and the point array is returned.

Vehicles move by interpolating along the point array (or a temporary lightweight spline is created on demand) and it is destroyed after exiting.

> This strategy reduces the total number of spline components from **O(tile count × direction combinations)** to **O(vehicle count)**, greatly lowering memory and spawning overhead.

---

### 2.3 Multi-cell Buildings & Interfaces

- Buildings can occupy rectangular areas such as **2×2** or **2×3**. They are represented in the grid by multiple `Building` cells all belonging to a single `ABuilding` Actor.
- A building automatically generates a potential road interface (**Doorway**) on the outside of the midpoint of each side; the interface coordinate sits **one cell outside** the building rectangle, with its direction facing outward.

  > **Example:** For a 2×2 building, the left-edge interface is at `(MinX-1, CenterY)` facing **Right**.

- Each interface records its owning building and connection status.
- When a road cell is placed exactly on that interface location, the building is automatically marked as **connected**.

---

### 2.4 L-System Branch Generation

During the Planning Phase, after laying arterial roads, the player can trigger the L-system to automatically grow a **capillary network** to connect any buildings not yet linked to the road network.

#### Starting Point Extraction

- Iterate over all arterial road cells. For each direction in the `ConnectedMask` that is **unset** and whose neighbour is `Empty`, record that cell and direction as a **"branch growth point"**.
- Additionally, all unconnected building interfaces act as separate starting points.

#### Probabilistic Grid L-System Interpreter

A recursive function `GrowBranch(Position, Direction, Budget)` simulates tree-like expansion analogous to an L-system. Core logic:

1. Attempt to step forward one cell; check legality (inside bounds, unoccupied). On success, place a road and consume 1 budget unit.
2. Calculate the **attraction direction** to the nearest unconnected building, using it to weight branch probabilities: the chance of turning toward the attraction direction is increased.
3. With probability `p_branch` (e.g., `0.6`), spawn branches by calling itself with left- and/or right-turn explorations, achieving an effect similar to `F → F[+F]F[-F]F` but steered by the attraction target.
4. If the forward step fails, the function backtracks, automatically trying other branch directions.

**Termination conditions:**

- Budget exhausted.
- All buildings connected.
- No legal expansion directions remain.

#### Growth Animation

- An `FTimerHandle` executes one expansion step every **0.1 seconds**, showing the capillary roads appearing tile-by-tile.
- Incomplete roads use a **translucent material**, switching to opaque on completion.
- Building highlights trigger when a branch connects.

#### Integration

Each generated road cell calls `GridManager::OccupyCell` and `RoadManager::CreateRoadTile`, updating the road graph so that subsequent vehicle pathfinding works correctly.

---

### 2.5 Vehicle AI: Spline-based Pathfinding & Traffic Handling

#### Global Pathfinding

- `GridManager` builds a road graph from the current grid (each road cell is a node; edges exist between adjacent cells whose connected directions match).
- An **A\* algorithm** computes the shortest node sequence from the origin building's interface cell to the destination building's interface cell.
- Paths are calculated **once per vehicle at spawn**; if the road network is modified during the Planning Phase, affected vehicle paths are recalculated.

#### Path to Spline Movement Plan

The A\* node sequence is converted into a continuous movement plan:

```
(Tile1, entry=None, exit=Right) → (Tile2, entry=Left, exit=Up) → …
```

When a vehicle enters a new Tile, it retrieves the driving path for its `(entry, exit)` pair:

- Straight segments supply a spline directly.
- Intersections supply a point array.

... and begins moving along it.

#### Congestion Handling

- Vehicles use `SphereOverlapActors` to detect a leading vehicle on the same path.
- If the distance is less than `MinFollowDistance`, the vehicle decelerates or stops, maintaining a **safe gap**.

#### Intersection Occupation

For intersection Tiles with connection count **≥ 3**, a simple resource lock is used:

- The intersection holds a `bOccupied` boolean and a **request queue**.
- Before entering, a vehicle requests permission:
  - If `!bOccupied`, permission is granted and `bOccupied` is set to `true`.
  - Released after the vehicle fully exits.
- Waiting vehicles stop just before the entrance, **without blocking other directions**.

---

### 2.6 Origin / Destination Generation & Scoring

#### Building Generation

- At the start of the game, a number of multi-cell buildings are randomly placed on the map, ensuring no overlaps and adequate spacing:
  - **Residence** = Origin
  - **Commerce / Office** = Destination
- Additional building pairs can appear at intervals (e.g., every **60 seconds**) to increase challenge.

#### Vehicle Spawning

- During the Simulation Phase, each origin building spawns a car at a fixed frequency (e.g., every **5 seconds**).
- A random currently existing destination is chosen, and A\* pathfinding is triggered.
- If **no legal path** exists, the car is not spawned and the player is alerted.

#### Scoring Mechanism

Adopts an **"accumulated arrival score + congestion penalty + efficiency bonus"** model to encourage efficient network design:

| Component | Rule |
|---|---|
| **Base Arrival Points** | Each car reaching a destination grants **+100** |
| **Congestion Penalty** | Every second, each road tile is checked; if it contains **> 2 vehicles** it is counted as a congestion point. Score **−= 5 × congestion point count** per second. |
| **Efficiency Bonus** | The sum of remaining arterial and capillary budgets × a coefficient is added to the final score. |
| **Full-Connectivity Bonus** | If **all buildings are connected** when simulation ends, an extra **+500** |

---

## 3. Performance Considerations

| Concern | Strategy |
|---|---|
| **Grid Scale Control** | Map size is limited to **20×20 up to 30×30**; total road tile count stays bounded, avoiding logic iteration overhead. |
| **Spline Component Optimisation** | As described in [2.2](#22-road-tile-automatic-morphing--spline-management), complex intersections use on-the-fly path calculation, tying the number of spline components to the **vehicle count** rather than the tile count. |
| **Vehicle Count** | Simultaneous vehicles are kept **below 50**, controlled by spawn frequency and number of buildings. |
| **A\* Caching** | The road graph is rebuilt only when changes occur in the Planning Phase; it is read-only during simulation, and path results can be **cached per vehicle**. |

---

## 4. Libraries / Tools Used

| Tool / Library | Usage |
|---|---|
| **Unreal Engine 5.4+** | Core logic implemented with Blueprints and minimal C++. |
| **`USplineComponent`** | Used for vehicle movement paths on simple road segments. |
| **`UStaticMeshComponent`** | Road and building visuals; assets sourced from engine built-ins or free low-poly packs. |
| **Custom A\*** (Blueprint implementation) | Global path planning on the grid graph. |
| **`LineTraceByChannel`** | Mouse-based placement interaction. |
| **Timer / Event System** | Growth animation, vehicle spawning, congestion detection cycles. |
| **No Third-party Middleware** | All functionality built within the engine, minimising dependency risk. |
