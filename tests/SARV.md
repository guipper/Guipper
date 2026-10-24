# SARV reference and validation

Add SARV with Shift+V in NODES. Shift+K still adds the separate Particulas box.
The eight parameters use the names and initial values read through the authorized
TouchDesigner MCP from Kinect_SARV.12.toe, /project1/constant7.

The implementation uses indexed RGBA32F GPU feedback at 60 fixed steps/sec,
512x424 or 1024x848 particles, staggered normalized lifetimes, the two flow
fields, per-step scale/translation, the five-color "american idiot" palette,
source translation/rotation, and horizontal 45-degree camera FOV.
Depth less than kinect_depth is retained: TD Threshold greater removes values above the threshold (verified against live TOP sample grids).
Time slices cache simulation frames (including duplicated 30 Hz sensor frames).
The existing shared capture backend selects Windows SDK or Linux libfreenect2.

Known visual differences: the coherent noise hash/interpolation is not TD's
internal implementation; the warped temporal ramp uses nearest cached frames
and omits the reference blur; surface spheres and cubes use point impostors
instead of randomly rotated instanced geometry. This is a port of the effect's
structure and controls, not a pixel-identical rendering of the .toe file.

Validation: build Release x64; create via Shift+V; confirm live depth, shader
status, eight default values, 868352 particles and 15 frames; switch shape tabs;
change lifetime/resolution; turn off and back on; confirm other box types remain.

Verified on 2026-10-24: Release x64 build passed; live Kinect depth rendered with TouchDesigner still open; 868352 particles / 15 frames displayed; all three mode tabs updated the parameter and output; scale values display 0.0015. Shader logs had no SARV errors. Intermittent sensor unavailability still occurs. Linux build was not run. Full persistence suite was not run.

Guía de continuidad en Linux y referencia detallada: [docs/SARV.md](../docs/SARV.md).
