# Image Plan and Missing Evidence

The final report is limited to 3-4 pages, so several screenshots should be combined into labelled multi-panel figures. Aim for six figure groups containing about 12 useful images.

## Figure 1 - Main gameplay overview

**Needed:**

- A clean Planning-phase screenshot at 16:9.
- Show several buildings, the visible grid, player roads, and the HUD.
- Hide debug markers unless they explain the screenshot.

**Status:** Added as `Main gameplay overview.png` and inserted into both drafts.

## Figure 2 - Player guidance and UX (three panels)

**Needed:**

1. Difficulty selection with the details text visible.
2. Valid and invalid road placement preview.
3. Final evaluation report with score categories.

**Status:** All three screenshots are available and inserted. The difficulty and evaluation images are portrait crops and should share one compact row in LaTeX.

## Figure 3 - Architecture diagram

**Needed:**

- A simple diagram showing `CityFlowGameMode`, `GridManager`, `LSystemManager`, `VehicleManager`, `ScoringManager`, and `CityFlowHUD`.
- Show the phase flow Planning -> Simulation -> Evaluation.
- Show delegates/events between managers.

**Status:** Mermaid source is available in `diagrams.md`; it still needs export to PDF/SVG after the LaTeX template is supplied.

## Figure 4 - PCG road generation (three panels)

**Needed:**

1. Player arterial roads before generation.
2. L-system/PCG growth in progress, preferably with debug colour or step display.
3. Final network showing all building entrances connected to one component.

**Status:** All three PCG stages are available and inserted. These should become one labelled `(a)-(c)` composite because this is the most important evidence for the critical feature.

## Figure 5 - Traffic simulation (three panels)

**Needed:**

1. Dense but readable traffic during Simulation.
2. A hovered vehicle with outline and destination arrow.
3. Intersection reservation/debug indicator with vehicles approaching from different directions.

**Status:** All three images are available and inserted. `vehicle with outline and destination arrow.png` is only 150 x 199 pixels and should be replaced with a larger capture before final submission.

## Figure 6 - Testing and iteration (two or three panels)

**Needed:** Choose the clearest examples rather than showing every bug.

- Grid and rotated-building alignment after the fix.
- Doorway debug markers on rotated buildings.
- L-system connectivity debug output.
- Before/after image of the procedural foundation/sidewalk.
- Before/after evidence of the entrance-intersection vehicle delay, if it can be shown clearly.

**Status:** The pre-fix rotated-building image and two sidewalk debugging stages are inserted. Still missing:

- Rotated rectangular building alignment **after** the fix.
- A clean final sidewalk image with no black backface or fan-shaped triangles.
- A clear entrance-intersection vehicle-delay before/after comparison, if this behaviour can be captured visually.
- Optional L-system connectivity debug evidence showing that all entrances belong to one shared component.

## Optional Figure 7 - Tutorial and localisation

**Needed:**

- Tutorial list with one entry, body text, and image.
- The same UI in English and Simplified Chinese, or a compact side-by-side comparison.
- Settings screen with Master Volume, SFX Volume, and Language.

**Status:** English tutorial, Chinese tutorial, and Settings screenshots are available and inserted. They should become one compact localisation/accessibility composite if the final layout has space.

## Additional available images

- `MainMenu.png`: inserted as art-style and presentation evidence.
- `Building and Entrances.png`: inserted in the grid/building system section.
- `Road mesh change.png`: inserted as automatic road-connection evidence.
- `SpecialVehicle.png`: inserted in the vehicle simulation section.

## Remaining image work before LaTeX layout

1. Capture the corrected rotated rectangular building.
2. Capture the fully corrected procedural sidewalk.
3. Replace the low-resolution hovered-vehicle image.
4. Export the Mermaid game-loop, architecture, and PCG diagrams to vector PDF/SVG.
5. Combine the 20 current Markdown images into about six labelled multi-panel figures so the report can remain within 3-4 pages.

## Capture guidance

- Use the same resolution, preferably 1920x1080, for gameplay screenshots.
- Use high-resolution screenshot mode if UI remains correctly scaled.
- Remove editor panels, console text, selection outlines, and unrelated debug lines.
- Keep one clear subject in each panel.
- Use short panel labels such as `(a) Before PCG`, `(b) Growth`, `(c) Connected result`.
- Save source images as PNG in `.codex/coursework_report/images/`.
- Do not stretch images in LaTeX; crop them to matching aspect ratios first.
- Each final figure needs a caption that explains why it is evidence, not only what is visible.

## Missing non-image information

- Two or three references already used in the PCG/L-system critical review.
- Exact playtest participant count and key observations, if formal playtesting was completed.
- Final packaged-build version and test platform specification.
- Final list of third-party art, sound, music, fonts, and tools for the README/credits.
- The official LaTeX template and its citation style.
