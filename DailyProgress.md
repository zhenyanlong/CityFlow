## 2026-06-20

- Added a Random Mode difficulty-selection step with Easy, Medium, and Hard profiles for building count, vehicle spawn interval, simulation duration, and road budget.
- Added `UCityFlowDifficultyWidget` with Blueprint-bindable controls, one hover-driven localized profile-details block, Back handling, and a complete native fallback layout when no Widget Blueprint is configured.
- Preserved the no-argument `StartRandomPlanningGame()` API as a Medium-difficulty compatibility path and added explicit difficulty-aware GameMode APIs.
- Reworked vehicle spawning around target active populations, burst refill pulses, per-attempt blocked-origin checks, route fallback, and hard population caps.
- Updated simulation countdown and runtime scoring inputs to use active per-match duration and spawn interval values.
- Added configurable default traffic-density controls to CityFlow Developer Settings and completed a successful `CityFlow Win64 Development` build.
- Updated the bilingual GDD and TDD with the difficulty curve, traffic-density design, runtime data flow, and Blueprint configuration contract.
- Verified the standalone game target successfully; the Editor target compiled all source actions but could not relink `UnrealEditor-CityFlow.dll` while a running Unreal Editor process held the module open.
- Added player-only placement SFX support with explicit SoundClass override so procedural building and L-system placement remain silent.
- Added `ABuildingShowcaseDirector` to initialize a real grid and automatically arrange every BuildingDataAsset class through the production `PlaceOnGrid()` path.
- Added optional all-rotation display, class deduplication, additional building classes, automatic grid expansion, spawn-animation control, and GridVisualizer creation for showcase maps.
- Added `ACityFlowShowcaseGameMode` to isolate manually authored showcase levels from the normal HUD and automated title-preview flow.
- Completed a successful `CityFlow Win64 Development` build for the showcase and placement-audio changes.

## 2026-06-19

### Main Menu, Tutorial, Settings, and Audio

- Removed the Start Game flow and added Tutorial and Settings entry points while retaining Random Mode and the animated title-screen preview.
- Added data-driven tutorial support through `UCityFlowTutorialDataAsset`, localizable tutorial entries, optional per-entry images, and `UCityFlowTutorialWidget` Blueprint extension hooks.
- Added persistent master-volume, SFX-volume, and language settings through `UCityFlowSettingsWidget` and SaveGame storage, with native Unreal culture switching.
- Added HUD-managed background music and SoundClass override support; individual sound assets still need correct SoundClass routing for the global SFX slider to affect them.

### Connectivity-Safe L-System Refactor

- Refactored `ULSystemManager` into a hybrid generator that reserves doorway-to-primary-road-component connection paths before spending surplus budget on organic branches.
- Replaced the old reachability assumption with true same-component validation, reusable existing-road paths, multi-source doorway searches, and exact connection-budget reservation.
- Added rotated doorway direction handling, topology-safe dead-end and straight-segment branch candidates, deduplicated priority growth, global doorway attraction, deterministic `FRandomStream` use, and a strict branch budget cap.
- Preserved the existing public controls and delegates, kept player-triggered generation within `LSystemBudgetShare`, and allowed automated title previews to use the full remaining budget needed for connectivity.
- Verified the revised generator in runtime smoke tests with all buildings connected and clean generation termination.

### Localization and Runtime Stability

- Converted player-facing native Source text to `LOCTEXT` or `NSLOCTEXT`, with dynamic values produced through culture-aware `FText` formatting.
- Verified Source localization gathering with 49 extracted entries and no namespace/key conflicts, and completed a successful editor build.
- Fixed the vehicle-death delegate binding by exposing its handler as a `UFUNCTION`.
- Hardened GridManager and ScoringManager teardown so late subsystem shutdown cannot index an empty grid or attempt final scoring after grid destruction.
- Verified a natural application-exit smoke test completed without the previous array-bounds assertion.

### Documentation

- Updated the English and Chinese GDD with the assisted capillary-road rules and the revised main-menu, tutorial, settings, audio, and localization experience.
- Updated the English and Chinese TDD with the L-system connection planner, organic-growth mechanics, menu/widget architecture, audio persistence and routing, native text localization, and teardown safety.

## 2026-06-16

### GDD-aligned Final Score Refactor

- Added `FCityFlowScoreBreakdown` as the shared final evaluation report data structure.
- Refactored `UScoringManager` to compute GDD-style final score categories at Evaluation: connectivity, traffic outcome, travel efficiency, budget efficiency, runtime, and map difficulty multiplier.
- Added vehicle travel-time and path-cell tracking so arrived vehicles contribute to average cell travel time.
- Updated `CF_ShowScoreStats` to print final score, raw score, category breakdown, planning stats, traffic stats, and map difficulty multiplier.

### Evaluation Report UI

