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
| **LSystemManager** | A `UWorldSubsystem` that runs at the end of the Planning Phase. Extracts selective branch starting points from dead-end and straight arterial segments, then executes a breadth-first, attraction-biased iterative growth algorithm to spawn a capillary road network that connects remaining buildings. Growth is animated via `FTimerHandle` at a configurable step interval. |
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

`GridManager` is a `UWorldSubsystem` that holds a `TArray<TArray<FGridCell>>` 2D array. The `FGridCell` structure:

```cpp
USTRUCT(BlueprintType)
struct FGridCell
{
    ECellType Type;       // Empty, Road, Building
    uint8 ConnectedMask;  // bit0: Up, bit1: Down, bit2: Left, bit3: Right
    int32 BuildingID;     // If a building cell, references the owning building
    TObjectPtr<AActor> RoadActor; // If a road cell, points to the corresponding Road Actor
};
```

A `EGridDirection` bitmask enum defines the four cardinal directions (`Up`, `Down`, `Left`, `Right`). A utility struct `FGridVector` provides a lightweight 2D integer coordinate with arithmetic operators, and `GridDirectionUtils` supplies direction-to-vector mappings.

All world coordinates are mapped to grid indices through `WorldToGrid(Location)`, guaranteeing snapping to cell centres during placement.

#### Player Placement Flow (with Preview)

1. On `BeginPlay`, the Controller spawns a **preview Actor** that enters preview state.
2. Every tick, `LineTrace` against the ground tracks the cursor position; the preview Actor snaps to the grid, following the mouse in real time. `CanPlaceAt()` checks whether the target cell is valid; `SetPreviewPlacementValid()` updates the preview material — valid cells show `PreviewMaterial`, occupied cells show `InvalidPreviewMaterial` (both configurable in Blueprint).
3. **Placement** — bound to `IA_PlaceItem` (left mouse button):
   - **Started:** reset `LastPlacedGridPos`, execute first placement.
   - **Triggered** (fires every frame while held): enables **drag-to-place**. The `TryPlaceAtCursor()` helper is called each frame; it skips if the grid coordinate matches `LastPlacedGridPos` (deduplication), otherwise attempts placement at the new cell.
   - **Completed** (on release): reset `LastPlacedGridPos`.
   - Placement logic:
     - Convert the hit world position to grid coordinate `(x, y)`.
     - **Validation:** target cell must be `Empty`.
     - On success: the preview Actor transitions to `EnterPlacedState()`, restoring original materials and enabling collision. The cell is set to the appropriate type and `ConnectedMask` is computed. A new preview Actor spawns immediately.
4. **Removal** — bound to `IA_RemoveItem` (right mouse button):
   - **Started:** reset `LastRemovedGridPos`, execute first removal.
   - **Triggered:** enables **drag-to-remove** with the same deduplication via `LastRemovedGridPos`.
   - **Completed:** reset `LastRemovedGridPos`.
   - Removal logic (`TryRemoveAtCursor`): raycast → `WorldToGrid()` → look up `Cell.RoadActor` from the grid (not collision). If the cell contains an `AGridPlaceableActor` with `IsPlacedOnGrid() == true`, call `RemoveFromGrid()` + `Destroy()`.
5. **Neighbour refresh:** Iterate over the four neighbours; if any is a road, recalculate its mask.

### 2.2 Grid Placeable Actor Hierarchy

All items that can be placed on the grid inherit from the abstract base class `AGridPlaceableActor`.

#### Class Hierarchy

```
AGridPlaceableActor  (Abstract)          ← state management + unified API
  └─ AMeshGridPlaceableActor (Abstract)  ← StaticMesh + preview material swap
       ├─ ATestGridPlaceableActor         ← test cube
       ├─ ARoadTile                       ← road with mask-based auto-morphing
       └─ ABuilding                       ← multi-cell building with configurable doorways
```

#### AGridPlaceableActor (State Management)

Pure state management with no visual logic. Provides a `USceneComponent` root to anchor all child components, enabling relative positioning of visual elements (StaticMesh, additional sub-objects for landscape, etc.).

**Type classification:** `PlaceableType` (`EPlaceableType` enum: `Road`, `Building`, `Landscape`) identifies the category of this placeable.

**Grid rotation:** `EGridRotation` enum (`Rot0`, `Rot90`, `Rot180`, `Rot270`) — available for subclasses that support rotation on the grid.

