# Derek Reverb (VST3 for FL Studio)

## Build (Windows)
1. Install Visual Studio 2022 (Desktop C++ workload), CMake and Git.
2. In this folder:
   cmake -B build -G "Visual Studio 17 2022"
   cmake --build build --config Release
3. The .vst3 is copied to C:\Program Files\Common Files\VST3 automatically
   (run the terminal as Administrator the first time).
4. FL Studio: Options > Manage plugins > Find installed plugins (verify scan).
   Add it from the mixer insert slot: Derek Reverb.

## Custom artwork
Put a PNG (about 1536x1024) at Assets/background.png, re-run cmake, rebuild.

## Controls
Mix = reverb send level, Decay = tail time, Pre Delay, Size = room size,
Damping = high-frequency absorption, Width = stereo width of the wet signal,
Wet/Dry = final balance, Output = drag the meter. Double-click a knob to reset.
