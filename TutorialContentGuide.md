# CityFlow Tutorial Content Guide

Use the following order for `DA_CityFlowTutorials.Entries`. `Id` values are stable internal identifiers and must not be localized. Because the Game localization target uses English as its native culture, enter the English `Title` and `Body` in the Data Asset first, gather text, and then enter the Chinese text as the `zh-Hans` translation in the Localization Dashboard.

## 01 — Game Overview

- **Id:** `game_overview`
- **Title (en):** `Welcome to CityFlow`
- **Title (zh-Hans):** `欢迎来到 CityFlow`
- **Body (en):**

  `CityFlow is a road-planning and traffic-simulation game. During Planning, build a clear road network that connects the entrances of every building. You may then ask the procedural road generator to complete smaller branches. During Simulation, the road network is locked and vehicles test how well your city works. Your goal is to connect every building while keeping traffic efficient and using the road budget wisely.`

- **Body (zh-Hans):**

  `CityFlow 是一款道路规划与交通模拟游戏。在规划阶段，你需要修建清晰的道路网络，连接所有建筑的出入口，也可以让程序化道路生成器补全较小的支路。进入模拟阶段后，道路网络会被锁定，车辆将检验城市的实际运行效果。你的目标是在合理使用道路预算的同时连接所有建筑，并保持交通高效。`

- **Suggested image:** Planning → Simulation → Evaluation flow diagram.

## 02 — Choose a Difficulty

- **Id:** `difficulty_selection`
- **Title (en):** `Choose a Difficulty`
- **Title (zh-Hans):** `选择难度`
- **Body (en):**

  `Random Mode begins with a difficulty selection. Easy uses 8 buildings, a 0.90-second vehicle spawn interval, 120 seconds of simulation time, and 220 road tiles. Medium uses 12 buildings, 0.65 seconds, 180 seconds, and 230 road tiles. Hard uses 16 buildings, 0.45 seconds, 240 seconds, and 240 road tiles. Higher difficulties provide slightly more total budget, but the number of buildings and traffic pressure rise much faster.`

- **Body (zh-Hans):**

  `Random Mode 会先进入难度选择。简单难度包含 8 栋建筑、0.90 秒车辆生成间隔、120 秒模拟时间和 220 格道路预算；中等难度包含 12 栋建筑、0.65 秒生成间隔、180 秒和 230 格预算；困难难度包含 16 栋建筑、0.45 秒生成间隔、240 秒和 240 格预算。高难度的总预算会略有增加，但建筑数量和交通压力增长得更快。`

- **Suggested image:** Difficulty widget with the shared details panel visible.

## 03 — Camera Controls

- **Id:** `camera_controls`
- **Title (en):** `Camera Controls`
- **Title (zh-Hans):** `镜头控制`
- **Body (en):**

  `Use W, A, S, and D to move across the city. Use the mouse wheel to zoom in or out. Hold Alt and move the mouse to rotate the camera. While Alt is held, road placement is temporarily disabled so that camera movement cannot place roads accidentally. Press Escape to open or close the pause menu.`

- **Body (zh-Hans):**

  `使用 W、A、S、D 在城市中移动，滚动鼠标滚轮进行缩放。按住 Alt 并移动鼠标可以旋转镜头。按住 Alt 时，道路放置会暂时关闭，避免调整镜头时误铺道路。按 Escape 打开或关闭暂停菜单。`

- **Suggested image:** Keyboard and mouse control diagram.

## 04 — Planning Phase

- **Id:** `planning_phase`
- **Title (en):** `Planning Phase`
- **Title (zh-Hans):** `规划阶段`
- **Body (en):**

  `Planning is the only phase in which roads can be added or removed. The HUD shows the current phase, remaining shared road budget, and the available planning actions. First create a simple main road structure that approaches the buildings. Leave useful straight sections and branch points for the procedural generator. When the network is ready, start the simulation.`

