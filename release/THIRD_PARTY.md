# Distribution notices

Guipper source and the original Getting Started examples: MIT; see LICENSE.txt.
Core runtime resources are enumerated and hash-checked in ASSETS.json.
Overpass Regular and SemiBold: SIL Open Font License 1.1; see licenses/OFL-Overpass.txt.
Montserrat Regular and Medium: SIL Open Font License 1.1; see licenses/OFL-Montserrat.txt.

The SDK dependency tree is pinned in dependencies.lock.json. openFrameworks,
ofxOsc, ofxMidi, ofxNDI, Spout and their transitive libraries retain their own
licenses. A binary release must include the corresponding SDK notices.

NDI runtime is not included by the asset staging script. Its installation and
redistribution requirements must be validated separately for each platform.
Contributed shader collections, Microsoft fonts, personal media and compositions
are deliberately absent from the initial distribution allowlist.

## Biblioteca de shaders en revisión

`release/shader-candidates.json` es una preselección local sin aprobación de redistribución. No forma parte de los recursos empaquetados. La colección oficial seguirá vacía hasta documentar autoría y permisos por archivo; ver `mds/SHADER_LIBRARY.md`.
