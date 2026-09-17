# Shader chains and FINAL alpha

Run in an isolated copy of the executable and data (the launcher leaves user
sessions and profiles untouched):

```sh
make -j2
xvfb-run -a python3 tests/run_shader_alpha.py
```

On Windows, after compiling Release x64, run
`python tests/run_shader_alpha.py` from the repository root on a desktop session.
Windows execution of this regression remains to be verified.

The native `GUIPPER_PERSISTENCE_TEST=shader_alpha_chain` case creates the production
chain **Gameboy Palette → Transform → Mirror** and composites it in FINAL over an
opaque blue background. It checks transparent, translucent and opaque sources,
three effect mix settings, and two FINAL layer opacities (18 combinations).
Checks include alpha at every node, the resulting FINAL color, and opaque coverage
of the combined image. Missing or broken shader fixtures fail the test.

Two regressions covered:

- Gameboy forced alpha to one, even at zero effect mix. It now mixes original RGBA
  with the pixelated palette's RGBA; opaque inputs retain their previous output.
- FINAL set separate alpha blend factors, but the layer renderer then reset them
  with `ofEnableAlphaBlending()`. The renderer now sets separate factors after that
  call, so translucent overlays cannot punch holes into an opaque background.

Logs default to `dist/shader-alpha-chain.log`; use `--log` to select another path.

Validated on Linux: application build, pure core suites, the 18 alpha-chain
cases, and the full native persistence/composition suite (including images,
groups, transitions, FINAL overlays and analog-neutral shaders) passed.
