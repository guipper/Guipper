# Your first visual in five minutes

1. Open Guipper. The starter distribution opens the animated rings composition.
2. Select **Rings** and change `density`. Red, green and blue control the color.
3. Double-click the node to make it the active output.
4. Save a copy under your own name, close it and reopen it to verify its location.
5. In AUDIO, select an audio input. Adjust `audio_amount` to control how strongly bass affects brightness. Check the input meter first; without audio the generative visual still works.

## Three starting compositions

- `savefiles/examples/01-generative.xml`: animated rings, density and color.
- `savefiles/examples/02-audio.xml`: the same visual for practicing input selection and calibration.
- `savefiles/examples/03-mix.xml`: rings mixed with an included test image. Select **Mix**, adjust `amount`, then drag your own image or video into the canvas and connect it to an input.

F10 opens version, updates and diagnostic export. Builds without a signed update
channel show updates as unavailable.

When startup offers recovery, F9 opens it and F8 dismisses it. Before recovery,
the current composition is saved as `savefiles/before-recovery.xml`. Save the
recovered composition under a new name afterward.

Report problems at https://github.com/guipper/Guipper/issues with reproduction
steps and an exported diagnostic report. Files are never submitted automatically.
