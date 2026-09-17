# macOS output capture

Native output capture requires macOS 14.2 or newer and a build using the macOS
14.2+ SDK. Older systems/builds keep the existing input devices and do not list
loopback sources. No virtual audio driver is required.

In AUDIO > DEVICE, choose the default output or a device marked `(loopback)`.
Grant system audio recording access when macOS requests it. Audio continues to
play normally; Guipper does not change the output route. The capture targets the
selected device's first output stream (including its channels), not individual
applications or additional streams on a multi-stream interface.

## Build integration

The backend is `src/JPutils/jp_audio_loopback_macos.mm`; it uses CoreAudio and
Foundation. The project Info.plist includes `NSAudioCaptureUsageDescription`.
The Makefile discovers the backend on macOS and excludes it on other platforms.
The saved Xcode project includes the new backend, but is a legacy project whose
source list predates other Guipper modules. Regenerate the complete project with
the openFrameworks project generator if using that Xcode project for a full build.
Keep the audio-capture usage description in the built app's Info.plist.

## Automated checks

Run `bash tests/run_audio_macos.sh` on a Mac. It runs audio-core tests, compiles
the native backend into an ad-hoc-signed test app and checks API availability.
The macOS CI job runs this non-recording check; it does not request permission.

For a hardware test, play audio and run
`bash tests/run_audio_macos.sh --capture`. Grant the requested permission. The
test measures ten seconds of the default output and fails if there is no signal.
An optional second argument selects one of the UIDs printed by the test.
If permission was denied, enable the test app (or its launching terminal) under
System Settings > Privacy & Security > Screen & System Audio Recording, then retry.

## Manual acceptance checks in Guipper

- Switch between a microphone and an output; verify level, spectrum, MIX/LEFT/RIGHT
  and assigned visual parameters, then switch AUDIO off and on.
- Stop playback and verify that the analyser decays to silence.
- Restart the app and verify restoration by UID, including duplicate device names.
- Change the macOS default output while using the default loopback source.
- Disconnect/reconnect an explicitly selected output and change its sample rate.
  Guipper should report/retry the selected output, never fall back to a microphone.
- Deny permission, then grant it in System Settings and retry capture.

The implementation was authored on Windows. Native compilation, permission
handling and hardware behaviour require validation on macOS; a passing Windows
audio-core test is not evidence that these platform checks passed.
