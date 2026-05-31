## 2026-05-31

### Vehicle Spawn System (v0.1, preliminary debug passed)

- Redesigned UVehicleDataAsset from per-vehicle parameter table to vehicle class registry: added FVehicleSpawnEntry struct (TSubclassOf<AVehicleActor> + SpawnWeight), referenced via UCityFlowDeveloperSettings::DefaultVehicleDataAsset
- Updated UVehicleManager: CacheSpawnEntries() loads registry on simulation start, PickRandomVehicleClass() performs weighted random selection per spawn, SpawnVehicle() uses picked class instead of hardcoded base class
- Removed AVehicleActor::InitializeFromDataAsset(); each BP subclass now configures its own Mesh/MoveSpeed/DebugColor in Class Defaults
- Added USceneComponent VehicleRoot as root component with VehicleMesh attached as child, enabling local rotation/scale adjustment in Blueprint subclasses
- Added VehicleZOffset (default 30) to AVehicleActor: all waypoint positions offset by this value to place vehicles on top of road surface instead of inside it
- Vehicle root rotation follows movement direction each frame via SetWorldRotation(Yaw from MoveDir)
- Vehicles now auto-destroy on arrival (Destroy() called in VehicleManager tick after broadcasting OnVehicleArrived)
- Added default Cube mesh fallback in AVehicleActor constructor so vehicles are visible even without a configured DataAsset

### VehicleManager Improvements (v0.1, preliminary debug passed)

- Vehicle spawning retry logic: each tick randomly picks an origin, shuffles all destinations (Fisher-Yates), and iterates until a connected pair is found; if nothing connects, tries all other origins
- Destination randomization: a fresh shuffled destination list is generated per spawn tick, avoiding fixed origin-to-dest bindings
- Added comprehensive UE_LOG diagnostics at every SpawnVehicle failure point (null building, no doorway on road, A* path failed, movement plan invalid, PickRandomVehicleClass returned null, SpawnActor failed, success with path details)
- Added logging to CacheSpawnEntries (missing DataAsset config, empty entries array, loaded types summary) and PickRandomVehicleClass (no entries fallback warning)

### Budget System Bug Fixes

- Fixed OccupyCell: budget check was after Cell.Type/BuildingID/RoadActor writes; on budget exhaustion the Cell was permanently corrupted as Type=Road/Mask=0 without broadcasting OnCellChanged. Moved budget guard before any Cell mutation.
- Fixed RegisterCells: OccupyCell return value was silently dropped; PlaceOnGrid would transition actor to Placed state even when grid occupancy failed. Changed RegisterCells to return bool; PlaceOnGrid now checks result and rolls back on failure.
- Fixed AGridPlaceableActor::PlaceOnGrid: on RegisterCells failure, now calls ClearCell on any partially-occupied cells and resets GridPosition, preventing state pollution.

### PlayerController Bug Fix

- Fixed TryPlaceAtCursor: PlaceOnGrid return value was unchecked. On failure, the preview actor had collision enabled but was neither placed nor destroyed, leaking in the world, blocking raycasts and preventing further placement on most cells. Now only clears PreviewActor pointer and spawns new preview on success; reverts collision on failure.

### GameMode Configuration Fix

- Removed CityFlowGameMode::BeginPlay() overwriting GridWidth/GridHeight/CellSize/BuildingCount with DeveloperSettings defaults. BP GameMode Class Defaults are now authoritative for grid and building parameters.

### C++ Foundations (code-complete, not yet gameplay-verified)

- ACityFlowGameMode state machine (Planning→Simulating→Evaluation): BeginPlay scene init, building spawn delegation, phase transitions, simulation timer, budget split (Player vs L-system share)
- UScoringManager (UWorldSubsystem): arrival points tracking, per-second congestion penalty, full-connectivity bonus, final score computation
- ACityFlowHUD: GameWidget/EvaluationWidget lifecycle management
- UCityFlowGameWidget C++ base: BlueprintImplementableEvent callbacks for phase/score/budget/L-system, Blueprint-callable StartSimulation/EndSimulation/RestartPlanning/TriggerLSystem
- UCityFlowCheatExtension: 15 CF_* console commands (phase control, vehicle spawning/debug, budget, stats, simulation speed)
- UCityFlowDeveloperSettings (Config=Game, Project Settings panel): grid, budget, simulation, scoring, vehicles, debug flags
- Build.cs: added UMG, Slate, SlateCore, DeveloperSettings module dependencies
- Updated TDD.md and TDD_Chinese.md sections 1, 2.6, 2.7, 2.10, 2.11, 2.12, 2.13

## 2026-05-30

