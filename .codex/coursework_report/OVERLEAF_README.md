# CityFlow Overleaf Project

## Files to upload

Upload the following into one new blank Overleaf project:

1. `CityFlowReport.tex`
2. `GamesEngineering.sty`
3. `GamesEngineering.bib`
4. The complete `images` folder

The other Markdown drafts are working notes and do not need to be uploaded.

## Overleaf setup

1. Sign in to Overleaf and choose **New Project -> Blank Project**.
2. Give it a name such as `CityFlow Game Project Report`.
3. Delete the generated `main.tex` or leave it unused.
4. Upload the three report files listed above.
5. Create or upload the `images` folder, keeping every image filename unchanged.
6. Open **Menu -> Main document** and select `CityFlowReport.tex`.
7. Keep the compiler as **pdfLaTeX**.
8. Click **Recompile**. BibLaTeX is configured to use the BibTeX backend supplied by the template.

## Required edits before submission

- Replace `[YOUR NAME]` and `[YOUR STUDENT ID]` near the top of `CityFlowReport.tex`.
- Check the AI-use statement against the exact University disclosure guidance.
- Confirm that the four references match the sources used in the earlier critical review. Replace them if the critical review used different editions or papers.
- Add measured playtest details if available.
- Check all image captions and remove any image that does not support the nearby text.
- Confirm that the compiled PDF is 3-4 pages. Float placement can change after Overleaf compilation.
- Add a clean corrected-building screenshot and final corrected-sidewalk screenshot if they become available.

## If the report is longer than four pages

Reduce content in this order:

1. Remove Figure 4, the three-panel debugging figure.
2. Shorten the traffic subsection.
3. Shorten the narrative and player-support paragraph.
4. Keep Figure 2, the PCG sequence, because it is the main critical-feature evidence.

Do not reduce the font size or page margins because they are controlled by the supplied template.

## If references do not appear

Use **Recompile from scratch** in Overleaf. The project uses `biblatex` with the `bibtex` backend, so Overleaf may need more than one compilation pass to resolve citations and the bibliography.