- **Body (zh-Hans):**

  `只有在规划阶段才能添加或删除道路。HUD 会显示当前阶段、共享道路的剩余预算以及可用的规划操作。首先修建一套接近各建筑的简洁主干结构，并为程序化生成器预留直线路段和合适的分支位置。路网准备完成后即可开始模拟。`

- **Suggested image:** Gameplay HUD during Planning with Phase, Budget, Generate, and Start Simulation highlighted.

## 05 — Place and Remove Roads

- **Id:** `road_placement`
- **Title (en):** `Place and Remove Roads`
- **Title (zh-Hans):** `铺设与删除道路`
- **Body (en):**

  `Move the cursor over the grid to preview a road tile. A valid preview uses the configured valid material; an occupied or invalid cell uses the invalid material. Click or hold the left mouse button to place roads continuously. Click or hold the right mouse button to remove placed grid actors. Every placed road consumes one tile from the shared road budget, while removing it restores the occupied grid cell and updates neighbouring road shapes.`

- **Body (zh-Hans):**

  `将鼠标移到网格上可以预览道路。合法位置使用配置的有效预览材质；被占用或不合法的位置使用无效材质。点击或按住鼠标左键可以连续铺路，点击或按住鼠标右键可以删除已放置的网格物体。每格道路都会消耗一格共享预算；删除道路会释放对应网格，并自动更新相邻道路的外观。`

- **Suggested image:** Valid and invalid road previews side by side.

## 06 — Buildings and Entrances

- **Id:** `buildings_and_entrances`
- **Title (en):** `Buildings and Entrances`
- **Title (zh-Hans):** `建筑与出入口`
- **Body (en):**

  `Buildings are placed automatically at the start of a match. A building only joins the road network when a road reaches one of its configured entrance cells. A short road beside the wrong wall does not count. Building markers help you find buildings that are on screen or outside the current view. Check every entrance before starting the simulation.`

- **Body (zh-Hans):**

  `每局开始时建筑会自动放置。只有道路真正连接到建筑配置的出入口网格时，该建筑才会接入路网；仅仅把道路铺在错误的墙边并不算连接成功。建筑标记可以帮助你找到屏幕内或当前视野外的建筑。开始模拟前，请检查每个建筑的出入口。`

- **Suggested image:** Close-up of a building entrance connected correctly and incorrectly.

## 07 — Automatic Road Shapes

- **Id:** `road_connections`
- **Title (en):** `Road Connections`
- **Title (zh-Hans):** `道路连接`
- **Body (en):**

  `Road tiles automatically change mesh and rotation to match their connected neighbours. Straight roads, corners, T-junctions, and crossroads are selected from the current connection mask. Roads also recognise a building when they touch one of its real entrances. If a road looks disconnected, check for a missing grid cell or an incorrect building entrance rather than trying to rotate the road manually.`

- **Body (zh-Hans):**

  `道路会根据相邻连接自动切换模型和旋转，直路、弯道、T 字路口与十字路口都由当前连接状态决定。当道路接触建筑的真实出入口时，也会正确识别建筑连接。如果道路看起来没有连上，请检查是否缺少网格或接错建筑入口，不需要手动旋转道路。`

- **Suggested image:** Straight, corner, T-junction, and crossroads examples.

## 08 — Shared Road Budget

- **Id:** `road_budget`
- **Title (en):** `Use the Road Budget Wisely`
- **Title (zh-Hans):** `合理使用道路预算`
- **Body (en):**

  `Player roads and procedurally generated roads consume the same finite road budget. Long detours and decorative loops leave fewer tiles for connecting buildings. Build a useful main network first, keep routes direct, and reserve enough space and budget for branches. The final report rewards efficient budget use only when the city is connected and vehicles can actually reach their destinations.`

- **Body (zh-Hans):**

  `玩家铺设的道路与程序化生成的道路会共同消耗有限的道路预算。过长的绕路和装饰性环路会减少连接建筑所需的可用道路。请优先搭建有效主干、保持路线直接，并为支路预留空间和预算。只有当城市确实连通且车辆能够抵达目的地时，结算报告才会奖励预算效率。`

