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
