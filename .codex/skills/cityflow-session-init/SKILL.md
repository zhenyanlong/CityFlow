---
name: cityflow-session-init
description: Initialize a CityFlow Codex conversation by reading the project's .trae/skills directory and executing the .trae/skills/session-init skill. Use when the user says to initialize the session, start work, continue development, sync progress, read Trae skills, use Trae session-init, or otherwise needs current CityFlow project context loaded at the start of a conversation.
---

# CityFlow Session Init

Use this project-local bridge skill to reuse the Trae workflow that already lives with the CityFlow repo.

## Workflow

1. Confirm the workspace root is the CityFlow project root containing `.trae/skills`.
2. List `.trae/skills` and read each direct child `SKILL.md` frontmatter/name/description to understand the available Trae skills.
3. Read `.trae/skills/session-init/SKILL.md` completely.
4. Execute the Trae `session-init` workflow as written:
   - Read `GDD.md`, `GDD_Chinese.md`, `TDD.md`, `TDD_Chinese.md`, `DailyProgress.md`, and `DailyProgress_Chinese.md`.
   - Check whether English/Chinese document pairs have matching structure and date coverage.
   - Summarize current project status, recently completed work, active focus, and reasonable next steps.
5. Preserve any additional rules found in `.trae/skills/session-init/SKILL.md`, especially project code organization and commit behavior.

## Project Rules

- Treat `.trae/skills` as the source of truth for Trae-era project workflows.
- Do not copy large Trae skill bodies into this skill; read them fresh from `.trae/skills` when this skill runs.
- Never run `git commit` unless the user explicitly asks for a commit. When useful, provide a commit summary instead.
- Prefer Chinese for user-facing summaries unless the user asks otherwise.
