# Project Constraints & Architecture Decisions

## Project Overview

## Architecture Decisions

- [ADR-001] `App` privately inherits `GlfwContext` → guarantees GLFW init before `Window` member construction
- [ADR-002] GLAD initialized inside `Window` constructor → `on_resize` calls `glViewport`, which requires loaded GL functions; the two are inseparable
- [ADR-003] `Window` move `= delete` → `resize_callback` lambda captures `this`; reassigning `glfwSetWindowUserPointer` alone does not fix dangling capture
- [ADR-004] `EventID` uses `template static int` variable, not `typeid`/`std::type_index` → RTTI is avoided

## Hard Constraints

## Environment