| Feature | API |
|---|---|
| State flags | `IsPreview()` / `IsPlacedOnGrid()` / `IsPreviewPlacementValid()` |
| Enter preview | `EnterPreviewState()` → fires `OnEnterPreview()` (BlueprintNativeEvent) |
| Enter placed | `EnterPlacedState()` → fires `OnEnterPlaced()` |
| Preview validity | `SetPreviewPlacementValid(bool)` → fires `OnPreviewValidChanged(bool)` (BlueprintNativeEvent). Tracks `bPreviewPlacementValid` flag. |
| Grid operations | `PlaceOnGrid()` / `RemoveFromGrid()` / `CanPlaceAt()` / `SnapToGridPosition()` |
| Placement callbacks | `OnPlacedOnGrid()` / `OnRemovedFromGrid()` (BlueprintNativeEvent) |
| Grid reverse lookup | `RegisterCells()` passes `this` to `OccupyCell()` as `RoadActor`, enabling grid→actor reverse lookup for right-click removal |
| Multi-cell support | `BuildingSize` (`FVector2D`, Blueprint configurable) defines grid footprint; `GetBuildingSize()` returns the footprint; `CalculateOccupiedCells()` computes the list of grid cells covered by the footprint; `ValidatePlacement()` for subclasses to add custom placement rules |
| Type classification | `GetPlaceableType()` returns `EPlaceableType` (Road / Building / Landscape) |
| Root component | `RootSceneComponent` (`USceneComponent`) as root for managing relative positions of child components |

#### AMeshGridPlaceableActor (Visual Layer)

Adds a `UStaticMeshComponent` and automatic material switching. The `MeshComponent` is attached to the `RootSceneComponent` (provided by `AGridPlaceableActor`), allowing additional child components to be placed independently.

| Feature | Detail |
|---|---|
| `MeshComponent` | `UStaticMeshComponent` attached to `RootSceneComponent` |
| `PreviewMaterial` | Configurable in Blueprint; transparent material for valid preview state |
| `InvalidPreviewMaterial` | Configurable in Blueprint; distinct material (e.g., red) shown when the preview actor hovers over an occupied cell |
| `OnEnterPreview` override | Saves all original materials → swaps to `PreviewMaterial` on every slot → disables collision |
| `OnEnterPlaced` override | Restores original materials per-slot → enables collision |
| `OnPreviewValidChanged` override | If in preview state, swaps material to `PreviewMaterial` when valid or `InvalidPreviewMaterial` when invalid |

#### Preview Appearance Extension (AGridPlaceableActor)

A `virtual void UpdatePreviewAppearance(const FGridVector& GridPos)` method is defined on `AGridPlaceableActor` (default empty). The Controller calls this every tick after `SetPreviewPlacementValid()`, allowing subclasses to update their visual appearance during preview based on the predicted grid position.

---

### 2.3 Road Tile Automatic Morphing & Spline Management

`ARoadTile` switches its visual and driving path automatically based on `ConnectedMask`.

#### Model Switching

`ARoadTile` inherits from `AMeshGridPlaceableActor` and uses a `FRoadMeshConfig` struct array for flexible Mask→model mapping. Each road type only needs one **canonical orientation (CanonicalMask)** model; all other orientations are derived via 90° clockwise Yaw rotation.

**EGridDirection bitmask:** Up=1, Down=2, Left=4, Right=8

**CanonicalMask and standard orientation conventions:**

| Type | CanonicalMask | Standard Orientation | Connections | Notes |
|------|---------------|---------------------|-------------|-------|
| DeadEnd | **8** (Right) | Opens toward +X | 1 | |
| Straight | **12** (Left+Right) | Horizontal, opens -X / +X | 2 (opposite) | Road along X-axis; 90° rotation → vertical |
| Corner | **10** (Down+Right) | Opens +X / -Y | 2 (perpendicular) | |
| TJunction | **14** (Down+Left+Right) | Missing Up | 3 | Opens -X / +X / -Y |
| Cross | **15** (All directions) | — | 4 | |

**Automatic rotation lookup:** `FindMeshConfig()` iterates over `RoadMeshConfigs`, rotating `CanonicalMask` 90° clockwise each step (up to 4 rotations), matching against the actual `ConnectedMask`. On match, returns rotation angle `Rot × 90°`.

