# Short Technical Design Document: CityFlow

---

## 1. Architecture Overview

CityFlow adopts a **phase-separated, component-based** architecture, splitting gameplay into a **Planning Phase** and a **Simulation Phase** that are strictly isolated to reduce real-time coupling complexity.

### Core Management Classes

Core management classes are gathered in the `GameMode` or delegated Manager components:

| Manager | Type | Responsibility |
|---|---|---|
| **GridManager** | `UWorldSubsystem` | Maintains a 2D logical grid. Provides grid snapping, placement validation, neighbour queries, connected-mask calculation, and building interface registration. **Manages the shared road budget** — both player and L-system placement consume from a single pool tracked by `RoadBudget`. |
| **LSystemManager** | `UWorldSubsystem` | **Optional** auxiliary capillary road generator. Extracts branch starting points from dead-ends and straight segments, then executes breadth-first, attraction-biased growth. Consumes from the **shared road budget** alongside the player. Triggered manually by player (UI button or console command). |
| **VehicleManager** | `UWorldSubsystem` + `FTickableGameObject` | Spawns and manages all vehicle Actors. Provides **A\* pathfinding** on the road graph, converts grid paths to world-space spline-based movement plans, handles **congestion detection** (per-cell vehicle count). Ticks every frame for vehicle state updates and periodic intersection lock sanitization. |
| **ScoringManager** | `UWorldSubsystem` | Tracks arrival count, arrival score, and congestion penalties during the Simulation Phase. Uses a periodic timer for congestion penalty deduction. Computes final score including full-connectivity bonus on evaluation. |
| **CityFlowGameMode** | `AGameModeBase` | Owns the **state machine** (`ECityFlowGamePhase`: Planning → Simulating → Evaluation). Initializes the grid, spawns default buildings, manages the shared road budget split (Player vs L-System), and triggers phase transitions. Provides Blueprint-callable API for UI control. |

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
     - On success: the preview Actor transitions to `EnterPlacedState()`, restoring original materials and enabling collision. The cell is set to the appropriate type and `ConnectedMask` is computed. A new preview Actor spawns immediately. After `OnPlacedOnGrid()`, the actor plays a **spawn scale animation** — scaling up from a configurable initial size to full scale with an ease-out curve.
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

#### Spawn Scale Animation (v0.9)

When a `GridPlaceableActor` is placed on the grid via `PlaceOnGrid()`, it plays a **scale-up animation** for visual feedback. The animation runs on a `FTimerHandle` (no per-frame Tick overhead) and follows an ease-out cubic curve.

**Insertion point:** At the end of `PlaceOnGrid()`, after `OnPlacedOnGrid()` and `OnGridPlaced` broadcast — this ensures subclasses (e.g., `ARoadTile::UpdateAppearance()`) have already applied their final `SetActorScale3D()` before the animation captures the target scale.

**Flow:**
1. `PlaySpawnAnimation()` captures current `GetActorScale3D()` as `TargetScale`.
2. Sets initial scale to `TargetScale × SpawnAnimationInitialScale`.
3. Starts `FTimerHandle` at ~60 Hz (0.016 s interval).
4. `TickSpawnAnimation()` increments elapsed time, computes `T = elapsed / Duration`, applies `Alpha = 1 - (1-T)^3` (ease-out cubic), sets `ActorScale = TargetScale × Alpha`.
5. On `T ≥ 1.0`, snaps to exact `TargetScale` and clears the timer.

**Blueprint-configurable properties (all on `AGridPlaceableActor`):**

| Property | Default | Description |
|---|---|---|
| `bPlaySpawnAnimation` | `true` | Master toggle; set to `false` to disable (e.g., L-system batch placement) |
| `SpawnAnimationDuration` | `0.2` | Animation duration in seconds |
| `SpawnAnimationInitialScale` | `0.05` | Initial scale fraction (0.0–1.0, clamped to ≥ 0.01 to avoid zero-scale issues) |

**Safety:** `EndPlay()` clears `SpawnAnimTimer` to prevent dangling callbacks after actor destruction.

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

#### Internal Spline Management (Hybrid Strategy) — ⚠️ Needs Refactoring

> **Current status:** `ARoadTile::GetSplinePath()` exists but is **not used by VehicleManager**. The spline path generation has been iterated on multiple times and has known issues — see 2.6 for the current (simplified) path construction approach. This section documents the intended design.

`ARoadTile::GetSplinePath(EntryDir, ExitDir)` computes a smooth path through the tile from the entry edge midpoint to the exit edge midpoint. No spline component is stored on the road tile — all computation is on-the-fly with no persistent state.

**Path computation (intended design):**
- **Straight-through (opposite directions):** Returns 2 world-space points: `[entry_edge_midpoint, exit_edge_midpoint]`. The vehicle's own spline interpolates linearly between them.
- **Turning (perpendicular or any non-opposite directions):** Computes a quadratic Bézier curve in local space:
  - P0 = entry edge midpoint
  - P1 = P0 + P2 (outer corner of the two edges, NOT the cell center)
  - P2 = exit edge midpoint
  - Sampled into 13 world-space points for a smooth arcing curve.

