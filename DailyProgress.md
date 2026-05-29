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
