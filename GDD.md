# Game Design Document: CityFlow

---

## 1. Core Mechanics

CityFlow is a strategy game centered on road planning and traffic simulation, divided into two independent phases: the **Planning Phase** and the **Simulation Phase**.

During the Planning Phase, the player works on a grid-based city map, using a limited "arterial budget" to lay down the city's skeleton road network, connecting randomly generated residential zones (origins) and commercial zones (destinations). The player freely decides the direction of arterial roads, leaves branch connection points, and after confirming the arterial layout, triggers the L-system to automatically generate the capillary road network — the system starts from the reserved arterial connection points and any unconnected buildings, procedurally growing side streets to fill in the blocks and attempting to connect all remaining buildings to the network. The player may iteratively adjust the arterial layout and re-generate the branch network until satisfied.

Once the Simulation Phase begins, the road network is locked, and vehicles begin pouring out from their origins, traveling along the network toward their respective destinations. The player can no longer modify roads and can only observe traffic flow. Vehicles automatically find paths, yield at intersections, and queue up on congested segments. If the road network is poorly designed, certain segments will experience visible congestion, lowering the score; conversely, an efficient network allows vehicles to arrive smoothly, yielding high score rewards.

The core appeal of the game lies in the strategic depth of "planning arterial roads to guide procedural generation" — the player cannot directly control the specific shape of branch roads, but can greatly influence the L-system's growth results through the position, direction, and reserved connection points of arterial roads, striving for maximum efficiency under a limited budget.

## 2. Win / Lose Conditions

The game uses a scoring system. There is no traditional "victory" or "defeat"; instead, the final total score measures the player's planning ability:

**Score sources:**
- Each vehicle that successfully reaches its destination awards base points
- Connecting all buildings awards a bonus
- Remaining road budget is proportionally converted to points

**Score penalties:**
- During the Simulation Phase, any road tile carrying too many vehicles simultaneously is flagged as congested, continuously deducting points per second

**Game end conditions:**
- The Simulation Phase has a fixed duration (e.g., 3 minutes); the evaluation panel appears when the timer expires
- If all vehicles have arrived and no new vehicles are spawning, the simulation also ends early

Players may replay repeatedly, optimizing arterial design and branch generation strategies to chase higher scores.

## 3. Narrative (Optional)

CityFlow is set in a rapidly expanding modern city. The municipal planner (the player) is responsible for designing the arterial road network, while surrounding neighborhoods and small commercial districts rely on self-growing capillary road networks to connect into the urban arteries. Each playthrough is the birth of a new city, where the player balances order (arterial planning) with emergence (L-system growth), aiming to build an efficient city with smooth traffic and universal accessibility.

## 4. Art Style

The game adopts a low-poly style. Roads are presented as clean gray modules, buildings are color-coded cuboid blocks (residential = blue tones, commercial = warm tones). Vehicles are small rectangular prisms in vibrant colors, making it easy to track their flow across the network. The overall visual approach references the clean, abstract style of *Mini Motorway*, complemented by soft top-down lighting and light-colored ground, keeping the road network structure and traffic flow as the absolute visual focus. The rationale for this style choice: low-poly asset production is inexpensive, visual readability is high, and it can achieve a cohesive, polished level of finish within a one-month development cycle.
