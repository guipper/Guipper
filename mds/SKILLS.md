# Guipper Technical Handbook (Thesis + Whole Project Code)

Sources used:
- Thesis: `Tesis V4/TesisV4.html`
- Extracted thesis text: `Tesis V4/TesisV4_extracted.md`
- Project code: `src/` (core app + boxes + utils + gui)

Purpose of this document:
- Provide an implementation-level map of the software.
- Explain how thesis goals materialize in the code.
- Serve as onboarding guide for extending/fixing Guipper.

---

## 1) Product Intent (from thesis, grounded in code)

Guipper is a real-time visual system that:
- Executes GLSL fragment shaders with low friction.
- Auto-exposes shader parameters as GUI controls.
- Supports node-style compositing via texture links between boxes.
- Accepts multiple visual source types (shader, image, video, camera, Spout, NDI, XML presets).
- Communicates with external tools through OSC, Spout, and NDI.
- Persists compositions (`savefiles/*.xml`) and runtime config (`settings.xml`).

Implementation reality:
- The thesis concepts map strongly to the current architecture.
- Core behavior is concentrated in `ofApp` + `JPboxgroup` + `JPbox_*` subclasses + `JPParameterGroup`.

---

## 2) Repository Structure (what matters most)

Primary runtime code:
- `src/main.cpp`: app entry.
- `src/defines.h`: compile flags.
- `src/ofApp.h/.cpp`: top-level app orchestration.
- `src/JPbox/*`: graph engine + box classes.
- `src/JPutils/*`: parameters, constants, transitions, utility primitives.
- `src/JPgui/*`: inspector UI control widgets.

Primary runtime data/content:
- `bin/data/shaders/*`: shader libraries.
- `bin/data/savefiles/*`: composition/session XML files.
- `bin/data/settings.xml`: startup/runtime settings.
- `bin/data/img/design/*`: interface assets.
- `bin/data/font/*`: fonts.

Third-party SDK copies:
- `src/SpoutSDK/*`
- `vendor/SpoutSDK/*`

Note: Spout SDK trees are external dependency code, not core Guipper logic.

---

## 3) Build Flags and Feature Gates

Defined in `src/defines.h`:
- `NDI`
- `SPOUT`
- `RELATIVEDIRS`

Practical effects:
- `NDI`: enables NDI sender in `ofApp` and NDI receiver box (`JPbox_ndi`).
- `SPOUT`: enables Spout sender in `ofApp` and Spout receiver box (`JPbox_spout`).
- `RELATIVEDIRS`: during drag-drop load path handling, attempts `data/...` conversion when possible.

---

## 4) Core Runtime Flow

## 4.1 Process Startup

1. `main.cpp` configures OF/GLFW windows and launches `ofApp`.
2. `ofApp::setup()` initializes window/defaults, constants, managers, OSC, settings, and first session load.
3. `JPboxgroup::setup()` initializes graph/inspector/transition state.
4. Optional: Spout and NDI senders are initialized depending on flags and settings.

## 4.2 Frame Loop

Per frame (`ofApp::update()`):
1. `boxes.update()` updates all box states and inspector interactions.
2. If Spout enabled and active: `drawSpout()` sends active render texture.
3. If NDI enabled: sends active render FBO image.
4. `updateOSC()` handles inbound and outbound OSC.
5. Handles async Save-As completion and session save.

Per frame (`ofApp::draw()`):
1. Draw active render background.
2. Depending on active screen, draw:
   - Node/inspector UI
   - Options screen
   - Tutorial screen
3. Optional debug overlay.

---

## 5) Module Architecture

## 5.1 `ofApp` (Application Orchestrator)

Responsibilities:
- User input and mode switching.
- Delegation to graph engine (`JPboxgroup boxes`).
- OSC network I/O.
- Settings I/O.
- External output protocols (Spout sender / NDI sender).
- External render window lifecycle.

Key functions:
- setup/update/draw cycle.
- `keyPressed`, `keycodePressed`, mouse + drag events.
- `loadSettings`, `saveSettings`.
- `updateOSC`.
- `openRenderWindow` + window event listeners.

## 5.2 `JPboxgroup` (Graph + Inspector + Session Engine)

This is the most important domain module.

Responsibilities:
- Own all boxes (`vector<JPbox*> boxes`).
- Own inspector controls for active box (`vector<JPcontroller*> controllers`).
- Handle node connections via `JPFbohandlerGroup` links.
- Manage active render selection and transition blending.
- Handle box dragging, outlet linking, and inspector manipulation.
- Save/load full session XML.
- Route OSC commands to graph/params.

