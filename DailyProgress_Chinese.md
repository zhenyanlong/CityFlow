## 2026-06-15

### 主菜单预览与随机规划流程（v0.17）

- 新增标题界面自动随机预览对局，复用现有系统生成景观、建筑、L-system 道路并启动模拟。
- 将玩家侧 Random Mode 拆分为 `StartRandomPlanningGame()`，只随机生成景观/建筑并停留在 Planning，让玩家自行设计道路。
- 将 Evaluation 的 Restart 改为启动一局全新的随机 Planning 对局，而不是简单回到上一局 Planning 阶段。
- 在 `ACityFlowGameMode` 上新增随机自动对局参数范围，用于配置网格尺寸、建筑数量和道路预算。

### 主菜单摄像机旋转

- 新增 `ACityFlowPawn::SetMainMenuCameraYawRotationEnabled()` 和 `MainMenuCameraYawSpeed`，用于标题界面缓慢旋转摄像机 yaw。
- 接入 HUD 状态切换，使主菜单开启摄像机旋转，进入游戏和结算界面时关闭。
- Pawn Tick 默认关闭，仅在主菜单摄像机旋转激活时启用。

### 景观装饰根 Actor 修复

- 修复快速重启 Random Mode 时景观装饰根 Actor 固定 UObject 名称冲突导致的 PIE 崩溃。
- 保留编辑器中可见的 `CityFlowLandscapeDecorations` Actor Label，同时让 Unreal 自动生成唯一运行时对象名。

### 文档更新

- 更新 TDD.md 和 TDD_Chinese.md，记录标题预览流程、Random Mode 规划流程、主菜单摄像机旋转、放置生命周期变化和景观根 Actor 命名修复。

## 2026-06-14

### 环境景观装饰与草地覆盖问题排查

- 为 `UCityFlowLandscapeDecorationManager`、草地覆盖采样和 HISM 实例清理补充运行时 Landscape 装饰技术文档。
- 记录当前开放问题：即使 PIE 日志显示 `G/R` 采样比例存在区分，草地稀疏度在视觉上仍不明显。
- 记录最新草地采样诊断基线：`RatioObserved=(0.674, 0.981, 1.202)`，并包含独立的 `BelowMin`、`Transition` 和 `Full` 采样计数。
- 标记后续优先排查方向：草模型缩放、`DensityPerCell`、按格随机采样导致的视觉冲淡，以及基于 cluster 或密度图的生成方案。
- 更新 TDD.md 和 TDD_Chinese.md，记录环境装饰设计与当前开放问题。

## 2026-06-13

### Debug Screen Message 开关

- 将交叉口相关 `AddOnScreenDebugMessage` 接入 `UCityFlowDeveloperSettings::bDebugDrawIntersections`，包括交叉口锁拒绝/通过、进入/离开 overlap、deadlock 释放锁提示
- 在 `UCityFlowDeveloperSettings` 中新增 `bDebugVehicleAbilities`，默认 `false`
- 在 `UCityFlowCheatExtension` 中新增 `CF_ToggleVehicleAbilityDebug`
- 将暴走和瞬移车辆能力触发的 screen debug message 接入 `bDebugVehicleAbilities`
- 已通过 `CityFlowEditor Win64 Development` 编译验证
- 更新 TDD.md 和 TDD_Chinese.md 第 2.13 节，记录新的 debug 控制项

### 车辆 Hover 目的地方向指示器（v0.16）

- 在 `ACityFlowPlayerController` 中新增仅模拟阶段生效的车辆 hover 检测，优先使用 Vehicle trace 通道，并以 Visibility 作为回退
- 新增 `AVehicleActor::SetHovered()`，用于切换 CustomDepth/CustomStencil 描边状态和目的地箭头 Widget
- 将 hover stencil 状态应用到车辆下所有 `UPrimitiveComponent` 子组件，覆盖蓝图子 Mesh，同时排除 `DestinationArrowWidget` 和 `PathSpline`
- 新增 `DestinationArrowWidget` 支持，并提供可配置的 hover stencil 值、箭头高度和旋转偏移
- 记录后处理描边可能盖到世界空间 3D Widget 上方这一已接受限制
- 更新 TDD.md 和 TDD_Chinese.md 第 2.6 节，记录 hover 指示器设计

## 2026-06-12

### 瞬移车辆类型（v0.15）

- 新增 ATeleportVehicle 车辆子类，等待超时时不死亡，而是沿自身 spline 朝目的地方向向前随机瞬移一段距离
- 新增 TeleportMinDistance、TeleportMaxDistance、TeleportOverlapRadius、TeleportBeforeVFX 和 TeleportAfterVFX 蓝图可配置属性，用于调整超时瞬移距离、重叠检测范围和前后特效
- 新增 TeleportVFXScale 和 TeleportVFXScaleParamName，使瞬移特效能像死亡特效一样接收 Niagara 缩放参数
- 新增 AVehicleActor::KillOverlappingVehicles() 辅助函数，通过现有死亡管线击杀瞬移落点重叠的活跃车辆
- 修复前向探测和暴走冲撞扫荡的起终点，现在从当前 spline 采样，而不是依赖 Actor transform/velocity
- 新增 ARampageVehicle 暴走状态下的急促红色材质闪烁
- 更新 TDD.md 和 TDD_Chinese.md 第 2.6 节，记录瞬移车辆设计

### 屏幕空间分数 Popup 反馈

- 将车辆分数 popup 从世界空间 `WidgetComponent` Actor 重构为由 `UCityFlowGameWidget` 管理的屏幕空间 UMG 反馈
- 新增 `UScoringManager::OnScorePopupRequested(WorldLocation, DeltaScore)`，计分层只广播带符号分数变化和世界锚点，不再生成 UI Actor
- 新增 `UScorePopupWidget`：每帧执行世界坐标到 UI 坐标投影，支持上升、淡出、缩放回落动画，并提供原生 TextBlock fallback
- 在 `UCityFlowGameWidget` 中新增可选 `PopupLayer` CanvasPanel 支持和 `ScorePopupWidgetClass` 配置
- 修复死亡 popup 可靠性：`UScoringManager` 直接绑定每辆新生成车辆的 `OnVehicleDeath`，同时保留 `VehicleManager::OnVehicleDied` 兜底，并通过 `ScoredDeathVehicles` 防重复计分/重复 popup
- 更新 TDD.md 和 TDD_Chinese.md 的计分/UI 章节，记录 popup 事件流和屏幕空间渲染设计

