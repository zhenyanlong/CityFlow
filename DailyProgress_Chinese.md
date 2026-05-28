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