Important behavior details:
- Double-click on box in graph changes active render (`updateTransition`).
- Auto-reload shader box when source file modification timestamp changes.
- Gallery mode (`activeSequence`) cycles active render based on `jp_constants::durationgallery`.
- Inspector is rebuilt dynamically when open GUI target changes (`setControllers`).

## 5.3 `JPbox` Base and Subclasses

### Base class: `JPbox`

Shared features:
- Positioning/hit-testing via `JPdragobject` inheritance.
- Display node box with outlet and on/off toggle.
- Own FBO for output texture.
- Hold parameters (`JPParameterGroup`) and input handlers (`JPFbohandlerGroup`).

### `JPbox_shader`

- Loads/executes shader (`default.vert` + selected fragment shader).
- Parses source file uniforms to auto-create:
  - Float and bool params
  - Texture input nodes from `sampler2D`/`sampler2DRect`
- Applies global uniforms each frame:
  - `feedback`, `resolution`, `mouse`, `globalframeNum`, `boxframeNum`, `window_mouse`, `time`
- Supports full `reload()` preserving matching params and texture links where possible.
- Optional source text overlay (`showCode`) for livecoding visual aid.

### `JPbox_image`

- Loads image and draws into FBO with transform controls.
- Params: `scalex`, `scaley`, `offsetx`, `offsety`, `strech`.
- Has autoreload retry logic if image failed to allocate.

### `JPbox_video`

- Async loads and plays video.
- Params: `scalex`, `scaley`, `offsetx`, `offsety`, `strech`, `speed`, `position`, `play`.
- Syncs `position` param with current frame and supports external seek control.

### `JPbox_cam`

- Initializes webcam capture and maps it to FBO with transform controls.
- Similar transform params plus camera index slot.

### `JPbox_spout` (flag `SPOUT`)

- Receives shared texture from Spout sender list.
- Supports sender selection with `reciever` param.
- Draws received texture to FBO with transform controls.

### `JPbox_ndi` (flag `NDI`)

- Receives NDI stream textures.
- Supports sender selection with `reciever` param.
- Draws received texture to FBO with transform controls.

### `JPbox_preset`

- Loads a nested composition from XML into internal `vector<JPbox*>`.
- Updates all nested boxes and outputs nested active render.

### `JPbox_framedifference`

- One-input effect using private shader (`shaders/private/framedifference.frag`).
- Maintains previous frame FBO and compares against current input frame.

---

## 6) Parameter and Automation System

## 6.1 `JPParameter`

Fields include:
- type: `BOOL`/`FLOAT`
- value states: `floatValue`, `floatLerpValue`, `boolValue`
- automation: `movtype`, `speed`, `min`, `max`, `seed`, direction
- UI sync helper: `needsUpdate`

Movement modes (`MovType` enum):
- `STANDART` (manual/static)
- `OSC` (ping-pong)
- `GODER` (one direction wrap)
- `GOIZQ` (opposite direction wrap)
- `RANDOM` (noise-based)

## 6.2 `JPParameterGroup`

Responsibilities:
- Add and store parameters (currently as pointers).
- Update automated params each frame.
- Getter/setter API for all value kinds.
- Movement mode and speed/range controls.

Automation update behavior:
- Only runs param update when `movtype != 0` or `needsUpdate == true`.

---

## 7) Inspector UI System (`JPgui`)

Controller hierarchy:
- Base: `JPcontroller`
- Main controls used by inspector:
  - `JPComplexSlider` for float params
  - `JPToogle` for bool params
- Supporting widgets:
  - `JPSlider`, `JPKnob`, `JPHandler`, `JPBang`

`JPComplexSlider` combines:
- Main value timeline slider.
- Movement mode toggle(s).
- Speed knob.
- Min/max handlers for automation range.

`JPToogle` has texture-based modes used for movement controls:
- collapse
- ida y vuelta
- direction
- random

---

## 8) Input Behavior Contract

## 8.1 Screen Switching

Keys:
- `1`: Nodes screen
- `2`: Options screen
- `3`: Tutorial screen

## 8.2 Nodes Screen Key Actions