## 2026-06-09

### 车辆死亡与停车闪烁系统（v0.12）

- 在 AVehicleActor 上实现了基于 virtual 方法的停车/死亡管线：OnVehicleStopped()、OnVehicleResumed()、HandleVehicleDeath()、ShouldResetStopTime()
- 新增 TotalStopTime 累加，与 CongestionWaitTime（死锁释放）互相独立
- 通过 UMaterialInstanceDynamic 实现材质红光闪烁，ScalarParameter "FlashIntensity" 驱动正弦波发射（频率 0.5→4 Hz 递增）
- 实现爆炸死亡序列：Niagara VFX 生成 + SetVariableFloat 控制缩放、SoundBase SFX、距离衰减 CameraShake
- 新增蓝图可配置死亡属性：DeathTimeout（5s）、bEnableTimeoutDeath、ExplosionVFX、ExplosionVFXScale、ExplosionVFXScaleParamName、ExplosionSFX、DeathCameraShake、DeathShakeMaxDistance
- 新增 AVehicleActor::FOnVehicleDeath 委托和 UVehicleManager::FOnVehicleDied 委托
- 在 VehicleManager 中集成 OnVehicleDeathHandler，销毁前从 ActiveVehicles 移除死亡车辆
- 在 UScoringManager 中新增死亡罚分：DeathPenaltyTotal 追踪，TotalScore = 到达分 - 拥堵罚分 - 死亡罚分
- 在 UCityFlowDeveloperSettings 中新增 DeathPenalty 配置（默认 50）
- Build.cs 新增 Niagara 模块依赖
- 修复 VFX 自动清除：Niagara 系统 LoopBehavior 需设为 Once；使用 ENCPoolMethod::None + bAutoDestroy=true
- 修复 VFX 缩放：使用 SetVariableFloat() 将缩放值直接推入 Niagara User Parameter，替代 SetWorldScale3D
- 更新 TDD.md 和 TDD_Chinese.md 第 2.6 节，记录车辆死亡与停车闪烁系统（v0.12）

### 建筑起点/终点解耦（v0.13）

- 移除 CollectOriginDestinations() 中基于 bIsDestination 的起点/终点分流 —— 所有建筑现在同时担任起点和终点
- 更新 StartSpawning() 守卫条件为要求 2+ 个建筑，替代非空起点+终点数组检查
- 更新 CF_SpawnVehicle 作弊命令，从任意两个不同建筑选择，不再检查 bIsDestination
- 更新 TDD.md 和 TDD_Chinese.md 第 2.7 节，记录 v0.13 变更

### 路口占用指示器（v0.13）

- 在 ARoadTile 上添加 IndicatorPlane（UStaticMeshComponent），使用引擎内置 Plane mesh
- 实现 UpdateIndicator()，管理位置/大小/可见性，带缩放补偿
- 实现 UpdateIndicatorState()，通过 IsAnyDirectionOccupied() 和 DMI 实现绿/红颜色切换
- 将指示器刷新挂接到 5 个事件点：UpdateIntersectionBox、BeginOverlap、EndOverlap、SanitizeOccupants、ExpirePendingReservations
- 新增 6 个蓝图可配置属性：IndicatorMaterial、IndicatorSize（0.4）、IndicatorZOffset（80）、IndicatorFreeColor（绿）、IndicatorOccupiedColor（红）
- 修复 Plane 缩放：除以 100 以补偿引擎 Plane 默认 100×100 的世界尺寸
- 更新 TDD.md 和 TDD_Chinese.md 第 2.6 节，记录路口占用指示器（v0.13）

## 2026-06-08

### 建筑与车辆生成 DataAsset 重构（v0.10）

- 创建 UBuildingDataAsset（UPrimaryDataAsset），包含 FBuildingDataEntry（BuildingClass + SpawnWeight）和统一的 BuildingEntries 数组
- 起点/终点角色由建筑 BP 自身的 bIsDestination 标记决定，消除分离的起点/终点数组
- 实现确定性建筑生成数量分配：使用最大余数法，floor(weight/totalWeight × DefaultBuildingCount)，余量按小数部分分配
- 在 ACityFlowGameMode 中添加 BuildingDataAsset 和 VehicleDataAsset 属性，保留旧版单类属性作为回退
- 添加 UVehicleManager::SetVehicleDataAsset() 和 ExternalVehicleDataAsset 成员；CacheSpawnEntries() 优先使用外部 DataAsset，回退到 DeveloperSettings
- 移除 GameMode 中未使用的 VehicleClass 属性

### 拥堵与交叉口 Bug 修复（v0.11）

- 修复 VehicleGridMap 拥堵检测 Bug：将破损的 `TMap<FGridVector, AVehicleActor*>` 替换为每帧 `TMap<FGridVector, int32>` 统计，删除死代码 `UpdateVehicleGridOccupancy()` 和 `IsOccupiedByVehicle()`
- 新增死锁超时机制：`AVehicleActor` 在 `WaitingCongestion` 中累加 `CongestionWaitTime`，超 `DeadlockTimeout`（默认 3s）后释放所有路口占用，打破相邻路口互锁
- 修复前向探测零距离过滤：`ProjDist > 0.0f` → `>= 0.0f`，`InterDist <= 0.0f` → `< 0.0f`，使起始位置就在路口内的车辆能正确获取锁
- 修复 spawn 动画期间地基缩放 Bug：`BuildFoundation` 接收显式 `InOwnerScale` 参数，避免在动画中间值重建地基导致全图巨型地基
- 更新 TDD.md 和 TDD_Chinese.md 第 2.6 节（拥堵检测、死锁超时、零距离探测修复、运动流控更新）和 2.4b 节（地基缩放修复）
- 实现 ABuilding::ValidatePlacement() 覆写：验证 doorway 连接点在边界内且未被其他建筑占据
- 添加 GetDoorwayConnectionPointForPosition() 辅助函数，支持放置前 doorway 验证
- 更新 TDD.md 和 TDD_Chinese.md 第 2.6、2.7、2.11 节，记录 v0.10 变更

## 2026-06-07

### UI 系统精细化（v0.8）