**Current approach (v0.3, refactored):**
- `BuildSplinePath()` generates spline points and corresponding tangent directions from the raw A\* path (all cells preserved, no merging).
- Returns a parallel `TArray<FVector>` of tangent directions (one per point) used by `SetSplinePath` to set exact spline tangents.
- Straight cell centers are included in the output — they serve as separators between consecutive turn sequences.
- **Turn offset:** each turning point is replaced by two offset points:
  - EntryOffset = `center - EntryDir * CellSize/2` (half-cell offset back toward the incoming direction), tangent = `EntryDir`
  - ExitOffset = `center + ExitDir * CellSize/2` (half-cell offset toward the next cell direction), tangent = `ExitDir`
- **Consecutive turns:** a sequence of back-to-back turns (no straight cells between them) only gets a full entry+exit pair from the *first* turn; subsequent turns only add an exit offset. This prevents offset-point duplication at shared cell boundaries.
- Straight cells output `cell_center` with tangent = path-forward direction.
- `SetSplinePath(Points, TangentDirs)` uses the provided tangent directions directly, setting each spline point's tangent to `TangentDir * CellSize/2`.
- `ARoadTile::GetSplinePath()` has never been used; retained for future reference.

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

- **Actor scale cancellation:** Building actors use `SetActorScale3D` for visual sizing. The procedural mesh vertices are computed in world units, then `SetRelativeScale3D(1/S.X, 1/S.Y, 1)` cancels the parent's scale to prevent double-transformation. **v0.11 fix:** `BuildFoundation` now accepts an explicit `InOwnerScale` parameter (computed by `RefreshFoundation` as `TargetScale = EffSize × CellSize / ReferenceMeshSize`) rather than reading `Owner->GetActorScale3D()` at build time, avoiding stale animation-intermediate scale values when the foundation is rebuilt during spawn animation.
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

### 2.6 Vehicle AI: A\* Pathfinding & Spline-Based Movement

#### Implementation Status: ✅ Implemented — v0.4 bidirectional with turn-aware tangent scaling

**VehicleActor** (`AVehicleActor`) is a Blueprintable `AActor` with a `UStaticMeshComponent` vehicle body and a `USplineComponent` (`PathSpline`) that stores the complete world-space path from origin to destination. The `PathSpline` uses absolute transform (not attached to the moving root), ensuring world-space queries remain correct as the vehicle moves.

**Spline-based movement model:** The vehicle maintains a `CurrentSplineDistance` float. Each frame, `TickMovementSpline(DeltaTime)` advances this distance by `MoveSpeed * DeltaTime`, queries the spline for the world-space position (`GetLocationAtDistanceAlongSpline`) and direction (`GetDirectionAtDistanceAlongSpline`), and updates the actor's location and rotation. When `CurrentSplineDistance >= SplineLength`, the vehicle arrives.

**Path construction (`BuildSplinePath`) — v0.4 turn-offset with arrive/leave separation:**
Processes the raw A\* path (all cells preserved) in a single pass, outputting spline points, per-point tangent directions, and **separate arrive/leave tangent length multipliers**:
- **First cell:** adds `cell_center`, tangent toward next cell, both multipliers = 1.0.
- **Turn cells:** replaces each turn with offset points:
  - Entry offset: `cell_center - EntryDir * CellSize/2`, tangent = `EntryDir`; in non-consecutive turns: arrive=1.0, leave=TurnMult.
  - Exit offset: `cell_center + ExitDir * CellSize/2`, tangent = `ExitDir`; arrive=TurnMult, leave=1.0 (may be overridden by next consecutive turn).
  - Consecutive turns: skip the entry offset; instead, update the previous exit point's leave multiplier to the current turn's TurnMult.
- **Straight cells:** adds `cell_center`, tangent=forward direction, both multipliers=1.0 (resets turn sequence).
- **Last cell:** adds `cell_center`, both multipliers=1.0.

**Turn direction detection & scaling (v0.4):**
- Uses cross-product Z component: `CrossZ = EntryDir.X × ExitDir.Y - EntryDir.Y × ExitDir.X`
  - `CrossZ > 0` → right turn; `CrossZ < 0` → left turn.
- `bRightHand == bIsRightTurn` → outside (long bend) → `TurnMult = 1.0 + LaneOffsetFactor`
- `bRightHand != bIsRightTurn` → inside (short bend) → `TurnMult = 1.0 - LaneOffsetFactor`

**Tangent control in `SetSplinePath` — handle-break approach (v0.4):**
1. Build spline the old way: `AddSplineWorldPoint` all points, `SetTangentAtSplinePoint` uniform tangents at `CellSize` length.
2. Break handle linkage: set every point type to `ESplinePointType::CurveCustomTangent` via `SetSplinePointType(i, CurveCustomTangent, false)` then `UpdateSpline()`.
3. Per-segment override via `SplineCurves.Position.Points[i]`: for each segment `(i, i+1)` where `LeaveTangentLengths[i] ≠ 1.0`:
   - `Points[i].LeaveTangent = TangentDir[i] * CellSize * LMult`
   - `Points[i+1].ArriveTangent = TangentDir[i+1] * CellSize * LMult`
   - Both handles share the same multiplier, ensuring symmetric curve segment deformation.
4. Final `UpdateSpline()` to apply.

**Bidirectional Lane Offset (v0.4):**
- After generating all spline points and tangent directions, `BuildSplinePath` applies a perpendicular offset to every point.
- Offset distance = `CellSize × LaneOffsetFactor` (configurable, default 0.2).
- Right perpendicular = `(TangentDir.Y, -TangentDir.X, 0.0)` — rotate tangent 90° clockwise in XY plane.
- Right-hand driving (`ECityFlowDrivingSide::RightHand`): points offset by `+RightPerp × Offset`
- Left-hand driving (`ECityFlowDrivingSide::LeftHand`): points offset by `−RightPerp × Offset`
- Configuration: `ACityFlowGameMode::DrivingSide` and `LaneOffsetFactor` passed to `UVehicleManager` via `SetDrivingSide()` / `SetLaneOffsetFactor()` at simulation start.