- Added EPlaceableType enum (Road/Building/Landscape) and EGridRotation enum (Rot0/Rot90/Rot180/Rot270) to CityFlowGridTypes.h
- Added RootSceneComponent (USceneComponent) to AGridPlaceableActor as root for managing child component relative positions; added PlaceableType property
- Changed AMeshGridPlaceableActor MeshComponent to attach to RootSceneComponent instead of being root
- Enhanced ABuilding: BuildingRotation supports grid rotation (Rot0/Rot90/Rot180/Rot270), CalculateOccupiedCells uses GetEffectiveBuildingSize
- Implemented TransformLocalPosition(): building-local coordinates transform with BuildingRotation
- Changed doorway RelativePosition to building-local coords (0-based), FacingDirection rotates with building; GetDoorwayConnectionPoint() computes world connection point (local + rotated direction offset)
- Fixed AutoGenerateDoorways to use internal coords; HasDoorwayAt/GetDoorwayWorldPositions unified via GetDoorwayConnectionPoint
- GridManager::CalculateConnectedMask now checks Building doorways: if neighbour is Building, checks HasDoorwayAt and includes in ConnectedMask
- Building auto-subscribes to OnDoorwayCellChanged on placement for live foundation refresh on road connect/disconnect
- Added DebugDoorwayMesh: spawns visual debug markers at each doorway connection point after placement
- Fixed FGridVector X/Y missing EditAnywhere specifier, preventing Blueprint editing of RelativePosition
- Added FBuildingSpawnRequest struct (BuildingClass + Count), TryPlaceBuildingRandom now supports random rotation, new TryPlaceBuildingsRandom for batch spawning
- Added ProceduralMeshComponent module dependency
- Implemented UFoundationComponent: procedural foundation mesh with top face, bottom face, and side walls (unique vertices + horizontal outward normals)
- Implemented Sidewalk border: rectangular ring with inner/outer walls, top face, and bottom face
- Blueprint configurable: FoundationHeight, Padding, CornerRadius, SidewalkWidth, SidewalkHeight, FoundationMaterial, SidewalkMaterial
- Fixed Z-axis layering: foundation body Z=0~FoundationHeight, sidewalk Z=FoundationHeight~+SidewalkHeight
- Fixed actor scale double-transformation: Foundation/Sidewalk meshes cancel parent scale via SetRelativeScale3D(1/S)
- Fixed wall normals: unique vertices + (Edge.Y, -Edge.X, 0) horizontal outward normals
- Fixed UV mapping: walls use cumulative UVOffset + edge-length ratio, top V=0 (no flip)
- Fixed triangle winding order: unified UE left-hand coordinate clockwise (CW) as front face
- Updated TDD.md and TDD_Chinese.md with section 2.4b FoundationComponent documentation
- Created ULSystemManager as a UWorldSubsystem for L-system capillary road generation
- Implemented selective start-point sampling: dead-end roads + spaced points on straight segments (corners/junctions skipped)
- Implemented breadth-first iterative queue growth strategy with FTimerHandle-based animation at configurable GrowthInterval
- Implemented multi-cell straight extension (StraightExtendLength configurable, default 3 cells per forward step)
- Implemented IsSideBranchValid() to prevent side branches from filling gaps between parallel roads
- Added attraction-biased sorting (DistScore + AlignScore blended by AttractionStrength) to steer branches toward unconnected buildings
- Added Blueprint-callable Setter/Getter for all parameters: RoadTileClass, BranchBudget, GrowthInterval, BranchProbability, AttractionStrength, StraightExtendLength, MinBranchSpacing
- Added event delegates: OnGenerationStarted, OnGenerationStep(int32), OnGenerationFinished(bool)
- Fixed depth-first and attraction-reversal bugs by changing Insert(NewPt,0) to Add(NewPt)
- Moved GetDoorwayConnectionPoint() and TransformLocalPosition() to public in ABuilding
- Refactored LSystemManager from AActor to UWorldSubsystem for consistency with GridManager
- Updated TDD.md and TDD_Chinese.md section 2.5 with full implementation details
- Updated TDD section 1 architecture overview to reflect LSystemManager as UWorldSubsystem
- Added FoundationCollisionProfileName and SidewalkCollisionProfileName to UFoundationComponent with dropdown options from GetCollisionProfileOptions()
- Updated TDD.md and TDD_Chinese.md section 2.4b with collision profile configuration

## 2026-05-26

- Defined grid core types: ECellType, EGridDirection, FGridCell, FGridVector, GridDirectionUtils
- Implemented GridManager (UWorldSubsystem) with grid init, coordinate conversion, placement validation, neighbor queries, and connected mask calculation
- Created AGridPlaceableActor (Abstract, state mgmt: preview/placed) → AMeshGridPlaceableActor (StaticMesh + multi-slot preview material swap) → ATestGridPlaceableActor
- Created CityFlowPawn (ACharacter, MOVE_Flying, WASD via Enhanced Input) with blueprint-configurable camera parameters
- Created CityFlowPlayerController with left-click raycast placement and live preview actor following cursor each tick
- Added GridVisualizer and GridPlaneVisualizer for editor grid line and ground plane rendering
- Organized source into Grid/, Player/, Test/ subdirectories