- 重构 CityFlowStartWidget：新增 Btn_RandomMode 及 OnRandomModeClicked 委托，修复 Btn_StartGame 无响应（ShowGameWidget 缺少 UFUNCTION）
- 在 HUD 中添加 HandleRandomModeClicked/HandleStartGameClicked，ShowGameWidgetRandom 调用 StartNewGame + EnablePlacement
- 修复随机模式拖拽放置时光标消失：ShowGameWidget 和 ShowGameWidgetRandom 中 FInputModeGameAndUI 添加 SetHideCursorDuringCapture(false)
- Alt 键按下时关闭放置功能（DisablePlacement），松开后仅在 Planning 阶段恢复，避免 Simulation 阶段误开启
- 修复道路预算：ClearCell 清理 Road 格时返还 +1 RoadBudget，解决了删除路块不恢复预算、L-system 耗尽预算后无法继续放置两个 Bug
- GameWidget 预算显示改为直接从 GridManager 读取，通过 OnCellChanged 绑定实时刷新
- 为 GameWidget 添加倒计时（Txt_Countdown）：进入 Simulating 阶段启动，每秒以 MM:SS 格式刷新，离开阶段时停止
- 创建 UCityFlowEvaluationWidget C++ 基类：展示总分、到达数、拥堵罚分、最高分（静态）、模拟时间（MM:SS），含 Btn_BackToMain 和 Btn_Restart
- HUD ShowEvaluationWidget 从 ScoringManager/GameMode 读取数据并调用 Populate，绑定 OnBackToMainClicked/OnRestartClicked
- 为 HUD 添加 HandleRestartClicked：移除 EvaluationWidget → GameMode::RestartPlanningPhase → 显示 GameWidget
- 更新 TDD.md 和 TDD_Chinese.md 第 2.8 节（Alt 放置开关）、2.10 节（ClearCell 返还）、2.12 节（v0.8 完整重写）

### GridPlaceableActor 放置生长动画（v0.9）

- 实现 GridPlaceableActor 放置时的缩放入场动画：从初始缩放平滑过渡到目标大小
- 使用 FTimerHandle 驱动（~60Hz），ease-out 三次方缓动曲线 `Alpha = 1 - (1-t)^3`，无每帧 Tick 开销
- 在 AGridPlaceableActor 基类上新增 3 个 UPROPERTY：bPlaySpawnAnimation（开关）、SpawnAnimationDuration（0.2s）、SpawnAnimationInitialScale（0.05）
- 动画插入点在 PlaceOnGrid() 末尾、OnPlacedOnGrid() 之后 — 确保 RoadTile::UpdateAppearance() 已确定最终 ActorScale3D
- EndPlay() 清除 SpawnAnimTimer 防止 Actor 销毁后回调悬空
- 编译验证通过
- 更新 TDD.md 和 TDD_Chinese.md 第 2.1 节（放置流程增加动画描述）、2.2 节（新增放置生长动画 v0.9 子节）

## 2026-06-06

### 完整游戏循环 Widget 系统（v0.7）

- 修复 StartSimulation 按钮无反应：UCityFlowGameWidget 的按钮回调缺少 `UFUNCTION()` 宏，导致 `AddDynamic`/`BindUFunction` 静默失败
- 重命名按钮回调为 `OnStartSimulationClicked`/`OnRestartPlanningClicked`/`OnTriggerLSystemClicked` 并添加 UFUNCTION 宏
- 移除死代码：`EndSimulation()`、`HandleSimulationEnd()`、`HideGameWidget()`
- 创建 `UCityFlowStartWidget` C++ 基类：主菜单 Widget，含 `Btn_StartGame`/`Btn_QuitGame` BindWidget 控件和委托 `OnStartGameClicked`/`OnQuitGameClicked`
- 创建 `UCityFlowPauseWidget` C++ 基类：暂停菜单 Widget，含 `Btn_Resume`/`Btn_ReturnToMain` BindWidget 控件和委托 `OnResumeClicked`/`OnReturnToMainClicked`
- 完整重写 `ACityFlowHUD`：管理完整 Widget 生命周期（StartWidget → GameWidget ↔ PauseWidget → EvaluationWidget → 循环）
- 添加 `TogglePause()`/`ShowPauseOverlay()`/`HidePauseOverlay()`，带 `SetGamePaused` 和 `FInputModeUIOnly`/`FInputModeGameAndUI` 切换
- 添加 `ReturnToMainMenu()` 蓝图可调用接口供结算界面使用
- HUD 自动监听 `GameMode::OnSimulationPhaseEnd` 以显示结算界面
- 消除双重 Widget 创建：从 `ACityFlowGameMode` 移除 `GameWidgetClass` 和 `GameWidgetInstance`；HUD 是唯一 Widget 生命周期所有者
- GameMode 推迟初始化：`BeginPlay()` 仅设置预算；新增 `StartNewGame()` 由 HUD 在 "开始游戏" 点击时调用
- 为 GameMode 添加 `ReturnToMainMenu()`：停止所有计时器/车辆，通过 `TActorIterator` 销毁所有 `AGridPlaceableActor`，重新初始化网格，中止 L-system
- 为 `ACityFlowPlayerController` 添加 `IA_Pause` Enhanced Input 动作实现 Esc 键暂停/恢复
- 修复 `Btn_RestartPlanning` 可见性：现在在 Simulating 阶段显示（中途返回 Planning），而非 Evaluation 阶段
- 修复编译错误：`FInputMode_UIOnly` → `FInputModeUIOnly`（UE 类名无下划线）
- 更新 TDD.md 和 TDD_Chinese.md 第 2.8 节（暂停输入）、2.11 节（推迟初始化 + 新 API）、2.12 节（完整重写 Widget 生命周期）

### 交叉口锁重新设计：方向占用 + 轮转调度（v0.6）