- Updated `UCityFlowEvaluationWidget` to consume `PopulateFromBreakdown()` and render the full score report.
- Added optional `ScoreReportPanel` dynamic row generation with left-aligned descriptions, right-aligned values, and outlined text.
- Added sequential number-roll animation so each report line counts up before the next line appears.
- Added image-based three-star rating support with configurable filled/empty star textures and automatic `StarRatingPanel` generation.
- Made `Txt_TotalScore` optional and confirmed missing bindings are skipped safely.
- Updated TDD.md and TDD_Chinese.md with the final scoring and evaluation UI implementation details.

### Camera and UI State Regression Fixes

- Kept `ACityFlowPawn` Tick enabled so Blueprint camera pitch and zoom interpolation continues outside the main menu.
- Added `ResetToInitialViewState()` and `StopCameraMovement()` to reset title/game camera state and clear movement velocity during UI transitions.
- Updated HUD transitions so returning to Main Menu resets the pawn location, entering gameplay resets title-screen yaw, and Evaluation/Pause flush held movement input.
- Updated TDD.md and TDD_Chinese.md with the camera/input state safety flow.

### Simulation Live Score Fix

- Restored simulation-phase live score updates in `UScoringManager` using `ArrivalScoreTotal - CongestionPenaltyTotal - DeathPenaltyTotal`.
- Broadcast `OnScoreChanged(0)` when scoring starts and rebroadcast after arrivals, deaths, and congestion penalties so `Txt_Score` updates in real time.
- Preserved the GDD-style final score report for Evaluation while keeping the live HUD score as immediate simulation feedback.
- Updated TDD.md and TDD_Chinese.md to document the difference between live HUD score and final evaluation score.

### Building Marker UI

- Added `UBuildingMarkerWidget` with native text fallback and Blueprint extension hooks for on-screen and off-screen marker states.
- Integrated building marker lifecycle into `UCityFlowGameWidget`, collecting placed `ABuilding` instances from `GridManager` and deduplicating multi-cell buildings by `BuildingID`.
- Implemented per-frame world-to-widget projection so visible buildings show markers at their screen position while off-screen or behind-camera buildings clamp to the screen edge and rotate toward their direction.
- Added optional `BuildingMarkerLayer`, marker widget class, edge padding, world offset, and phase-specific visibility toggles to the game HUD.
- Verified the building marker C++ implementation with a successful `CityFlowEditor Win64 Development` build.
- Updated TDD.md and TDD_Chinese.md with the screen-space building marker design and configuration flow.

### Foundation Sidewalk Scale Fix

- Fixed `UFoundationComponent::BuildSidewalk()` to use the explicit target owner scale passed from `RefreshFoundation()` instead of reading `Owner->GetActorScale3D()` during rebuild.
- Kept `FoundationMesh` and `SidewalkMesh` on the same parent-scale cancellation path so sidewalk size remains tied to the building footprint during spawn-scale animation or transient actor scaling.
- Updated TDD.md and TDD_Chinese.md with the sidewalk scale-safety behavior.
- Full build verification after this fix was blocked because Unreal Live Coding was active.

## 2026-06-15

### Main Menu Preview and Random Planning Flow (v0.17)

- Added an automated randomized title-screen preview match that generates scenery, buildings, L-system roads, and simulation using existing systems.
- Split player-facing Random Mode into `StartRandomPlanningGame()`, which only randomizes scenery/buildings and stops in Planning for player road design.
- Updated Evaluation restart to start a fresh randomized Planning game instead of simply returning to the previous Planning phase.
- Added randomized auto-match parameter ranges for grid size, building count, and road budget on `ACityFlowGameMode`.

### Main Menu Camera Rotation

- Added `ACityFlowPawn::SetMainMenuCameraYawRotationEnabled()` and `MainMenuCameraYawSpeed` for slow title-screen camera yaw rotation.
- Wired HUD state transitions so the main menu enables camera rotation while gameplay and evaluation screens disable it.
- Kept pawn Tick disabled by default and enabled it only while the main-menu camera rotation is active.

### Landscape Decoration Root Actor Fix

- Fixed a PIE crash when rapidly restarting Random Mode by removing the fixed UObject spawn name for the landscape decoration root actor.
- Preserved the editor-facing `CityFlowLandscapeDecorations` actor label while allowing Unreal to generate unique runtime object names.

### Documentation Updates

- Updated TDD.md and TDD_Chinese.md with the title preview flow, Random Mode planning flow, main-menu camera rotation, placement lifecycle changes, and landscape root actor naming fix.

## 2026-06-14

### Environment Decoration and Grass Coverage Investigation

- Added runtime Landscape decoration documentation for `UCityFlowLandscapeDecorationManager`, grass coverage sampling, and HISM instance cleanup.
- Recorded the current open issue where grass sparse-density contrast remains visually unclear despite PIE logs showing distinguishable `G/R` sampling ratios.
- Captured the latest grass sampling diagnostic baseline: `RatioObserved=(0.674, 0.981, 1.202)` with separate `BelowMin`, `Transition`, and `Full` sample counts.
- Identified likely next investigation targets: grass mesh scale, `DensityPerCell`, per-cell random sampling washout, and possible cluster or density-map based generation.
- Updated TDD.md and TDD_Chinese.md with the environment decoration design and open issue.

## 2026-06-13