**Scale strategy:**
- `ReferenceCellSize` — model design reference size; runtime `BaseScale = CellSize / ReferenceCellSize`
- Each config entry has `ScaleMultiplier` (FVector), per-axis independent scaling
- Final `ActorScale = ScaleMultiplier × BaseScale`
- On 90° or 270° rotation, automatically Swap(ScaleMultiplier.X, ScaleMultiplier.Y) so vertical straight roads scale correctly

**Neighbour refresh:** `GridManager::OccupyCell` / `ClearCell` call `UpdateNeighborMasks()` to recompute four neighbours' `ConnectedMask`, broadcasting `OnCellChanged`. `ARoadTile` listens to this delegate and automatically calls `UpdateAppearance()` to switch Mesh / Rotation / Scale.

Meshes are configured in Blueprint via the `RoadMeshConfigs` array; at runtime, switching is done via `SetStaticMesh` + `SetActorRotation` + `SetActorScale3D`.

#### Preview Appearance

`ARoadTile` overrides `UpdatePreviewAppearance()` to predict the future `ConnectedMask` before placement. Each tick while in preview state:

1. Calls `GridManager::CalculateConnectedMask(GridPos)` to get the mask that would result if placed at the current cursor position.
2. Runs `FindMeshConfig()` to look up the matching Mesh/Rotation/Scale.
3. Applies the mesh, rotation, and scale to the preview actor.
4. Overrides all material slots with `PreviewMaterial` or `InvalidPreviewMaterial` depending on placement validity.

This allows the player to see exactly which road mesh configuration will appear at the target cell before clicking.

#### Mesh Material Cache

`ARoadTile` maintains a `TMap<UStaticMesh*, TArray<UMaterialInterface*>> MeshMaterialCache` to reliably restore original mesh materials after placement. The cache is populated lazily via `EnsureMeshMaterialsCached()` which reads default materials from `UStaticMesh::GetStaticMaterials()`.

- **Preview:** `UpdatePreviewAppearance` calls `EnsureMeshMaterialsCached()` before `SetStaticMesh()`, then overwrites all material slots with preview/invalid material.
- **Placement:** `OnEnterPlaced()` overrides the parent implementation to restore materials from the cache after the parent's `RestoreOriginalMaterials()`. `UpdateAppearance()` also calls `EnsureMeshMaterialsCached()` + `RestoreMeshMaterials()` when the mesh changes due to neighbour updates.
- **`OnPreviewValidChanged`** is overridden with an empty body to prevent the parent's material-swapping logic from interfering with the per-mesh preview appearance managed by `UpdatePreviewAppearance`.

This approach avoids race conditions between `OnPreviewValidChanged`, `OnEnterPreview`, and `UpdatePreviewAppearance` that would cause materials to randomly flip between preview/invalid/original states.

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

### 2.4 Multi-cell Buildings & Interfaces

`ABuilding` inherits from `AMeshGridPlaceableActor` and provides multi-cell building placement with configurable doorways and rotation on the grid.

#### ABuilding Overview

| Feature | Detail |
|---|---|
| `BuildingSize` (`FVector2D`) | Grid footprint in cells (e.g., `(2,2)`, `(3,2)`), configurable in Blueprint |
| `BuildingRotation` (`EGridRotation`) | Rotation on the grid (`Rot0`/`Rot90`/`Rot180`/`Rot270`); affects effective footprint, mesh rotation, and doorway positions |
| `MeshComponent` | Building mesh from `AMeshGridPlaceableActor`, configurable in Blueprint |
| `ReferenceMeshSize` | Reference world-size of the mesh (default `100`), used with `CellSize` to derive scale |
| `bIsDestination` | Whether this building is a destination (commercial) or origin (residential) |
| `Doorways` (`TArray<FBuildingDoorway>`) | Manually configurable doorway list; each doorway position is in **building-local grid coordinates** |
| `AutoGenerateDoorways()` | Auto-populates doorways at the midpoint of each building side, one cell outside |

#### Building Placement & Visuals