- 将单车 `LockHolder` 模型替换为 `ARoadTile` 上的方向占用表（`DirectionOccupants`、`PendingReservations`、`VehicleEntryDirs`）
- 为 `ARoadTile` 添加 `UBoxComponent IntersectionBox`，通过物理 Overlap 事件驱动锁生命周期（仅十字/T型路口启用，尺寸抵消 Actor 缩放）
- 修复 IntersectionBox 碰撞：ObjectType=ECC_Vehicle，对 Vehicle 通道设 ECR_Overlap，使 VehicleMesh（QueryVehicle 预设）生成 BeginOverlap/EndOverlap 事件
- 新增 `ECC_GameTraceChannel2`（Intersection）碰撞通道用于前向探测 Box 扫描
- 实现 `TryAcquireIntersectionLock(Vehicle, EntryDir)`，支持同方向跟车放行、交叉方向拒绝
- 实现方向轮转调度（`ServingDirection`、`ServedCount`、`WaitingDirs`、`MaxConsecutiveGrants=1`），防止单一方向饿死
- 添加四层保险：已入驻全放行、定期物理重叠校验（每 2 秒）、预占超时过期（5 秒）、已过路口追踪
- 移除所有 v0.5 遗留：`VehicleManager::IntersectionLocks`、`AcquireIntersectionLock()`、`UpdateIntersectionLocks()`、`IsIntersectionLockedByOther()`、`CachedIntersections`、`VehicleActor::PathIntersectionCells`、`SetPathIntersections()`、`FIntersectionLock`、`FIntersectionOccupant`
- 添加车辆进出 Overlap 及锁授予/拒绝的屏幕调试消息
- 更新 `DebugDrawIntersections()` 改为通过 `TActorIterator` 查询 `ARoadTile::IsAnyDirectionOccupied()`
- 在 VehicleManager 中添加定期 `SanitizeAllIntersectionLocks()` 定时器（每 2 秒）
- 修复 VehicleManager.h 的 include 顺序（ECityFlowDrivingSide 前置声明改为 include）
- 更新 TDD.md 和 TDD_Chinese.md 第 2.6 节，记录 v0.6 交叉口占用和运动状态机文档

## 2026-06-04

### 车辆拥堵与前向探测重构

- 启用 `VehicleMesh` 碰撞（`QueryOnly`）用于物理前车检测
- 实现 `PerformForwardProbe()`：统一 `SweepMultiByChannel`（ECC_GameTraceChannel1）+ 通过预存 `PathIntersectionCells` 查交叉口锁
- 新增 `SetPathIntersections()`：在生成时从 A\* 网格路径预计算交叉口格
- 在 `GridDirectionUtils` 中新增 `DirectionFromGridDelta()`：用轴对齐的网格增量推导入口方向（替代 `DirectionFromWorldVector` 用于交叉口锁）
- `IntersectionLocks` 从 `TMap<Vehicle*, EGridDirection>` 改为 `TMap<Vehicle*, FIntersectionOccupant{EntryDir, ExitDir}>`
- 新增 `IsIntersectionLockedByOther(Pos, Self)` 简化版占用检查
- 移除 `WaitingIntersection` 状态 — 全部停车统一为 `WaitingCongestion`，通过 `StartDeceleration=2500` 平滑减速
- 简化 `TickMovementSpline`：单一优先级流控（探测 → 刹车或前进）
- 新增 `IsBuildingBlocked()`：防止从仍有未驶出车辆的建筑生成新车
- 新增蓝图可配置探测参数：`ForwardProbeRadius`、`ForwardProbeDistance`、`SelfAvoidOffset`、`ProbeVerticalOffset`、`SafeDistanceMin`、`SafeDistanceSeconds`、`StartAcceleration`、`StartDeceleration`
- 新增 `bDebugDrawProbe`：用链式球体可视化扫描体积

### 交叉口锁 — ⚠️ 未解决 Bug

- 尽管采用了统一前向探测架构和预存 `PathIntersectionCells`，交叉口锁仍然无法可靠防止多车同时进入
- 特定条件下车辆仍无视锁；怀疑根因包括 `UpdateIntersectionLocks` 在 `VehicleManager::Tick`（所有 Actor 之后）的释放时序，以及简化版 `IsIntersectionLockedByOther` 缺少方向感知路径交叉逻辑
- 方向感知双向冲突规则需要在下个会话完全重新设计

### TDD 更新

- 更新 TDD.md 和 TDD_Chinese.md 第 2.6 节（交叉口占用 v0.5、运动状态机 v0.5），已记录已知 Bug

## 2026-06-03

### 玩家摄像机与移动重构

- 重构 `ACityFlowPawn::Move()`，使用 `CameraYaw`（蓝图更新的朝向 yaw）替代 `GetControlRotation()` 计算移动方向
- 新增 `IA_Look`、`IA_Zoom`、`IA_Alt` Enhanced Input Action 用于摄像机控制
- 实现 Alt+鼠标视角控制：按住 Alt 切换为 `FInputModeGameOnly()`（捕获鼠标），松开恢复 `FInputModeGameAndUI` + 光标用于放置
- 实现 `Look()`，仅通过 `AddControllerYawInput()` 控制 yaw（pitch 控制移交蓝图，避免 SpringArm/ControlRotation 冲突）
- 实现 `Zoom()`，通过滚轮调整 `TargetSpringArmLength`（Clamp [300, 20000]，默认 10000）
- 在 `BeginPlay()` 中通过 `SetControlRotation` 设置初始 controller pitch 为 `DefaultCameraPitch`（-60°）
- 新增 `MinCameraPitch`（-80°）和 `MaxCameraPitch`（-30°）属性供蓝图驱动 pitch 钳位
- 修复鼠标 delta 未进入 Enhanced Input 的问题：在 Alt 按下/松开时切换输入模式
- 修复 pitch 抖动问题：将事后 `SetControlRotation` 钳位改为预钳位 delta 方案（后简化为蓝图全权处理 pitch）
- 更新 TDD.md 和 TDD_Chinese.md 第 2.8 节，记录完整的摄像机/移动架构

### 放置开关与 UI BindWidget

- 在 `ACityFlowPlayerController` 中添加 `EnablePlacement()` / `DisablePlacement()` / `IsPlacementEnabled()`，守卫 `Tick()` 预览更新和 `TryPlaceAtCursor()` / `TryRemoveAtCursor()`
- 禁用时停止光标采样并销毁预览 Actor；启用时生成新预览并恢复
- `UCityFlowGameWidget::StartSimulation()` 自动禁用放置；`RestartPlanning()` 自动恢复
- 在 `UCityFlowGameWidget` 中添加基于 `BindWidget` 的 UMG 控件：`Btn_TriggerLSystem`、`Btn_StartSimulation`、`Btn_RestartPlanning`、`Txt_Phase`、`Txt_Budget`、`Txt_Score`
- 在 `NativeConstruct()` 中实现按钮 `OnClicked` 事件自动绑定，`NativeDestruct()` 中清理
- 添加 `UpdateButtonStates()` 实现阶段感知的按钮显隐（操作按钮在规划阶段可见，重新规划按钮在结算阶段可见）
- 添加 `UpdatePhaseText()` / `UpdateBudgetText()` 在 C++ 委托回调中自动更新 TextBlock 内容
- 更新 TDD.md 和 TDD_Chinese.md 第 2.8 节（放置开关）和第 2.12 节（UI 系统 BindWidget）

