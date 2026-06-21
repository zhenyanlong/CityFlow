# CityFlow: Procedural Road Planning and Traffic Simulation

> **Draft note:** This is a content draft for the later LaTeX report. Replace all square-bracket notes before submission. The English uses short sentences and a simple academic style. The final version must fit 3-4 pages.

## 1. Short Game Design Document

### 1.1 Game concept and core loop

CityFlow is a strategy game about road planning and traffic flow. The player acts as a city planner. Each match has three main phases: Planning, Simulation, and Evaluation.

During Planning, buildings are placed on a grid and the player builds the main road network. Road tiles use a limited shared budget. The player can place roads with the left mouse button and remove them with the right mouse button. Road models change automatically when their neighbours change. This creates straight roads, corners, T-junctions, and crossroads without manual rotation.

The player can then start the procedural branch generator. This system tries to connect building entrances to the main road network. It can also add smaller side roads when enough budget remains. The player still makes the important planning choices. The generator supports the player's plan rather than replacing it.

During Simulation, road editing is disabled. Vehicles spawn from buildings, use A* pathfinding, and follow spline paths to their destinations. They queue behind other vehicles and request access to busy intersections. The player watches the traffic and can hover over a vehicle to see its destination direction. When the timer ends, the Evaluation screen shows a detailed score report.

![A generated city during the Planning phase](<images/Main gameplay overview.png>)

*Figure 1. A generated city with buildings, the grid, player roads, and available planning controls.*

### 1.2 Goals, scoring, and engagement

CityFlow does not use a simple win or lose screen. The aim is to create a connected and efficient city. The final score measures five areas: building connectivity, traffic results, travel efficiency, budget efficiency, and runtime. A small difficulty multiplier helps compare maps with different sizes.

This scoring model gives the player several goals. Connecting every building is important, but it is not enough by itself. A road network can be fully connected and still create long journeys or heavy traffic. The player must also use the road budget carefully and avoid placing too many vehicles through one junction.

Random Mode has Easy, Medium, and Hard settings. Higher levels use more buildings, faster vehicle generation, and longer simulations. They provide slightly more total road budget, but less budget per building. This increases pressure without changing the basic rules.

The main source of engagement is the relationship between control and surprise. The player controls the arterial roads, but the PCG system controls some of the smaller branches. A clear main road often produces a clean result. A weak plan can create long branches, wasted budget, or disconnected areas. This makes each generated city different while keeping the player's decisions meaningful.

![Difficulty selection and its current parameter summary](<images/Difficulty selection.png>)

*Figure 2. Difficulty selection explains the building count, traffic rate, runtime, and budget before a match.*

![Valid and invalid road placement previews](<images/Valid and invalid road placement preview.png>)

*Figure 3. Green and red preview materials give immediate feedback during road placement.*

### 1.3 Narrative and art style

The game has a light narrative. The player is a planner in a growing modern city. Residential and commercial buildings need access to one shared road system. The city grows from a mix of planned main roads and generated local streets.

The visual style is low-poly and colourful. Roads use simple grey shapes. Buildings use clean forms and readable colours. Vehicles are small and bright, so traffic can be followed from a top-down camera. This style was selected because it is clear during play and realistic for the project schedule. Rounded foundations and bevelled sidewalks help the buildings sit more naturally on the grid.

The game also includes a data-driven tutorial, English and Simplified Chinese localisation, master and SFX volume settings, placement sounds, traffic warnings, and score popups. These features support players who are learning the rules.

![CityFlow main menu over an automated city preview](<images/MainMenu.png>)

*Figure 4. The low-poly visual style is also used in the animated main-menu background.*

## 2. Short Technical Design Document

### 2.1 Technical overview and architecture

CityFlow is developed in Unreal Engine 5.6 for Windows. Most gameplay systems are written in C++, while Blueprints are used to configure assets, Widget Blueprints, meshes, materials, audio, and visual effects. The project uses Unreal systems including UMG, Enhanced Input, Niagara, ProceduralMeshComponent, World Subsystems, Data Assets, Sound Classes, and the native localisation system.

The architecture separates global systems into managers:

- `CityFlowGameMode` controls the Planning, Simulation, and Evaluation phases.
- `GridManager` stores the logical grid, occupied cells, road connections, and road budget.
- `LSystemManager` creates the assisted PCG road network.
- `VehicleManager` spawns vehicles, finds routes, and manages traffic density.
- `ScoringManager` collects results and builds the final score breakdown.
- `CityFlowHUD` controls the menu, tutorial, settings, game HUD, pause screen, and evaluation screen.

