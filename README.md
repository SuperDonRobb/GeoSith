# GeoSith

**Build Jedi Knight levels and 3DO models from inside the game — solo or with friends.**

GeoSith is an in-game editor built on [OpenJKDF2](https://github.com/shinyquagsire23/OpenJKDF2). **Windows x64** is the tested platform. You must supply your own **Jedi Knight: Dark Forces II** installation; game resources are not included.

[**Download the latest release**](https://github.com/SuperDonRobb/GeoSith/releases/latest) · [**Editor guide**](docs/EDITOR-GUIDE.md) · [**Multiplayer setup**](NETWORKING.md) · [**Build from source**](#build-from-source)

## Get started

1. **Download the Windows x64 ZIP** from [Releases](https://github.com/SuperDonRobb/GeoSith/releases/latest). Choose the GeoSith Windows ZIP, rather than GitHub's source-code archive.
2. **Extract the entire ZIP** to a writable folder, such as `Documents\GeoSith`. Keep the supplied DLLs beside `GeoSith.exe`.
3. **Run `Setup GeoSith.cmd`** and select the folder containing your Jedi Knight `Episode` and `Resource` folders. You can also put your game installation in the supplied `JK` folder.
4. **Run `Launch GeoSith.cmd`.** Choose **Create New Level** in single player or on the multiplayer host screen, then enter a level name.
5. **Press F8** once the level loads to enter edit mode. Press F8 again to playtest.

New single-player levels start with one room and one walkplayer. Multiplayer levels start with four walkplayers. Use **Escape → Save Level** to save a playable `.gob` in your game's `Episode` folder.

> Use a writable game folder. Setup generates editor catalogs there, and GeoSith saves levels there. Avoid `Program Files` for your editing setup.

## Features

| Feature | What you can do |
| --- | --- |
| **3D level editing** | Select and move surfaces, edges, and vertices; cleave, extrude, weld, and clean up geometry. |
| **2D map views** | Edit from six directions, mirror geometry, draw spline corridors, and use adaptive grid snapping with blue origin axes. |
| **Terrain sculpting** | Create terrain across angled surfaces and sectors, sculpt, grab, smooth, and paint textures across connected seams. |
| **Custom resources** | Load mods, convert PNG textures to MAT, and synchronize resources with multiplayer clients. |
| **Materials and lighting** | Browse textures, stitch UVs, bake decals and texture paint, and place vertex lights. |
| **Things and animation paths** | Place game objects, edit attributes, and move or rotate translucent path-frame previews. |
| **3DO model builder** | Start from primitives, cleave and extrude faces, apply materials, and save reusable models or walkstructs. |
| **Templates and COGs** | Create custom templates and assign script parameters by picking things, surfaces, or sectors in the level. |
| **Level and episode settings** | Edit colormaps, physics, sky settings, loading text, and episode sequences. |
| **Multiplayer collaboration** | Edit together with host validation, compressed transfers, painting locks, and synchronized undo. |
| **Undo and redo** | Undo up to 30 edits. A continuous drag counts as one edit. |
| **Stock-level conversion** | Open a stock map for editing and save a separate custom episode. Original game archives stay intact. |

## Essential controls

**Fire 1** means primary fire; **Fire 2** means secondary fire. They follow your Jedi Knight bindings, usually left and right mouse buttons.

| Control | Action |
| --- | --- |
| **F8** | Switch between editing and playtesting |
| **0–5, 8, 9** | Choose an editing tool; see the table below |
| **Hold Fire 2** | Grab and move geometry or the current tool's target |
| **Fire 1 while holding Fire 2** | Extrude a grabbed face by one grid unit |
| **Double-tap Jump** | Toggle flight |
| **Jump / Crouch while flying** | Move up / down |
| **Q while flying** | Toggle player collision |
| **Home** | Go to the next walkplayer |
| **Tab** | Switch between 3D editing and the overhead map |
| **Ctrl+Z** | Undo |
| **Ctrl+Y** or **Ctrl+Shift+Z** | Redo |
| **T** | Chat / console, using your normal binding |
| **F12** | Screenshot, using your normal binding |
| **Escape** | Open the pause menu and saving/settings options |

Editing starts with walking and collision enabled. Flight stops when you release movement controls. If an edit leaves you outside the level, GeoSith keeps your viewpoint and enables flight with collision off.

## Editing tools

Select a weapon slot to use its editor tool. Full controls and examples are in the [editor guide](docs/EDITOR-GUIDE.md).

| Slot | Tool | Main use | Middle mouse |
| --- | --- | --- | --- |
| **0** | [Cleave](docs/EDITOR-GUIDE.md#0--cleave) | Cut a surface or sector; stamp polygon outlines | Choose a line or polygon shape |
| **1** | [Select / Clean Up](docs/EDITOR-GUIDE.md#1--select-and-clean-up) | Select, move, extrude, dissolve, or simplify geometry | Choose Select or Clean Up |
| **2** | [Materials](docs/EDITOR-GUIDE.md#2--materials) | Paint surfaces and move UVs | Browse materials |
| **3** | [Things](docs/EDITOR-GUIDE.md#3--things-and-frames) | Place, move, rotate, and edit objects and frames | Browse templates |
| **4** | [Lights](docs/EDITOR-GUIDE.md#4--lights) | Place lights and adjust sector brightness | — |
| **5** | [Placed COGs](docs/EDITOR-GUIDE.md#5--placed-cogs) | Add scripts and assign their parameters | Open Placed COGs |
| **8** | [Template Creator](docs/EDITOR-GUIDE.md#8--template-creator) | Create or edit custom object templates | Open Template Creator |
| **9** | [3DO Builder](docs/EDITOR-GUIDE.md#9--3do-builder) | Create or edit reusable models | Browse saved models; choose primitives while building |

## Editing with friends

1. Use the **same GeoSith release** and matching Jedi Knight resources on every computer.
2. Host a multiplayer game and allow connected players to edit in **Network Details**.
3. Join through LAN discovery, the host's address, or a shared VPN.

The host sends the current level automatically; clients do not need its GOB beforehand. Game textures, models, and sounds must already be installed on each computer.

Client edits appear immediately while the host validates them. Accepted edits reach everyone; rejected edits revert. Undo and redo use the shared host history.

See [multiplayer setup and troubleshooting](NETWORKING.md) for addresses, ports, router mapping, and connection problems.

## Build from source

The downloadable release does **not** require development tools. To build the source, install:

- Visual Studio 2022 with the **Desktop development with C++** workload and a Windows SDK
- CMake, Git, and Python 3

```powershell
git clone --recurse-submodules https://github.com/SuperDonRobb/GeoSith.git
cd GeoSith
powershell -ExecutionPolicy Bypass -File tools/build.ps1
```

To run your build, put your Jedi Knight installation in `JK`, prepare its catalogs, and launch:

```powershell
python tools/prepare_assets.py
& '.\Launch GeoSith.cmd'
```

For standalone tests and live-engine checks, see [testing](docs/EDITOR-GUIDE.md#testing).

## Project and license

GeoSith's combined build, including the Imperial Relay networking integration, is distributed under **GPL-2.0-or-later**. See [LICENSE.md](LICENSE.md) and [COPYING](COPYING). The original OpenJKDF2 code and third-party components retain their respective notices; see the [upstream documentation](README-OpenJKDF2.md). GeoSith-specific behavior on platforms other than Windows x64 has not been verified.