## 2026-06-02

### 双向车道与驾驶方向配置

- 在 `CityFlowGameTypes.h` 中添加 `ECityFlowDrivingSide` 枚举（`RightHand` 右舵 / `LeftHand` 左舵）
- 在 `ACityFlowGameMode` 中添加 `DrivingSide` 和 `LaneOffsetFactor`（默认 0.2）属性，可在蓝图中配置
- GameMode 在模拟开始时通过 `SetDrivingSide()` / `SetLaneOffsetFactor()` 将驾驶配置传递给 `UVehicleManager`

### BuildSplinePath 车道偏移

- 生成所有样条点位置和切线方向后，`BuildSplinePath` 对每个点施加垂直偏移
- 偏移 = `CellSize × LaneOffsetFactor`，方向 = 切线的右垂直方向（`(Tangent.Y, -Tangent.X, 0)`）
- 右舵：`+RightPerp × Offset`；左舵：`−RightPerp × Offset`
- 新增 `GridDirectionUtils::DirectionFromWorldVector()` 和 `ArePerpendicular()` 辅助函数

### 方向感知交叉口锁（v0.4）

- 将 `IntersectionLocks` 从 `TMap<FGridVector, AVehicleActor*>` 改为 `TMap<FGridVector, TMap<TObjectPtr<AVehicleActor>, EGridDirection>>` — 支持同一交叉口多车辆同时存在，并追踪进入方向
- `IsIntersectionLockedByOther(Pos, AskingVehicle, EntryDir)`：仅在进入方向垂直（交叉路径）时阻塞；同向和对向车辆可同时通过
- `AcquireIntersectionLock(Pos, Vehicle, EntryDir)`：以进入网格方向注册车辆
- `VehicleActor::TickMovementSpline` 通过 `GridDirectionUtils::DirectionFromWorldVector` 从 `VelocityDirection` 推导进入方向
- 更新 `UpdateIntersectionLocks()`、`DebugDrawIntersections()`、`ClearAllVehicles()` 适配新数据结构

### TDD 更新

- 更新 TDD.md 和 TDD_Chinese.md 第 2.6 节（双向车道偏移 + 方向感知交叉口锁）和第 2.11 节（GameMode 新增属性）

### 弯道感知切线缩放（v0.4）

- `BuildSplinePath` 改为输出独立的 `OutArriveTangentLengths` / `OutLeaveTangentLengths` 每点乘数
- 通过叉积判断左右转（`CrossZ > 0` = 右转），修复了符号 Bug
- 右舵：右转 → 外侧大弯 → `1.0 + LaneOffsetFactor`；左转 → 内侧小弯 → `1.0 - LaneOffsetFactor`
- 左舵：镜像反向
- 连续弯道：上一个弯道出口点的 leave 乘数覆盖为当前弯道的 TurnMult
- 基准切线长度从 `CellSize * 0.5` 改为 `CellSize`

### SetSplinePath 手柄打断重构

- 第一步：`AddSplineWorldPoint` + `SetTangentAtSplinePoint` 建统一切线样条
- 第二步：`SetSplinePointType(i, CurveCustomTangent)` 打断全点手柄联动
- 第三步：通过 `SplineCurves.Position.Points[i]` 按段覆盖 — 对 `LMult ≠ 1.0` 的段 `(i, i+1)`，将 `Points[i].LeaveTangent` 和 `Points[i+1].ArriveTangent` 设为相同缩放值，保证曲线变形对称
- 第四步：`UpdateSpline()` 应用

### TDD 文档更新

- 将 TDD.md 和 TDD_Chinese.md 第 2.6 节重写为 v0.4 版本，完整记录弯道检测、arrive/leave 分离切线、手柄打断 + 按段覆盖方案

## 2026-06-01

### 车辆样条运动（v0.2）— 今日较早

- 将基于航点的 `FVehicleMovementPlan` 替换为 `AVehicleActor` 上的 `USplineComponent`（`PathSpline`），存储从起点到终点的完整世界空间路径
- `TickMovementSpline()` 每帧沿样条推进 `CurrentSplineDistance`，查询世界位置/方向，更新 Actor 位置/旋转——运动模型工作正常
- 简化交叉口锁系统：车辆提前 `CellSize * 0.5` 前瞻检查，在交叉口格获取锁，离开后释放；`IsIntersectionLockedByOther()` 和 `AcquireIntersectionLock()` 作为公开 API
- `ARoadTile::GetSplinePath()` 存在但**未被 VehicleManager 使用**——经过多次调试迭代后，逐块样条方案已被放弃

### 路径算法重构（v0.3）：转弯偏移样条构建

- 重写 VehicleManager 中的 `BuildSplinePath()`：将边中点方案替换为转弯偏移策略
- A\* 路径（经 `SmoothPath`）提供方向变化处的格子中心
- 每个转弯点被替换为两个半格偏移点：
  - `EntryOffset = center - EntryDir * CellSize/2`（向来时方向回退半格）
  - `ExitOffset = center + ExitDir * CellSize/2`（向下一格方向偏移半格）
- USplineComponent 在两个偏移点之间自然插值，在转角处产生平滑弧线——消除转角锯齿问题
- **修正：** 将 EntryOffset 符号从 `+ EntryDir` 改为 `- EntryDir`——`EntryDir = Curr - Prev` 指向转弯格本身，入口偏移应回退到来的方向
- 移除废弃的 `GetEdgeMidpoint()` 和 `GetDirectionFromDelta()` 静态辅助函数
- 添加 `GridDeltaToWorldDir()` 辅助函数，用于网格增量到世界方向的转换
- 更新 TDD.md 和 TDD_Chinese.md 第 2.3 和 2.6 节，记录 v0.3 方案

### 样条切线控制 & 连续弯道修复

- **Bug: 默认自动切线过长** — 通过 `SetTangentAtSplinePoint` 覆写切线，长度 = `CellSize * 0.5`
- **Bug: 连续弯道导致样条打结** — 前一个弯道的 exit offset 和后一个弯道的 entry offset 重合在格子边界。修复：添加 `bPreviousWasTurn` 标记；连续弯道序列中仅首个弯道输出 entry+exit，后续弯道仅输出 exit offset
- **Bug: 连续弯道切线方向斜对角偏斜** — 从偏移点世界位置计算的切线方向为斜对角线（如 `exit_B→exit_C` 向量）。修复：`BuildSplinePath` 现在同步输出 `TArray<FVector> OutTangentDirs`，每点使用格网正交方向：
  - 入口偏移：切线 = `EntryDir`（指向格子中心）
  - 出口偏移：切线 = `ExitDir`（远离格子中心，与格网正交）
  - 直道格子中心：切线 = 路径前进方向（同时作为弯道序列分隔符）
