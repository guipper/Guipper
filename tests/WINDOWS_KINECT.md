# Kinect v2 on Windows

Keep the Microsoft Kinect v2 driver and runtime. Do not replace the composite
device driver with libusbK for this backend.

Install Kinect for Windows SDK 2.0 before building. The x64 Visual Studio project
enables `GUIPPER_KINECT_SDK` when `$(KINECTSDK20_DIR)inc\Kinect.h` exists. It uses
the SDK for color, synchronized depth/IR, camera intrinsics and color/depth
registration. The runtime is loaded on demand, so Guipper can still start on a
machine without Kinect20.dll and report the missing runtime in the node.

Linux continues to use the existing `KINECT2` / libfreenect2 backend.

In an x64 Native Tools Command Prompt, from the repository root:

```bat
cl /nologo /EHsc /std:c++17 /I "%KINECTSDK20_DIR%inc" tests\windows_kinect_probe.cpp /Fo:dist\windows_kinect_probe.obj /Fe:dist\windows_kinect_probe.exe
dist\windows_kinect_probe.exe
```

The hardware probe uses the same capture implementation as Guipper. Exit 0
requires 60 depth/IR frames, color frames, nonzero depth/IR pixels, calibrated
intrinsics and depth registered to color, within 20 seconds. It releases the
sensor on exit. To verify reopening, run it again. Then open a Kinect or
PointerCloud node in Guipper and check that it reports STREAMING with live data.

Pass `--soak` to run for the full 20 seconds and also fail if the SDK delivers
no depth/IR frames for more than one second after capture has started.