- **Grid representation:** Each cell in the building's rectangular footprint is set to `ECellType::Building`, with `BuildingID` referencing the actor's `GetUniqueID()`. All cells point back to the same `ABuilding` via `RoadActor`.
- **Rotation:** `BuildingRotation` controls how the building sits on the grid. `Rot0`/`Rot180` keep the original `W×H` footprint; `Rot90`/`Rot270` swap to `H×W` (e.g., a `2×3` building rotated 90° occupies `3×2` cells). `CalculateOccupiedCells()` is overridden to account for the effective size via `GetEffectiveBuildingSize()`.
- **Visual centering & rotation:** After placement (`OnPlacedOnGrid`), the actor repositions from the top-left grid anchor to the geometric center of the occupied area, scales the mesh to fill the effective footprint, and applies `SetActorRotation(Yaw = Rotation × 90°)`.
- **Preview:** `UpdatePreviewAppearance()` applies the same centering, scaling, and rotation logic in real time, allowing the player to see the building footprint and orientation before placement.
- **Neighbour refresh on placement:** `OnPlacedOnGrid_Implementation` iterates all cells neighbouring the building's occupied area and broadcasts `OnCellChanged` for any Road cell, triggering road tiles to recalculate their `ConnectedMask`.

#### FBuildingDoorway Struct

```cpp
USTRUCT(BlueprintType)
struct FBuildingDoorway
{
    FGridVector RelativePosition;   // Relative to building's top-left grid anchor (building-local coords)
    EGridDirection FacingDirection; // Direction the doorway faces (also building-local, rotates with building)
};
```

- Doorways are defined in **building-local** grid-relative coordinates (e.g., for a `2×3` building, `(1, 2)` specifies the cell at column 1, row 2 of the building footprint).
- When the building is rotated, doorway positions transform **with** the building via `TransformLocalPosition()`:
  - `Rot0`: `(lx, ly)` → `(lx, ly)`
  - `Rot90`: `(lx, ly)` → `(ly, W-1-lx)`
  - `Rot180`: `(lx, ly)` → `(W-1-lx, H-1-ly)`
  - `Rot270`: `(lx, ly)` → `(H-1-ly, lx)`
- `GetDoorwayWorldPositions()` transforms each doorway's local position to an absolute grid coordinate.
- `HasDoorwayAt(WorldPos)` checks if a given absolute grid position matches any doorway.
- Doorways are fully configurable in Blueprint — designers can add, remove, or reposition entries per specific grid cell.

#### Doorway-Road Connection Detection

When `CalculateConnectedMask` runs for a road cell, it now checks both `Road` and `Building` neighbour types:

- If a neighbour cell is `Road` → set the corresponding direction bit (as before).
- If a neighbour cell is `Building` → cast the `RoadActor` to `ABuilding` and check `HasDoorwayAt(road pos)`. If the building has a doorway at the road cell's position (meaning the road is placed exactly on the building's doorway point), set the direction bit.

This allows road tiles to see building doorways as "connected neighbours" and update their mesh appearance accordingly. Preview mode also benefits: `ARoadTile::UpdatePreviewAppearance()` calls `CalculateConnectedMask`, so the predicted mask includes doorway connections.

#### GridManager Random Placement (Debug)

---

### 2.4b Building Foundation & Sidewalk (ProceduralMesh)

`UFoundationComponent` is a `UActorComponent` attached to `ABuilding` that procedurally generates a 3D foundation platform and surrounding sidewalk using two `UProceduralMeshComponent` instances.

#### Architecture

```
ABuilding
  └─ UFoundationComponent (CreateDefaultSubobject)
       ├─ UProceduralMeshComponent (FoundationMesh)   ← body + walls
       └─ UProceduralMeshComponent (SidewalkMesh)     ← border ring
```

**Key design principles:**

- **Actor scale cancellation:** Building actors use `SetActorScale3D` for visual sizing. The procedural mesh vertices are computed in world units, then `SetRelativeScale3D(1/S.X, 1/S.Y, 1)` cancels the parent's scale to prevent double-transformation.
- **Z-axis convention:** Foundation sits on ground (Z=0), extruding upward to `FoundationHeight`. Sidewalk sits on top of the foundation (`FoundationHeight` → `FoundationHeight + SidewalkHeight`). Bottom face included for completeness at Z=0.
- **Consistent winding order:** UE uses a left-hand coordinate system where **clockwise (CW)** winding is the front face. The outline is generated CCW; all face/top/wall triangles are wound CW to face outward.

#### Blueprint-Configurable Properties

