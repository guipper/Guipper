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
- Complete transactional session loading: XML parsing and basic composition/source-field validation now run before clearing live state. Next, construct and validate candidate nodes and nested groups before replacing the active graph.
- Check write failures and make project/preset saves atomic where possible; introduce schema versioning and explicit migrations.
- Refactor pointer-heavy ownership (`JPbox*`, `JPcontroller*`, `JPParameter*`) incrementally. `JPParameterGroup` already deletes its parameters and deep-copies them; do not treat its `clear()` as a known leak.
- Continue separating state ownership from `ofApp` and `JPboxgroup`. Session orchestration, cue and output runtime now have dedicated translation units; parameter XML and node construction have shared APIs. See [architecture](ARQUITECTURA.md).
- Shader reload now validates candidate compile/link status before replacing live state; preprocessor-aware uniform reflection remains pending. The shared uniform lexer/parser, structural diagnostics and empty-file protection are implemented. See [parser contract](PARSER_UNIFORMS.md).
- Add better error UI for shader compile failures (inline message + line hints).
- Recovery snapshots now run every two minutes with background writes and an explicit restore prompt; retention and large-document capture timing remain pending. See [release readiness](PUBLICACION.md).

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

## Architecture progress

The parameter XML codec is shared by main compositions, presets and clipboard;
node construction is shared by add/load/paste/cue. Legacy differences are explicit
contexts, not silent behavior changes. Session, cue and output lifecycle methods
are separated into dedicated translation units. Full class decomposition and
migration of graph/history/draft ownership to smart pointers remain future work.

## Persistence progress

The first load-safety increment returns `JPboxgroup::LoadResult`, preserves the
current graph and save destination on XML preflight failure, and shows an ES/EN
non-modal notice. Legacy files without `activerender` and intentionally empty
projects remain accepted. Nested asset construction, atomic saves, schema
versioning and recovery are still pending.

Targeted graphics-backed regression check after rebuilding:

```bash
cd bin
GUIPPER_PERSISTENCE_TEST=load_safety ./Guipper
```

Set `GUIPPER_LOAD_ERROR_CAPTURE=1` with that check to capture the notice in
both languages under `data/uishots/persistence/load-safety/`.

## Verification baseline

`make -C tests run` passed all nine suites on 2026-09-12: audio, media,
PAINT, graph history, text wrapping, keymap, OSC addresses, editor shortcuts
and mapping booleans. This does not validate OpenGL rendering, device I/O,
application XML integration or builds on other platforms.