- **Suggested image:** Efficient compact network versus wasteful detours.

## 09 — Procedural Branch Generation

- **Id:** `procedural_branches`
- **Title (en):** `Generate Supporting Branches`
- **Title (zh-Hans):** `生成辅助支路`
- **Body (en):**

  `The branch generator supports your plan; it does not replace it. Select Generate Branches during Planning after creating a useful main road component. The system first reserves roads needed to connect building entrances to the main network, then spends only surplus budget on organic side branches. It reuses existing roads whenever possible and stops when all buildings share one network, its budget is exhausted, or no legal connection remains.`

- **Body (zh-Hans):**

  `支路生成器用于辅助你的规划，而不是代替你完成规划。先建立有效的主道路连通分量，再在规划阶段点击“生成支路”。系统会优先预留连接建筑出入口所需的道路，只有多余预算才会用于有机侧向生长；它会尽量复用现有道路，并在所有建筑进入同一网络、预算耗尽或不存在合法连接时停止。`

- **Suggested image:** Main roads before generation and the completed branch network after generation.

## 10 — Start the Simulation

- **Id:** `simulation_phase`
- **Title (en):** `Simulation Phase`
- **Title (zh-Hans):** `模拟阶段`
- **Body (en):**

  `Select Start Simulation when you are satisfied with the road network. Road placement is disabled and vehicles begin travelling between buildings. The countdown shows the remaining simulation time. Vehicles find routes automatically, follow lane offsets, queue behind traffic, and reserve busy intersections. You may select Restart Planning during the simulation to clear all active vehicles and edit the same road network again.`

- **Body (zh-Hans):**

  `对路网满意后，点击“开始模拟”。道路编辑会被关闭，车辆开始在建筑之间行驶，倒计时会显示剩余模拟时间。车辆会自动寻路、沿车道偏移行驶、在车流后排队并预占繁忙路口。模拟过程中可以点击“重新规划”，清除所有在途车辆并继续编辑当前路网。`

- **Suggested image:** Active simulation with countdown, traffic, and Restart Planning button.

## 11 — Inspect Traffic

- **Id:** `traffic_inspection`
- **Title (en):** `Inspect Traffic`
- **Title (zh-Hans):** `观察交通`
- **Body (en):**

  `During Simulation, move the cursor over a vehicle to highlight it and display an arrow pointing toward its destination. Use this to understand where traffic is trying to go. Repeated queues, long waits, and overloaded intersections usually indicate indirect routes or too many flows sharing one junction. A clear hierarchy of main roads and short branches is usually more reliable than one giant intersection.`

- **Body (zh-Hans):**

  `在模拟阶段，将鼠标悬停在车辆上可以高亮该车辆，并显示指向目的地的箭头。你可以借此判断车流真正想去哪里。反复排队、长时间等待和过载路口通常意味着路线过于绕远，或太多车流共用同一个路口。清晰的主干与短支路层级通常比一个巨型路口更可靠。`

- **Suggested image:** Hovered vehicle with outline and destination arrow.

## 12 — Special Vehicles

- **Id:** `special_vehicles`
- **Title (en):** `Special Vehicle Abilities`
- **Title (zh-Hans):** `特殊车辆能力`
- **Body (en):**

  `Some vehicles react dangerously when blocked for too long. A Rampage vehicle may stop respecting intersection reservations and force its way forward. A Teleport vehicle may jump farther along its route and can destroy vehicles overlapping its arrival point. The HUD warns you when either ability activates. These events are symptoms of severe congestion: reduce long queues and intersection conflicts to prevent them.`

- **Body (zh-Hans):**

  `部分车辆在被阻塞过久后会产生危险行为。暴走车辆可能不再遵守路口预占并强行前进；瞬移车辆可能沿路线向前跳跃，并摧毁落点附近重叠的车辆。能力触发时 HUD 会显示警告。这些事件通常意味着路网发生了严重拥堵，请通过减少长队和路口冲突来避免它们。`

- **Suggested image:** Split image showing Rampage and Teleport activation effects.

