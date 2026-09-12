# Feature Backlog for Guipper

Reviewed against local source on **2026-09-12**. “Implemented” describes code
present in this checkout, not a cross-platform runtime certification.
Historical design plans are not the active backlog.

## Implemented
- **Cross-platform build** — Linux/macOS in addition to Windows (openFrameworks 0.12.1). Windows-only paths, fonts and Spout guarded; Windows-saved sessions load on Linux/macOS.
- **Internal shader browser (Import page)** — left-half navigator with folder tree, live search, per-shader favorites (starred, pinned to a "favorites" folder on top, persisted to `shader_favorites.xml`), in-panel live preview, keyboard navigation (Up/Down + Enter), and double-click / LOAD to add to the canvas. Images/videos/savefiles still use drag-and-drop.
- **Internal shader editor** — multiple tabs, GLSL syntax highlighting, selection, vertical/horizontal scrolling, zoom, and save-triggered shader reload.
- **Box grouping** — presets act as collapsible sub-compositions (group view with its own graph + active render).
- **Cue / crossfade staging** — stage parameter/link/bypass/delete changes into a draft (including inside box-groups) and Apply with a crossfade. Adding a box is permanent, survives cue cancellation, and is deliberately not undoable.
- **MIDI mapping with learn mode** — per-device profiles (all devices usable at once, loose ALSA name matching); bindable actions incl. `ADD_SHADER_BOX`, bindable inline from the Import page.
- **Performance-ready audio reactivity** — live-device capture, 48/44.1 kHz negotiation, bounded lock-free handoff, FFT bands, adaptive kick/snare onsets, tempo confidence, calibration, auto-gain, clipping/overrun diagnostics, parameter shaping, and global shader uniforms.
- **Scrollable inspector polish** — inspector-specific typography and spacing, content-sized panel, sticky header, clipped body hit-testing, compact audio cards, and scrollbar only when content overflows.
- **Audio and persistence regression tests** — deterministic signal fixtures, frame-rate-independent smoothing, ThreadSanitizer queue stress, legacy/current/invalid XML checks, and environment-triggered inspector captures.

- **Node editing** — copy/paste, multi-selection and movement, grouping/ungrouping, zoom/pan, and scoped graph undo/redo.
- **PAINT** — layers, frame animation, selection transforms, timeline editing, undo/redo and PNG/GIF/sprite-sheet export. See [PAINT](PAINT.md).
- **Mapping and live outputs** — advanced mapping, FINAL/MAP image stacks, multiple output windows, source binding by stable UID, wall crops and bezel adjustment.
- **Additional sources** — camera depth, pointer cloud and optional native Kinect v2 backend.
- **Diagnostics** — CPU frame-stage timings and media diagnostics already exist; GPU timing is separate proposed work.

## High Priority
- Extend the internal file browser/picker to images, videos, and savefiles (shaders done; these are still drag-and-drop only).
- Make session loading transactional: `JPboxgroup::load()` currently clears the graph before checking whether XML loading succeeded. Validate a candidate composition before replacing the active state.
- Check write failures and make project/preset saves atomic where possible; introduce schema versioning and explicit migrations.
- Refactor pointer-heavy ownership (`JPbox*`, `JPcontroller*`, `JPParameter*`) incrementally. `JPParameterGroup` already deletes its parameters and deep-copies them; do not treat its `clear()` as a known leak.
- Extract responsibilities from `ofApp` and `JPboxgroup` while preserving cue, history and nested-group behavior.
- Harden uniform parsing against formatting differences, malformed declarations and unchecked indexing.
- Add better error UI for shader compile failures (inline message + line hints).
- Add autosave and crash-recovery session restore.

## Livecoding and Shader Workflow
- Add GLSL linting and richer compiler diagnostics; syntax highlighting is implemented.
- Support shader variants/branches per box.
- Add shader tags (favorites + search done); curated metadata.
- Add uniform support beyond float/bool (`vec2`, `vec3`, `vec4`, color picker).

## Performance and Stability
- Extend existing CPU/frame diagnostics with GPU timing.
- Reduce update/draw coupling in GUI widgets for cleaner real-time performance.
- Cache and optimize connection drawing / hit testing for large node graphs.
- Add optional "performance mode" (reduced UI redraw frequency).

## Node Graph UX
- Add align/distribute tools for selections.
- Add a minimap; zoom/pan already exists.
- Extend undo to renaming boxes and to exposing parameters and texture inputs.

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
1. Transactional load/save, explicit failures, schema migration and recovery.
2. Incremental modularization and uniform-parser hardening.
3. Broader import browser and editor diagnostics.
4. Graph navigation/arrangement and measured performance improvements.
5. Content packaging, metadata and optional interoperability extensions.

## Verification baseline

`make -C tests run` passed all nine suites on 2026-09-12: audio, media,
PAINT, graph history, text wrapping, keymap, OSC addresses, editor shortcuts
and mapping booleans. This does not validate OpenGL rendering, device I/O,
application XML integration or builds on other platforms.