### Debug Screen Message Toggles

- Gated intersection-related `AddOnScreenDebugMessage` output behind `UCityFlowDeveloperSettings::bDebugDrawIntersections`, including intersection lock reject/grant, enter/exit overlap, and deadlock lock-release messages
- Added `bDebugVehicleAbilities` to `UCityFlowDeveloperSettings` with default `false`
- Added `CF_ToggleVehicleAbilityDebug` to `UCityFlowCheatExtension`
- Gated rampage and teleport vehicle ability screen debug messages behind `bDebugVehicleAbilities`
- Verified with `CityFlowEditor Win64 Development` build
- Updated TDD.md and TDD_Chinese.md section 2.13 with the new debug controls

### Vehicle Hover Destination Indicator (v0.16)

- Added simulation-only vehicle hover detection in `ACityFlowPlayerController`, using the Vehicle trace channel with a Visibility fallback
- Added `AVehicleActor::SetHovered()` to toggle CustomDepth/CustomStencil outline state and a destination arrow widget
- Applied hover stencil state to all vehicle `UPrimitiveComponent` children, covering blueprint child meshes while excluding `DestinationArrowWidget` and `PathSpline`
- Added `DestinationArrowWidget` support with configurable hover stencil value, arrow height, and rotation offset
- Documented the accepted limitation that post-process outlines may render over world-space 3D widgets
- Updated TDD.md and TDD_Chinese.md section 2.6 with the hover indicator design

## 2026-06-12

### Teleport Vehicle Type (v0.15)

- Added ATeleportVehicle, a vehicle subclass that teleports forward along its spline when wait timeout expires instead of dying
- Added TeleportMinDistance, TeleportMaxDistance, TeleportOverlapRadius, TeleportBeforeVFX, and TeleportAfterVFX Blueprint-configurable properties for timeout teleport tuning
- Added TeleportVFXScale and TeleportVFXScaleParamName so teleport VFX can receive a Niagara scale parameter like death VFX
- Added AVehicleActor::KillOverlappingVehicles() helper to kill active vehicles overlapping the teleport destination through the existing death pipeline
- Fixed forward probe and rampage ram-kill sweep endpoints to sample from the current spline rather than actor transform/velocity
- Added urgent red material flashing while ARampageVehicle is in berserk mode
- Updated TDD.md and TDD_Chinese.md section 2.6 with Teleport Vehicle documentation

### Screen-Space Score Popup Feedback

- Reworked vehicle score popups from world-space WidgetComponent actors into screen-space UMG feedback owned by `UCityFlowGameWidget`
- Added `UScoringManager::OnScorePopupRequested(WorldLocation, DeltaScore)` so scoring reports signed score deltas without spawning UI actors
- Added `UScorePopupWidget` with per-frame world-to-widget projection, rise/fade/scale animation, and native TextBlock fallback
- Added optional `PopupLayer` CanvasPanel support and `ScorePopupWidgetClass` configuration to `UCityFlowGameWidget`
- Fixed death popup reliability by binding `UScoringManager` directly to each spawned vehicle's `OnVehicleDeath`, while keeping the `VehicleManager::OnVehicleDied` fallback and deduplicating with `ScoredDeathVehicles`
- Updated TDD.md and TDD_Chinese.md scoring/UI sections with the popup event flow and screen-space rendering design

## 2026-06-09

### Vehicle Death & Stop Flash System (v0.12)

- Implemented virtual-method-based stop/death pipeline on AVehicleActor: OnVehicleStopped(), OnVehicleResumed(), HandleVehicleDeath(), ShouldResetStopTime()
- Added TotalStopTime accumulation independent of CongestionWaitTime (deadlock release)
- Implemented material red flash via UMaterialInstanceDynamic with accelerating sine-driven emissive (0.5→4 Hz) controlled by ScalarParameter "FlashIntensity"
- Added explosion death sequence: Niagara VFX spawn with SetVariableFloat for scale, SoundBase SFX, proximity-scaled CameraShake
- Added blueprint-configurable death properties: DeathTimeout (5s), bEnableTimeoutDeath, ExplosionVFX, ExplosionVFXScale, ExplosionVFXScaleParamName, ExplosionSFX, DeathCameraShake, DeathShakeMaxDistance
- Added FOnVehicleDeath delegate on AVehicleActor and FOnVehicleDied delegate on UVehicleManager
- Integrated OnVehicleDeathHandler in VehicleManager to remove dead vehicles from ActiveVehicles before Destroy()
- Added death penalty scoring in UScoringManager: DeathPenaltyTotal tracking, TotalScore = ArrivalScore - CongestionPenalty - DeathPenalty
- Added DeathPenalty (default 50) to UCityFlowDeveloperSettings
- Added Niagara module dependency to Build.cs
- Fixed VFX auto-cleanup: Niagara system LoopBehavior must be set to Once; ENCPoolMethod::None + bAutoDestroy=true
- Fixed VFX scale: use SetVariableFloat() to push scale value directly to Niagara User Parameter instead of SetWorldScale3D
- Updated TDD.md and TDD_Chinese.md section 2.6 with Vehicle Death & Stop Flash System (v0.12)