| Property | Default | Description |
|---|---|---|
| `FoundationHeight` | 50 | Foundation extrusion height (Z=0 to Z=50) |
| `Padding` | 50 | Inward margin from building edge to foundation edge |
| `CornerRadius` | 40 | Rounded corner radius (auto-adapts to neighbour padding) |
| `SidewalkWidth` | 20 | Sidewalk ring width (outward from foundation edge) |
| `SidewalkHeight` | 10 | Sidewalk extrusion height above foundation top |
| `FoundationMaterial` | — | Material for foundation body |
| `SidewalkMaterial` | — | Material for sidewalk ring |
| `FoundationCollisionProfileName` | `None` | Collision profile for the foundation procedural mesh. Exposed as a dropdown via `GetCollisionProfileOptions()`, listing all engine and project collision profiles. |
| `SidewalkCollisionProfileName` | `None` | Collision profile for the sidewalk procedural mesh. Same dropdown behaviour as foundation. |

#### Per-Edge Connection Handling

`BuildFoundation` receives 4 flags (`bTopConnected`/`bRightConnected`/`bBottomConnected`/`bLeftConnected`). Connected edges use `Padding = 0` (flush with building), while unconnected edges use the configured `Padding` value. Corner radii adapt dynamically — if either adjacent edge has `Padding = 0`, the corner radius is clamped to `min(CornerRadius, maxPad)`.

#### Mesh Generation Details

| Component | Geometry | Winding |
|---|---|---|
| **Top face** | N-gon fan triangulation from Outline vertices at Z=`FoundationHeight` | `(0, i+2, i+1)` CW |
| **Bottom face** | Same N-gon at Z=0, normal = `(0,0,-1)` | `(0, i+1, i+2)` (CCW → CW from below) |
| **Wall quads** | Per segment: `(A_Top, B_Top, B_Bot, A_Bot)`, normal = `(Edge.Y, -Edge.X, 0)` (outward) | `(0,1,2), (0,2,3)` CW |
| **Sidewalk outer/inner walls** | 4 outer + 4 inner wall quads around the ring | CW |
| **Sidewalk top face** | 4 trapezoids forming the ring top at Z=`FoundationHeight + SidewalkHeight` | `(Base, Base+2, Base+1)` CW |
| **Sidewalk bottom face** | 4 trapezoids at Z=`FoundationHeight`, normal = `(0,0,-1)` | `(Base, Base+1, Base+2)` (flipped for downward) |

#### Connection State Refresh

`ABuilding::OnDoorwayCellChanged` listens to `GridManager::OnCellChanged`. When a doorway's connection point changes to/from `Road` type, `DetermineEdgeConnections()` re-evaluates all 4 edges and `RefreshFoundation()` rebuilds the mesh.

---

### 2.5 L-System Branch Generation

`ULSystemManager` is a `UWorldSubsystem` that triggers at the end of the Planning Phase. It uses a **breadth-first iterative queue** to automatically grow a capillary road network, connecting any buildings not yet linked to the road network.

#### Architecture

`ULSystemManager` is accessed via `GetWorld()->GetSubsystem<ULSystemManager>()`. All configuration is done through Blueprint-callable setter functions (no `UPROPERTY(EditAnywhere)` exposed).

#### Starting Point Extraction

Starting points are collected from two sources:

**A. Dead-end road cells:**
- Iterate over all road cells; for each dead-end (`ConnectedMask` has exactly 1 bit set), record growth points in the unconnected directions.

**B. Straight road segments (spaced sampling):**
- Identify straight road segments (`ConnectedMask` with 2 opposite bits: Up+Down or Left+Right).
- Walk the segment in both axis directions to collect the full contiguous segment.
- If segment length < 3 cells, skip (too short to branch from).
- Sample perpendicular branch points every `MinBranchSpacing + 1` cells along the segment.
- Corner, T-junction, and Cross tiles are **skipped** (already well-connected).

**C. Unconnected building doorways:**
- Iterate all unconnected buildings; for each doorway whose connection point is `Empty`, add a growth point from the building's grid edge cell in the doorway's facing direction.

#### Breadth-First Iterative Growth

Instead of a recursive backtracking algorithm, an **iterative queue** drives growth:

1. A `FLSystemGrowthPoint` struct holds a grid position and a growth direction.
2. `ProcessGrowthStep()` is called on an `FTimerHandle` at `GrowthInterval` (default 0.1s).
3. Each step pops the **front** of the queue, calls `TryGrowAt()`.
4. New growth points are inserted at the **back** of the queue, producing breadth-first alternation between all active branches.

#### Multi-cell Straight Extension