**Vehicle Spawning:** `UVehicleManager::SpawnVehicle(Origin, Destination)` picks doorway connection points, runs A\* via `BuildPath()`, calls `BuildSplinePath()` to produce the world-space point array, spawns the vehicle, snaps to the first spline point, and calls `Vehicle->SetSplinePath(Points)`.

**A\* Pathfinding:**
- Nodes = road cells (`ECellType::Road`); edges = `ConnectedMask` direction bits.
- Cost = 1 per step (uniform); heuristic = Manhattan distance.
- Algorithm: standard A\* with `TMap<FGridVector, FAStarNode>` for open/closed sets.

**Intersection Occupation (v0.6 — ✅ Direction-based with round-robin scheduling):**

**Physical trigger box:**
- Each `ARoadTile` with `ConnectedMask` ≥ 3 (Cross / T-Junction) enables a `UBoxComponent` (`IntersectionBox`) sized to exactly one grid cell (actor-scale-neutralised: `BoxExtent = CellSize / ActorScale / 2`).
- Box uses `ObjectType = ECC_Vehicle` with `ECR_Overlap` to the Vehicle channel so that `VehicleMesh` (QueryVehicle preset, Vehicle→Vehicle = Overlap) generates `OnBeginOverlap` / `OnEndOverlap` events for the lock life-cycle.
- The box also blocks `ECC_GameTraceChannel2` (Intersection) for the forward-probe sweep.

**Direction-occupancy tables (ARoadTile):**
```
TMap<EGridDirection, TSet<TWeakObjectPtr<AVehicleActor>>> DirectionOccupants    // vehicles physically inside the box
TMap<EGridDirection, TSet<TWeakObjectPtr<AVehicleActor>>> PendingReservations   // probed & granted but not yet inside
TMap<TWeakObjectPtr<AVehicleActor>, EGridDirection>        VehicleEntryDirs      // reverse-lookup
TMap<TWeakObjectPtr<AVehicleActor>, float>                 PendingReservationTimestamps  // for expiry
```

**Core lock protocol:**
1. **Forward-probe sweep (Channel2):** `PerformForwardProbe()` sweeps with `ECC_GameTraceChannel2`, derives entry direction from `GridDirectionUtils::DirectionFromWorldVector(Velocity)`, and calls `RoadTile->TryAcquireIntersectionLock(this, EntryDir)`.
2. **Conflict check:** Collect `OccupiedDirs = DirectionOccupants.Keys ∪ PendingReservations.Keys`.
   - Empty → grant.
   - Only contains `EntryDir` → same-direction flow, grant (if under `MaxConsecutiveGrants`).
   - Contains other directions → crossing conflict. Grant only if `ServingDirection == EntryDir && ServedCount < MaxConsecutiveGrants`.
3. **Pending→Occupants transition:** `OnBeginOverlap` moves the vehicle from `PendingReservations[EntryDir]` to `DirectionOccupants[EntryDir]`.
4. **Lock release:** `OnEndOverlap` removes the vehicle from both tables. **No explicit release API needed** — the overlap events drive the entire life-cycle.

**Round-robin direction scheduling (v0.6):**
- `ServingDirection`: currently served direction; `ServedCount`: vehicles granted this round; `WaitingDirs`: set of competing directions; `MaxConsecutiveGrants = 1`: vehicles per direction per round.
- On `EndOverlap` when the box becomes empty: peek (not remove) the first `WaitingDir` as the new `ServingDirection`. The direction is removed from `WaitingDirs` only when a vehicle from it is actually granted in `TryAcquireIntersectionLock`.
- Cross-direction requests while the box is occupied: grant only if `EntryDir == ServingDirection && ServedCount < MaxConsecutiveGrants`; otherwise reject. When `ServedCount` hits the limit that direction is **not re-enqueued** — it yielded its turn.
- Single-direction traffic: no round-robin intervention (behaves identically to the simple direction-occupancy model).

**Safety nets:**
| Mechanism | Trigger | Purpose |
|---|---|---|
| Re-entry all-pass | `TryAcquire` — vehicle already in `VehicleEntryDirs` | Eliminates false rejections from direction-derivation drift on curved splines |
| Physical overlap sanctise | `VehicleManager::SanitizeAllIntersectionLocks()` every 2 s via `IsOverlappingActor` | Removes zombie `DirectionOccupants` entries from lost `EndOverlap` events |
| Pending reservation expiry | Same timer, `ExpirePendingReservations(5.0f)` | Clears pre-reservations from vehicles stuck in traffic that never enter the box |
| Passed-intersection tracking | `AVehicleActor::PassedIntersections` — `MarkIntersectionPassed()` on `EndOverlap`; checked in `TryAcquire` | Prevents self-re-entry: vehicle that just exited can still sweep the box via forward probe |

**Collision channels:**
| Channel | Use |
|---|---|
| `ECC_GameTraceChannel1` (Vehicle) | VehicleMesh body → forward-probe physical vehicle detection |
| `ECC_GameTraceChannel2` (Intersection) | IntersectionBox → forward-probe intersection reservation |

