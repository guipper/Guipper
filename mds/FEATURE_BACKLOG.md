# Feature Backlog for Guipper

## ✅ Completed (2026)
- **Cross-platform build** — Linux/macOS in addition to Windows (openFrameworks 0.12.1). Windows-only paths, fonts and Spout guarded; Windows-saved sessions load on Linux/macOS.
- **Internal shader browser (Import page)** — left-half navigator with folder tree, live search, per-shader favorites (starred, pinned to a "favorites" folder on top, persisted to `shader_favorites.xml`), in-panel live preview, keyboard navigation (Up/Down + Enter), and double-click / LOAD to add to the canvas. Images/videos/savefiles still use drag-and-drop.
- **Internal shader editor** — EDITOR tab with live compile/reload.
- **Box grouping** — presets act as collapsible sub-compositions (group view with its own graph + active render).
- **Cue / crossfade staging** — stage param/link/bypass/add/delete changes into a draft (including inside box-groups) and Apply with a crossfade.
- **MIDI mapping with learn mode** — per-device profiles (all devices usable at once, loose ALSA name matching); bindable actions incl. `ADD_SHADER_BOX`, bindable inline from the Import page.
- **Performance-ready audio reactivity** — live-device capture, 48/44.1 kHz negotiation, bounded lock-free handoff, FFT bands, adaptive kick/snare onsets, tempo confidence, calibration, auto-gain, clipping/overrun diagnostics, parameter shaping, and global shader uniforms.
- **Scrollable inspector polish** — inspector-specific typography and spacing, content-sized panel, sticky header, clipped body hit-testing, compact audio cards, and scrollbar only when content overflows.
- **Audio and persistence regression tests** — deterministic signal fixtures, frame-rate-independent smoothing, ThreadSanitizer queue stress, legacy/current/invalid XML checks, and environment-triggered inspector captures.

## High Priority
- Extend the internal file browser/picker to images, videos, and savefiles (shaders done; these are still drag-and-drop only).
- Improve session save/load robustness with schema versioning and migration support.
- Refactor pointer-heavy ownership (`JPbox*`, `JPcontroller*`, `JPParameter*`) to safer memory management.
- Add better error UI for shader compile failures (inline message + line hints).
- Add autosave and crash-recovery session restore.

## Livecoding and Shader Workflow
- Add syntax highlighting and basic GLSL linting in-editor.
- Support shader variants/branches per box.
- Add shader tags (favorites + search done); curated metadata.
- Add uniform support beyond float/bool (`vec2`, `vec3`, `vec4`, color picker).

## Performance and Stability
- Add frame-time and GPU timing diagnostics overlay.
- Reduce update/draw coupling in GUI widgets for cleaner real-time performance.
- Cache and optimize connection drawing / hit testing for large node graphs.
- Add optional "performance mode" (reduced UI redraw frequency).

## Node Graph UX
- Add copy/paste/duplicate for boxes and selections.
- Add multi-select move and align/distribute tools.
- Add undo/redo stack for graph edits.
- Add minimap and zoom/pan controls.

## Automation and Control
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
- Add dockable/resizable panels (the inspector is currently fixed at 450 px).

## Suggested Milestone Plan
1. Core stability pass (memory safety + save/load hardening + autosave).
2. Internal editor and file browser.
3. Node UX improvements (undo/redo, selection tools, minimap).
4. MIDI + advanced automation.
5. Content ecosystem (library metadata, packaging, community sync).
