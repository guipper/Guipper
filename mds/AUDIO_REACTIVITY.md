# Audio-Reactive Visuals

Guipper can drive shader parameters and global shader uniforms from a live
audio input. The analyzer is designed for live devices and OS loopback inputs;
it does not play audio files or capture Guipper's internal output mix.

## Quick Start

1. Open **SETTINGS** and enable audio input.
2. Select the input device and `Mix`, `Left`, or `Right` channel mode.
3. Play representative material and confirm that the level and spectrum meters
   respond without persistent clipping.
4. Run the three-second calibration while the source is at its normal idle
   level. Leave auto-gain enabled for material with changing loudness.
5. Open a shader box, expand a parameter, and select the audio mode button.
6. Choose a source. Rhythm sources also expose `Every 1/2/4/8/16` onset
   divisions. Open **Shaping** for detailed response controls.

The parameter value at the moment audio mode is enabled becomes its base value.
When audio stops, the parameter releases smoothly back to that value.

## Parameter Sources

Continuous sources are normalized to `0..1`:

- `Low`, `Mid`, `High`: frequency-band energy.
- `Level`: overall input level.
- `Kick`, `Snare`: detected onset envelopes.
- `Low bass`: combined low-band and kick response.
- `High mid`: combined high-band and snare response.

Rhythm sources are available independently for kick and snare:

- `Trigger`: short pulse on the selected onset count.
- `Envelope`: holds the detected energy for the count cycle.
- `Logic`: toggles between zero and one on the selected count.

The `Every N` setting means every Nth detected onset. Direct onset sources stay
available when tempo confidence is low; clock-derived beat phase and beat pulse
are suppressed until tempo tracking is stable.

## Shaping Controls

Audio-enabled float parameters process their source in this order:

```text
threshold/remap -> curve -> invert -> attack/release -> min/max -> amount/base blend
```

- **Amount**: modulation depth from the captured base value.
- **Threshold**: ignores the quiet part of the normalized source.
- **Curve**: changes response sensitivity without changing the output range.
- **Invert**: reverses polarity.
- **Attack**: rise time in milliseconds.
- **Release**: fall time in milliseconds and return time after audio stops.

Logic sources snap instead of applying continuous smoothing. All fields are
saved append-only in composition and group XML. Older files load with compatible
defaults, and invalid values are clamped.

## Global Shader Uniforms

Declare any of these uniforms in a shader. They are injected centrally and do
not create inspector sliders.

```glsl
uniform vec4 audio_bands;     // low, mid, high, level; each 0..1
uniform vec4 audio_hits;      // kick, snare, low+kick, high+snare; each 0..1

uniform float audio_trigger;  // divided kick trigger; 0 or 1
uniform float audio_express;  // divided kick envelope; 0..1
uniform float audio_logic;    // divided kick toggle; 0 or 1

uniform vec4 audio_onsets;    // kick trigger, snare trigger, kick logic, snare logic
uniform vec4 audio_rhythm;    // beat phase 0..1, pulse 0/1, BPM, confidence 0..1

uniform vec4 audio_spectrum0; // log-spaced bins 0..3; each 0..1
uniform vec4 audio_spectrum1; // bins 4..7
uniform vec4 audio_spectrum2; // bins 8..11
uniform vec4 audio_spectrum3; // bins 12..15
```

The shader beat division is selected in **SETTINGS** and applies to
`audio_trigger`, `audio_express`, and `audio_logic`.

## Analyzer and Threading

The audio callback only downmixes, applies atomic manual gain, and pushes fixed
blocks into a bounded single-producer/single-consumer queue. FFT analysis,
adaptive spectral-flux onset detection, percentile auto-gain, calibration, and
tempo/phase tracking run on the main thread. A full queue drops the incoming
block without overwriting unread audio and reports the overrun in **SETTINGS**.

The stream prefers 48 kHz, falls back to 44.1 kHz or another device-supported
rate, and reports the actual format. Failed setup is retried periodically.

## Diagnostics and Tests

From the project root:

```bash
make -C tests run
make -C tests tsan
make Release -j2
```

Optional application-level diagnostics, run with the `bin` directory as the
working directory:

```bash
cd bin
GUIPPER_AUDIO_TEST=1 ./Guipper          # deterministic 120 BPM material
GUIPPER_AUDIO_TEST=2 ./Guipper          # logarithmic sine sweep
GUIPPER_UISHOT=inspector ./Guipper      # 26 inspector captures + geometry data
GUIPPER_PERSISTENCE_TEST=1 ./Guipper    # current/legacy/invalid XML checks
```

Generated captures and persistence fixtures are written below
`bin/data/uishots/` and are ignored by Git.