- `t`: toggle XML load behavior (preset/composition mode)
- `h`: add Spout receiver box (if enabled)
- `n`: add NDI receiver box (if enabled)
- `c`: add camera box
- `i`: add frame-difference box
- `DEL`: delete selected box under mouse
- `s`: save current session to `savedirectory`
- `l`: load session from `savedirectory`
- `d`: toggle debug overlay
- `r`: reload active shader/open shader
- `w`: open external render window
- `m`: export active render image (`exportimgs/export-...png`)
- `e`: toggle sequence/gallery mode
- `x`: toggle shader source text overlay for open GUI box
- `b` (keycode handler): clear all boxes

## 8.3 Mouse + Drag

- Drag in nodes screen updates box drags and link drags.
- Click in nodes screen updates active/open box, controls selection, and double-click detection.
- Drag-and-drop files creates boxes at drop location; multi-file drop arranges in grid.

## 8.4 Tutorial language toggle

- Clicking in tutorial screen toggles language 0/1.

---

## 9) OSC Contract (Detailed)

## 9.1 Inbound

`ofApp::updateOSC()` passes every message to `JPboxgroup::listenToOsc(address, firstFloatArg)`.

Special application-level load:
- Addresses containing `load` trigger path extraction and `boxes.load("savefiles/" + dir)`.

Graph-level control commands handled in `listenToOsc`:
- `/setactiverender`
- `/nextshader`
- `/prevshader`
- `/nextshader_gallerymode`
- `/prevshader_gallerymode`
- `/setactiveshader`
- `/setactivecycle`

Parameter addressing:
- `/{shaderName}/{paramName}` updates matching box parameter.
- `/{shaderName}/onoff` toggles box activation.
- `/openguinumber/vN` style updates active inspector float param index when allowed.

## 9.2 Outbound

Two modes from `ofApp::updateOSC()`:

Mode 1 (`oscout_mode1`):
- Sends all boxes, all params continuously.
- Address format: `{boxName}/{paramName}`.

Mode 2 (`oscout_mode2`):
- Sends only params of currently open GUI box.
- Address format: `v0`, `v1`, `v2`, ...

Type-aware arg payload:
- Bool params send bool arg.
- Float params send float arg.

---

## 10) Persistence Formats

## 10.1 `settings.xml` format

Read by `ofApp::loadSettings()`:
- `renderwidth`
- `renderheight`
- `window_x`
- `window_y`
- `window_width`
- `window_height`
- `window_open`
- `window_fullscreen`
- `osc_port_in`
- `osc_port_out`
- `osc_ip_out`
- `oscout_mode1`
- `oscout_mode2`
- `durationgallery`
- `spouton` (when compiled with Spout)

Write behavior in `saveSettings()`:
- Writes fixed `window_width=600`, `window_height=600`, `window_fullscreen=false` currently.
- Persists render dimensions, osc endpoints, gallery duration, output modes, window open flag.

## 10.2 Session XML (`savefiles/*.xml`)

Saved by `JPboxgroup::save`.

Top-level:
- `activerender`
- repeated `box`

Per box fields:
- `nombre`
- `x`
- `y`
- `directory`
- `onoff`

Optional `parameters` subtree:
- repeated `param`
- float param: `name`, `min`, `max`, `value`, `movtype`, `speed`
- bool param: `name`, `value`

Optional `fboslinks` subtree:
- children named by input node name
- value = source box name

Load behavior notes:
- Box type is inferred by directory extension/keyword.
- Active render clamped to loaded box range.
- FBO links restored by name matching.

---

## 11) Active Render and Transition System

Transition owner:
- `JPutils/TransitionSR.*`

Mechanism:
- Holds two FBO pointers (`fbo1`, `fbo2`) and mixes via private blend shader (`shaders/private/mix.frag`).
- `JPboxgroup::updateTransition()` sets previous and next FBO pointers and resets lerp.
- `TransitionSR::update()` increments lerp each frame and renders mixed result into internal FBO.

Usage:
- Used for changing active render (double click, OSC set active, sequence mode).

---

## 12) Graph Interaction Details

Connection model:
- Each box has one output (outlet) and zero/many inputs (`JPFbohandlerGroup`).
- Linking is done by dragging outlet toward an input node.
- Input nodes store direct FBO pointer + source name.

Box drag model:
- Mouse press selects box or outlet path.
- Mouse drag moves selected box or performs link action.
- Mouse release clears grab states.

Inspector model:
- `openguinumber` determines which box params are shown/controlled.
- Controllers are reconstructed when open box changes or param topology changes.