### Building Origin/Destination Decoupling (v0.13)

- Removed bIsDestination-based origin/destination split in CollectOriginDestinations() — all buildings now serve as both
- Updated StartSpawning() guard to require 2+ buildings instead of non-empty origin+destination arrays
- Updated CF_SpawnVehicle cheat command to pick any two different buildings without bIsDestination check
- Updated TDD.md and TDD_Chinese.md section 2.7 with v0.13 change

### Intersection Occupancy Indicator (v0.13)

- Added IndicatorPlane (UStaticMeshComponent) to ARoadTile using engine built-in Plane mesh
- Implemented UpdateIndicator() for position/size/visibility management with scale compensation
- Implemented UpdateIndicatorState() using IsAnyDirectionOccupied() for green/red colour switching via DMI
- Hooked indicator refresh into 5 event points: UpdateIntersectionBox, BeginOverlap, EndOverlap, SanitizeOccupants, ExpirePendingReservations
- Added 6 blueprint-configurable properties: IndicatorMaterial, IndicatorSize (0.4), IndicatorZOffset (80), IndicatorFreeColor (green), IndicatorOccupiedColor (red)
- Fixed Plane scale: divided by 100 to compensate for engine Plane's default 100x100 world size
- Updated TDD.md and TDD_Chinese.md section 2.6 with Intersection Occupancy Indicator (v0.13)

## 2026-06-08

### Building & Vehicle Spawn DataAsset Refactor (v0.10)

- Created UBuildingDataAsset (UPrimaryDataAsset) with FBuildingDataEntry (BuildingClass + SpawnWeight) and single BuildingEntries array
- Origin/destination roles determined by building BP's own bIsDestination flag, eliminating separate origin/destination arrays
- Implemented deterministic building spawn count allocation using largest-remainder method: floor(weight/totalWeight × DefaultBuildingCount) with remainder distributed by fractional part
- Added BuildingDataAsset and VehicleDataAsset properties to ACityFlowGameMode with fallback to legacy single-class properties
- Added UVehicleManager::SetVehicleDataAsset() and ExternalVehicleDataAsset member; CacheSpawnEntries() now prefers external DataAsset over DeveloperSettings
- Removed unused VehicleClass property from GameMode

### Congestion & Intersection Bug Fixes (v0.11)

- Fixed VehicleGridMap congestion detection bug: replaced broken `TMap<FGridVector, AVehicleActor*>` with per-tick `TMap<FGridVector, int32>` counting in `UpdateCongestion()`, removed dead code `UpdateVehicleGridOccupancy()` and `IsOccupiedByVehicle()`
- Added deadlock timeout mechanism: `AVehicleActor` accumulates `CongestionWaitTime` in `WaitingCongestion`, releases all intersection reservations when exceeding `DeadlockTimeout` (default 3s), breaking adjacent intersection hold-and-wait cycles
- Fixed forward probe zero-distance filtering: changed `ProjDist > 0.0f` → `>= 0.0f` and `InterDist <= 0.0f` → `< 0.0f` so vehicles starting inside an intersection box (e.g., doorway cell is an intersection) can correctly acquire locks
- Fixed foundation scale bug during spawn animation: `BuildFoundation` now accepts explicit `InOwnerScale` parameter from `RefreshFoundation` instead of reading `Owner->GetActorScale3D()` at build time, preventing giant foundation meshes when rebuilt mid-animation
- Updated TDD.md and TDD_Chinese.md sections 2.6 (congestion detection, deadlock timeout, zero-distance probe fix, updated movement flow) and 2.4b (foundation scale fix)
- Implemented ABuilding::ValidatePlacement() override: validates doorway connection points are in-bounds and not occupied by other buildings
- Added GetDoorwayConnectionPointForPosition() helper for pre-placement doorway validation
- Updated TDD.md and TDD_Chinese.md sections 2.6, 2.7, and 2.11 with v0.10 changes

## 2026-06-07

### UI System Refinement (v0.8)

- Refactored CityFlowStartWidget: added Btn_RandomMode with OnRandomModeClicked delegate, fixed Btn_StartGame not responding (ShowGameWidget missing UFUNCTION)
- Added HandleRandomModeClicked/HandleStartGameClicked to HUD, with ShowGameWidgetRandom that calls StartNewGame + EnablePlacement
- Fixed mouse cursor hiding during click-and-drag in Random Mode: added SetHideCursorDuringCapture(false) to FInputModeGameAndUI in ShowGameWidget and ShowGameWidgetRandom
- Alt key now disables placement on press (DisablePlacement), re-enables on release only if in Planning phase (avoids accidental placement in Simulation)
- Fixed road budget: ClearCell now refunds +1 RoadBudget when clearing a Road cell, fixing two bugs — deleting roads not restoring budget, and L-system-depleted budget blocking further placement after removal
- GameWidget budget display now reads from GridManager directly and updates via OnCellChanged binding, ensuring real-time budget display on every placement/removal
- Added countdown timer (Txt_Countdown) to GameWidget: starts on Simulating phase entry, ticks every 1s in MM:SS format, stops on phase change
- Created UCityFlowEvaluationWidget C++ base: displays total score, arrival count, congestion penalty, high score (static), simulation time (MM:SS), with Btn_BackToMain and Btn_Restart
- HUD ShowEvaluationWidget now populates data from ScoringManager/GameMode, binds OnBackToMainClicked/OnRestartClicked
- Added HandleRestartClicked to HUD: removes EvaluationWidget, calls GameMode::RestartPlanningPhase, shows GameWidget
- Updated TDD.md and TDD_Chinese.md sections 2.8 (Alt placement disable), 2.10 (ClearCell refund), 2.12 (v0.8 full rewrite with StartWidget/GameWidget/EvaluationWidget changes)