**Removed (v0.5 legacy):**
- `VehicleManager::IntersectionLocks` TMap, `AcquireIntersectionLock()`, `IsIntersectionLockedByOther()`, `UpdateIntersectionLocks()`
- `VehicleManager::CachedIntersections`, `bIntersectionsDirty`
- `VehicleActor::PathIntersectionCells`, `SetPathIntersections()`, `SetWaitingForIntersection()`
- `CityFlowVehicleTypes.h`: `FIntersectionLock`, `FIntersectionOccupant` structs

**Movement State Machine (v0.6):**
| State | Description |
|---|---|
| `Idle` | Initial or error state |
| `Moving` | Following spline path; forward probe runs each frame (two sweeps: Ch1 vehicles + Ch2 intersections) |
| `WaitingCongestion` | Stopped due to front vehicle or locked intersection ahead; accumulates `CongestionWaitTime` for deadlock detection; resumes automatically when obstacle clears |
| `Arrived` | Reached destination; clears all reserved intersections, broadcasts event |

**v0.11 movement flow:**
```
TickMovementSpline:
  1. PerformForwardProbe():
     a. Sweep Ch1 → find closest physical vehicle
     b. Sweep Ch2 → for each hit IntersectionBox:
        - TryAcquireIntersectionLock(this, EntryDir) → granted: track in ReservedIntersections
        - Rejected → treat as virtual obstacle, take min distance
     c. ClosestDist = min(vehicleDist, intersectionDist)
     d. bFrontVehicleTooClose = closest ≤ safeDist (vehicle) or any virtual obstacle
  2. if bFrontVehicleTooClose:
     a. State → WaitingCongestion, accumulate CongestionWaitTime
     b. if CongestionWaitTime >= DeadlockTimeout → release all intersection locks, clear PassedIntersections, reset timer
     c. decelerate → return
  3. if cleared → Moving → reset CongestionWaitTime → accelerate → advance spline
```

**Congestion Detection (v0.11 — ✅ Fixed):**
- `UpdateCongestion()` builds a per-tick `TMap<FGridVector, int32>` by iterating all `ActiveVehicles` and counting vehicles per grid cell via `WorldToGrid()`.
- Cells with `count > CongestionThreshold` (default 3) are flagged congested.
- Congestion data is queryable via `GetCongestedCells()` and broadcasts `OnCongestionUpdated`.
- **v0.11 fix:** Previous design used a persistent `TMap<FGridVector, AVehicleActor*>` (`VehicleGridMap`) which could only store one vehicle per cell (TMap duplicate-key assert). The new per-tick-counting approach is both correct and simpler, eliminating the `UpdateVehicleGridOccupancy()` and `IsOccupiedByVehicle()` dead code.

**Deadlock Timeout (v0.11 — ✅ New):**
- When two adjacent intersections lock each other (Vehicle A occupies Intersection 1 waiting for Intersection 2, Vehicle B occupies Intersection 2 waiting for Intersection 1), both vehicles enter a hold-and-wait deadlock.
- `AVehicleActor` accumulates `CongestionWaitTime` while in `WaitingCongestion` state. If it exceeds `DeadlockTimeout` (default 3.0s, Blueprint-configurable), the vehicle forcibly releases all intersection reservations via `ReleaseVehicleFromAllTables()`, clears `ReservedIntersections` and `PassedIntersections`, and resets the timer.
- This breaks the deadlock: the freed intersection becomes available for the other vehicle, allowing progress.
- `CongestionWaitTime` resets when exiting `WaitingCongestion` (obstacle cleared) or on receiving a new spline path (`SetSplinePath`).

**Forward Probe — Zero-Distance Fix (v0.11):**
- `PerformForwardProbe()` previously filtered sweep hits with `ProjDist > 0.0f` (vehicle sweep) and `InterDist <= 0.0f` (intersection sweep), which ignored hits when the probe volume already overlapped a collision body at sweep start.
- Fixed to `ProjDist >= 0.0f` and `InterDist < 0.0f` respectively, allowing vehicles starting inside an intersection (e.g., spawned at a building whose doorway cell is an intersection) to correctly acquire intersection locks.

#### Vehicle Spawn Table

`UVehicleDataAsset` (`UPrimaryDataAsset`) acts as a **vehicle class registry**:

```cpp
USTRUCT()
struct FVehicleSpawnEntry
{
    TSubclassOf<AVehicleActor> VehicleClass;   // Blueprint subclass of AVehicleActor
    float SpawnWeight = 1.0f;                  // Relative spawn probability
};
```

`UVehicleDataAsset::VehicleEntries` is an array of `FVehicleSpawnEntry`. Vehicle spawning uses a two-tier priority: if `ACityFlowGameMode::VehicleDataAsset` (a `TObjectPtr<UVehicleDataAsset>`) is configured, it is passed to `UVehicleManager::SetVehicleDataAsset()` and used directly; otherwise, `CacheSpawnEntries()` falls back to `DeveloperSettings::DefaultVehicleDataAsset`. `PickRandomVehicleClass()` performs weighted random selection per spawn from whichever source was loaded.

Each `AVehicleActor` subclass (e.g. `BP_Car`, `BP_Truck`) configures its own `VehicleMesh`, `MoveSpeed`, `DebugColor`, etc. directly in its Blueprint defaults — no DataAsset-driven property override is needed.

---

### 2.7 Origin / Destination Generation & Scoring

#### Implementation Status: ✅ Implemented — v0.10 DataAsset-driven spawn with doorway validation

