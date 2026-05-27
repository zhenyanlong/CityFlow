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