### GridPlaceableActor Spawn Scale Animation (v0.9)

- Implemented spawn scale-up animation for GridPlaceableActor on placement: smooth transition from initial scale to full size
- Driven by FTimerHandle (~60 Hz) with ease-out cubic curve `Alpha = 1 - (1-t)^3`, zero per-frame Tick overhead
- Added 3 UPROPERTYs on AGridPlaceableActor base class: bPlaySpawnAnimation (toggle), SpawnAnimationDuration (0.2s), SpawnAnimationInitialScale (0.05)
- Insertion point at end of PlaceOnGrid(), after OnPlacedOnGrid() — ensures RoadTile::UpdateAppearance() has already set final ActorScale3D
- EndPlay() clears SpawnAnimTimer to prevent dangling callback after actor destruction
- Build verified with zero errors
- Updated TDD.md and TDD_Chinese.md section 2.1 (placement flow now mentions animation) and 2.2 (new Spawn Scale Animation v0.9 subsection)

## 2026-06-06

### Full Game Loop Widget System (v0.7)

- Fixed StartSimulation button not responding: button callbacks in UCityFlowGameWidget were missing `UFUNCTION()` macro, causing `AddDynamic`/`BindUFunction` to silently fail
- Renamed button callbacks to `OnStartSimulationClicked`/`OnRestartPlanningClicked`/`OnTriggerLSystemClicked` with UFUNCTION macro
- Removed dead code: `EndSimulation()`, `HandleSimulationEnd()`, `HideGameWidget()`
- Created `UCityFlowStartWidget` C++ base: main menu widget with `Btn_StartGame`/`Btn_QuitGame` BindWidget controls, delegates `OnStartGameClicked`/`OnQuitGameClicked`
- Created `UCityFlowPauseWidget` C++ base: pause overlay widget with `Btn_Resume`/`Btn_ReturnToMain` BindWidget controls, delegates `OnResumeClicked`/`OnReturnToMainClicked`
- Complete rewrite of `ACityFlowHUD`: now manages full widget lifecycle (StartWidget → GameWidget ↔ PauseWidget → EvaluationWidget → loop)
- Added `TogglePause()`/`ShowPauseOverlay()`/`HidePauseOverlay()` with `SetGamePaused` and `FInputModeUIOnly`/`FInputModeGameAndUI` switching
- Added `ReturnToMainMenu()` Blueprint-callable for evaluation screen
- HUD auto-listens to `GameMode::OnSimulationPhaseEnd` to show evaluation widget
- Eliminated dual widget creation: removed `GameWidgetClass` and `GameWidgetInstance` from `ACityFlowGameMode`; HUD is sole widget lifecycle owner
- GameMode deferred initialization: `BeginPlay()` only sets budget; new `StartNewGame()` called by HUD on "Start Game" click
- Added `ReturnToMainMenu()` to GameMode: stops all timers/vehicles, destroys all `AGridPlaceableActor` via `TActorIterator`, re-initialises grid, aborts L-system
- Added `IA_Pause` Enhanced Input action to `ACityFlowPlayerController` for Esc-key pause/resume
- Fixed `Btn_RestartPlanning` visibility: now shows during Simulating phase (mid-game return to Planning), not Evaluation
- Fixed compilation error: `FInputMode_UIOnly` → `FInputModeUIOnly` (UE class names have no underscores)
- Updated TDD.md and TDD_Chinese.md sections 2.8 (pause input), 2.11 (deferred init + new APIs), 2.12 (full rewrite with widget lifecycle)

### Intersection Lock Redesign: Direction-Based Occupancy with Round-Robin Scheduling (v0.6)