These systems use Unreal delegates for important events. For example, road changes update neighbouring road meshes and building foundations. Vehicle arrival and death events update scoring and UI feedback. This reduces direct links between systems and makes the code easier to test and change.

The architecture and game-loop diagram sources are included in [diagrams.md](diagrams.md). They will be exported as vector figures when the LaTeX template is available.

### 2.2 Grid, buildings, and roads

The grid is stored as a two-dimensional array of cells. Each cell records whether it is empty, road, or building. It also stores a four-direction connection mask. Road actors read this mask and select the correct mesh and rotation.

Buildings can occupy several cells and can be rotated by 90-degree steps. Their entrances are stored as local grid positions and directions. The code converts these values after rotation, so roads only connect at real entrances. Building foundations are generated with procedural meshes. Their rounded shape changes when a doorway is connected to a road.

Player placement uses a preview actor. It snaps to the grid and changes material when a position is valid or invalid. Holding the mouse button supports continuous placement. This makes large road plans faster to build.

![A building footprint and its configured entrance markers](<images/Building and Entrances.png>)

*Figure 5. Building entrances are stored on the logical grid and remain separate from the visual mesh.*

![Road mesh examples for different connection masks](<images/Road mesh change.png>)

*Figure 6. A road tile changes mesh and rotation when its neighbour mask changes.*

### 2.3 Procedural road generation

The original idea came from the critical review of PCG road generation and L-systems. Early L-system prototypes produced organic roads, but they did not always connect all buildings. A small road near each entrance could also give a false impression of success even when those roads belonged to separate networks.

The final implementation uses a hybrid approach. First, it finds the main road component. It then calculates connection paths from unconnected entrances and reserves the budget needed for these paths. Existing roads have zero extra placement cost, so the system reuses them when possible. Only the remaining budget can be used for organic branches. Branch candidates are scored by distance and direction towards useful entrances, with controlled random choice.

This keeps the visual quality of rule-based growth, but gives a stronger gameplay result. The generated roads still look different between matches, while the system has a clear goal and respects the player's limited budget.

![Player arterial roads before procedural generation](<images/arterial roads before generation.png>)

*Figure 7(a). The player first defines the arterial road structure.*

![L-system-inspired road growth in progress](<images/L-system growth in progress.png>)

*Figure 7(b). Supporting branches grow step by step while the shared budget decreases.*

![Final network connecting the generated buildings](<images/Final network showing all building.png>)

*Figure 7(c). The final network reuses arterial roads and connects the building entrances into one larger component.*

### 2.4 Vehicle simulation and feedback

Vehicles use A* to find a path through connected road cells. The grid path is converted into a world-space spline with lane offsets. Vehicles use forward probes to detect other vehicles. T-junctions and crossroads use direction-based reservations, so vehicles do not enter a busy junction at the same time from conflicting directions.

The simulation also includes two special vehicle behaviours. A Rampage vehicle may ignore intersection control after waiting too long. A Teleport vehicle can move forward along its route after a long delay. The HUD warns the player when these abilities start. These events make serious congestion visible and support the game's feedback loop.

![Traffic during the Simulation phase](<images/traffic during Simulation.png>)

*Figure 8(a). Different vehicles use the generated network during Simulation.*

![Hovered vehicle with outline and destination arrow](<images/vehicle with outline and destination arrow.png>)

*Figure 8(b). Hover feedback helps the player understand where a vehicle is travelling.*

![Direction-based intersection reservation indicators](<images/Intersection reservation indicator.png>)

*Figure 8(c). Green and red indicators show free and occupied intersection directions during debugging.*

![Configured standard, Rampage, and Teleport vehicle types](<images/SpecialVehicle.png>)

*Figure 9. Vehicle variety includes standard traffic and two congestion-related special behaviours.*

## 3. Critical Feature Justification

The critical feature is **procedural content generation of road networks**, based on the earlier review of L-systems. [ADD 2-3 CITATIONS FROM THE CRITICAL REVIEW HERE.] This feature is central to CityFlow because it changes the road layout, the use of budget, vehicle routes, congestion, and the final score.

For graphics, PCG creates a larger variety of visible city layouts from a limited set of road meshes. Automatic connection masks keep roads visually joined after each generated step. This supports visual variety without requiring a unique hand-made map for every match.

