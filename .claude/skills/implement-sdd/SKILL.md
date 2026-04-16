# Claude Code Behavior Rules

## Before Starting Any SDD
1. Read `.claude/CONSTRAINTS.md` in full
2. Check if any existing ADRs relate to this task
3. Review `## Outcome & Decisions` of any referenced SDDs in `.claude/sdd/`

## After Completing Any SDD
1. Append `## Outcome & Decisions` to the SDD file:
    - Final technical choices and key parameters
    - Rejected alternatives and reasons
    - Notes that future SDDs need to be aware of
2. Update `.claude/CONSTRAINTS.md` under `## Architecture Decisions`:
    - [ADR-XXX] One-line decision → One-line rationale

Proactively remind me to run the wrap-up after every SDD, unless I say otherwise.