#### Building Generation

`CityFlowGameMode::InitializeDefaultScene()` now supports two paths:

**Primary: `UBuildingDataAsset`** (new in v0.10) — a `UPrimaryDataAsset` with a single `BuildingEntries` array of `FBuildingDataEntry` (each specifying `TSubclassOf<ABuilding>` + `float SpawnWeight`). The roles of origin vs. destination are determined by each building BP's own `bIsDestination` flag — no separate origin/destination arrays are needed.

Spawn counts are allocated deterministically using the **largest-remainder method**: each entry receives `floor(weight / totalWeight × DefaultBuildingCount)`, and any remaining slots are assigned to entries with the largest fractional parts.

**Fallback:** If `BuildingDataAsset` is not set, the legacy `OriginBuildingClass` / `DestinationBuildingClass` single-class properties are used as before (50/50 split).

Buildings are then placed via `GridManager::TryPlaceBuildingsRandom()` with random positions and rotations.

#### Building Doorway Placement Validation (v0.10)

`ABuilding::ValidatePlacement()` now validates that every doorway's connection point (the grid cell immediately outside the building footprint in the doorway's facing direction) satisfies two conditions:
- **In-bounds:** `IsValidGridPos(ConnPt)` — the doorway cell must be within the grid boundary.
- **Not occupied by another building:** `GetCell(ConnPt).Type != ECellType::Building` — prevents doorway cells from overlapping with other buildings' occupied cells.

If any doorway fails, `CanPlaceAt()` returns `false`, and `TryPlaceBuildingRandom()` automatically retries with the next candidate position/rotation.

A new helper `GetDoorwayConnectionPointForPosition(Doorway, BasePos)` computes a doorway's world-grid connection point against an arbitrary candidate position (before `GridPosition` is set), enabling pre-placement validation.

#### Vehicle Spawning

`UVehicleManager::Tick()` spawns vehicles at `SpawnInterval` intervals (default 5s). Each tick picks a random origin and destination, calls `SpawnVehicle()` which computes an A\* path and spawns the actor. If no path exists, the vehicle is silently skipped; no alert is raised yet (future).

#### Scoring Mechanism (UScoringManager)

| Component | Rule |
|---|---|
| **Base Arrival Points** | Each vehicle arriving at destination grants **+ArrivalScore** (default 100, configurable via DeveloperSettings) |
| **Congestion Penalty** | Every second, `UpdateCongestionPenalty()` checks `GetCongestedCells()`; penalises **−CongestionPenaltyPerSecond × congested_cell_count** (default 5/cell/s) |
| **Full-Connectivity Bonus** | If all buildings connected at evaluation, **+FullConnectivityBonus** (default 500) |
| **Efficiency Bonus** | Remaining road budget at end (future: proportional bonus) |

Scoring starts on `StartScoring()` (called by GameMode on Simulation begin) and stops on `StopScoring()` (on Evaluation). Final score computed in `ComputeFinalScore()`.

---

### 2.8 Player & Camera System

#### CityFlowPawn

A `ACharacter` subclass configured for top-down free-flight control with orientation-based movement:

| Feature | Implementation |
|---|---|
| Movement mode | `MOVE_Flying` (gravity-free, any-axis movement) |
| Input | `Enhanced Input` → `IA_Move` (Axis2D), `IA_Look` (Axis2D), `IA_Zoom` (Axis1D), `IA_Alt` (Digital) |
| Movement direction | Derived from `CameraYaw` (set by Blueprint from camera orientation) — WASD moves relative to the player's facing direction, not the live camera rotation |
| Camera orientation | `CameraYaw` (BlueprintReadWrite, float) — Blueprint updates this each tick from the live camera yaw; `Move()` builds `FRotator(0, CameraYaw, 0)` for forward/right vectors |
| Alt + Mouse look | `IA_Alt` + `IA_Look` — holding Alt sets `bAltHeld = true`, switches to `FInputModeGameOnly()` (captures mouse), drives `AddControllerYawInput()` from mouse delta (yaw only in C++; pitch handled in Blueprint), and **disables placement** (`DisablePlacement()`) so the cursor and preview actor do not interfere with camera rotation. Releasing Alt restores `FInputModeGameAndUI` + mouse cursor, and **re-enables placement only if the current phase is `Planning`** (avoids accidentally enabling placement during Simulation). |
| Scroll wheel zoom | `IA_Zoom` adjusts `TargetSpringArmLength` (clamped [Min, Max]). Blueprint reads this variable each tick to drive spring arm length interpolation |
| Configurable (Blueprint) | `MoveSpeed`, `LookSensitivity`, `ZoomSpeed`, `MinSpringArmLength`, `MaxSpringArmLength`, `DefaultCameraPitch`, `MinCameraPitch`, `MaxCameraPitch` |
| Camera setup | Handled in Blueprint: `USpringArmComponent` + `UCameraComponent` as child components; spring arm uses `bUsePawnControlRotation = true`; character auto-possesses |

**Key variables maintained by C++ for Blueprint consumption:**

| Variable | Default | Description |
|---|---|---|
| `CameraYaw` | 0 | Current facing yaw — Blueprint updates from camera; `Move()` computes movement from this |
| `TargetSpringArmLength` | 10000 | Desired spring arm length — Blueprint reads and interpolates toward this |
| `DefaultCameraPitch` | -60 | Initial camera pitch set on BeginPlay via `SetControlRotation` |
| `MinCameraPitch` | -80 | Minimum camera pitch (most top-down) |
| `MaxCameraPitch` | -30 | Maximum camera pitch (most horizontal) |

