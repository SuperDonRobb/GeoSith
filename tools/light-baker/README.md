# JED Light Baker compatibility host

A separate x86 process adapts GeoSith snapshots to the JED 0.951 COM-style interface used by the user's unmodified Light Baker DLL. No light-baking algorithms are included here. The interface declarations in JedAbi.h originate from LadyEebs/JED-Light-Baker, Source/IJed.h: https://github.com/LadyEebs/JED-Light-Baker . JedDefaults.h provides default implementations for unused ABI methods.

Build with `cmake -S tools/light-baker -B build-light-baker -A Win32`, then `cmake --build build-light-baker --config Release`. Copy the helper next to the game executable. The main build script performs these steps. Requires the Visual Studio x86 C++ toolchain and Windows SDK. Static CRT; the plugin may need its own runtime.

Input and output are bounded GeoSith wire snapshots. Results are validated and applied as one normal editor transaction. Missing plugin/helper hides the mode. The plugin's settings COG is represented only inside this process and stored as local settings, never added to the playable level. This adapter specifically targets Light Baker, not arbitrary JED plugins.