When `TryGrowAt()` runs on a growth point, it does not place a single cell. Instead, it attempts to place up to `StraightExtendLength` (default 3) cells in the point's direction:

- Each cell is placed via `World::SpawnActor<ARoadTile>` + `PlaceOnGrid()`.
- Stops early if the path is blocked, budget exhausted, or placement fails.
- Only the **last** successfully placed cell generates continuation points.

#### Side Branch Validation

Before a side branch (left/right turn) is added, `IsSideBranchValid()` checks the two cells adjacent to the branch target cell (relative to the branch direction). If **either** side is already a Road cell, the branch is **rejected** — this prevents branches from filling in gaps between parallel roads.

#### Forward Continuation & Probabilistic Branching

For each newly placed cell's valid neighbours (excluding the back-direction):

| Direction | Behaviour |
|---|---|
| Forward (same direction) | **Always** added as a continuation point |
| Left / Right | Added with `BranchProbability` (default 0.6) — only if `IsSideBranchValid()` passes |

#### Attraction-biased Sorting

New growth points are sorted by an attraction score before insertion:

```
Score = Lerp(DistScore, AlignScore, AttractionStrength)
  DistScore  = 1 / (1 + euclidean distance to nearest unconnected building)
  AlignScore = dot(normalised direction to building, growth direction), clamped ≥ 0
```

Higher-scored points execute first within the batch. This steers branches toward unconnected buildings without overriding the breadth-first guarantee.

#### Configurable Parameters (Blueprint Setters)

| Setter | Default | Description |
|---|---|---|
| `SetRoadTileClass(TSubclassOf<ARoadTile>)` | — | Road tile class for spawned branches |
| `SetBranchBudget(int32)` | 50 | Maximum total road cells to place |
| `SetGrowthInterval(float)` | 0.1 | Seconds between each growth step (animation speed) |
| `SetBranchProbability(float)` | 0.6 | Probability of spawning a side branch at each step |
| `SetAttractionStrength(float)` | 0.7 | Weight of direction alignment vs. distance in attraction score |
| `SetStraightExtendLength(int32)` | 3 | Number of cells placed per forward step (multi-cell extension) |
| `SetMinBranchSpacing(int32)` | 3 | Minimum spacing between branch points on straight segments |

#### Event Delegates

| Delegate | Signature | When |
|---|---|---|
| `OnGenerationStarted` | `()` | After start points collected, before first step |
| `OnGenerationStep` | `(int32 RemainingBudget)` | After each growth step |
| `OnGenerationFinished` | `(bool bAllBuildingsConnected)` | On completion, abort, or termination |

#### Termination Conditions

- Budget exhausted (all remaining budget consumed).
- All buildings connected (early success).
- Queue empty — no more legal growth directions.

#### Road Tile Creation

`CreateRoadTile()` spawns an `ARoadTile` via `World::SpawnActor` + `PlaceOnGrid()`. `PlaceOnGrid` internally calls `GridManager::OccupyCell`, triggering neighbour mask updates and the `OnCellChanged` broadcast — so `ARoadTile` automatically switches to the correct mesh/rotation on placement.

#### Growth Animation

- `FTimerHandle` calls `ProcessGrowthStep()` every `GrowthInterval` seconds, placing cells one batch at a time (batch = up to `StraightExtendLength` cells per step).
- (Future) Incomplete roads use a translucent material; completed roads switch to opaque.
- (Future) Building highlights trigger when a branch connects.

---

### 2.6 Vehicle AI: Spline-based Pathfinding & Traffic Handling

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

### 2.7 Origin / Destination Generation & Scoring

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

### 2.8 Player & Camera System

#### CityFlowPawn

A `ACharacter` subclass configured for top-down free-flight control:

| Feature | Implementation |
|---|---|
| Movement mode | `MOVE_Flying` (gravity-free, any-axis movement) |
| Input | `Enhanced Input` → `IA_Move` (Action, ValueType Axis2D) |
| Movement direction | Derived from camera rotation via `GetControlRotation()` — WASD moves relative to the current camera view |
| Configurable (Blueprint) | `MoveSpeed` |
| Camera setup | Handled entirely in Blueprint: add `USpringArmComponent` + `UCameraComponent` as child components; the character auto-possesses and becomes the view target |

#### CityFlowPlayerController