#### CityFlowPlayerController

| Feature | Implementation |
|---|---|
| Cursor | `bShowMouseCursor = true` (managed by Pawn: hidden during Alt, restored on release) |
| Preview system | Spawns a preview actor on `BeginPlay`; follows cursor via `Tick()` → `GetHitResultUnderCursor()` → `SnapToGrid()`; each tick calls `SetPreviewPlacementValid()` for validity, then `UpdatePreviewAppearance()` to let `ARoadTile` show the predicted mesh in preview |
| Placement | `IA_PlaceItem` (left mouse button) → `Started`/`Triggered`/`Completed` events → `TryPlaceAtCursor()` helper with `LastPlacedGridPos` deduplication for drag-to-place |
| Removal | `IA_RemoveItem` (right mouse button) → `Started`/`Triggered`/`Completed` events → `TryRemoveAtCursor()` helper with `LastRemovedGridPos` deduplication for drag-to-remove. Looks up the actor from `Cell.RoadActor` in the grid instead of relying on collision hit. |
| Configurable (Blueprint) | `PlaceableActorClass` (any `AGridPlaceableActor` subclass); `IA_PlaceItem`, `IA_RemoveItem`, `IA_Pause` |
| Pause | `IA_Pause` → `OnPausePressed` → `HUD::TogglePause()` — toggles pause overlay and `SetGamePaused` |

#### Placement Toggle

`ACityFlowPlayerController` provides a placement on/off switch for coordinating with other systems (L-system, simulation):

| API | Description |
|---|---|
| `EnablePlacement()` | Resumes cursor sampling, spawns a new preview actor, shows mouse cursor |
| `DisablePlacement()` | Stops cursor sampling, destroys the preview actor |
| `IsPlacementEnabled()` | Queries current placement toggle state |

When placement is disabled, `Tick()` skips `UpdatePreviewPosition()` and both `TryPlaceAtCursor()` / `TryRemoveAtCursor()` are no-ops. Placement is automatically disabled when simulation starts and re-enabled on restart.

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

### 2.10 Road Budget System

#### Implementation Status: ✅ Implemented

A **shared road budget** pool is tracked by `GridManager::RoadBudget`. Both player placement and L-system growth consume from the same pool.

**Budget Flow:**
1. `CityFlowGameMode::BeginPlay()` sets `GridManager::SetRoadBudget(TotalRoadBudget)`.
2. `GridManager::OccupyCell()` for `ECellType::Road` decrements `RoadBudget`; returns `false` when budget exhausted (prevents placement).
3. `LSystemManager::StartGenerate()` reads current budget from `GridManager::GetRemainingBudget()`; each `ProcessGrowthStep()` re-syncs.
4. GameMode exposes split-tracked `PlayerBudget` / `LSystemBudget` for UI display but actual enforcement is through GridManager.

**API:**
| Method | Description |
|---|---|
| `SetRoadBudget(int32)` | Sets absolute budget value |
| `GetRemainingBudget()` | Returns current remaining |
| `ConsumeRoadBudget(int32)` | Attempts to deduct |
| `AddRoadBudget(int32)` | Adds to budget (debug/cheat) |

**ClearCell Refund:** `ClearCell()` restores +1 to `RoadBudget` when the cleared cell was `ECellType::Road`, maintaining budget symmetry with `OccupyCell`. This means deleting road tiles refunds budget, and L-system-depleted budgets can be replenished by removing roads.

---

### 2.11 GameMode State Machine

#### Implementation Status: ✅ Implemented — v0.7 deferred init

`ACityFlowGameMode` owns the game lifecycle via `ECityFlowGamePhase`:

| Phase | Transition | Actions |
|---|---|---|
| **None** → **Planning** | `StartNewGame()` (called by HUD on "Start Game") | Init grid, spawn default buildings, set budget |
| **Planning** → **Simulating** | `StartSimulationPhase()` (UI/Cheat) | Lock road placement, start VehicleManager spawning + ScoringManager, start simulation timer |
| **Simulating** → **Evaluation** | Timer expiry or `EndSimulationPhase()` (UI/Cheat) | Stop spawning, finalize scoring, broadcast events |
| **Evaluation** → **Planning** | `RestartPlanningPhase()` (UI/Cheat) | Clear vehicles, reset budget, re-enable placement |
| **Any** → **None** | `ReturnToMainMenu()` (HUD) | Stop timers, destroy all placed actors, reset grid, abort L-system, clear phase |

`BeginPlay()` now only sets budget — scene creation is **deferred** to `StartNewGame()`, triggered by HUD when the player clicks "Start Game" on the main menu.

**New APIs:**

| Method | Description |
|---|---|
| `StartNewGame()` | Initialises default scene and transitions to Planning. Guards: only from `None` phase. |
| `ReturnToMainMenu()` | Full cleanup: stops spawning/vehicles/scoring/timer, destroys all `AGridPlaceableActor` via `TActorIterator`, re-initialises grid, calls `LSystemManager::AbortGeneration()`, returns phase to `None`. |

**Removed from GameMode:** `GameWidgetClass` and `GameWidgetInstance` — HUD is now the sole widget lifecycle owner.

