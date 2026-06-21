# Report Diagram Sources

These Mermaid diagrams can be previewed in a Markdown viewer and later exported as SVG or PDF for LaTeX.

## Game loop

```mermaid
flowchart LR
    A["Difficulty Selection"] --> B["Planning"]
    B --> C["Player Builds Arterial Roads"]
    C --> D["PCG Generates Supporting Branches"]
    D --> E["Simulation"]
    E --> F["Traffic and Intersection Control"]
    F --> G["Evaluation Report"]
    G -->|"Replay"| A
    E -->|"Restart Planning"| B
```

Suggested caption: **CityFlow game loop. Player planning guides PCG growth before the road network is tested by traffic.**

## Runtime architecture

```mermaid
flowchart TB
    GM["CityFlowGameMode\nPhase State Machine"]
    GRID["GridManager\nCells, Connections, Budget"]
    PCG["LSystemManager\nReserved Paths + Organic Growth"]
    VM["VehicleManager\nSpawn, A*, Traffic Density"]
    SCORE["ScoringManager\nMetrics and Final Breakdown"]
    HUD["CityFlowHUD / UMG\nMenus, Tutorial, Feedback"]

    GM -->|"Initialise / Change Phase"| GRID
    GM -->|"Trigger Growth"| PCG
    GRID -->|"Grid Queries"| PCG
    PCG -->|"Place Road Tiles"| GRID
    GM -->|"Start Simulation"| VM
    GRID -->|"Road Graph"| VM
    VM -->|"Arrival / Death Events"| SCORE
    SCORE -->|"Score Events"| HUD
    GM -->|"Phase Events"| HUD
    GRID -->|"Budget / Cell Events"| HUD
```

Suggested caption: **Main CityFlow runtime systems. World Subsystems share data through the grid and send events to scoring and UI.**

## Hybrid road-generation flow

```mermaid
flowchart TD
    A["Read Player Roads and Building Entrances"] --> B["Find Main Road Component"]
    B --> C["Find Unconnected Entrances"]
    C --> D["Calculate Reusable Connection Paths"]
    D --> E["Reserve Required Budget"]
    E --> F["Place Connection Paths Step by Step"]
    F --> G{"All Buildings Connected?"}
    G -->|"No"| C
    G -->|"Yes"| H{"Surplus Budget?"}
    H -->|"Yes"| I["Score and Grow Organic Branches"]
    I --> H
    H -->|"No"| J["Finish and Validate Shared Component"]
```

Suggested caption: **Hybrid PCG process. Global connection planning protects connectivity before local L-system-inspired growth spends surplus budget.**

