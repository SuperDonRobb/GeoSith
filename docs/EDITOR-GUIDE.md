# GeoSith editor guide

[← Back to the README](../README.md) · [Download](https://github.com/SuperDonRobb/GeoSith/releases/latest) · [Multiplayer setup](../NETWORKING.md)

## Contents

- [Movement, grabbing, and undo](#movement-grabbing-and-undo)
- [0 — Cleave](#0--cleave)
- [1 — Select and Clean Up](#1--select-and-clean-up)
- [2 — Materials](#2--materials)
- [3 — Things and frames](#3--things-and-frames)
- [4 — Lights](#4--lights)
- [5 — Placed COGs](#5--placed-cogs)
- [6 — Presets](#6--presets)
- [8 — Template Creator](#8--template-creator)
- [9 — 3DO Builder](#9--3do-builder)
- [Overhead map](#overhead-map)
- [Saving and level settings](#saving-and-level-settings)
- [Multiplayer editing](#multiplayer-editing)
- [Limits and geometry rules](#limits-and-geometry-rules)
- [Testing](#testing)

## Movement, grabbing, and undo

**Fire 1** and **Fire 2** use your primary and secondary fire bindings. Ctrl and Shift are editor modifiers; binding a modifier alone to fire does not trigger editor fire actions.

| Control | Action |
| --- | --- |
| F8 | Toggle editor / game mode |
| Normal movement and look | Walk, jump, and look around with collision |
| Double-tap Jump | Toggle flight; use two separate presses within 350 ms |
| Jump / Crouch in flight | Ascend / descend |
| Q in flight | Toggle collision |
| Home | Cycle through walkplayer positions and orientations |
| Hold Fire 2 | Grab the target; move or turn to drag it |
| Ctrl+Z | Undo |
| Ctrl+Y or Ctrl+Shift+Z | Redo |
| T | Use the normal chat / console binding |
| F12 | Use the normal screenshot binding |

Flight stops immediately when movement controls are released. Turning flight off restores collision. Edits that strand you outside the level preserve your position and facing direction, then enable flight without collision.

### Moving geometry

- Aim at a surface, edge, or circular vertex marker to highlight it.
- Surface dragging follows the strongest local horizontal, vertical, or normal direction.
- Vertex dragging follows connected edges and stops at the next vertex. Hold **Shift** for free movement.
- Drag a vertex onto another vertex to weld them where the resulting geometry is valid.
- The world grid is **0.0625 JK units**. Things use half that spacing.

### Extruding a face

1. Aim at the face and hold **Fire 2**.
2. Click **Fire 1** to extrude it outward by one grid unit.
3. Release Fire 2, then grab the new face to move it farther.

Hold **Shift** when clicking Fire 1 to extrude a level surface horizontally and make its far face vertical. This is useful for square tunnels from angled walls. A harmless rail-detonator effect marks the extrusion.

### Undo history

GeoSith retains **30 edits**, including geometry, materials, UVs, objects, lights, and settings. A continuous drag or group operation counts as one edit. Making a new edit after undo clears redo; loading a level resets history.

## 0 — Cleave

| Control | Action |
| --- | --- |
| Hold Fire 1, aim, then release | Preview and cut a line through a surface |
| Shift + line cleave | Cut the whole sector into two adjoining sectors |
| Middle mouse | Choose a line or polygon shape |
| Wheel | Cycle shapes |
| Shift + wheel | Adjust polygon radius |
| Ctrl + wheel | Rotate the polygon |
| Fire 1 with a polygon selected | Cut the projected outline into the surface |

Polygon outlines can extend beyond the surface: the cut is clipped to its boundary. Surface operations must respect the **24-vertex limit**.

In sector-cleave mode, Shift highlights the whole sector. Draw the cut across one of its surfaces; the two resulting convex sectors connect through an adjoin.

## 1 — Select and Clean Up

Middle mouse opens the mode menu. The wheel also switches between **Select** and **Clean Up**.

### Select

| Control | Action |
| --- | --- |
| Fire 1 | Toggle the highlighted surface, edge, or vertex in the selection |
| Double-click Fire 1 | Deselect everything |
| Hold Fire 2 | Move the selection; with no selection, grab the highlighted item |
| Fire 1 while holding Fire 2 | Extrude selected surfaces together |
| Hold Shift while aiming | Highlight adjoins instead of looking through them |
| Enter with surfaces selected | Open Surface Attributes |
| / with exactly two vertices selected | Cleave their shared surface between the selected vertices |
| / with exactly two edges selected | Cleave their common sector through both edges when possible; non-coplanar edges use two boundary-following planar cuts when both selected faces can be preserved |
| / with exactly two faces selected | Cleave the connecting sides of a convex pillar / beam between the faces in their common sector; surrounding sectors remain intact |
| Delete on an edge or vertex | Dissolve it if the remaining geometry is valid and convex |
| Delete on a surface | Delete its sector |

Selected geometry is red. Up to 256 items can be selected. Shared corners move only once during group movement. Group extrusion requires surfaces; the new outer faces remain selected.

**Surface Attributes** includes material, U/V scale, geometry/texture/light modes, extra light, and adjoin distance. Separate checkbox dialogs edit **Surface Flags**, **Face Flags**, and **Adjoin Flags**.

Mixed values stay unchanged unless edited; `~` marks mixed flag bits. Only toggled bits apply to every selected surface. Select both sides of an adjoin to edit both sides. Apply commits one undo step; Cancel discards the draft.

### Clean Up

Aim at a surface and press **Fire 1** to simplify its entire sector.

Cleanup combines compatible coplanar faces into simpler convex patches and removes redundant vertices and edges. It preserves openings, materials, UV boundaries, flags, and adjoins. A concave wall may become several clean faces rather than one polygon.

### Create stairs

1. Start with a **six-surface sector** with horizontal floor and ceiling.
2. Select the two opposite vertical end walls: **high end first, low end second**.
3. Press **Enter** to open Surface Attributes.
4. Open **Generate**, set **Stair steps**, then click **Create Stairs**.

### Slash grate

Select an adjoin surface, press **Enter**, then choose **Generate → Slash Grate**. This applies `08tgrate.mat` to both sides, blocks movement through the adjoin, and adds `00_slashgrate.cog` with the selected side as `grate` and the opposite side as `grateback`. Saber or explosion damage breaks the grate and opens the passage. You can also select both sides of the same adjoin. The operation supports undo.

The result uses visible stair wedges and an invisible **Floor + Impassable** ramp so the player walks smoothly. Creation is one undo step.

## 2 — Materials

| Control | Action |
| --- | --- |
| Middle mouse | Open the searchable material browser |
| Wheel | Switch texture, decal, and texture-paint modes |
| Shift + wheel | Cycle materials |
| Ctrl + wheel | Adjust decal size or paint brush size |
| Fire 1 on a surface | Apply the current material, including visible adjoins |
| Hold Fire 2 on a surface | Move its UVs by moving or turning |

The current material appears at the bottom right. Browser sections include **Models**, **Other**, **All**, and numbered level groups. Thumbnails keep their aspect ratio and use available material palettes or associated level colormaps.

World geometry uses planar UV mapping at **256 texels per JK unit**. Moving geometry changes texture repetitions instead of stretching the texture. UV offsets survive later edits and saving.

Surface-material shortcuts are also available outside tools that override them: **Shift + Fire 1** copies, **Ctrl + Fire 1** pastes, and **Shift + Fire 2** pans UVs. With the saber, Shift selects sector-cleave mode instead.

Apply a compatible colormap through **Escape → Level Header** if indexed textures show incorrect colors. The change updates the running level immediately; save to keep it. The `01` texture set normally uses `01narsh.cmp`.

## 3 — Things and frames

### Place, move, and rotate

| Control | Action |
| --- | --- |
| Middle mouse | Open the searchable template browser |
| Wheel | Cycle templates |
| Fire 1 | Place the chosen template at the aimed surface |
| Hold Fire 2 on an object | Move it |
| Hold Fire 2 on its green end circle | Rotate it |
| Shift while rotating | Allow rotation on all axes |
| Enter on an object | Open Thing Attributes |
| Delete on an object | Remove it |

Placement accounts for model bounds to reduce wall/floor overlap. A thing can still be placed when its model does not fit, provided its placement point is inside a sector.

Movement uses half the world grid. Rotation snaps to **5° increments** around the vertical axis by default. The rotation handle can be picked through a wall when its object is visible. Markers follow visible sectors; offset models such as elevators retain handles when their geometry reaches a visible room.

The browser previews textured models, sprites, or nonvisual markers. Sections include Sprites, Walkstructs, Projectiles, Pickups, Ammo, Weapon pickups, Enemies, NPC, Dark Jedi, Ships, Elevators, Ghosts, Decor, Throwable, and Other.

Objects remain still while editing and resume their template behavior during playtesting. Walkplayers are editable spawn points; moving the player does not move them. At least one walkplayer must remain.

### Add animation/path frames

1. Highlight the thing and press **Enter**.
2. Use **+** beside Numframes to add frames. The first copies the current pose; later frames start one grid unit above the previous frame.
3. Use **< / >** to choose a frame, **−** to remove it, or edit its coordinates and angles directly.
4. Click **Apply**, then use Fire 2 in the level to move its red preview or rotate its end circle.

Frames appear as translucent red copies with zero-based frame numbers. Their positions are independent of the original object. GeoSith saves native `numframes` and frame overrides and configures path movement automatically. Up to **64 frames per thing** are supported.

## 4 — Lights

| Control | Action |
| --- | --- |
| Fire 1 | Place a light in front of you |
| Hold Fire 2 on a light | Move it |
| Wheel on a light | Adjust intensity by 0.1 per notch |
| Shift + wheel on a light | Adjust range by one grid unit per notch |
| Wheel on a surface | Adjust its sector brightness by 0.05, from 0 to 1 |
| Delete on a light | Remove it |

New sectors start at **0.5 brightness**. Point lights add to that baseline without resetting it. Default light intensity is 1 and range is 4 JK units.

Lighting updates with distance, surface angle, and wall occlusion; adjoins pass light. Save Level stores editor light positions and bakes vertex intensities for ordinary gameplay.

> Vertex lighting interpolates between polygon corners. Add cleaves when you need finer lighting detail.

## 5 — Placed COGs

1. Press **middle mouse** to open Placed COGs.
2. Choose **Add COG** and find a script.
3. Select a script, then select a parameter to assign it.
4. For a thing, sector, or surface reference, the menu closes temporarily. Aim at the requested target and press **Fire 1**. **Escape** cancels the pick.
5. Click **Apply** to commit the script and its assignments as one edit.

Materials can be picked from surfaces, templates from things, and vectors from aimed surface points. Numeric and resource parameters accept typed values. COG references can point to another placed COG; **Edit value** also allows manual item numbers.

**Clear value** unassigns a reference or zeros a numeric/vector value. Local script symbols are omitted. Editor mode suspends scripts; F8 starts or restarts them for playtesting. Save Level includes the selected scripts and parameter assignments in the GOB.

## 6 — Presets

Presets preserve a reusable assembly of sectors, things, path frames, custom templates/models, and associated COGs across levels.

1. Select weapon **6**. Click things and sectors to toggle their selection. **Shift+click** selects the sector behind a thing. **Double-click Fire 1** clears all sectors and things selected for preset creation.
2. Click the intended mounting face when selecting the first sector. **Ctrl+click** targets adjoins and explicitly sets the mounting face, adding its sector to the selection. The mounting face must be on the boundary of the selected sectors, with the selected sectors behind it. Selected things' centers and rotation handles turn **purple**.
3. Press **Enter** and choose **Create Preset**, then give it a name. For sector assemblies, the containing sectors of selected things are included automatically so a door or lift is not saved without its room. If an associated COG refers to another unselected sector or thing, select that item too before creating the preset.
4. Choose a preset using the **mouse wheel** or **middle-mouse library menu**. Aim at a surface to see its yellow placement preview, then press **Fire 1** to place it. The mounting face aligns to that surface and the sectors extend beyond it, creating an adjoin where they meet.
   Use **Numpad 8/2/4/6** to offset the preview up/down/left/right along the surface, one editor grid unit (0.0625 map units) per press. **Numpad 5** recenters it. The offset applies to placement too, stays for repeated placements, and resets when you choose a preset from the library or wheel.
5. Press **Escape** or choose **Select Geometry** to return to selection. **Ctrl+Z** undoes a placement, including its things and COGs. **Ctrl+Y** or **Ctrl+Shift+Z** redoes it; these shortcuts work with weapon 6 still selected.

The library menu also provides **Delete Selected**. Deleting a library entry does not remove copies already placed in a level. Copies receive their own COG references, templates, and model names.

Personal presets are `.gsp` files in the game's `presets` folder (normally `JK/presets`). Curated defaults can be installed in `presets` beside the executable; development builds also read the project-root `presets` folder. The export packager includes curated `.gsp` files. Presets carry referenced custom resources; stock resources come from the installed game. You can share these files independently of a level.

In multiplayer, placements use the usual host approval and undo paths. If a client preset introduces custom resources that the host does not yet have, the host must place that preset once first. The host then distributes those resources with the level.

## 8 — Template Creator

Press **middle mouse** to open the list of created templates.

1. Choose **New**, or select an existing custom template to edit it.
2. Choose its base from the searchable template list.
3. Use **+ Parameter** to search for a parameter and add it.
4. Enter its value. Click a `model3d` value to browse game and custom 3DOs.
5. Save the template. It appears in weapon 3's object browser immediately.

Saving with an existing custom name replaces that template. **Delete template** removes it. Remove placed things or dependent templates before deleting a template or model they use.

Custom templates support undo and multiplayer synchronization and are included when saving the level.

## 9 — 3DO Builder

Outside the builder, **Fire 1** starts a new draft. **Middle mouse** opens the custom-model library, where saved models can be edited or deleted.

The world turns blue while the active model retains its textured colors. Each primitive adds one mesh.

### Place primitives

| Control while weapon 9 is selected | Action |
| --- | --- |
| Middle mouse | Choose Box, Sphere, Cylinder, or Pyramid |
| Wheel | Cycle primitives |
| Shift + wheel | Adjust size |
| Ctrl + wheel | Rotate the primitive |
| Alt + wheel | Cycle three detail levels for spheres and cylinders |
| Fire 1 | Place the wireframe preview |

Cube vertices snap during placement. Other primitives retain their generated shape; their vertices can snap when moved later.

### Edit the model

| Tool / control | Action |
| --- | --- |
| Weapon 0 | Cleave faces; cuts snap to half the world grid |
| Weapon 1 | Select and move faces, edges, or vertices |
| Weapon 2 | Paint materials or drag UVs with Fire 2 |
| Fire 1 while holding Fire 2 on a face | Extrude one grid unit along its outward normal |
| Weapon 1 + Fire 2 on the origin circle | Move the model's center point without moving its geometry |
| Shift while dragging a vertex | Allow free movement instead of following connected edges |
| Ctrl+Z / Ctrl+Y | Undo / redo |

Cleave endpoints are shared by adjacent faces. Moving a vertex onto a connected corner can merge them and remove collapsed faces. A stationary grab leaves the mesh unchanged; face/edge movement preserves its shape.

### Finish or save

Press **Escape** to open the naming dialog:

| Option | Result |
| --- | --- |
| **Discard** | Remove the draft; an existing library model stays unchanged |
| **Save as 3DO** | Save to the reusable model library and remove the workspace preview |
| **Save as _walkstruct** | Save the model, create a same-name `_walkstruct` template, and place it where you built it |

Editing an existing model opens a copy in front of you. Save with its original name to replace it. Save Level packages the model files, templates, and editable metadata inside the GOB.

## Overhead map

Press **Tab** to enter or leave the map. Green geometry sits on a black background with gray grid lines; selections are red. Player movement and other 3D inputs are suspended while the map is open.

| Control | Action |
| --- | --- |
| Hold Space and move the mouse | Pan |
| Wheel | Zoom around the cursor |
| Left-click a vertex or edge | Toggle selection; overlapping handles at different heights are picked together |
| Left-click inside a sector | Select the sectors beneath the cursor |
| Left-drag | Box-select sectors |
| Right-drag | Move the selection on the XY grid, keeping its height |
| Ctrl+C | Copy sectors and start an unplaced preview |
| Ctrl+V | Start another preview from the clipboard |
| Insert | Start a default six-sided-sector preview |
| Left-click during placement | Place and merge the preview |
| C or / | Start a sector cleave |
| Delete | Delete selected sectors, keeping at least one |
| Escape | Cancel the current preview, drawing, or cleave |
| Ctrl+Z / Ctrl+Y | Undo / redo |

Weapon 1 selections transfer between the 3D and map views. Moving whole sectors also moves their objects, walkplayers, frames, and lights. Copying copies sector geometry only.

### Draw a sector

1. **Shift + left-click** to place the first vertex.
2. Click to add walls. A red preview means the next wall would cross another wall or make the shape non-convex; placement is blocked there.
3. Use **Backspace** to remove the most recent vertex.
4. Click the first vertex to close an outline with at least three walls.

New sectors use the current material and are one JK unit tall, starting at the floor height of the room occupied when the map opened. Escape cancels unfinished drawing.

### Cleave selected sectors

Press **C** or **/**, then click-drag a line or click its two endpoints. The preview is yellow when it crosses selected sectors and red when it misses. Completing the line makes a vertical cut and joins the halves with adjoins. Each completed cleave is one undo step.

## Flowing Water SIM

Select one sector, open **Sector Attributes → Generate → Flowing Water SIM**. The selected sector continuously supplies water. Blue outlines show only the highest wet surface in each vertical grid column; sparse pale arrows show the current. You can move around during the preview. Mouse wheel up/down increases/decreases simulation speed from 0.25× to 10×; each simulation starts at 1× and displays its current speed. Large simulations remain limited by the per-frame work budget. Other edits and switching to play/map mode wait until it finishes.

Press **Escape** to bake the current water levels. It also bakes automatically when the connected space fills or the source cannot add more water and flow stops. Partial sectors are cleaved along fitted water-surface planes; the lower portions receive underwater flags, `00underwater.wav` at volume `0.25`, and tint `0.1 0.1 1`. Water/air adjoins use translucent, textured `oasis_water.mat` on both sides. Placed instances of the stock `00_conveyor.cog` scroll the water surfaces using the simulated flow direction and speed. Each generated underwater sector also receives thrust in its volume-weighted simulated flow direction, with strength equal to twice that speed. A bundled startup COG restores these currents when the saved level loads. Walkable floors below water shallower than the player’s floor-to-origin standing height receive shallow, deep, or very deep water surface flags in three equal depth bands. Each floor uses its deepest corner; metal and dirt footstep flags are replaced so water sounds take priority. **Ctrl+Z** undoes the entire generation.

The simulation uses a sparse 3D voxel grid with fractional water volumes. Water falls into available voxels below, then spreads sideways to equalize neighboring water heights. This lets water follow a sloped channel while pools settle toward a level surface. Grid spacing starts at 0.0625 units and adapts for large connected volumes to bound memory use. The grid exists only in the simulation and is never saved as level geometry. Baking ignores unsupported falling water when measuring pool surfaces. Flat-bottomed reservoirs receive horizontal planes from their accumulated water volume; sloped channels use fitted planes from supported water-surface samples. This adds at most one cleave per sector. Where both water surfaces cross a shared passable adjoin, their planes are matched along the waterline, avoiding stepped seams while keeping reservoir planes horizontal. Matching does not flatten an inclined channel into the reservoir plane or join a stream above an opening to a pool below it. Empty and completely filled sectors need no cleave. It follows gravity and open, passable adjoins; doors and other thing models are not fluid obstacles. Connected editable splines must first be converted to sectors. The live preview is local in multiplayer; finished geometry goes through normal host approval and replication. If another edit changes the level during simulation, the preview is discarded rather than overwriting that edit.

## Saving and level settings

Open the pause menu with **Escape**.

### Save Level

Enter a name to write a playable GOB in your game's `Episode` folder. The new-level name initially fills the save name, level title, and episode name. Header titles can later be edited independently of the filename.

Stock levels from `JK1.GOB` and `JK1MP.GOB` can be converted for editing. Saving suggests an `_edited` name and protects the stock archive names. Only the current map is exported; the original campaign archives are unchanged.

### Level Header

| Page | Settings |
| --- | --- |
| **General** | Display name, colormap, gravity, ceiling sky height, horizon distance, pixels per revolution |
| **Rendering** | Sky offsets, mipmap/LOD distances, perspective distance, Gouraud distance |
| **Intro Text** | All 20 loading-screen text lines |

Apply updates the working level; Save Level makes it persistent. Colormap changes take effect immediately, including undo. Text fields use JK's string format and cannot contain double quotes.

### Episode Editor

Edit the episode name/type and up to 64 sequence rows. Each row includes line/CD/level numbers, LEVEL or CUT type, filename, force-power grants, Goto A/B, display name, and loading text.

- **Goto −1** ends the episode; other targets must match a sequence line.
- Episode type flags are **1** for single player, **2** for multiplayer, and **8** for CTF; combine them as needed.
- The current row receives the edited geometry. Other rows reference level/cutscene files already available to the game.
- The current row's name and loading text are shared with Level Header.
- Apply is undoable. Save Level writes the episode settings; sequence changes take effect when the saved episode is loaded.

## Multiplayer editing

Use matching GeoSith releases and game resources. See [NETWORKING.md](../NETWORKING.md) for connection instructions.

- Joining clients receive the full current level, including objects, frames, lights, model/template libraries, headers, and placed scripts.
- Clients show edits immediately as previews while the host validates them.
- Accepted edits are broadcast; rejected or stale edits revert to the accepted host state.
- Undo and redo use the shared host history. Continuous drags count as one transaction.
- Incoming changes can cancel an active grab; re-grab the item to continue.
- Clients wait for pending edits before saving. Connection-cache GOBs do not replace existing episodes.

## Limits and geometry rules

| Item | Limit / behavior |
| --- | --- |
| Surface vertices | 24 maximum; operations that exceed this are rejected |
| Sector and surface storage | Grows with the level |
| Selected geometry | 256 items |
| Undo history | 30 edits |
| Path frames | 64 per thing |
| Custom 3DO library | 8 models and 16 total meshes per level |
| 3DO mesh | 64 faces and 512 face-corner vertices |
| Objects and placed COGs | Growing storage bounded by engine ID/table limits, with capacity reserved for gameplay |
| Multiplayer snapshots | 256 MiB uncompressed document / 512 MiB compressed transfer |

Sector operations preserve closed, planar, convex geometry. Overlaps are removed and shared boundaries become reciprocal adjoins. Exact intersections may remain off-grid to keep the level sealed.

Deleting a sector closes its former adjoins and relocates stranded walkplayer markers into remaining geometry. In edit mode, the player's viewpoint stays put with collision-free flight if it is left outside.

Imported stock geometry has a small rounding tolerance; newly created geometry uses stricter validation. Imported UVs, baked light, sector metadata, object paths, and script references are retained where conversion succeeds.

## Testing

The standalone tests do not require Jedi Knight resources:

```powershell
cmake -S tests -B build-tests -A x64
cmake --build build-tests --config Release
ctest --test-dir build-tests -C Release --output-on-failure
```

They cover geometry operations, networking helpers, growing storage, serialization, and GOB output.

Live-engine tests require a prepared `JK` installation:

```powershell
powershell -ExecutionPolicy Bypass -File tools/smoke.ps1
powershell -ExecutionPolicy Bypass -File tools/stress.ps1 -Render
```

The smoke test covers live edits and save/reload. The render stress test exercises repeated edits during rendering and collision updates. Logs are written under `build/`.


## Latest editor additions

### Terrain — weapon 7

- Select surfaces with weapon 1, then click with weapon 7 to create terrain. Surfaces can meet at different angles and span sectors.
- Fire1 raises terrain; Shift + Fire1 lowers it; Ctrl + Fire1 smooths neighboring vertices. Shift + wheel changes brush radius; Ctrl + wheel changes brush intensity. Fire2 grabs terrain with brush falloff.
- Each sector gets a separate model and thing, while shared seams sculpt together. Creating terrain leaves the original sector geometry unchanged.
- The wheel switches sculpt and texture-paint modes. In texture mode, Fire1 paints texture slot 1 and Fire2 paints slot 2 with falloff. Middle mouse opens texture slots, intensity, and conversion options. Delete removes the terrain group. These operations support undo.
- Save Level preserves editable terrain metadata and includes playable walkstructs with dirt footsteps.

### Selection and map editing

- Ctrl + Fire1 with weapon 1 selects whole sectors. Hold Fire2 to move selected sectors together. Enter opens sector attributes; M attempts a convex merge.
- Shift + wheel scales selected surfaces or sectors when valid.
- In map view, keys 1–6 select top, front, left, bottom, back and right views. Blue grid axes mark the world origin.
- The grid adapts to zoom, and movement snaps to the visible grid. Middle mouse opens X/Y/Z mirroring options.
- S starts a spline corridor. Click to add points, drag to adjust handles, and click the first point to close a loop. Escape or right-click finishes drawing.
- Select a spline and press Enter to set its shape, width, height, sector count and texture. Use Convert to Sectors before ordinary cleaving/extrusion. Until then, it remains editable through its spline controls.

### Materials, resources and sound

- Add resource folders or GOBs under mods. PNGs in a mod's mat folder convert to 16-bit MAT textures; fully transparent pixels retain one-bit transparency.
- Custom resources are included when saving and synchronized to joining multiplayer players.
- Weapon 2: Shift + Fire2 rotates UVs; hover an edge while rotating to align. Ctrl + Fire2 pans at half the usual texture grid size.
- Saber idle hum is muted while editing. Remote-player sound identity synchronization fixes incorrect weapon sounds.

Use the same release on all multiplayer peers (editor protocol 22).

### Painting and multiplayer sessions

- Weapon 2 decal mode previews the selected texture; Fire1 bakes it onto the surface. Texture-paint mode uses a round falloff brush. Selected surfaces restrict painting to those faces.
- Baked surface and terrain textures save as 16-bit MAT resources. Applying a regular material restores ordinary surface tiling.
- Multiplayer terrain sculpting/painting and surface texture painting lock the edited target to one player. The owner sees a red border; other editors see a red tint and cannot edit it.
- Press Escape, switch weapons, or start editing another target to finish. Surface painting also finishes when changing texture mode. The completed session is sent once and becomes one shared undo step.
- Join snapshots are compressed. Custom materials, painted textures, models, and COG resources synchronize with the level.
- Clients joining while the host is editing can look up and down immediately; entering gameplay first is no longer necessary.

### Spline previews and sector groups

- Endpoint and interior handles orient the corridor in three dimensions. Drag a point while placing it to set its handles. Fire2 on the curve moves the whole spline.
- Invalid spline geometry appears red in the map and stays out of the 3D world. Adjust its points to make it valid; cancelling a spline with only one point removes it.
- Moving an editable spline restores its previous intersections instead of leaving cuts in surrounding sectors. Escape cancels active drawing/cleaving before opening the pause menu.
- In map view, select sectors and press Enter to create a named group. Add an array along an axis, set the copy count, or delete the group. Choose a group in the spline menu to bend repeated copies along the curve using their original textures.


### Door generator

Select one sector with weapon 1, press Enter, then **Generate → Door**. Choose sliding or split panels, an opening axis, and Automatic, Player Activate, Wall switch, or Key pickup activation. Key doors use the chosen red, blue, or yellow key; place that pickup with weapon 3.

Wall switch returns to the 3D view. Aim the switch preview at a solid wall outside the door sector and press Fire1. This cuts a square, applies `00tcall.mat`, and connects the switch to the door. Escape cancels the pending door and switch without changing the level.

Models, templates, movement frames, sounds and the generated COG are assigned automatically and included on save. Doors close after two seconds and reopen if blocked while closing. Change speed or delay in Placed COGs. Generation supports undo and host approval; leave room for the panels to retract along their chosen axis.


### Conveyor Belt surface generator

Select one or more surfaces with weapon 1, press Enter, then **Generate → Conveyor Belt**. This returns to 3D with a rotation ring and direction arrow on the first selected face. Hold Fire2 on the arrow's end circle and move the crosshairs to rotate the shared belt direction in that surface plane.

Press Enter or Fire1 to apply all belts in one undo step. Escape cancels. Each belt uses `00_conveyor.cog` at speed 2; edit speed in Placed COGs. The script saves with the level, and generation follows normal multiplayer host approval.

### Teleporter thing generator

In weapon 3, use **Shift+Fire1** to select two placed ghost things. Selected ghosts have purple handles and numbered labels. Press **Enter → Teleporter**, then choose **Two-way** or **One-way (1 to 2)**. For one-way travel, select the entrance first and the destination second.

Players teleport when they approach within 0.25 units of a ghost in its sector. The destination ghost controls the exit position and facing. Both ends play the stock teleport/spawn sound (`ForceThrow01.wav`) and emit `+telesparks` particles. Step away from the exit before returning through a two-way teleporter. Place the ghosts at least 0.5 units apart, within sectors.

The generator adds a `gs_teleport.cog` instance to Placed COGs, where you can adjust the radius or travel mode. The script is included in the saved level and uses the normal multiplayer edit approval path. **Ctrl+Z** undoes generation.


### Things in the 2D map

All six map views show thing centers and model bounding boxes. Click a center to toggle its selection; right-drag to move selected things in the current view plane. Movement uses half the visible map grid and keeps the hidden-axis position. With weapon 3 active, drag a selection box to select things; other tools retain sector box selection. Double-click clears selection, and Ctrl+Z undoes movement. Thing centers must remain inside a sector.

### Wind tunnels

Select a sector with weapon 1, press Enter, then **Generate → Wind Tunnel**. The editor returns to 3D with a direction gizmo at the sector center. Hold **Fire2** on its tip and move the crosshairs to aim the thrust in 3D. The **mouse wheel** adjusts strength from 0.25 to 100. **Enter** or **Fire1** applies it; **Escape** cancels. Reopen the generator to adjust existing wind. Ctrl+Z undoes the operation.

For editable sector splines, open **Spline Shape and Geometry → Wind Tunnel**, choose **Off**, **Forward**, or **Reverse**, then select a strength and Apply. Forward follows the point order; Reverse goes toward the start. Each generated sector follows the curve's local direction, including vertical bends and mirrored copies. Reshaping the spline updates its wind. Walkstruct splines do not create sectors, so their wind option requires turning off Make walkstruct first.

Wind uses sector thrust and a generated `gs_wind.cog` included with the saved level. Multiplayer uses normal host-approved edits; all players need this version because spline wind adds network document data.
