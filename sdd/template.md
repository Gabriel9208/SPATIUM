# SDD: [Feature / Task Name]

> **Version**: 1.0  
> **Status**: `DRAFT` | `READY` | `IN_PROGRESS` | `DONE` | `BLOCKED`  
> **Created**: YYYY-MM-DD  
> **Last Updated**: YYYY-MM-DD  
> **Author**: [Your Name]

---

## 0. Quick Reference (for Claude Code)

> TL;DR for Claude Code to orient itself at the start of every session. Read this first.

```
TASK   : [One-line description]
SCOPE  : [Files/modules affected]
GOAL   : [Single measurable outcome]
AVOID  : [What NOT to do — hard constraints]
STATUS : [What's done / what's next]
```

---

## 1. Context & Background

### 1.1 Why This Task Exists
<!--
  Explain where this task fits in the broader project.
  Skip boilerplate — only write the domain context Claude Code actually needs.
-->

- **Problem being solved**: ...
- **Relation to other modules**: ...
- **Dependencies (upstream)**: ...
- **Dependents (downstream)**: ...

### 1.2 Reference Materials
<!--
  List all materials Claude Code may need to consult, including file paths or URLs.
-->

| Resource | Path / URL | Notes |
|----------|-----------|-------|
| CONSTRAINTS.md | `./CONSTRAINTS.md` | Global hard constraints — never violate |
| Related module | `src/...` | ... |
| Paper / Doc | https://... | ... |

---

## 2. Specification

### 2.1 Objective
<!--
  SMART goal: Specific / Measurable / Achievable / Relevant / Time-bound
  One sentence that defines what "done" means.
-->

**Definition of Done**:
- [ ] ...
- [ ] ...
- [ ] ...

### 2.2 Input / Output Contract (if applicable)
<!--
  Skip this section if the task is not a data pipeline or has no clear I/O boundary.
  For system-level modules (e.g., window management, event systems), the data flow
  is typically better described in §3.2 Architecture / Data Flow.
-->

**Inputs**:
```
Describe inputs: data format, source, type
e.g.:
- image tensor: torch.Tensor, shape (B, 3, H, W), float32, normalized [0, 1]
- config: dict, see Section 4.2
```

**Outputs**:
```
Describe outputs: data format, type, semantics
e.g.:
- segmentation mask: torch.Tensor, shape (B, C, H, W), float32, softmax probabilities
```

**Side Effects** (if any):
```
Describe side effects, e.g.: files written, state mutated
```

### 2.3 Functional Requirements

> Each requirement must be independently verifiable.

| ID | Requirement | Priority |
|----|-------------|----------|
| FR-01 | ... | MUST |
| FR-02 | ... | MUST |
| FR-03 | ... | SHOULD |
| FR-04 | ... | NICE-TO-HAVE |

### 2.4 Non-Functional Requirements

| Category | Requirement |
|----------|-------------|
| Performance | e.g., inference < 200ms on RTX 3080 |
| Memory | e.g., VRAM usage < 8GB during training |
| Compatibility | e.g., Python 3.11, PyTorch >= 2.0 |
| Code Style | e.g., follow style rules in CONSTRAINTS.md |

---

## 3. Design

### 3.1 High-Level Approach
<!--
  2–5 sentences describing the implementation strategy.
  Focus on design decisions and their rationale, not "how to write the code".
-->

**Chosen approach**: ...  
**Rationale**: ...  
**Rejected alternative**: ... (because ...)

### 3.2 Architecture / Data Flow

```
[Use ASCII diagram or pseudo-code to describe the data flow]

e.g.:
Input Image
    │
    ▼
Backbone (YOLO encoder)
    │
    ├─── Detection Head → BBox predictions
    │
    └─── Segmentation Head
              │
              ▼
         Mask output (B, C, H, W)
```

### 3.3 Key Interfaces / Implementations
<!--
  For functions or classes being implemented, write the signature + docstring skeleton.

  For system-level code (C++ RAII classes, lifecycle managers, event systems, etc.),
  include full implementation specs — member tables, method tables, complete
  constructor/destructor bodies, and constraint annotations. In these cases the
  implementation details ARE the design; stripping them to bare signatures loses
  critical information about initialization order, resource ownership, and invariants.

  Use subsections (§3.3.1, §3.3.2, ...) to organize multiple classes or modules.
-->

```python
# Example: Python function signature
def function_name(
    param1: Type,
    param2: Type,
    *,
    keyword_only: Type = default,
) -> ReturnType:
    """
    One-line summary.

    Args:
        param1: Description.
        param2: Description.

    Returns:
        Description of return value.

    Raises:
        ErrorType: When this condition occurs.
    """
    ...
```

### 3.4 File Structure Changes

```
project/
├── src/
│   ├── [new file]          ← ADD: purpose
│   ├── [modified file]     ← MODIFY: what changes
│   └── [deleted file]      ← DELETE: reason
├── tests/
│   └── [new test file]     ← corresponding tests
└── [config/other files]
```