| Feature | Implementation |
|---|---|
| Cursor | `bShowMouseCursor = true` |
| Preview system | Spawns a preview actor on `BeginPlay`; follows cursor via `Tick()` → `GetHitResultUnderCursor()` → `SnapToGrid()`; each tick calls `SetPreviewPlacementValid()` for validity, then `UpdatePreviewAppearance()` to let `ARoadTile` show the predicted mesh in preview |
| Placement | `IA_PlaceItem` (left mouse button) → `Started`/`Triggered`/`Completed` events → `TryPlaceAtCursor()` helper with `LastPlacedGridPos` deduplication for drag-to-place |
| Removal | `IA_RemoveItem` (right mouse button) → `Started`/`Triggered`/`Completed` events → `TryRemoveAtCursor()` helper with `LastRemovedGridPos` deduplication for drag-to-remove. Looks up the actor from `Cell.RoadActor` in the grid instead of relying on collision hit. |
| Configurable (Blueprint) | `PlaceableActorClass` (any `AGridPlaceableActor` subclass); `IA_PlaceItem`, `IA_RemoveItem` |

---

### 2.9 Grid Visualization

Two Actor classes provide runtime grid line rendering for debugging and visual reference during the Planning Phase.

#### AGridPlaneVisualizer (Primary)

Uses a single Plane mesh with a translucent world-aligned material for efficient single-drawcall grid line rendering.

| Feature | Implementation |
|---|---|
| Rendering method | Single `UStaticMeshComponent` (Plane mesh) scaled to match the full grid dimensions |
| Material | `UMaterialInstanceDynamic` created at runtime from a configurable `GridMaterial`; parameters `CellSize`, `LineWidth`, `LineColor`, `OriginX`, `OriginY` are passed from `GridManager` |
| Grid alignment | Plane size and position are derived from `GridManager::GetGridWidth()/GetHeight()/GetCellSize()/GetGridOrigin()` at `SetupPlane()` |
| Visual customization | `LineColor`, `LineWidth`, `ZOffset` are all Blueprint-configurable |

The material uses `M_PrototypeGrid` with `Blend Mode = Translucent` and simple world-position-based math in the material graph. The Plane is placed slightly above the grid origin plane via `ZOffset` to avoid Z-fighting.

**Blueprint API:**

| Function | Description |
|---|---|
| `SetupPlane()` | Reads grid parameters from `GridManager`, configures Plane position, scale, and dynamic material |
| `UpdateMaterialParams()` | Refreshes all material parameters from current `GridManager` state and property values |
| `SetGridVisible(bool)` | Toggles Plane visibility |

#### AGridVisualizer

Uses `ULineBatchComponent::DrawLine()` to procedurally draw individual grid lines. Replaced in favour of `AGridPlaneVisualizer` for performance, but retained as an alternative renderer.

---

## 3. Performance Considerations

| Concern | Strategy |
|---|---|
| **Grid Scale Control** | Map size is limited to **20×20 up to 30×30**; total road tile count stays bounded, avoiding logic iteration overhead. |
| **Spline Component Optimisation** | As described in [2.3](#23-road-tile-automatic-morphing--spline-management), complex intersections use on-the-fly path calculation, tying the number of spline components to the **vehicle count** rather than the tile count. |
| **Vehicle Count** | Simultaneous vehicles are kept **below 50**, controlled by spawn frequency and number of buildings. |
| **A\* Caching** | The road graph is rebuilt only when changes occur in the Planning Phase; it is read-only during simulation, and path results can be **cached per vehicle**. |

---

## 4. Libraries / Tools Used

| Tool / Library | Usage |
|---|---|
| **Unreal Engine 5.6** | Core logic implemented with C++. |
| **Enhanced Input** | WASD movement and placement action binding via `UInputAction` + `UInputMappingContext`. |
| **`USplineComponent`** | Used for vehicle movement paths on simple road segments. |
| **`UStaticMeshComponent`** | Road and building visuals; assets sourced from engine built-ins or free low-poly packs. |
| **`UWorldSubsystem`** | `GridManager` as a globally-accessible singleton subsystem. |
| **Custom A\*** (Blueprint implementation) | Global path planning on the grid graph. |
| **`LineTraceByChannel`** | Mouse-based placement interaction and preview tracking. |
| **Timer / Event System** | Growth animation, vehicle spawning, congestion detection cycles. |
| **No Third-party Middleware** | All functionality built within the engine, minimising dependency risk. |