- `SetSplinePath(Points, TangentDirs)` 直接使用提供的切线方向设置样条切线
- 更新 TDD.md 和 TDD_Chinese.md 第 2.3 和 2.6 节，完整记录切线方向与连续弯道文档

### 代码清理

- 清理 RoadTile.cpp 中未使用的 `PopCount`，移除 VehicleManager.cpp 中 `#include "Grid/RoadTile.h"`

### 车辆加速/减速

- 统一速度目标模型：`CurrentSpeed` 通过 `FInterpConstantTo` 向 `TargetSpeed` 平滑过渡
- `TargetSpeed = MoveSpeed × min(剩余距离/DecelerationDistance, 1.0)`，同时实现加速和减速
- 新增 `Acceleration`（800）和 `DecelerationDistance`（200）Blueprint 可配置参数
- 到达终点时自然减速至零，不再瞬间停止

### SpawnVehicle 起点/终点改为 Building Cell

- 车辆路径起点和终点从 doorway 外面的道路格改为 doorway 所在的建筑格
- 遍历 `Origin->Doorways` 数组，同步记录 `StartBuildingCell` / `EndBuildingCell`
- `Path.Insert(StartBuildingCell, 0)` + `Path.Add(EndBuildingCell)`，A* 寻路仍在 road cell 之间进行
- 车辆从建筑格出发 → 驶向道路格 → 沿途行驶 → 驶入目标建筑格

### 移除 SmoothPath，BuildPath 直接返回原始 A\* 路径

- **Bug:** SmoothPath 合并共线中间格导致两个弯道群之间无直道格分隔，`bPreviousWasTurn` 永不重置
- **修复:** `BuildPath` 直接返回 `FindRoadPath` 原始路径，直道格作为弯道序列分隔符正确重置标记
- 移除废弃的 `SmoothPath()` 和 `CanPathBetween()`
- 原始 A\* 路径在 20×20 网格下最多约 50 个点，对样条性能无影响
- 同步更新 TDD.md / TDD_Chinese.md 移除 SmoothPath 引用

### Bug 修复

- 修复 UE 5.6 上 DeveloperSettings `GetSectionText()` 编译错误（重命名为 `GetSectionDescription()`，添加 `#if WITH_EDITOR` 守卫）
- 修复 `BuildSplinePath` 首/末格仅返回 1 个点的问题（恢复 `GetOpposite` 方向补全，后续改为显式首/中/末格逻辑）

## 2026-05-31

### 车辆生成系统（v0.1，通过初步调试）

- 重新设计 UVehicleDataAsset：从单车型参数表改为车辆类注册表，新增 FVehicleSpawnEntry 结构体（TSubclassOf<AVehicleActor> + SpawnWeight），通过 UCityFlowDeveloperSettings::DefaultVehicleDataAsset 引用
- 更新 UVehicleManager：CacheSpawnEntries() 在模拟开始时加载注册表，PickRandomVehicleClass() 每次生成时按加权随机选取车辆子类，SpawnVehicle() 使用选取的类而非硬编码基类
- 移除 AVehicleActor::InitializeFromDataAsset()；每个 BP 子类在 Class Defaults 中自行配置 Mesh/MoveSpeed/DebugColor
- 添加 USceneComponent VehicleRoot 作为根组件，VehicleMesh 作为子组件挂载其下，支持蓝图中自由调整本地旋转/缩放
- 添加 VehicleZOffset（默认 30）：所有航点坐标偏移此值，使车辆位于道路表面之上而非陷入道路内部
- 车辆每帧通过 SetWorldRotation(Yaw from MoveDir) 使 VehicleRoot 朝向行进方向
- 车辆到达后自动销毁（VehicleManager Tick 中广播 OnVehicleArrived 后调用 Destroy()）
- AVehicleActor 构造函数中添加默认 Cube mesh 回退，即使未配置 DataAsset 车辆也可见

### VehicleManager 改进（v0.1，通过初步调试）

- 车辆生成重试逻辑：每帧随机选取一个 origin，将全部 destinations 随机打乱（Fisher-Yates 洗牌），遍历直到找到可连通对；全都不通则尝试换下一个 origin
- 目的地随机化：每次生成间隔重新洗牌 destination 列表，避免固定 origin→dest 绑定
- 在 SpawnVehicle 每个失败点添加了全面的 UE_LOG 诊断（建筑为空、出入口不在道路上、A* 寻路失败、运动计划无效、PickRandomVehicleClass 返回空、SpawnActor 失败、成功时打印路径详情）
- CacheSpawnEntries 添加日志（未配置 DataAsset、条目数组为空、已加载类型汇总）和 PickRandomVehicleClass（无条目回退警告）

### 预算系统 Bug 修复

- 修复 OccupyCell：预算检查在 Cell.Type/BuildingID/RoadActor 写入之后执行；预算耗尽时 Cell 被永久污染为 Type=Road/Mask=0，且不广播 OnCellChanged。将预算守卫移到所有 Cell 变更之前。
- 修复 RegisterCells：OccupyCell 返回值被静默丢弃；PlaceOnGrid 即使网格占用失败也会将 Actor 切换为 Placed 状态。将 RegisterCells 改为返回 bool；PlaceOnGrid 现在检查结果并在失败时回滚。
- 修复 AGridPlaceableActor::PlaceOnGrid：RegisterCells 失败时，现在对已部分占用的 Cell 调用 ClearCell 并重置 GridPosition，防止状态污染。

### PlayerController Bug 修复

- 修复 TryPlaceAtCursor：PlaceOnGrid 返回值未被检查。失败时预览 Actor 碰撞已开启但既未放置也未销毁——泄漏在世界中，阻挡射线检测，导致大部分格子无法继续放置。现在仅在成功后清除 PreviewActor 指针并生成新预览；失败时恢复碰撞。

### GameMode 配置修复

- 移除 CityFlowGameMode::BeginPlay() 中用 DeveloperSettings 默认值覆盖 GridWidth/GridHeight/CellSize/BuildingCount 的逻辑。BP GameMode Class Defaults 现为网格和建筑参数的权威数据源。