<!--
  Additional §3.x subsections may be added as needed for domain-specific
  design topics (e.g., GL State Ownership, Protocol Design, Schema Migrations).
-->

---

## 4. Implementation Plan

### 4.1 Task Breakdown

> Claude Code should execute steps in order and checkpoint after each one.

- [ ] **Step 1**: [Action description] — `[target file]`
    - Expected result: ...
    - Completion signal: ...

- [ ] **Step 2**: [Action description] — `[target file]`
    - Expected result: ...
    - Completion signal: ...

- [ ] **Step 3**: [Action description] — `[target file]`
    - Expected result: ...
    - Completion signal: ...

- [ ] **Step 4**: Run tests to validate
    - Command: `[command]`
    - Expected output: ...

### 4.2 Configuration / Constants

```python
# Centralize all magic numbers / hyperparameters here
CONFIG = {
    "learning_rate": 1e-4,
    "batch_size": 16,
    # ...
}
```

---

## 5. Testing & Validation

### 5.1 Test Cases

| Test ID | Description | Input | Expected Output | Pass Criteria |
|---------|-------------|-------|-----------------|---------------|
| T-01 | Happy path | ... | ... | ... |
| T-02 | Edge case: empty input | ... | ... | ... |
| T-03 | Edge case: max size | ... | ... | ... |
| T-04 | Failure case | ... | raises `ErrorType` | ... |

### 5.2 Validation Commands

```bash
# Run unit tests
[command]

# Run integration tests
[command]

# Run linter / type check
[command]

# Manual validation (if applicable)
[command]
```

### 5.3 Acceptance Criteria

> All conditions are AND — every item must pass before status is set to DONE.

- [ ] All unit tests pass
- [ ] FR-01 through FR-0N verified individually
- [ ] NFR performance / memory targets met
- [ ] No regression on existing tests
- [ ] Self-review checklist completed (§9)

---

## 6. Constraints & Anti-Patterns

<!--
  Task-specific constraints go here.
  Global constraints live in CONSTRAINTS.md — do not duplicate, only reference.
  Focus on pitfalls Claude Code is likely to hit during implementation.
-->

### 6.1 Hard Constraints (MUST NOT)

- ❌ Do NOT modify `[protected file/module]` (because ...)
- ❌ Do NOT introduce new dependencies without documenting them in this SDD first
- ❌ Do NOT change public API signatures (downstream callers depend on them)
- ❌ ...

### 6.2 Known Pitfalls & Limitations

<!--
  Include both implementation pitfalls (things that will break if done wrong)
  and known design limitations (accepted trade-offs, deferred improvements).
  For limitations, note severity and possible future solutions.
-->

- ⚠️ [Known issue 1]: e.g., CUDA OOM risk — watch batch size carefully
- ⚠️ [Known issue 2]: e.g., logits must be cast with `.float()` or loss becomes NaN
- ⚠️ ...

| Limitation | Description | Severity | Possible Solution |
|---|---|---|---|
| ... | ... | Low / Medium / High | ... |

---

## 7. Open Questions & Decision Log

<!--
  Record trade-offs and items pending a decision.
  This signals to Claude Code which areas are deliberate choices vs. still uncertain.
-->

### Open Questions

| # | Question | Owner | Deadline |
|---|----------|-------|----------|
| Q1 | ... | Author / Team | ... |

### Decision Log

| # | Decision | Rationale | Date |
|---|----------|-----------|------|
| D1 | Chose X over Y | Because ... | YYYY-MM-DD |

---

## 8. Progress Tracker

<!--
  Update this at the end of every Claude Code session.
  Lets the next session pick up exactly where things left off.
-->

### Session Log

| Session | Date | Work Completed | Remaining Issues |
|---------|------|----------------|-----------------|
| #1 | YYYY-MM-DD | ... | ... |
| #2 | YYYY-MM-DD | ... | ... |

### Current Blockers

- 🔴 [Blocker description] — waiting on [dependency]
- 🟡 [Risk description] — mitigation: ...

---

## 9. Implementation Verification Checklist

<!--
  A per-item checklist of concrete pitfalls to verify after implementation.
  Unlike §5 Acceptance Criteria (which defines "what counts as done"),
  this section is a "have you checked this specific thing?" list —
  each item reflects a real bug or mistake encountered in practice.

  Organize by module or class. Every item should be independently checkable.
  Claude Code should walk through this list after completing implementation
  and before declaring the task DONE.
-->

### [Module / Class Name]
- [ ] [Specific verification item]
- [ ] [Specific verification item]

### [Module / Class Name]
- [ ] [Specific verification item]
- [ ] [Specific verification item]

---

## 10. Appendix (Optional)

### 10.1 Related SDDs

- [SDD: Related Feature A](./SDD_RELATED_A.md)
- [SDD: Related Feature B](./SDD_RELATED_B.md)

### 10.2 Scratch Pad / Notes

<!--
  Free-form area: experiment results, rough ideas, temporary notes.
  No need to organize — Claude Code is not expected to read this section.
-->

---

*End of SDD*