## 2026-05-27

- Implemented AGridVisualizer using ULineBatchComponent for persistent runtime grid line rendering with configurable color, thickness, and Z-offset via Blueprint
- Implemented AGridPlaneVisualizer using a single Plane mesh with world-aligned dynamic material instance, passing CellSize/LineWidth/LineColor/Origin via material parameters at runtime
- Designed and documented material node graph using M_PrototypeGrid with Translucent blend mode for efficient single-drawcall grid line rendering
- Provided DrawGrid / ClearGrid / RedrawGrid / SetGridVisible Blueprint APIs for both visualizers

## 2026-05-28

- Added InvalidPreviewMaterial support: preview actor shows distinct material when hovering over occupied grid cells via SetPreviewPlacementValid() and OnPreviewValidChanged callback
- Implemented right-click removal of placed actors using grid-based reverse lookup (Cell.RoadActor) instead of collision hit detection
- Enabled drag-to-place: bound IA_PlaceItem to Started/Triggered/Completed events with LastPlacedGridPos deduplication for continuous placement while holding left mouse button
- Enabled drag-to-remove: bound IA_RemoveItem to Started/Triggered/Completed events with LastRemovedGridPos deduplication for continuous removal while holding right mouse button
- Refactored placement and removal logic into TryPlaceAtCursor() and TryRemoveAtCursor() helper methods with debug screen messages
- Registered AGridPlaceableActor this-pointer in GridManager::OccupyCell() to enable grid-cell-to-actor reverse lookup
- Fixed compilation error on UE 5.6 by removing Deprecated specifier from AGridVisualizer
- Updated TDD.md and TDD_Chinese.md to reflect implemented features (preview validity, drag-to-place/remove, right-click removal)
- Created ARoadTile (inherits AMeshGridPlaceableActor) with FRoadMeshConfig struct: CanonicalMask + Mesh + ScaleMultiplier(FVector) per-entry
- Implemented FindMeshConfig() with 90° clockwise mask rotation to match any orientation variant from a single canonical model
- Implemented UpdateAppearance() to auto-switch Mesh / Rotation / Scale based on Grid cell ConnectedMask with per-axis ScaleMultiplier
- Added ReferenceCellSize for CellSize-relative base scaling; auto Swap(ScaleMultiplier.X,Y) on 90°/270° rotation for vertical straight roads
- Subscribed ARoadTile to GridManager::OnCellChanged delegate for automatic neighbour refresh on placement / removal
- Defined CanonicalMask values: DeadEnd=8(Right), Straight=12(Left+Right), Corner=10(Down+Right), TJunction=14(Down+Left+Right), Cross=15(All)
- Updated TDD.md and TDD_Chinese.md with correct CanonicalMask mapping table and rotation/scale strategy

## 2026-05-29

- Fixed a model bug in SM_Road_Main by importing a new SM_Road_Main_Extend mesh from Blender
- Added virtual UpdatePreviewAppearance() method to AGridPlaceableActor base class (default empty)
- Overrode UpdatePreviewAppearance() in ARoadTile: preview actor now predicts ConnectedMask via GridManager::CalculateConnectedMask and switches mesh/rotation/scale via FindMeshConfig with preview material
- Added MeshMaterialCache (TMap<UStaticMesh*, TArray<UMaterialInterface*>>) to ARoadTile, lazily caching original materials per-mesh from UStaticMesh::GetStaticMaterials()
- Overrode OnEnterPlaced() and OnPreviewValidChanged() in ARoadTile to prevent material flipping between preview/invalid/original states
- Added EnsureMeshMaterialsCached() + RestoreMeshMaterials() in UpdateAppearance() for correct material restoration during neighbour updates
- Called UpdatePreviewAppearance() every tick in CityFlowPlayerController::UpdatePreviewPosition()
- Updated TDD.md and TDD_Chinese.md with preview appearance and MeshMaterialCache architecture

- Created ABuilding class inheriting AMeshGridPlaceableActor with multi-cell grid placement support
- Added FBuildingDoorway struct (RelativePosition + FacingDirection) for configurable doorway definition
- Implemented AutoGenerateDoorways() to compute mid-point doorways one cell outside each building side
- Implemented GetDoorwayWorldPositions() for doorway-to-absolute-grid-coordinate conversion
- Implemented UpdatePreviewAppearance() override for real-time building preview with centering and scaling
- Implemented UpdateBuildingAppearance() for placed-state visual centering and mesh scaling based on BuildingSize and CellSize
- Added bIsDestination flag to distinguish origin (residential) vs destination (commercial) buildings
- Added public GetBuildingSize() accessor to AGridPlaceableActor
- Added UGridManager::TryPlaceBuildingRandom() for debug random building placement from Level Blueprint
- Updated TDD.md and TDD_Chinese.md with ABuilding class hierarchy and full 2.4 section documentation
- Updated class hierarchy diagrams in both TDD versions to include ARoadTile and ABuilding