### C++ 基础框架（代码完成，尚未通过玩法验证）

- ACityFlowGameMode 状态机（Planning→Simulating→Evaluation）：BeginPlay 场景初始化、建筑生成委托、阶段切换、模拟计时器、预算分配（玩家 vs L-system 份额）
- UScoringManager（UWorldSubsystem）：到达分数追踪、每秒拥堵惩罚、全连通奖励、最终分数计算
- ACityFlowHUD：GameWidget/EvaluationWidget 生命周期管理
- UCityFlowGameWidget C++ 基类：用于阶段/分数/预算/L-system 变化的 BlueprintImplementableEvent 回调，蓝图可调用的 StartSimulation/EndSimulation/RestartPlanning/TriggerLSystem
- UCityFlowCheatExtension：15 个 CF_* 控制台命令（阶段控制、车辆生成/调试、预算操作、统计打印、模拟速度）
- UCityFlowDeveloperSettings（Config=Game，Project Settings 面板）：网格、预算、模拟、计分、车辆、调试开关的默认值
- Build.cs：添加 UMG、Slate、SlateCore、DeveloperSettings 模块依赖
- 更新 TDD.md 和 TDD_Chinese.md 第 1、2.6、2.7、2.10、2.11、2.12、2.13 节

## 2026-05-30

- 添加了 EPlaceableType 枚举（Road/Building/Landscape）和 EGridRotation 枚举（Rot0/Rot90/Rot180/Rot270）到 CityFlowGridTypes.h
- 在 AGridPlaceableActor 基类中添加了 RootSceneComponent（USceneComponent）作为根组件，管理子组件的相对位置；添加了 PlaceableType 属性
- AMeshGridPlaceableActor 的 MeshComponent 改为挂载到 RootSceneComponent 下（不再作为根组件）
- 完善了 ABuilding：BuildingRotation 支持网格旋转（Rot0/Rot90/Rot180/Rot270），CalculateOccupiedCells 使用 GetEffectiveBuildingSize
- 实现了 TransformLocalPosition()：建筑本地坐标随 BuildingRotation 变换
- 出入口 RelativePosition 改为建筑本地坐标（0-based），FacingDirection 跟随建筑旋转，通过 GetDoorwayConnectionPoint() 计算世界连接点（本地坐标 + 旋转后方向向量偏移）
- 修复了 AutoGenerateDoorways 使用内部坐标，HasDoorwayAt/GetDoorwayWorldPositions 统一使用 GetDoorwayConnectionPoint
- GridManager::CalculateConnectedMask 现在检测 Building 出入口：邻居为 Building 时检查 HasDoorwayAt，有出入口则计入 ConnectedMask
- 建筑放置时通过 OnDoorwayCellChanged 订阅网格事件，道路连接/断开时自动刷新地基
- 添加了 DebugDoorwayMesh：放置后在每个出入口位置生成可视化标识 Sphere/Arrow
- 修复了 FGridVector 的 X/Y 属性缺少 EditAnywhere 导致蓝图中无法编辑 RelativePosition 的问题
- 添加了 FBuildingSpawnRequest 结构体（BuildingClass + Count），TryPlaceBuildingRandom 支持随机旋转，新增 TryPlaceBuildingsRandom 批量生成
- 添加了 ProceduralMeshComponent 模块依赖
- 实现了 UFoundationComponent：程序化生成建筑地基（ProceduralMesh），包含顶面、底面、侧墙（独立顶点+水平朝外法线）
- 实现了人行道 (Sidewalk) 边框：矩形环带，含内/外墙、顶面、底面
- 蓝图可配置：FoundationHeight、Padding、CornerRadius、SidewalkWidth、SidewalkHeight、FoundationMaterial、SidewalkMaterial
- 修复了 Z 轴层级：地基主体 Z=0~FoundationHeight，人行道 Z=FoundationHeight~+SidewalkHeight
- 修复了 Actor 缩放双重变换：Foundation/Sidewalk mesh 通过 SetRelativeScale3D(1/S) 抵消父级缩放
- 修复了侧墙法线：独立顶点 + (Edge.Y, -Edge.X, 0) 朝外水平法线
- 修复了 UV 贴图：侧墙使用累计 UVOffset + 长度比例，顶部 V=0 不翻转
- 修复了三角形缠绕顺序：统一使用 UE 左手坐标系顺时针 (CW) 为正面
- 更新了 TDD.md 和 TDD_Chinese.md，添加了 2.4b FoundationComponent 章节
- 创建了 ULSystemManager（UWorldSubsystem）用于 L-system 毛细道路生成
- 实现了选择性起点采样：死胡同 + 直路段间隔点（跳过转角/交叉口）
- 实现了广度优先迭代队列生长策略，基于 FTimerHandle 以可配置 GrowthInterval 进行动画
- 实现了多格直道延伸（StraightExtendLength 可配置，默认每前进步骤 3 格）
- 实现了 IsSideBranchValid()，防止侧向分支在平行道路间填空
- 添加了吸引偏向排序（DistScore + AlignScore，按 AttractionStrength 混合），将分支引向未连接建筑
- 为所有参数添加了蓝图可调用的 Setter/Getter：RoadTileClass、BranchBudget、GrowthInterval、BranchProbability、AttractionStrength、StraightExtendLength、MinBranchSpacing
- 添加了事件委托：OnGenerationStarted、OnGenerationStep(int32)、OnGenerationFinished(bool)
- 修复了 Insert(NewPt,0) 导致的深度优先和吸引反转 bug，改为 Add(NewPt)
- 将 GetDoorwayConnectionPoint() 和 TransformLocalPosition() 提升为 ABuilding 的 public 方法
- 将 LSystemManager 从 AActor 重构为 UWorldSubsystem，与 GridManager 保持一致
- 更新了 TDD.md 和 TDD_Chinese.md 第 2.5 节，添加完整的实现细节
- 更新了 TDD 第 1 节架构概览，反映 LSystemManager 为 UWorldSubsystem
- 为 UFoundationComponent 添加了 FoundationCollisionProfileName 和 SidewalkCollisionProfileName，通过 GetCollisionProfileOptions() 提供下拉菜单选项
- 更新了 TDD.md 和 TDD_Chinese.md 第 2.4b 节，添加碰撞预设配置

## 2026-05-26