**Blueprint-Configurable Properties:**
- `BuildingDataAsset` — `UBuildingDataAsset*` for weighted building spawn (primary; fallback: `OriginBuildingClass`/`DestinationBuildingClass`)
- `VehicleDataAsset` — `UVehicleDataAsset*` for weighted vehicle spawn (primary; fallback: `DeveloperSettings::DefaultVehicleDataAsset`)
- `OriginBuildingClass` / `DestinationBuildingClass` — building BP classes (legacy fallback)
- `RoadTileClass` — road tile BP class
- `TotalRoadBudget`, `LSystemBudgetShare` — budget split
- `SimulationDuration`, `DefaultBuildingCount`, `DefaultGridWidth/Height/CellSize`
- `DrivingSide` — `ECityFlowDrivingSide` (RightHand / LeftHand)
- `LaneOffsetFactor` — float (0.0~0.45, default 0.2)

**Events:** `OnGamePhaseChanged`, `OnPlanningPhaseEnd`, `OnSimulationPhaseEnd`

---

### 2.12 UI System

#### Implementation Status: ✅ Implemented — v0.8 full widget lifecycle with evaluation & countdown

CityFlow's UI is managed by **ACityFlowHUD** as the sole widget lifecycle owner. Widgets follow a main-menu-first flow with four states.

#### Widget Lifecycle

```
StartWidget (主菜单)
  ├─ Btn_StartGame → HUD::ShowGameWidget() → GameMode::StartNewGame()
  ├─ Btn_RandomMode → HUD::ShowGameWidgetRandom() → StartNewGame + EnablePlacement
  ├─ Btn_QuitGame → Quit
  ↓
GameWidget (规划/模拟 HUD 覆盖层)
  ├─ [Planning] Btn_TriggerLSystem / Btn_StartSimulation
  ├─ [Simulating] Btn_RestartPlanning (回到 Planning)
  ├─ Esc → HUD::TogglePause()
  ├─ Txt_Countdown: MM:SS countdown during Simulation
  ↓
PauseWidget (Overlay, ZOrder=100)
  ├─ Btn_Resume → HUD::HidePauseOverlay()
  └─ Btn_ReturnToMain → HUD::ReturnToMainMenu() → StartWidget
  ↓
EvaluationWidget (结算)
  ├─ Btn_BackToMain → HUD::HandleEvaluationReturn() → StartWidget
  └─ Btn_Restart → HUD::HandleRestartClicked() → RestartPlanning → GameWidget
```

**ACityFlowHUD** — central widget manager:
- `BeginPlay()` shows `StartWidget` (main menu); listens to `GameMode::OnSimulationPhaseEnd` to auto-show `EvaluationWidget`.
- `TogglePause()` / `ShowPauseOverlay()` / `HidePauseOverlay()` — pause with `FInputModeUIOnly`, resume with `FInputModeGameAndUI`.
- `ReturnToMainMenu()` — Blueprint-callable; cleans up and returns to `StartWidget`.
- `HandleReturnToMainClicked()` — pause → `GameMode::ReturnToMainMenu()` → `ShowStartWidget()`.
- `HandleEvaluationReturn()` — evaluation → `GameMode::ReturnToMainMenu()` → `ShowStartWidget()`.

**Blueprint-configurable Widget classes (on HUD):**

| Property | Type | Purpose |
|---|---|---|
| `StartWidgetClass` | `TSubclassOf<UCityFlowStartWidget>` | Main menu widget |
| `GameWidgetClass` | `TSubclassOf<UCityFlowGameWidget>` | Planning/Simulation HUD overlay |
| `PauseWidgetClass` | `TSubclassOf<UCityFlowPauseWidget>` | Pause menu overlay |
| `EvaluationWidgetClass` | `TSubclassOf<UCityFlowEvaluationWidget>` | Results screen |

#### CityFlowEvaluationWidget

`UCityFlowEvaluationWidget` is the results screen shown after simulation ends. It displays all relevant statistics and offers two actions.

**Displayed data (Populated from `ScoringManager` + `GameMode`):**

| Field | Source | Description |
|---|---|---|
| Total Score | `ScoringManager::GetTotalScore()` | `ArrivalScore - CongestionPenalty + FullConnectivityBonus` |
| Arrival Count | `ScoringManager::GetArrivalCount()` | Number of vehicles that reached destinations |
| Congestion Penalty | `ScoringManager::GetCongestionPenalty()` | Total penalty deducted |
| High Score | Static `GlobalHighScore` | Best score across all games this process lifetime |
| Simulation Time | `SimulationDuration - GameMode::GetSimulationTimeRemaining()` | Elapsed time in `MM:SS` |

**BindWidget controls:**
- `Txt_TotalScore` — total score display
- `Txt_Arrivals`, `Txt_Penalty`, `Txt_HighScore`, `Txt_SimulationTime` — BindWidgetOptional detail rows
- `Btn_BackToMain` → broadcasts `OnBackToMainClicked` (HUD → `HandleEvaluationReturn` → main menu)
- `Btn_Restart` → broadcasts `OnRestartClicked` (HUD → `HandleRestartClicked` → `GameMode::RestartPlanningPhase()` → GameWidget)

**Public API:**
| Method | Description |
|---|---|
| `Populate(TotalScore, Arrivals, Penalty, ElapsedTime)` | Sets all data and refreshes UI; auto-updates `GlobalHighScore` |

`ShowEvaluationWidget()` in HUD reads from `ScoringManager` and `GameMode`, then calls `Populate()`.

#### CityFlowStartWidget