- Replaced per-vehicle `LockHolder` model with direction-occupancy tables (`DirectionOccupants`, `PendingReservations`, `VehicleEntryDirs`) on `ARoadTile`
- Added `UBoxComponent IntersectionBox` to `ARoadTile` for physical overlap-driven lock life-cycle (enable only for Cross/T-Junction tiles, size neutralised against actor scale)
- Fixed IntersectionBox collision: ObjectType=ECC_Vehicle with ECR_Overlap to Vehicle channel so VehicleMesh (QueryVehicle preset) generates BeginOverlap/EndOverlap events
- Added `ECC_GameTraceChannel2` (Intersection) for forward-probe box sweep
- Implemented `TryAcquireIntersectionLock(Vehicle, EntryDir)` with same-direction follow-through and cross-direction rejection
- Implemented round-robin direction scheduling (`ServingDirection`, `ServedCount`, `WaitingDirs`, `MaxConsecutiveGrants=1`) to prevent single-direction starvation
- Added four safety nets: re-entry all-pass, periodic physical overlap sanitisation (2s), pending reservation expiry (5s), passed-intersection tracking
- Removed all v0.5 legacy: `VehicleManager::IntersectionLocks`, `AcquireIntersectionLock()`, `UpdateIntersectionLocks()`, `IsIntersectionLockedByOther()`, `CachedIntersections`, `VehicleActor::PathIntersectionCells`, `SetPathIntersections()`, `FIntersectionLock`, `FIntersectionOccupant`
- Added screen debug messages for vehicle enter/exit overlap and lock grant/reject events
- Updated `DebugDrawIntersections()` to query `ARoadTile::IsAnyDirectionOccupied()` via `TActorIterator`
- Added periodic `SanitizeAllIntersectionLocks()` timer in VehicleManager (every 2s)
- Fixed include order in VehicleManager.h (ECityFlowDrivingSide forward declaration replaced with include)
- Updated TDD.md and TDD_Chinese.md section 2.6 with v0.6 intersection occupation and movement state machine documentation

## 2026-06-04

### Vehicle Congestion & Forward Probe Refactor

- Enabled `VehicleMesh` collision (`QueryOnly`) for physical forward vehicle detection
- Implemented `PerformForwardProbe()` with unified `SweepMultiByChannel` (ECC_GameTraceChannel1) + intersection-lock lookup via pre-stored `PathIntersectionCells`
- Added `SetPathIntersections()` to pre-compute intersection cells from the A\* grid path at spawn time
- Added `DirectionFromGridDelta()` to `GridDirectionUtils` for axis-aligned entry direction derivation (replaced `DirectionFromWorldVector` for intersection locks)
- Changed `IntersectionLocks` from `TMap<Vehicle*, EGridDirection>` to `TMap<Vehicle*, FIntersectionOccupant{EntryDir, ExitDir}>`
- Added `IsIntersectionLockedByOther(Pos, Self)` simplified occupancy check
- Removed `WaitingIntersection` state — all stopping unified under `WaitingCongestion` with smooth deceleration by `StartDeceleration=2500`
- Simplified `TickMovementSpline`: single priority-based flow (probe → brake or advance)
- Added `IsBuildingBlocked()` to prevent spawning from buildings still occupied by departing vehicles
- Added Blueprint-configurable probe parameters: `ForwardProbeRadius`, `ForwardProbeDistance`, `SelfAvoidOffset`, `ProbeVerticalOffset`, `SafeDistanceMin`, `SafeDistanceSeconds`, `StartAcceleration`, `StartDeceleration`
- Added `bDebugDrawProbe` with chain-of-spheres visualization for the sweep volume

### Intersection Lock — ⚠️ UNRESOLVED BUG

- Despite the unified forward-probe architecture and pre-stored `PathIntersectionCells`, intersection locks do not reliably prevent multi-vehicle entry
- Vehicles still ignore locks under certain conditions; suspected root causes include `UpdateIntersectionLocks` release timing in `VehicleManager::Tick` (after all actors) and the simplified occupancy-only `IsIntersectionLockedByOther` lacking direction-aware path-crossing logic
- Direction-aware bidirectional conflict rules need a full redesign in the next session

### TDD Updates

- Updated TDD.md and TDD_Chinese.md sections 2.6 (Intersection Occupation v0.5, Movement State Machine v0.5) with known bug documented

## 2026-06-03

### Player Camera & Movement Refactor

- Refactored `ACityFlowPawn::Move()` to use `CameraYaw` (Blueprint-updated facing yaw) instead of `GetControlRotation()` for movement direction computation
- Added `IA_Look`, `IA_Zoom`, `IA_Alt` Enhanced Input actions for camera control
- Implemented Alt+Mouse look: holding Alt switches to `FInputModeGameOnly()` (captures mouse), releasing restores `FInputModeGameAndUI` + cursor for placement
- Implemented `Look()` with yaw-only `AddControllerYawInput()` (pitch control delegated to Blueprint to avoid SpringArm/ControlRotation fighting)
- Implemented `Zoom()` via scroll wheel adjusting `TargetSpringArmLength` (clamped [300, 20000], default 10000)
- Set initial controller pitch to `DefaultCameraPitch` (-60°) in `BeginPlay()` via `SetControlRotation`
- Added `MinCameraPitch` (-80°) and `MaxCameraPitch` (-30°) properties for Blueprint-driven pitch clamping
- Fixed mouse delta not reaching Enhanced Input by switching input mode on Alt press/release
- Fixed pitch jitter by replacing post-clamp `SetControlRotation` with pre-clamp delta approach (later simplified to Blueprint-only pitch)
- Updated TDD.md and TDD_Chinese.md section 2.8 with full camera/movement architecture