- 定义了网格核心类型：ECellType、EGridDirection、FGridCell、FGridVector、GridDirectionUtils
- 实现了 GridManager（UWorldSubsystem）：网格初始化、坐标转换、放置验证、邻居查询、连接掩码计算
- 创建了 AGridPlaceableActor（抽象基类，纯状态管理：预览/放置）→ AMeshGridPlaceableActor（StaticMesh + 多材质槽预览切换）→ ATestGridPlaceableActor
- 创建了 CityFlowPawn（ACharacter，飞行模式，基于 Enhanced Input 的 WASD 移动），摄像机参数可在蓝图中配置
- 创建了 CityFlowPlayerController：左键射线放置，每帧鼠标跟随的预览 Actor
- 添加了 GridVisualizer 和 GridPlaneVisualizer 用于编辑器中网格线和地平线渲染
- 将源码整理到 Grid/、Player/、Test/ 子目录中

## 2026-05-27

- 实现了 AGridVisualizer（基于 ULineBatchComponent），支持运行时持久化网格线渲染，线条颜色、粗细、Z轴偏移均可通过蓝图配置
- 实现了 AGridPlaneVisualizer（单 Plane Mesh + 世界对齐动态材质实例），运行时通过材质参数传递 CellSize/LineWidth/LineColor/Origin
- 设计并记录了材质节点方案，最终采用 M_PrototypeGrid + Translucent 混合模式，实现单次 DrawCall 的高效网格线渲染
- 为两个 Visualizer 均提供了 DrawGrid / ClearGrid / RedrawGrid / SetGridVisible 蓝图 API

## 2026-05-28

- 添加了 InvalidPreviewMaterial 支持：预览 Actor 悬浮在已占用格子上时通过 SetPreviewPlacementValid() 和 OnPreviewValidChanged 回调显示不同材质
- 实现了右键删除已放置 Actor 功能，通过网格 Cell.RoadActor 反向查找替代碰撞命中检测
- 实现了左键拖拽连续放置：将 IA_PlaceItem 绑定到 Started/Triggered/Completed 事件，通过 LastPlacedGridPos 去重实现按住左键连续放置
- 实现了右键拖拽连续删除：将 IA_RemoveItem 绑定到 Started/Triggered/Completed 事件，通过 LastRemovedGridPos 去重实现按住右键连续删除
- 将放置和删除逻辑重构为 TryPlaceAtCursor() 和 TryRemoveAtCursor() 辅助方法，并添加了调试屏幕消息
- 在 GridManager::OccupyCell() 中注册了 AGridPlaceableActor 的 this 指针，实现从网格单元反向查找 Actor
- 修复了 UE 5.6 上的编译错误：移除了 AGridVisualizer 上的 Deprecated 标记
- 更新了 TDD.md 和 TDD_Chinese.md 以反映已实现的功能（预览有效性、拖拽连续放置/删除、右键删除）
- 创建了 ARoadTile（继承 AMeshGridPlaceableActor），引入 FRoadMeshConfig 结构体（CanonicalMask + Mesh + ScaleMultiplier(FVector) 逐项配置）
- 实现了 FindMeshConfig() 含 90° 顺时针掩码旋转查找，从单个标准朝向模型匹配所有方向变体
- 实现了 UpdateAppearance()，根据网格 ConnectedMask 自动切换 Mesh / Rotation / Scale，支持逐轴 ScaleMultiplier
- 添加了 ReferenceCellSize 用于基于 CellSize 的基础缩放；90°/270° 旋转时自动 Swap(ScaleMultiplier.X,Y)
- 将 ARoadTile 订阅到 GridManager::OnCellChanged 委托，放置/删除时自动触发邻居外观刷新
- 定义了 CanonicalMask 值：死胡同=8(Right)、直路=12(Left+Right)、转角=10(Down+Right)、T字路口=14(Down+Left+Right)、十字路口=15(全方向)
- 更新了 TDD.md 和 TDD_Chinese.md，记录了正确的 CanonicalMask 映射表和旋转/缩放策略

## 2026-05-29

- 通过从 Blender 导入新的 SM_Road_Main_Extend 模型，修复了 SM_Road_Main 的模型 bug
- 在 AGridPlaceableActor 基类添加了 virtual UpdatePreviewAppearance() 方法（默认空实现）
- 在 ARoadTile 覆写 UpdatePreviewAppearance()：预览时通过 GridManager::CalculateConnectedMask 预测 ConnectedMask 并用 FindMeshConfig 切换 mesh/rotation/scale，同时使用 preview material
- 在 ARoadTile 添加了 MeshMaterialCache (TMap<UStaticMesh*, TArray<UMaterialInterface*>>)，从 UStaticMesh::GetStaticMaterials() 延迟缓存每 mesh 的原始材质
- 在 ARoadTile 覆写了 OnEnterPlaced() 和 OnPreviewValidChanged()，防止材质在 preview/invalid/original 之间随机切换
- 在 UpdateAppearance() 中添加了 EnsureMeshMaterialsCached() + RestoreMeshMaterials()，确保邻居更新时正确恢复材质
- 在 CityFlowPlayerController::UpdatePreviewPosition() 中每帧调用 UpdatePreviewAppearance()
- 更新了 TDD.md 和 TDD_Chinese.md，记录预览外观和 MeshMaterialCache 架构

- 创建了 ABuilding 类，继承 AMeshGridPlaceableActor，支持多单元格网格放置
- 添加了 FBuildingDoorway 结构体（RelativePosition + FacingDirection），用于可配置的出入口定义
- 实现了 AutoGenerateDoorways()，在建筑每条边外侧一格的中点自动生成出入口
- 实现了 GetDoorwayWorldPositions()，用于出入口相对坐标到绝对网格坐标的转换
- 覆写了 UpdatePreviewAppearance()，实现建筑预览的实时居中和缩放
- 实现了 UpdateBuildingAppearance()，用于放置状态的视觉居中和基于 BuildingSize/CellSize 的 Mesh 缩放
- 添加了 bIsDestination 标志，用于区分起点（住宅）和目的地（商业）建筑
- 在 AGridPlaceableActor 中添加了公开的 GetBuildingSize() 访问器
- 添加了 UGridManager::TryPlaceBuildingRandom()，支持从关卡蓝图随机放置建筑用于调试
- 更新了 TDD.md 和 TDD_Chinese.md，包含 ABuilding 类层级和完整的 2.4 章节文档
- 在 TDD 中英文版中更新了类层级图，将 ARoadTile 和 ABuilding 纳入其中