Main menu widget with `BindWidget` controls:
- `Btn_StartGame` → broadcasts `OnStartGameClicked` (HUD listens → `HandleStartGameClicked` → `ShowGameWidget()`)
- `Btn_RandomMode` → broadcasts `OnRandomModeClicked` (HUD listens → `HandleRandomModeClicked` → `ShowGameWidgetRandom()` — starts game with placement enabled)
- `Btn_QuitGame` → broadcasts `OnQuitGameClicked`
- `Txt_Title`, `Txt_Version` (BindWidgetOptional) — display text

`ShowGameWidget()` and `ShowGameWidgetRandom()` both use `FInputModeGameAndUI` with `SetHideCursorDuringCapture(false)` to prevent the cursor from vanishing during click-and-drag.

#### CityFlowPauseWidget

Pause overlay widget with `BindWidget` controls:
- `Btn_Resume` → broadcasts `OnResumeClicked` (HUD listens → `HidePauseOverlay()`)
- `Btn_ReturnToMain` → broadcasts `OnReturnToMainClicked` (HUD listens → `ReturnToMainMenu()`)

#### CityFlowGameWidget (Planning / Simulation overlay)

`UUserWidget` C++ base using `BindWidget` for UMG controls:
- **Bound controls:** `Btn_TriggerLSystem`, `Btn_StartSimulation`, `Btn_RestartPlanning`, `Txt_Phase`, `Txt_Budget`, `Txt_Score`, `Txt_Countdown` (BindWidgetOptional)
- **Button auto-binding:** `NativeConstruct()` binds `OnClicked` via `AddDynamic` (callbacks are `UFUNCTION()` — required by `BindUFunction`); `NativeDestruct()` cleans up.
- **Button visibility:** `UpdateButtonStates(Phase)`:
  - Planning: `Btn_TriggerLSystem` + `Btn_StartSimulation` visible
  - Simulating: `Btn_RestartPlanning` visible
  - Otherwise: all hidden
- **Placement toggle:** `OnStartSimulationClicked` calls `PC->DisablePlacement()`; `OnRestartPlanningClicked` calls `PC->EnablePlacement()`.
- **Auto-updating text:** `HandleGamePhaseChanged`, `HandleScoreChanged`, `HandleLSystemStep` update `Txt_*` in C++. Budget reads from `GridManager::GetRemainingBudget()` via `HandleCellChanged` bound to `OnCellChanged`, ensuring real-time updates on every placement/removal.
- **Countdown timer:** When phase transitions to `Simulating`, `StartCountdown()` reads `SimulationDuration` and starts a 1-second recurring `TickCountdown()` (marked `UFUNCTION()`). Each tick decrements `CountdownSeconds` and updates `Txt_Countdown` to `MM:SS` format. The timer stops when seconds reach zero or the phase changes away from Simulating.
- **BlueprintImplementableEvents:** `OnPhaseChanged_BP`, `OnScoreChanged_BP`, `OnBudgetChanged_BP`, `OnSimulationTick_BP`, `OnEvaluation_BP`, `OnLSystemStep_BP`, `OnLSystemFinished_BP`.
- **Delegates bound in `NativeConstruct()`:** `GameMode::OnGamePhaseChanged`, `ScoringManager::OnScoreChanged`, `LSystemManager::OnGenerationStep`, `LSystemManager::OnGenerationFinished`, `GridManager::OnCellChanged`.

#### Ownership

HUD is the **sole widget lifecycle owner** — GameMode no longer creates widgets. GameMode's `GameWidgetClass` and `GameWidgetInstance` have been removed. The BP GameMode's old "Game Widget Class" value should be moved to the BP HUD's `GameWidgetClass`.

---

### 2.13 Debug Infrastructure

#### Console Commands (CityFlowCheatExtension)

All commands prefixed with `CF_`, accessible via console (~):

| Command | Description |
|---|---|
| `CF_StartSimulation` | Triggers simulation phase |
| `CF_EndSimulation` | Ends simulation early |
| `CF_RestartPlanning` | Returns to planning |
| `CF_TriggerLSystem` | Manually triggers L-system growth |
| `CF_SpawnVehicle` | Spawns a single test vehicle |
| `CF_ClearVehicles` | Destroys all vehicles |
| `CF_TogglePathDebug` | Toggles path line drawing |
| `CF_ToggleIntersectionDebug` | Toggles intersection box drawing |
| `CF_ToggleCongestionDebug` | Toggles congestion box drawing |
| `CF_SetBudget N` | Sets absolute budget |
| `CF_AddBudget N` | Adds budget |
| `CF_ShowGridStats` | Prints grid statistics |
| `CF_ShowVehicleStats` | Prints vehicle list and states |
| `CF_ShowScoreStats` | Prints scoring breakdown |
| `CF_SetSimulationSpeed X` | Sets time dilation |

#### Visual Debug (DeveloperSettings toggles)
- `bDebugDrawPaths` — Draws vehicle path lines + waypoints
- `bDebugDrawCongestion` — Draws red boxes on congested cells
- `bDebugDrawIntersections` — Draws orange/red boxes on intersections

#### DeveloperSettings (Config=Game)
`UCityFlowDeveloperSettings` defaults all gameplay parameters (`TotalRoadBudget`, `SimulationDurationSeconds`, `VehicleSpawnInterval`, `ArrivalScore`, `CongestionPenaltyPerSecond`, `CongestionThreshold`, etc.) with in-editor configuration via Project Settings → CityFlow.

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
