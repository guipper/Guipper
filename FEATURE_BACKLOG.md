# Feature Backlog for Guipper

## High Priority
- Add an internal file browser/picker for shaders, images, videos, and savefiles (not only drag-and-drop).
- Improve session save/load robustness with schema versioning and migration support.
- Refactor pointer-heavy ownership (`JPbox*`, `JPcontroller*`, `JPParameter*`) to safer memory management.
- Add better error UI for shader compile failures (inline message + line hints).
- Add autosave and crash-recovery session restore.

## Livecoding and Shader Workflow
- Build an internal shader editor panel with live compile/reload.
- Add syntax highlighting and basic GLSL linting in-editor.
- Support shader variants/branches per box.
- Add shader favorites/tags and quick search.
- Add uniform support beyond float/bool (`vec2`, `vec3`, `vec4`, color picker).

## Performance and Stability
- Add frame-time and GPU timing diagnostics overlay.
- Reduce update/draw coupling in GUI widgets for cleaner real-time performance.
- Cache and optimize connection drawing / hit testing for large node graphs.
- Add optional "performance mode" (reduced UI redraw frequency).
- Add automated tests for XML load/save round trips.

## Node Graph UX
- Add box grouping and collapse/expand groups.
- Add copy/paste/duplicate for boxes and selections.
- Add multi-select move and align/distribute tools.
- Add undo/redo stack for graph edits.
- Add minimap and zoom/pan controls.

## Automation and Control
- Add MIDI mapping system with learn mode.
- Add envelopes/LFO modulators as native automation sources.
- Add trigger grid scene launcher (Resolume-like banks).
- Add OSC mapping presets and address aliasing.
- Add parameter smoothing curves and quantization options.

## I/O and Interop
- Add Syphon support for macOS.
- Add optional WebSocket/HTTP remote control API.
- Add NDI/Spout sender selector and status diagnostics UI.
- Add recording/export pipeline for video capture.
- Add thumbnail generation for presets and shaders.

## Content and Library
- Add curated shader library metadata (author, tags, complexity, FPS cost).
- Add one-click import/export package for patches.
- Add community shader sync/download workflow.
- Add starter templates for common effect categories.
- Add compatibility checker for missing assets in loaded sessions.

## UI/UX Improvements
- Add onboarding flow with first-run setup wizard.
- Add searchable command palette.
- Add configurable keyboard shortcuts.
- Add language localization framework beyond ES/EN toggle.
- Add dockable/resizable inspector and panels.

## Suggested Milestone Plan
1. Core stability pass (memory safety + save/load hardening + autosave).
2. Internal editor and file browser.
3. Node UX improvements (undo/redo, selection tools, minimap).
4. MIDI + advanced automation.
5. Content ecosystem (library metadata, packaging, community sync).