### Placement Toggle & UI BindWidget

- Added `EnablePlacement()` / `DisablePlacement()` / `IsPlacementEnabled()` to `ACityFlowPlayerController`, guarded `Tick()` preview update and `TryPlaceAtCursor()` / `TryRemoveAtCursor()`
- Disable stops cursor sampling and destroys preview actor; Enable spawns new preview and resumes
- `UCityFlowGameWidget::StartSimulation()` auto-disables placement; `RestartPlanning()` auto-re-enables
- Added `BindWidget`-based UMG controls to `UCityFlowGameWidget`: `Btn_TriggerLSystem`, `Btn_StartSimulation`, `Btn_RestartPlanning`, `Txt_Phase`, `Txt_Budget`, `Txt_Score`
- Implemented auto-binding of button `OnClicked` events in `NativeConstruct()`, cleanup in `NativeDestruct()`
- Added `UpdateButtonStates()` for phase-aware button visibility (action buttons visible in Planning, restart in Evaluation)
- Added `UpdatePhaseText()` / `UpdateBudgetText()` to auto-update TextBlock content from C++ delegate callbacks
- Updated TDD.md and TDD_Chinese.md sections 2.8 (Placement Toggle) and 2.12 (UI System BindWidget)

## 2026-06-02

### Bidirectional Lanes & Driving Side Configuration

- Added `ECityFlowDrivingSide` enum (`RightHand` / `LeftHand`) to `CityFlowGameTypes.h`
- Added `DrivingSide` and `LaneOffsetFactor` (default 0.2) properties to `ACityFlowGameMode`, Blueprint-configurable
- GameMode passes driving config to `UVehicleManager` via `SetDrivingSide()` / `SetLaneOffsetFactor()` on simulation start

### Lane Offset in BuildSplinePath

- After generating all spline point positions and tangent directions, `BuildSplinePath` applies a perpendicular offset to every point
- Offset = `CellSize × LaneOffsetFactor`, direction = right perpendicular of tangent (`(Tangent.Y, -Tangent.X, 0)`)
- Right-hand driving: `+RightPerp × Offset`; left-hand driving: `−RightPerp × Offset`
- Added `GridDirectionUtils::DirectionFromWorldVector()` and `ArePerpendicular()` helpers

### Direction-Aware Intersection Lock (v0.4)

- Changed `IntersectionLocks` from `TMap<FGridVector, AVehicleActor*>` to `TMap<FGridVector, TMap<TObjectPtr<AVehicleActor>, EGridDirection>>` — supports multiple vehicles at same intersection with entry direction tracking
- `IsIntersectionLockedByOther(Pos, AskingVehicle, EntryDir)`: only blocks when entry directions are perpendicular (crossing paths); same-direction and opposite-direction vehicles pass through simultaneously
- `AcquireIntersectionLock(Pos, Vehicle, EntryDir)`: registers vehicle with its entry grid direction
- `VehicleActor::TickMovementSpline` derives entry grid direction from `VelocityDirection` via `GridDirectionUtils::DirectionFromWorldVector`
- Updated `UpdateIntersectionLocks()`, `DebugDrawIntersections()`, and `ClearAllVehicles()` for new data structure

### TDD Updates

- Updated TDD.md and TDD_Chinese.md sections 2.6 (bidirectional lane offset + direction-aware intersection lock) and 2.11 (GameMode new properties)

### Turn-Aware Tangent Scaling (v0.4)

- Changed `BuildSplinePath` to output separate `OutArriveTangentLengths` / `OutLeaveTangentLengths` per point
- Turn direction detection via cross-product (`CrossZ > 0` = right turn), corrected sign bug
- Right-hand driving: right turn → outside long bend → `1.0 + LaneOffsetFactor`; left turn → inside short bend → `1.0 - LaneOffsetFactor`
- Left-hand driving: inversely mirrored
- Consecutive turns: previous turn's exit point's leave multiplier updated to current turn's TurnMult
- Base tangent length changed from `CellSize * 0.5` to `CellSize` (longer handles for better curve control)

### SetSplinePath Handle-Break Refactoring

- Step 1: build spline with uniform tangents via `AddSplineWorldPoint` + `SetTangentAtSplinePoint`
- Step 2: break handle linkage by setting all points to `ESplinePointType::CurveCustomTangent`
- Step 3: per-segment override via `SplineCurves.Position.Points[i]` — for each segment `(i, i+1)` with `LMult ≠ 1.0`, set `Points[i].LeaveTangent` and `Points[i+1].ArriveTangent` to the same scaled tangent, ensuring symmetric curve deformation
- Step 4: final `UpdateSpline()` to apply

### TDD Documentation Refresh

- Rewrote TDD.md and TDD_Chinese.md section 2.6 to v0.4, documenting turn detection, arrive/leave tangent separation, and the handle-break+per-segment override approach

## 2026-06-01

### Vehicle Spline-Based Movement (v0.2) — earlier today