---

## 13) Shader Uniform Parsing Rules (Current Implementation)

Implemented in `JPbox_shader::setUniforms`:
- Only lines starting with `uniform` are considered.
- `uniform float ...` creates float parameter.
- `uniform bool ...` creates bool parameter.
- `uniform sampler2D...` and `uniform sampler2DRect...` create texture input nodes.

Float default value behavior:
- Parser tries to detect inline assignment (formatted token pattern).
- If no default is detected, random initial value in `[0,1]` is used.

Global uniforms are always injected at render time (not parsed from file).

---

## 14) Known Fragile/Legacy Areas

- Pointer ownership uses many raw pointers (`JPbox*`, `JPcontroller*`, `JPParameter*`).
- `JPParameterGroup::clear()` clears vector without explicit delete of allocated params.
- Several code paths/comments indicate historic crash workarounds and defensive hacks.
- Some settings save behavior is hardcoded instead of round-tripping current runtime values.
- Uniform parsing is string-format sensitive and not a full GLSL parser.
- Spout SDK is duplicated in multiple directories.

---

## 15) Extension Playbook (Where to Change What)

Add a new source/effect box type:
1. Create subclass in `src/JPbox` inheriting `JPbox`.
2. Implement `setup/update/updateFBO/draw/clear`.
3. Add type inference in `JPboxgroup::addBox` and `JPboxgroup::load`.
4. Ensure session serialization compatibility if params/links are used.

Add new global shader uniform:
1. Add in `JPbox_shader::update_globalUniforms`.
2. Keep naming consistent with shader library expectations.

Change inspector behavior:
1. Edit `JPboxgroup::setControllers` and relevant `JPgui/*` widgets.
2. Validate float/bool parameter path and automation path.

Modify OSC API:
1. Inbound logic: `JPboxgroup::listenToOsc` and `ofApp::updateOSC` special cases.
2. Outbound logic: `ofApp::updateOSC` mode1/mode2 emitters.
3. Keep backward compatibility if existing controllers/presets depend on addresses.

Change persistence schema:
1. Update `JPboxgroup::save` and `JPboxgroup::load` together.
2. Consider migration strategy for old XML savefiles.

---

## 16) Manual QA Checklist for Core Changes

After non-trivial changes, validate:
- app boot with no `settings.xml` and with existing `settings.xml`
- drag-drop of `.frag`, image, video, `.xml`
- box linking/unlinking and render correctness
- inspector edits for float and bool params
- shader autoreload after file save
- session save and reload round-trip
- OSC inbound controls and both outbound modes
- external window open/resize/fullscreen + settings save on exit
- Spout/NDI sender output if compiled in
- sequence/gallery mode transitions

---

## 17) Thesis Alignment Summary

What is strongly implemented:
- Real-time shader manipulation workflow.
- Uniform-to-GUI parameter exposure.
- Box-based composition with texture routing.
- OSC + Spout + NDI interoperability.
- Save/load and practical operation screens.

What remains future-facing vs thesis/roadmap language:
- Deeper integrated IDE/livecoding authoring workflow.
- MIDI workflow in current production code path.
- Further cleanup/refactor for memory safety and modularity.

---

## 18) Quick Function Index (Core)

`ofApp`:
- lifecycle: `setup`, `update`, `draw`
- UI: `draw_debugInfo`, `draw_instrucciones`, `draw_opciones`
- input: `keyPressed`, `keycodePressed`, mouse events, `dragEvent`
- settings: `loadSettings`, `saveSettings`
- comms: `updateOSC`
- external window: `openRenderWindow`, `window_*` listeners
- output protocols: `drawSpout`, NDI send in `update`

`JPboxgroup`:
- graph render/update: `draw`, `draw_activerender`, `update`
- interaction: `update_mouseDragged`, `update_mousePressed`
- inspector: `setControllers`, `draw_paramswindow`, `update_paramswindow`
- transition: `updateTransition`
- persistence: `save`, `load`
- OSC route: `listenToOsc`
- object lifecycle: `addBox`, `clear`, `deleteSelectedShader`

`JPbox_shader`:
- setup/reload: `setup`, `reload`, `reloadShaderonly`
- uniforms: `setUniforms`, `update_globalUniforms`, `update_NonglobalUniforms`
- render: `updateFBO`

`JPParameterGroup`:
- add/get/set/update for all param types and automation state.