For gameplay depth, the player must plan roads that guide the generator. The player cannot only search for one shortest path. They must consider branch space, building entrances, shared components, and future traffic. The reserved connection budget also creates a clear trade-off between safe connectivity and decorative growth.

For engagement, the generated result gives useful surprise. The same player plan can create small differences because branch scoring includes controlled randomness. However, the connection plan reduces unfair failures. This balance gives the player a reason to replay and improve the arterial layout.

The project also shows a limitation of a pure L-system. Local production rules are good for creating repeated organic patterns, but they do not automatically understand global connectivity. CityFlow therefore combines local growth with path planning and graph validation. This was an important design change from the early prototype.

![The final evaluation report](<images/Final evaluation report.png>)

*Figure 10. The evaluation screen reports connectivity, road use, arrivals, deaths, and travel time.*

## 4. Testing and Debugging Summary

Testing used repeated manual play sessions, standalone and Editor builds, runtime logs, cheat commands, and visual debug tools. There is not yet a large formal unit-test suite. This is a current limitation and should be stated honestly.

Important tests included:

- placing and removing roads by clicking and dragging;
- checking all four building rotations and doorway positions;
- generating roads on different random seeds and budgets;
- confirming that all buildings belong to one shared road component;
- running Easy, Medium, and Hard traffic profiles;
- checking intersection reservations, congestion, arrivals, and deaths;
- testing English and Simplified Chinese UI;
- checking tutorial entries with and without images;
- building the standalone Windows target after major code changes.

Several bugs were found through this process. The first PCG version could report success when buildings were connected to different road components. This was fixed by checking real graph components and reserving complete connection paths. Rotated rectangular buildings used their rotated size twice, which caused the mesh and foundation to use the wrong footprint. The fix separated local mesh size from the rotated logical footprint. Random grid dimensions also caused a half-cell shift when an axis changed between odd and even sizes. The grid now uses a stable centre anchor cell, and visualisers redraw after every new match.

Another bug occurred when a vehicle spawned on a building entrance that was also an intersection. The vehicle entered the intersection collision box before its route direction was ready. It was then marked as having passed the junction and waited before moving. The spawn point was moved to the first spline point inside the building, and an intersection is now marked as passed only after a valid entry.

These examples show an iterative development process. Visual debug information was especially useful because many problems involved a difference between the logical grid and the visible world.

![Grid and rotated-building alignment before correction](<images/Grid and rotated-building alignment before the fix.png>)

*Figure 11. A rotated rectangular building exposed a mismatch between the logical footprint and local visual scale.*

![A procedural sidewalk with reversed face winding](<images/Before image of the procedural sidewalk.png>)

*Figure 12(a). Reversed triangle winding caused the procedural sidewalk surface to appear black.*

![A later inset-outline error exposed after the winding correction](<images/After image of the procedural sidewalk.png>)

*Figure 12(b). After the first correction, an excessive corner inset produced long fan-shaped triangles. This led to convexity checks and a safe inset limit.*

The menu and help systems were also tested in both supported languages:

![English data-driven tutorial](<images/TutorialWidget.png>)

*Figure 13(a). The English tutorial shows a generated topic list, text, and an optional image.*

![Simplified Chinese data-driven tutorial](<images/TutorialWidget_Chinese.png>)

*Figure 13(b). The same tutorial content after Unreal's native culture switch to Simplified Chinese.*

![Audio and language settings](<images/SettingWidget.png>)

*Figure 14. Persistent settings provide master sound, SFX volume, and language selection.*

## 5. Conclusion and Future Work

CityFlow applies PCG road research to a playable planning game. Its main contribution is the combination of player-built arterial roads, L-system-inspired growth, and connectivity-aware path planning. The project also connects this system to traffic simulation, scoring, tutorial support, localisation, and player feedback.

Future work should include formal automated tests for grid rotation, connectivity, and road budgets. More playtest data would help tune difficulty and traffic density. The PCG system could also compare several valid connection plans and select one based on road shape, travel time, or visual quality.

## AI Use Declaration Draft

AI tools were used to support planning, language drafting, and review. The student checked the technical statements against the project code and design documents, selected the final content, and will revise the report before submission. [EDIT THIS STATEMENT TO MATCH THE UNIVERSITY'S REQUIRED DISCLOSURE FORMAT.]