- Replaced waypoint-based `FVehicleMovementPlan` with `USplineComponent` on `AVehicleActor` (`PathSpline`), storing the complete world-space path from origin to destination
- `TickMovementSpline()` advances `CurrentSplineDistance` along the spline each frame, queries world position/direction, updates actor location/rotation — movement model is working
- Simplified intersection lock system: vehicles check ahead by `CellSize * 0.5`, acquire lock when on intersection cell, release when departed; `IsIntersectionLockedByOther()` and `AcquireIntersectionLock()` as public API
- `ARoadTile::GetSplinePath()` exists but is **not used** by VehicleManager — per-tile spline approach discarded after multiple debugging iterations

### Path Algorithm Refactoring (v0.3): Turn-Offset Spline Construction

- Rewrote `BuildSplinePath()` in VehicleManager: replaced the edge-midpoint approach with a turn-offset strategy
- A* path (via `SmoothPath`) provides cell centers at direction-change points
- At each turning point, the single cell center is replaced by two half-cell offset points:
  - `EntryOffset = center - EntryDir * CellSize/2` (offset back toward incoming cell)
  - `ExitOffset = center + ExitDir * CellSize/2` (offset toward next cell)
- USplineComponent naturally interpolates between the two offset points, producing smooth rounded curves at corners — eliminating the jagged-turn problem
- **Fix:** Corrected EntryOffset sign from `+ EntryDir` to `- EntryDir` — `EntryDir = Curr - Prev` points toward the turn cell, so the incoming offset should go back toward the previous cell
- Removed obsolete `GetEdgeMidpoint()` and `GetDirectionFromDelta()` static helpers
- Added `GridDeltaToWorldDir()` helper for grid-delta-to-world-direction conversion
- Updated TDD.md and TDD_Chinese.md sections 2.3 and 2.6 to document v0.3 approach

### Spline Tangent Control & Consecutive Turn Fix

- **Bug: default auto tangents too long** — overrode tangents via `SetTangentAtSplinePoint`, length = `CellSize * 0.5`, direction from neighboring spline points
- **Bug: consecutive turns cause spline knotting** — preceding turn's exit offset and next turn's entry offset collide at the shared cell boundary. Fix: added `bPreviousWasTurn` flag; in a consecutive turn sequence, only the first turn outputs entry+exit offsets, subsequent turns output exit offset only.
- **Bug: tangent direction skews diagonal on consecutive turns** — tangents computed from world positions of offset points go diagonal (e.g. `(exit_B, exit_C)` vector is diagonal). Fix: `BuildSplinePath` now outputs a parallel `TArray<FVector> OutTangentDirs` with grid-orthogonal directions per point:
  - Entry offset: tangent = `EntryDir` (toward cell center)
  - Exit offset: tangent = `ExitDir` (away from cell center, orthogonal to grid)
  - Straight cell centers: tangent = path-forward direction (also included to act as turn-sequence separators)
- `SetSplinePath(Points, TangentDirs)` sets tangents directly from the provided directions
- Updated TDD.md and TDD_Chinese.md 2.3 and 2.6 with full tangent direction and consecutive turn documentation

### Code Cleanup

- Cleaned up unused `PopCount` from RoadTile.cpp, removed `#include "Grid/RoadTile.h"` from VehicleManager.cpp

### Vehicle Acceleration / Deceleration

- Unified speed-target model: `CurrentSpeed` smoothly transitions toward `TargetSpeed` via `FInterpConstantTo`
- `TargetSpeed = MoveSpeed × min(remaining distance / DecelerationDistance, 1.0)`, handling both acceleration and deceleration in one formula
- New `Acceleration` (800) and `DecelerationDistance` (200) Blueprint-configurable properties
- Vehicles naturally decelerate to zero on arrival instead of instantly stopping

### SpawnVehicle: Start/End Path from Building Cell

- Vehicle path start/end changed from road cell outside doorway to the building cell containing the doorway
- Iterates `Origin->Doorways` array directly, records `StartBuildingCell` / `EndBuildingCell` alongside the road connection point
- `Path.Insert(StartBuildingCell, 0)` + `Path.Add(EndBuildingCell)`, A* still runs between road cells only
- Vehicle spans: building cell → roadway → target building cell

### Remove SmoothPath, BuildPath Returns Raw A\* Path

- **Bug:** SmoothPath merged collinear cells, eliminating straight-cell separators between turn clusters; `bPreviousWasTurn` was never reset
- **Fix:** `BuildPath` returns `FindRoadPath` raw results directly, with straight cells correctly resetting the consecutive-turn flag
- Removed defunct `SmoothPath()` and `CanPathBetween()`
- Raw A\* path maxes at ~50 points on a 20×20 grid, no spline performance impact
- Updated TDD.md / TDD_Chinese.md to remove SmoothPath references

### Bug Fixes

- Fixed DeveloperSettings `GetSectionText()` compilation error on UE 5.6 (renamed to `GetSectionDescription()` with `#if WITH_EDITOR` guard)
- Fixed `BuildSplinePath` returning only 1 point for first/last cells (restored `GetOpposite` direction completion, later replaced with explicit first/middle/last logic)

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