## 13 — Evaluation and Scoring

- **Id:** `evaluation_and_scoring`
- **Title (en):** `Evaluation Report`
- **Title (zh-Hans):** `结算报告`
- **Body (en):**

  `When the timer ends, CityFlow evaluates the plan instead of declaring a simple win or loss. Connectivity measures how many buildings share a usable network. Traffic Outcome rewards arrivals and penalises deaths. Travel Efficiency measures average time per road cell. Budget Efficiency rewards compact construction when the network works. The report also shows runtime information and a small map-difficulty multiplier. A high score requires both complete connections and healthy traffic.`

- **Body (zh-Hans):**

  `倒计时结束后，CityFlow 会评价你的规划，而不是简单判定胜负。连通性衡量有多少建筑处于可用的共同路网中；交通结果奖励抵达并惩罚死亡；通行效率衡量车辆通过每格道路的平均时间；预算效率会在路网有效时奖励紧凑建设。报告还会显示运行时间信息和小幅地图难度倍率。高分需要同时具备完整连接与健康车流。`

- **Suggested image:** Evaluation widget with the five score areas highlighted.

## 14 — Settings and Language

- **Id:** `settings_and_language`
- **Title (en):** `Audio and Language Settings`
- **Title (zh-Hans):** `声音与语言设置`
- **Body (en):**

  `Settings contains Master Volume, SFX Volume, and language controls. Master Volume affects the whole game mix. SFX Volume affects sounds routed through the SFX SoundClass, including configured road-placement feedback. Language switching uses Unreal Engine's native localization system and supports English and Simplified Chinese. Settings are saved and restored in later sessions.`

- **Body (zh-Hans):**

  `设置界面包含主音量、音效音量和语言选项。主音量控制整个游戏混音；音效音量控制路由到 SFX SoundClass 的声音，包括已正确配置的道路放置反馈。语言切换使用 Unreal Engine 原生本地化系统，支持英文与简体中文。设置会被保存，并在之后的会话中恢复。`

- **Suggested image:** Settings widget showing both sliders and the language selector.

## Editor Checklist

1. Open `/Game/UI/Data/DA_CityFlowTutorials` and create the 14 entries in the order above.
2. Keep every `Id` exactly as written and unique.
3. Enter the English title/body as the native source text.
4. Assign an image only when it materially explains the entry; leaving `Image` empty automatically hides the image widget.
5. Save the Data Asset, then run Gather Text for the `Game` localization target.
6. Enter the Chinese translations under `zh-Hans`, then Compile Text.
7. In `WBP_TutorialWidget`, assign `DA_CityFlowTutorials` to `TutorialData`.
8. Keep `bBuildDefaultEntryButtons` enabled unless the Blueprint creates its own list buttons and calls `SelectTutorial(Index)`.
9. Verify the optional widgets are named `TutorialList`, `Txt_TutorialTitle`, `Txt_TutorialBody`, `Img_Tutorial`, and `Btn_Back`.
10. Test the first entry, an entry with an image, an entry without an image, Back, English, and Simplified Chinese.

### Generated Button Appearance

When `bBuildDefaultEntryButtons` is enabled, open `WBP_TutorialWidget` → **Class Defaults** → **CityFlow | Tutorial | Default Entry Appearance**:

- `Entry Button Style`: normal, hovered, pressed, and disabled brushes for unselected entries.
- `Selected Entry Button Style`: persistent button appearance for the current tutorial entry.
- `Entry Text Font`: font family, typeface, size, outline, and letter-spacing settings.
- `Entry Text Color`: label color for unselected entries.
- `Selected Entry Text Color`: label color for the current entry.
- `Entry Content Padding`: space between each button border and its text.
- `Entry Slot Padding`: space around each button inside `TutorialList`.

The generated buttons fill the horizontal space of `TutorialList`. Selection styling is applied automatically when `SelectTutorial(Index)` runs. If appearance values are changed at runtime, call `RefreshGeneratedEntryStyles()` to reapply them.
