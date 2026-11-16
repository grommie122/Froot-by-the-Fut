# Froot by the Fut

Froot by the Fut is a Windows x64 tape-style VST3 echo. Set Time from 45 to 1000 ms, Repeats for feedback, Wow & Flutter for pitch movement, and Mix for the wet/dry blend. The repeat path uses saturation and low-pass filtering.

## Build

Install CMake and Visual Studio with Desktop development with C++. From
PowerShell in this folder, run:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release
```

CMake fetches JUCE. To build with an existing JUCE checkout, add
`-DJUCE_DIR=C:/path/to/JUCE` to the first command.

## Install in REAPER

Copy the entire `Froot by the Fut.vst3` folder from
`build/FrootEcho_artefacts/Release/VST3` into a directory in REAPER's VST
search path. On this machine the per-user path is
`%LOCALAPPDATA%\Programs\Common\VST3`.
In REAPER, open Preferences > Plug-ins > VST and click Re-scan. On a track,
open FX and search for Froot by the Fut.



