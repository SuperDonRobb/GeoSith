OPTIONAL JED LIGHT BAKER

Download LadyEebs' JED Light Baker from:
https://github.com/LadyEebs/JED-Light-Baker

Put Light Baker.dll in this plugins folder. Keep any dependencies supplied
with the plugin beside it. GeoSithLightBaker.exe must be beside GeoSith.exe.
The DLL is optional and is not included in GeoSith exports.
This integration supports the Windows 32-bit JED Light Baker interface;
it is not a general loader for arbitrary JED plugins. A DirectX 11 capable
GPU and the plugin's required x86 Visual C++ runtime are required.

USING IT
Select weapon 4 in edit mode. Middle mouse opens Lighting Tool; choose
Light Baker. Fire 1 opens the plugin's own window. Configure the bake and
click Bake All (or bake selected sectors). A successful bake closes the
plugin window and applies the result automatically. Ctrl+Z undoes it.
Messages appear below the plugin controls, including missing-light errors.
The light/sun shadow-cleaving checkboxes enable shadow cuts for all lights
of that type. No separate per-light cleave flag is needed. The plugin may
ask for confirmation before cutting; the bake remains one undoable edit.
Shadow cuts follow occlusion boundaries (such as doorways), so an empty
room with no shadow boundary may not acquire new surfaces.
Place at least one editor light first; existing sector brightness is not
an editor light. Cancel in GeoSith discards the pending result.

Use Lights mode to place/move lights. Point at a light, middle click, then
choose Light Properties to set point/sun/sky/ambient type, intensity,
range and RGB color. Sun direction is controlled by the plugin's sun and
anchor lights. Its Sun / Sky / Anchor buttons return to GeoSith and open
the selected light's properties. Use its Save/Load Settings buttons to retain bake settings
on this computer. Settings are separate from gameplay COGs.

Baked lighting is saved in the level GOB and synchronized through the
normal host-approved edit system. Other players do not need the plugin
to see the results. Everyone needs the same updated GeoSith version.
Avoid editing the level while a bake is running: stale results are rejected.

Terrain lighting
---------------
Terrain triangles participate in the lighting bake and occlusion. Bake
Selected includes terrain faces inside the selected sectors. Terrain is
not shadow-cleaved: the baker lights its existing sculptable vertices.
The result appears immediately, supports undo, and is saved in both the
standard 3DO vertex intensities and editable terrain metadata. Multiplayer
clients receive it with the completed bake. Standard JK 3DO files store
brightness per vertex rather than RGB color. Re-bake after sculpting or
changing lights to update the shadows.
