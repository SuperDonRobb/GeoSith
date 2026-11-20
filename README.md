# GeoSith

An in-game level editor built on OpenJKDF2.
Supply your own Jedi Knight installation during setup.

Run **Launch GeoSith.cmd**, then choose **Create New Level** in single player
or on the multiplayer host screen. New levels contain one rectangular sector.
Single-player levels start with one walkplayer; multiplayer levels start with
four walkplayers spaced around the room. The launcher selects this project's JK installation.
New sectors start with sector light 0.5. Extrusions inherit their source sector light.
Placed lights add vertex lighting on top of that baseline; adding/removing lights
does not reset sector brightness. Sector light is saved and synchronized.

| Control | Action |
| --- | --- |
| F8 | Toggle editor / play mode |
| Normal movement and look controls | Walk and jump with collision in editor mode |
| Double-tap jump | Toggle flight on/off; collision starts enabled |
| Home | Teleport to the next walkplayer, wrapping back to the first |
| Q while flying | Toggle player collision |
| Tab | Toggle overhead sector map / 3D editor |
| Aim near a vertex, edge, or surface | Highlight it |
| 1, then middle mouse click | Choose Select or Clean Up mode |
| 1, then primary fire | Toggle the highlighted item in the red selection |
| 1, then double-click primary fire | Clear the entire selection |
| 1, then hold secondary fire | Move all selected items together |
| 1, then hold secondary fire and click primary fire | Extrude all selected surfaces together by one grid unit |
| 2, then middle mouse click | Open the searchable material thumbnail browser |
| 2, then hold secondary fire | Grab a surface and move its UVs with travel or mouse-look |
| 2, then mouse wheel | Switch Texture / Decal / Texture Paint modes |
| 2, then Shift + mouse wheel | Cycle the current material |
| 2, Decal mode, Ctrl + mouse wheel | Resize the decal preview |
| 2, then primary fire on a surface | Apply the current material |
| 3, then middle mouse click | Open the searchable object template browser |
| 3, then mouse wheel | Cycle the selected template |
| 3, then primary fire | Place the chosen template at the aimed collision point |
| 3, then hold secondary fire | Grab and move an object or walkplayer |
| 4, then primary fire | Place an editor light in front of the player |
| 4, then hold secondary fire | Grab and move a light on the editing grid |
| 4, then wheel while aiming at a surface | Adjust its sector light by 0.05 per notch (0-1) |
| 4, then wheel while aiming at a light | Adjust intensity by 0.1 per notch |
| 4, then Shift + wheel while aiming at a light | Adjust range by one grid unit per notch |
| 4, then Delete while aiming at a light | Delete the light |
| 5, then middle mouse click | Open Placed COGs; add scripts and assign exposed parameters |
| 6, then primary fire | Multi-select things and sectors; Shift+click selects a sector through things |
| 6, then Enter or middle mouse | Create, choose, or delete a preset in the shared library |
| 6, then mouse wheel | Choose a preset; primary fire places its yellow preview |
| 6, then Escape | Return from preset placement to selection |
| 8, middle mouse | Template creator: add/search parameters, browse 3DOs, save a template for Weapon 3 |
| 9, then primary fire | Enter the 3DO builder with a new draft |
| 9 inside the 3DO builder | Primary fire places the wireframe primitive; wheel cycles shapes; Shift+wheel sizes it; Ctrl+wheel rotates it; middle mouse opens the list |
| Escape inside the 3DO builder | Discard the draft or Save as 3DO to the model library; both remove its workspace preview |
| Hold secondary fire | Grab; player travel and mouse-look move the selection |
| 0, then hold primary fire on a surface | Start the saber cleave preview |
| 0 (saber), then Shift + primary fire | Cleave the highlighted sector into two adjoining sectors |
| Release primary fire | Split the surface along the preview line |
| Hold secondary fire, click primary fire | Extrude the grabbed surface outward by one grid unit |
| Hold secondary fire, then Shift + primary fire | Extrude horizontally to a vertical far face; also works on selected surfaces |
| Shift + primary fire | Copy a surface material |
| Ctrl + primary fire | Paste the copied material |
| Shift + hold secondary fire | Move the surface UVs by moving the player |
| Ctrl + Z | Undo, up to 30 edits |
| Ctrl + Y or Ctrl + Shift + Z | Redo |
| 1, then Delete on an edge or vertex | Dissolve it if the remaining geometry is convex |
| Delete while highlighting a surface | Delete its sector; Ctrl+Z restores it |
| Escape → Save Level | Name and save `JK/Episode/<name>.gob` |
| Escape → Level Header | Edit the level title, colormap, physics, sky, rendering and intro text |
| Escape -> Episode Editor | Edit episode.jk settings, sequence entries and loading strings |

Primary and secondary fire follow the player's bindings. Ctrl and Shift act as
modifiers in the editor; a fire binding on either modifier alone is ignored.
Assign fire to mouse buttons in the JK control menu if needed.
Editor mode starts with flight off. Two separate presses of your bound jump
control within 350 ms toggle flight. Holding jump does not toggle it. While
flying, jump ascends and crouch descends; double-tap jump again to resume gravity.
Weapon 9: primary fire starts a new 3DO; middle mouse opens the custom model
library, where you can edit or delete saved 3DOs. Editing opens a copy in front
of you; saving with the original name replaces the library model, while
Discard preserves it. Reopening needs free capacity for the editable copy.
Weapon 9 enters a separate 3DO workspace. The world turns blue while the current
model keeps its normal textured colors. A circle marks its origin. With weapon 1, aim at that circle and hold Fire 2
to drag the origin on the world grid without moving the mesh geometry. This
works through the mesh and supports undo/redo. 3DO cleave previews and cuts
snap to half the world grid, matching thing movement. Each placed
primitive adds one mesh. Only cube vertices snap during placement; spheres,
cylinders and pyramids keep their generated shapes. Moved vertices snap to the
world grid. Alt+mouse wheel cycles three sphere/cylinder detail levels.
While building, weapon 0 cleaves faces (hold primary fire, draw, release), weapon
1 selects faces/edges/vertices and secondary fire drags the selection, and weapon
2 paints materials or adjusts UVs with secondary fire. Hold secondary fire and
click primary fire on a face to extrude one grid unit along its positive/outward
normal. Movement is grid-snapped; mesh edits never perform level-sector CSG.
3DO cleave endpoints are connected to adjacent faces. Vertex dragging follows
connected edges; hold Shift for free movement. Dragging onto a connected corner
merges the vertices and removes collapsed faces.
Ctrl+Z / Ctrl+Y use the shared undo history and multiplayer host approval.
Escape opens a name dialog with Discard, Save as 3DO, and Save as _walkstruct.
Save as _walkstruct creates a same-name template based on _walkstruct and places
it exactly where you built the model. Discard removes the
draft. Save as 3DO keeps it in the level's reusable model library. Both choices
remove the workspace model from the level view. Weapon 8 opens the Template
Creator with middle mouse: select a created template to edit or choose New.
Saving an existing custom name replaces that template. The edit menu also has
Delete template. Remove placed things or dependent templates before deleting a
template or model they use. Choose the base from the
searchable template list, use + Parameter to search
all supported JK parameters, and edit their values. Clicking a model3d value
opens a searchable, textured browser of game and custom models. Save template
adds it to Weapon 3's object browser. Save Level includes standard 3DO 2.1 files
under 3do/<name>.3do, template definitions, and editable library metadata.
The builder currently supports 8 models and 16 total meshes per level, with 64
faces and 512 face-corner vertices per mesh, and 24 vertices per face. These
models are saved to the level's reusable library. Weapon 8 creates templates
from game or custom 3DOs, with a base template and editable JK parameters.
Saved templates immediately appear in Weapon 3. Save Level packages the models
and templates inside the GOB. Custom templates support undo and multiplayer;
all peers must use this version.
Multiplayer participants must all run this version because mesh data extends
the editor network format.

Releasing the movement controls while flying stops you immediately.
Turning flight off restores collision. Home uses the walkplayer positions and
orientations saved in the level; walking around does not move those spawn points.
In the overhead map, green lines show sector geometry on a black background and
gray lines show the grid. Vertices have circular handles. Left-click a vertex or
edge to toggle its selection; click other handles to add them. Overlapping handles
at different heights are picked together. Click inside a sector to select sectors
beneath the cursor, or left-drag a rectangle to select sectors. Selected geometry
is red. Right-drag moves the selection on the XY grid without changing its height.
Weapon 1 vertex, edge and surface selections are retained exactly when switching
between 3D and map views.
Press C or / to cleave selected sectors: click and drag a line, or click its two
endpoints. The snapped line previews yellow when it crosses selected sectors
and red when it misses. Completing the line cuts touched selected sectors
vertically and connects the halves with adjoins. Escape cancels the cleave.
Delete removes all selected sectors and closes former adjoins, keeping at least
one sector in the level. Each completed cleave or deletion is one undo step
and uses the multiplayer host approval path. Ctrl+C still copies sectors.
Hold Space and move the mouse to pan; use the wheel to zoom around the cursor.
Right-drag moves selected sectors on the editing grid, including their existing
objects, walkplayers, path frames and lights. Copying uses sector geometry only.
Ctrl+C immediately starts an unplaced preview; Ctrl+V recalls the clipboard as
another preview. Its yellow outline follows the cursor without changing the
level. Left-click places and merges it, preserving internal adjoins and closing
exposed boundaries. Escape cancels placement. Insert starts a preview of a new
default six-sided sector with the same placement controls.
Shift+left-click starts drawing a sector outline on the grid. Further clicks
add walls; click the first vertex to close an outline with at least three walls.
Backspace removes the last placed vertex so drawing continues from the previous
vertex. Escape cancels unfinished drawing. New sectors use the current material and a
height of one JK unit, starting at the floor height of the room occupied when
the map was opened. Only convex outlines are accepted. The live wall turns red when it would make
the outline non-convex or cross another wall, and clicks there are ignored.
Drawing no longer automatically splits concave shapes. Placed overlaps use the same rules as
3D edits. Ctrl+Z/Y undo/redo, including host approval in multiplayer. Player
movement, look, weapons and other 3D inputs are suspended until Tab returns to
the 3D editor. F12 screenshots remain available.

Editor status, tool statistics, object/frame labels and map text use font
a fixed warm-white tint, with glyph shading read through 01narsh.cmp and colors
remapped into the active palette. Changing the level
colormap updates this mapping without changing the level lighting or menus.

Highlights use thick outlined edges and circular vertex markers. The selected
vertex has a larger circle; markers use the world viewport independently of HUD
and crosshair settings. The 3D editor uses a compact yellow crosshair with a dark
orange outline, independent of the normal gameplay crosshair setting.
With weapon 1 in Select mode, hold Shift to highlight a whole sector and Shift+Fire 1 to toggle sector selections. Press M to merge selected sectors only when their combined boundary is convex. The lowest-numbered sector supplies the merged sector attributes; internal adjoin surface references are cleared. Press Enter for sector flags, extra light, colormap, tint, ambient sound and volume. Mixed attributes stay unchanged unless edited. Sector selections transfer to and from map view. Layers are saved editor labels. Sector edits support undo and multiplayer replication; all players need the same build.

Weapon 1 keeps a selection of up to 256 surfaces, edges, or vertices, shown in
red. Press Enter with one or more surfaces selected to open Surface Attributes.
Edit the material name, U/V scale, geometry/texture/light modes, extra light,
and adjoin distance. Surface Flags, Face Flags, and Adjoin Flags open separate
checkbox dialogs, including unknown bits. Mixed values remain unchanged unless
edited; a `~` marks mixed flag bits. Only toggled bits are applied to all selected
surfaces. Apply updates the running level as one undo step; Cancel discards the
draft. Save Level writes the attributes and scaled UVs into the GOB. Weapon 1
can select adjoin boundaries; their connection number is read-only, while their
flags are editable. The opposite side is edited by selecting it too.
 Shared corners move only once during a group drag. Group extrusion requires
surfaces only; the new outer faces remain selected for further edits. Adjacent
extrusions connect through adjoins. The whole group operation uses one undo step.
Undo retains the latest 30 edits, including geometry, UVs, materials, deletion,
and header changes. A continuous drag counts as one edit. A new edit after undo
starts a new history branch and clears redo. History resets when loading a level.
Before a vertex or edge move, cleave endpoints are connected to every incident
face edge, including edges that previously omitted that endpoint. This keeps
door-frame junctions shared during welding instead of reconstructing extra
polygons around disconnected face loops.
Vertex welding checks the closed sector boundary, repairs collapsed/reversed
face patches, and keeps supporting face hulls free of interior vertices so
merging a corner cannot silently remove a wall triangle. Vertex dragging follows
connected edge segments and stops at the next vertex; redundant edge vertices
follow their boundary edge. Hold Shift while dragging a vertex to move it freely
in all directions. Shift still pans UVs when grabbing a whole surface.
With weapon 3, aim at a placed thing and press Enter to open Thing Attributes.
Edit its position, rotation, or thing flags. Numframes has + and - controls:
the first frame copies the thing's current pose, and additional frames start one
grid unit above the previous frame. The minus button removes the displayed frame;
use < and > to choose a frame or type its coordinates and angles directly.
Apply commits one undo step; Cancel discards the dialog changes.

Frames appear in edit mode as red translucent model copies with zero-based frame
numbers and rotation circles. In weapon 3, hold Fire 2 over a frame to move it,
or over its circle to rotate it. Movement snaps to the editor grid; rotation snaps
to 5 degrees and stays on the vertical axis unless Shift is held. Frame positions
are independent of the original thing. Saved levels include native JK numframes
and frame overrides; path movement is configured automatically, with no manual
thing-flag setup required. Up to 64 frames per thing are supported.
Weapon 3 reads the supplied `master.tpl` (also accepts `mater.tpl`). Its browser
shows textured 3D models, sprite thumbnails, or markers for templates without a
visual asset; the chosen template also appears at bottom right. Its section button
filters Sprites, Walkstructs, Projectiles, Pickups, Ammo, Weapon pickups, Enemies,
NPC, Dark Jedi, Ships, Elevators, Ghosts, Decor, Throwable, or Other. Sections can
overlap; Walkstructs includes descendants of `_walkstruct`. Placed objects
stay still while editing and resume template behavior in play mode. Walkplayer
spawn markers are visible in editor mode and can be moved independently of the
player. Every object and walkplayer has a green facing line with a circle on its
end. With weapon 3, hold Fire 2 on that circle and drag to rotate the object;
grabbing the body still moves it. Rotation keeps the center fixed, updates live,
and uses one undo step per drag. Rotation snaps to 5-degree increments and
changes yaw around the vertical axis by default; hold Shift to rotate freely. With weapon 3, Delete removes the hovered object or walkplayer; at least
one walkplayer is kept. Deletion supports undo/redo. Placement uses the model
hierarchy bounds (with radius as a fallback) to keep models outside solid walls
and floors, including corners. Positions snap to the geometry grid along the
target surface, with a small perpendicular offset for model clearance.
Up to 1024 objects and 32 walkplayers can be saved. Template definitions are
included in the saved GOB.
Weapon 2: hold Shift while starting a right-button grab to rotate the texture.
Move around the surface center to turn it; hover an edge while grabbing to
align the texture horizontally along that edge. Release the right button to
finish. Normal right-button grabs still pan UVs. Rotation survives geometry
edits, undo, saved levels, and multiplayer (all players need the updated build).

Weapon 2 shows the current material above the bottom-right ammo display. Its
browser lists materials from installed GOBs and loose material folders, with
search and paging. Click its section button to choose Models, Other, All, or a
numbered level group. Thumbnails preserve the source aspect ratio. Embedded MAT
palettes take priority; otherwise previews use colormaps identified from the
installed campaign's surface references, falling back to the default palette.
Run `tools/prepare_assets.py` again after installing additional asset packs to
refresh these associations. Saving includes references to any additional
materials used by the level.
With weapon 1, Delete on a highlighted edge merges its adjacent coplanar
surfaces when their union is convex. Delete on a vertex removes redundant
vertices, merging incident coplanar faces as needed. Deletions must leave closed,
planar, convex geometry; unsafe deletions leave the level unchanged. Shared
adjoins are updated on both sides, and surface COG references are remapped.
Merged faces keep the highlighted surface's material and settings. These edits
support undo/redo and the normal multiplayer host approval path.
Delete on a highlighted surface removes its owning sector and closes its former
adjoins. Walkplayers stranded by the deletion move inside a remaining sector;
the player also relocates if necessary. The final sector cannot be deleted.
Dragging compares consecutive crosshair ray intersections with a reference
plane parallel to the grabbed surface. The plane follows player travel so walking
past the original wall cannot lose the grab. Cursor motion stays independent of
live geometry rebuilds. Parallel or missed rays pause only cursor tracking;
player travel continues, and valid tracing resumes without a jump. Surface movement follows the strongest local
left/right, up/down, or normal/depth component. UV dragging follows both tangent
components together, with the visible texture following the cursor. With an empty selection, weapon 1 can directly grab the
highlighted edge or vertex. Coincident vertices weld to one shared position in
the live world and saved level, while retaining separate UVs at texture seams.
Hold Shift with the saber to highlight the entire sector. Start a cleave while
holding Shift, aim its line on a surface, and release primary fire to cut the
sector through that line. The two convex sectors connect through an adjoin.
Without Shift the saber continues to split a single surface.
Extrusion immediately adds one grid unit along the outward face normal and ends
the grab. Release secondary fire, then grab the new face to move it farther.
A harmless rail detonator sprite and sound mark the original surface center.
The grab distance stays fixed, so distant grabs respond more to mouse-look.
Edges and vertices follow combined movement with grid snapping. The grid is
1/16 JK unit (0.0625), about half the default player's width. Surface drags snap
along their selected local axis; vertex/edge handles and cleave guides snap to
the grid. Exact CSG intersections are retained to keep sectors sealed.

Textures use world-aligned planar UV projection at 256 texels per JK unit.
Moving geometry reveals more or fewer texture repetitions instead of stretching
the old UVs. Shift-grab adjusts the projection offset in grid-sized increments;
offsets survive later edits and save/reload. Multiple walkplayers and their
orientations are preserved when saving.
Existing editor levels receive the corrected texel density when entering the
editor; earlier world-mapping offsets are converted with it. Save to retain it.

Level Header has General, Rendering, and Intro Text pages. General includes the
display name, colormap chooser, gravity, ceiling sky height, horizon distance
and pixels per revolution. Rendering includes horizon/ceiling offsets, all four
mipmap and LOD distances, perspective distance and Gouraud distance. Intro Text
provides all 20 JK loading-screen text lines. Apply updates the working level;
Save Level writes the settings, title and intro strings into its GOB. The title
is independent of the save filename. Applying a colormap updates the running
level and its texture caches immediately, including when undoing the change.
Save Level makes it persistent. Indexed textures still need a compatible level
palette (the 01 set normally uses `01narsh.cmp`). JK format version stays at the
engine-supported 1.
JK's string format requires text without double quotes.

Convex sector subtraction removes overlapping volume. Opposing faces are split
at their shared intersection to create reciprocal adjoins. Vertex and edge moves
rebuild affected convex hulls when necessary; this can change face topology.
Planar cleanup moves preserve existing cleave partitions. Extra collinear
vertices on sector edges can slide along that edge and snap onto another
vertex in the same sector to weld. Degenerate faces disappear after welding;
cleaning ceiling-edge corners avoids unnecessary extrusion side walls.
Sector and surface storage grows with the level; there is no fixed 1024-sector
or 192-surfaces-per-sector ceiling. Geometry scratch buffers, map selections,
undo snapshots and network snapshots grow with their contents. Surfaces still
support at most 24 vertices. Placed objects and COGs use growing editor storage
up to the engine's ID/table limits, with room reserved for players and gameplay.
Snapshots allocate their actual encoded size. Multiplayer retains a 256 MiB
uncompressed document budget (including manifests/scripts) and a 512 MiB
compressed-transfer budget. Hosts and clients must use the same build.

Multiplayer edits and undo are synchronized through the host. Stock levels
can be converted for editing; saving a converted stock level uses a separate
custom episode rather than replacing the original game archive.

## Build

Visual Studio 2022 with the C++ desktop workload, a Windows SDK, CMake, Git, and
Python 3 are required. From this folder:

```powershell
python tools/prepare_assets.py
powershell -ExecutionPolicy Bypass -File tools/build.ps1
```

`prepare_assets.py` extracts resource references and templates from the local
`JK/Episode/JK1MP.GOB` and prepares the installation marker OpenJKDF2 requires.
Game assets and generated levels are excluded from source control.

The build includes the native geometry tests. To run them separately:

```powershell
cmake -S tests -B build-tests -A x64
cmake --build build-tests --config Release
.\build-tests\Release\editor_tests.exe
```

Tests exercise picking, cleaving, vertex/edge edits, extrusion, overlap removal,
partial portals, repeated portal rebuilding, UV density, level headers, validation,
and GOB output. Save/reload integration also checks title, all header settings,
the selected colormap and intro text.
`GEOSITH_SELFTEST=1` opts into the live rebuild integration test only when
running the generated `sithed_smoke` episode.
Run `powershell -ExecutionPolicy Bypass -File tools/smoke.ps1` for the geometry
tests, live engine test, and a reload of the exported two-sector GOB. Test logs
are written under `build/`.
The live test also exercises queued AI sound events and surface/sector animations
across repeated geometry replacement. Run
`powershell -ExecutionPolicy Bypass -File tools/stress.ps1 -Render`
to exercise 240 edits during normal rendering and
collision updates. The test verifies that frames were rendered; logs are saved
as `build/stress.stdout.log` and `build/stress.stderr.log`.

Upstream engine: https://github.com/shinyquagsire23/OpenJKDF2
The upstream license is in `LICENSE.md`.

Edit mode honors the normal screenshot binding (F12 by default), capturing the
finished frame including editor highlights and HUD.

To create optimized stairs, select two opposite vertical end walls of the same
six-surface sector with weapon 1: select the high end first and the low end
second. Press Enter, set **Stair steps**, and click **Create Stairs**. The floor
and ceiling must be horizontal. The default uses approximately one grid unit of
rise per step; the step count is no longer tied to a fixed sector/surface storage capacity.
The tool cleaves a floor-to-ceiling diagonal ramp, retains the air wedges above
each visible tread, and removes the volumes below the treads. The invisible ramp
adjoins are Floor + Impassable, with visibility/sound enabled and movement through
the ramp disabled. Walking uses the smooth ramp while the visible geometry shows
steps. Creation is one undo step; Save Level keeps the new sectors and flags.

Weapon 4 places editor point lights in growing storage (default intensity 1, range 4 JK units). Light markers and the hovered light's intensity/range appear in editor mode. Lighting updates immediately with distance falloff, surface angle and solid-wall occlusion; adjoins pass light. Sectors start at light 0.5. Aiming at a surface shows its wireframe and sector brightness; the wheel adjusts that sector in 0.05 steps from 0 to 1. Placed lights add Gouraud vertex lighting without resetting sector brightness; deleting the last light preserves the sector baseline. Lights, movement and wheel edits support undo/redo. Save Level writes a JED-style Section: Lights plus baked vertex intensities to the JKL in the GOB; ordinary play uses those baked values without editor markers. Vertex lighting interpolates between polygon corners, so add cleaves where you need finer lighting detail.

The pause menu's Episode Editor edits episode name/type and up to 64 sequence rows. Click a row to set its line number, CD, level number, LEVEL/CUT type, filename, light/dark force-power grants, Goto A/B, display name and 20 loading-text lines. Add, delete and reorder rows with the sequence buttons. Goto -1 ends the episode; other targets must match a sequence line. Episode type is a numeric bitmask: 1 single player, 2 multiplayer, 8 CTF (combine flags as needed). Apply commits one undoable edit; Cancel discards the draft. Use Save Level afterward to write episode.jk and the associated strings into the GOB. The marked current row receives the edited geometry under its chosen JKL filename; other rows reference level/cutscene files already available to the game. The current level's name/loading text is shared with Level Header. Changes to the episode sequence take effect when the saved episode is loaded.

Weapon 5 opens the Placed COGs editor with middle mouse. Add COG searches scripts in the mounted resources, the original game episode GOBs, and loose cog folders. Select a script on the left; click a parameter on the right. Thing, sector and surface parameters return to the level with only that target type highlighted: aim and press Fire 1 to assign it, or Esc to cancel the pick and reopen the menu. Materials can be picked from surfaces, templates from things, and vectors from aimed surface points. Numeric/resource parameters accept typed values; COG references choose another placed COG. Edit value also allows manual item numbers; Clear value unassigns a reference (-1) or zeros a numeric/vector value. Local script symbols are omitted. Apply commits the draft as one undoable edit, including all assignments. F8 starts/restarts placed scripts for playtesting; editor mode suspends them. Save Level writes the COGS section and bundles each selected script in the GOB. Up to 64 placed scripts and 128 exposed parameters per script are supported.

Multiplayer editing uses the existing game connection. Everyone must use this
updated GeoSith build with the JK game assets. Host an editor episode as a
multiplayer game; joining players do not need the host's episode GOB. The client
creates a private netedit episode to connect, then receives the full current level
before the normal JK join sync begins. This includes geometry, surface attributes,
materials by name, objects and frames, lights, level/episode headers, and placed
COGs with their scripts and parameter assignments.

The host automatically validates every submitted edit and its document revision.
Client edits appear immediately as local previews while the host checks them.
Accepted edits are broadcast to everyone; rejected previews revert immediately
to the last accepted level and then synchronize with the current host version.
Newer drag positions stay visible when an older position is approved. Continuous
drags share one undo transaction. Ctrl+Z and
Ctrl+Y on a client request the shared host history (30 steps); the accepted result
is replicated to everyone. If another player changes the level before a pending
edit can be accepted, the stale request is rejected and the current host version
is restored. Re-grab the item to continue editing. An incoming edit also cancels
an active grab on the host so that an old drag cannot overwrite the new change.

Level transfers use bounded chunks, checksums, acknowledgments and retries;
partial snapshots never replace the running level. Private netedit GOBs are
connection caches and do not overwrite existing episodes. Save Level exports the
current accepted document as a normal playable GOB; clients wait for pending
edits to finish before saving.

Multiplayer connectivity and troubleshooting: see NETWORKING.md. The new network
protocol requires matching GeoSith releases and game Resource archives.

Whole-surface movement includes shared boundary junctions omitted from the selected
face loop. For example, raising a ceiling also raises the wall-cleave endpoints
on its perimeter, preserving existing partitions rather than generating repair
triangles and interior vertices.

Weapon 1 has Select and Clean Up modes in its middle-mouse menu; the mouse
wheel also switches between them. Select keeps
the normal selection, dragging and extrusion behavior. In Clean Up, aim at a
surface and press Fire 1 to simplify its entire sector. If sectors are selected,
Fire 1 cleans all selected sectors instead. Both sides of each cleaned adjoin
are updated together, even when its neighbor is not selected. Connected coplanar
patches with matching materials, UV mappings, flags and rendering settings merge
when their union is convex and fits the 24-vertex limit. Redundant interior and
collinear vertices disappear. Concave regions and material/settings boundaries
remain separate. Adjoin patches merge only with matching reciprocal patches in
the neighbor, preserving connections. Cleanup is one undoable edit and uses the
normal multiplayer host validation and replication path.

Cleanup also rebuilds dense coplanar solid meshes around preserved openings
and other material regions into simpler convex patches. This can reduce a
fragmented wall to a few quads even when its concave outline cannot form one
convex polygon. Openings, volume, material/UV boundaries, flags and adjoins are
preserved; each proposed merge must retain a closed sector boundary.

Stock-level conversion
----------------------
Opening a level from JK1.GOB or JK1MP.GOB imports it into GeoSith for F8 editing, including multiplayer synchronization. Save Level suggests the map filename with `_edited` appended (for example, `m10_edited`). JK1 and JK1MP are protected save names. Only the current map is saved in the new episode; the original campaign/episode archive is unchanged.

Conversion retains original UV coordinates, baked vertex lighting, sector flags/tint/ambient sounds, object AI settings and paths, and remaps placed COG references to the editor's object/surface order. Imported sectors allow up to 0.001 game units of rounding error; newly created sectors retain the stricter checks. Invalid or over-capacity imports fail without replacing the running geometry.

The multiplayer editor protocol changed for the imported metadata: hosts and clients must use the same updated build. Undo snapshots use compressed populated records to limit memory use on large maps.

While flying in edit mode, press your bound Force Speed control to toggle between normal and double movement speed. Turning flight off resets the boost.

With weapon 1 in Select mode, Backspace switches selected adjoin surfaces to their opposite sides. With weapon 4 aimed at a surface, Shift + mouse wheel adjusts that surface's extra light; the wheel alone adjusts sector light. When aimed at a placed light, Shift + wheel still adjusts its range.

Sector Attributes provides an Ambient Sound Browse button for game and custom WAV resources. Choose <none> to clear the ambient sound.

Weapon 2: hold Ctrl when starting a right-mouse texture drag to pan in half-grid steps, including in the 3DO editor. Shift + right-drag still rotates textures.

Spline corridors (overhead map)
------------------------------
- Press S at the mouse cursor to start a spline. Click to add points; hold and
  drag while adding a point to set its tangent handles. Escape or right-click
  finishes drawing and keeps the spline editable.
- Click the curve to select it. Enter opens cross-section, sector count, width
  and height settings. The default is square; shapes run from triangle through
  a 32-sided circle. Width and height are in game units.
- Click a point to show its paired handles. Drag the point or either handle
  with the left mouse button. Enter on a point opens its X/Y/Z coordinates;
  Z is the corridor center height. In 3D, weapon 1 can select and grab points.
- Point and handle drags regenerate the corridor on release. Invalid splines
  remain editable as red map previews, without merging or appearing in 3D.
  Fix the controls to regenerate valid geometry. Select the curve and hold
  Fire 2 to move the entire spline. Ctrl+Z/Ctrl+Y undo and redo edits.
- Splines are saved with the level and synchronized in multiplayer. Intersections
  and mirrored connections keep spline controls. Use Convert to Sectors in the
  spline shape menu before cleaving, extruding or moving its generated geometry.
  Conversion is one undo step; undo restores the controls.
- Multiplayer participants must use this updated build (editor protocol 25).

Spline tunnel textures follow the corridor length and wrap around its cross-section.
The perimeter is stretched to a whole number of texture repeats for a closed seam.
Apply the spline settings to regenerate older splines with this mapping. Curved
sloping sides are triangulated when their four corners do not share one plane;
planar sides remain quads. Custom UVs survive saving, undo and ordinary cleaves.

Map spline editing: Delete removes the selected spline and its generated sectors,
or just the selected control point and regenerates the corridor. The final point
removes the spline. At least one sector must remain in the level. Select a point
and hold right mouse / Fire 2 to move it. While drawing, click the first point
(after placing at least three points) to close the loop. Closed splines remain
editable, save with the level, and support undo/redo.


Terrain tool (weapon 7)
-----------------------
- Click a solid sector surface to turn it into subdivided terrain. To combine
  surfaces, select them with weapon 1 first. Connected surfaces may span sectors
  and meet at different angles.
- Aim at terrain to see the circular brush. Hold Fire 1 to raise it along the
  current terrain surface normal; Shift + Fire 1
  lowers it. Ctrl + Fire 1 strongly smooths connected vertices in all directions,
  keeping shared seams together. Hold Fire 2 to grab a patch with brush falloff
  and move it freely. Shift + wheel changes brush radius. Ctrl + wheel changes
  sculpt/smooth/paint intensity (1-100%, default 25%); middle mouse also offers a
  numeric intensity setting. Terrain uses a shared tiled UV map, recalculated
  after edits to reduce stretching and keep linked seams aligned.
- Wheel switches between Sculpt and Texture modes. In Texture mode, Fire 1
  paints texture slot 1 and Fire 2 paints texture slot 2 with the same circular
  brush, falloff, radius and intensity controls. Middle mouse selects the two
  paint textures. Changing a slot preserves previous painting, so you can build
  up more than two materials (up to 64 layers per mesh).
- Paint weights are stored at vertices and interpolated across triangles.
  Saving bakes each terrain model into one RGB565 MAT atlas (up to 2048 square),
  included in the GOB, while preserving the source layers for editing later.
  Converted 3DOs also use the baked texture. Painting supports undo and network
  synchronization; multiplayer peers must use the same updated build.
- Middle mouse also opens conversion options. Convert to 3DO keeps a model in the custom library;
  convert to walkstruct keeps an ordinary placed object in the level.
- Terrain edges can be raised and lowered with the brush. Delete while pointing
  at terrain removes it, its generated template and custom model (Ctrl+Z restores
  it). Terrain walkstructs use dirt footsteps. Creating terrain leaves all sector surfaces and vertices unchanged. New terrain
  faces sit slightly above the supporting surfaces to avoid overlapping them.
  Selected surfaces may meet at different angles or span sectors. Each sector
  gets its own terrain model and thing; shared edges sculpt together. Texture,
  conversion and deletion apply to the linked terrain group.
  Floors, walls, ceilings and angled solid surfaces are supported. Sculpting
  stops at sector boundaries. Larger areas use coarser subdivisions to fit the
  available custom-model mesh capacity.
- Save Level includes a playable walkstruct and the editable terrain data.
  Reopening it in GeoSith restores sculpting. Terrain edits and conversions
  support undo and multiplayer synchronization; all players need this build.

Map views: the map opens in Top view. Press 1 Top, 2 Front, 3 Left,
4 Bottom, 5 Back, or 6 Right. The grid, selection, movement, placement,
cleaving and spline handles follow the chosen view. Switching views keeps
selections and cancels any unfinished drawing or placement gesture.

Map mirroring: middle mouse opens Editing Options. Mirror X, Mirror Y and
Mirror Z reflect map geometry edits across world X=0, Y=0 and Z=0. Enable
multiple axes for all combinations. Placement previews show mirrored copies.
Existing matching counterparts follow geometry movement, cleaves and deletion.
Each mirrored operation is one undo step and uses normal multiplayer approval.
The options affect subsequent map edits; enabling them does not copy the whole
existing level. Reflected spline sectors stay attached to the source spline. Its saved mirror
axes are reused when regenerating it, including after reopening the level.

Weapon 1 / Select: Shift + mouse wheel scales selected surfaces or sectors
about their combined center (wheel up: 10% larger; down: inverse step).
Shared vertices move together. Nonplanar or non-convex results are rejected;
editable splines must first be converted to sectors. Ctrl+Z undoes scaling.

### Surface decals (weapon 2)

Use the wheel to switch between Texture and Decal modes. Shift + wheel picks
materials in either mode; middle mouse opens the material browser. In Decal
mode, aim at a surface to preview the texture, use Ctrl + wheel to change its
size, and press Fire 1 to stamp it. The preview clips at the surface edges.
Transparent parts of the decal preserve the texture underneath.

Each stamp bakes over the existing surface material. Undo/redo and multiplayer
synchronization include the result. Saving embeds a full-surface 16-bit MAT in
the level GOB, so playing the level does not require native decal support.
All multiplayer participants need the same updated version.

### Sector texture painting (weapon 2)

Switch to Texture Paint with the mouse wheel. Hold Fire 1 to paint the selected
material using a round brush with soft falloff. Shift + wheel chooses the
material; Ctrl + wheel changes the brush diameter. Middle mouse opens the
material browser. The inner brush ring indicates the stronger part of the brush.
Textures tile in a fixed position on the face as you paint, and you can switch
materials to build up multiple textures. Each held stroke is one undo step.
Painted faces save as full-surface 16-bit MATs in the level GOB, using the same
save and multiplayer synchronization path as decals.

Texture Paint respects whole surfaces selected with weapon 1: when any surfaces
are selected, only those surfaces can be painted, and the brush is hidden over
unselected faces. Clear the surface selection to paint freely again.

### Sector groups and arrays

In the 2D map, select one or more sectors and press Enter. Use **+ Create Group
from Selection** and give the group a name. Each group is a saved geometry
snapshot, including surface materials and UVs. It contains sectors, not placed
things, lights or COGs.

Enter the number of **additional copies** beside a group, then choose **Add
Array** and a direction: +X, -X, +Y, -Y, +Z or -Z. Copies are spaced by the full
group bounding-box size along that axis. Touching geometry is adjoined using
the normal sector placement rules. Applying again replaces that group's
previous array; zero copies removes it. The source sectors stay in place.

Groups also appear after the polygon shapes in the spline cross-section picker.
Choose a group to bend copies along the spline, keeping its own textures.
The spline's copy count, width and height control the result. The group's
array direction is its forward axis (default +Y); set that direction in the
array menu before creating the spline. Tight bends can be rejected if they
cannot produce valid convex sectors. Use narrower geometry or more copies.
Group splines remain editable until **Convert to Sectors**.

Groups and generated geometry are saved in the GOB and support undo/redo and
multiplayer synchronization. All participants need this updated version.

Surface texture painting in multiplayer (weapon 2): the first brush stroke locks
the surface to you. You see a red border; other editors see a red tint and cannot
change its texture. Strokes stay local until you press Escape, change weapons or
texture mode, or begin painting another surface. The completed painting is sent
once and becomes one shared undo step.

In **Spline Shape and Geometry**, enable **Make walkstruct** to generate an
outward-facing solid 3DO and `_walkstruct` template instead of sectors. The
spline points, handles, shape and texture remain editable. Saving the level
includes the models and templates in its GOB. Splines spanning sectors use
sector-local walkstruct pieces so their collision works along the entire curve;
they still share one editable spline. Large models are split into meshes
with at most 500 triangles each. Uncheck the option to return to sector output.

Spline Shape and Geometry: Sector Flags opens the checkbox picker. Apply changes
the selected flags across all spline sectors, including mirrored copies. Choices
persist through regeneration, Save Level, and multiplayer synchronization.

Spline Sector tint accepts red, green, and blue values from 0 to 1 (for example,
0.1 0.2 0.3). Use 0 0 0 for no tint, or blank to inherit sector-group tint.

### Generate destructible walls

Select sectors with weapon 1 (Shift+fire1), press Enter, then choose **Generate → Destructible**. This creates a textured wall model in place, its template, and a placed `gs_destructible.cog` with all references assigned. Sector volumes remain available after the wall is destroyed. Editable splines must be converted to sectors first.

The generator creates 12 irregular Voronoi debris models by default (more when selecting more than 12 sectors). Saber or explosion damage breaks the wall once; ordinary weapon shots do not. Pieces use throwable physics, scatter outward, and disappear after four seconds. The raildet explosion effect has no additional blast damage. All generated models, templates, and the COG are included when saving the level; the script is only packaged when referenced. Generation uses the normal undo and multiplayer approval paths. It requires 14 free custom-template slots by default and supports up to 32 selected sectors, subject to available template capacity.

### Generate a lift

Select one sector with weapon 1 (Shift+fire1), press Enter, then choose **Generate → Lift_Bottom**. The sector becomes a saved 3DO and `_walkstruct` template. Its walking surface starts 0.03125 units above the bottom of the selected sector; frame 1 returns it to its original position. Leave a clear shaft beneath the selected sector, connected by adjoins. The generator retains those sector volumes and opens their adjoins for travel.

The stock `12_trooperelev.cog` is assigned automatically: stepping onto the lift moves it to the other frame, then it returns after a two-second pause. The default speed is 8, with the stock `sm_elev.snd` movement sounds. Edit its speed and delay in Placed COGs, or adjust the two frames in Thing Attributes. Model, template, frames and script are saved with the level. Generation supports undo and multiplayer edits.

**Lift_Top** uses the same generator but starts at the original top position: the first frame is the top and the second is the bottom. Both variants use the same speed, sounds, and return delay.

### Generate conveyor belts

Select one or more surfaces with weapon 1, press Enter, then choose **Generate → Conveyor Belt**. The editor returns to the 3D view with a direction arrow and rotation ring on the first selected face.

Hold Fire2 on an arrow's end circle and move the crosshairs to rotate it in that surface's plane. The direction applies to all selected surfaces, projected into each surface plane. Press **Enter** or **Fire1** to apply all belts together; **Escape** cancels. The operation uses one undo step and normal multiplayer host approval.

Each belt gets the game's `00_conveyor.cog`, with default speed 2. Change its speed in Placed COGs. The script is included when saving. Regenerating a previously generated belt replaces its conveyor assignment instead of stacking scripts; surfaces already controlled by a shared conveyor COG must be removed from that COG first.

### Generate a door

Select one sector with weapon 1, press Enter, then choose **Generate → Door**. Choose sliding or split panels, an opening axis, and an activation mode:

- **Automatic:** opens when a player approaches.
- **Player Activate:** opens when the player activates the door.
- **Wall switch:** returns to the 3D view with a projected switch. Aim at a solid wall outside the door sector and press Fire 1 to cut a square, texture it with `00tcall.mat`, and connect it to the door. Escape cancels the pending door and switch together.
- **Key pickup:** requires a red, blue, or yellow key before activating the door. Place the matching pickup with weapon 3.

The generator creates the panel models, `_walkstruct` templates, two movement frames per panel, door sounds, and a placed `gs_door.cog`. Panels close after two seconds and reopen if blocked while closing. Speed and delay can be changed in Placed COGs. The generated models and script are included in the level GOB. Generation is one undoable edit and uses multiplayer host approval. Leave space for the panels to retract along the chosen axis; convert editable splines first.

### Edit a placed model

With weapon 9 selected, aim at a placed object's visible 3DO and press fire1. Its mesh opens in the 3DO editor; use weapon 2 for materials, or the selection and cleave tools for geometry. Press Escape and save under the existing model name to update all templates and placed instances that share it. Discard preserves the original. Aiming away from objects still starts a new model.

Game/mod models are imported as static rest-pose geometry, without their animation hierarchy or alternate LODs. The edited model is saved inside the level GOB; the original game/mod resource files are unchanged.

The bound third-person camera button also works in the 3D editor view.

Adjoin cleaving: with weapon 0, hold Ctrl to target adjoins, then use fire1
to cleave. Both sides are split together. In map view, select solid or adjoin
surfaces (or carry their selection from 3D), press C or /, and draw a cleave line.

Placed COGs: click a sound parameter to open a searchable sound browser.
Apply your changes, then return to play mode to restart scripts with their
new parameters. Entering edit mode stops sounds from placed COGs and clears
their pending timers; deleting a COG no longer leaves its sound looping.

With weapon 1 in Select mode, select a surface and press L to select its sector. This also works with selected adjoin surfaces and with multiple selected surfaces. Hold Ctrl to target adjoin surfaces when selecting them. Hold Shift and click to select sectors.

Weapon 5 / Placed COGs: Import COG opens a Windows file picker and adds the script to Add COG. Create COG makes a starter script and opens the associated edit application (Notepad fallback). Edit COG lists custom scripts, including mod scripts, and opens a working copy under JK/editor-cogs. Save in the text editor to reload; valid changes preserve matching placed parameter values and restart running placed COGs. Invalid changes retain the last working version. Custom scripts are included when saving the level and shared with multiplayer clients; client changes require host approval. All players should use the current build.

Weapon 2 texture paint: a locked surface has a red border for its painter and a red tint for other players.

### Optional JED Light Baker

Terrain meshes participate in the bake, including shadowing and the plugin's lighting settings. Bake Selected includes terrain faces inside the selected sectors. Results appear immediately and are stored as standard 3DO vertex intensities, with editable lighting data retained for reload, undo, and multiplayer synchronization. Standard JK 3DO vertex lighting is brightness-only; RGB sector lighting remains supported separately. Re-bake after changing terrain geometry or lights.

Install `Light Baker.dll` from [JED Light Baker](https://github.com/LadyEebs/JED-Light-Baker) in `plugins/`. In weapon 4, middle-click and choose **Light Baker**, then press Fire 1 to open the plugin. A successful Bake automatically returns to GeoSith and applies the result; Ctrl+Z undoes it. Messages appear below the plugin controls. Place editor lights before baking. **Lights** mode remains available. **Light Properties** edits the aimed light's type, intensity, range, and RGB color.

The Windows integration uses `GeoSithLightBaker.exe` beside GeoSith to host the unmodified 32-bit DLL. Baked lighting is saved with the level and synchronized to other players, who do not need the plugin. See `plugins/README.txt` for requirements and controls.

In edit mode, the **Force Seeing binding** toggles a full-bright preview. This is a local viewing option; saved lighting is unchanged, and normal lighting returns in play mode.

Custom 3DO lighting: in weapon 9, open the custom 3DO library with middle mouse, select a model, check **Visible to light baker**, and click **Accept**. The optional Light Baker bakes the first placed instance into the shared model's vertex lighting. Other instances use the same baked lighting. Unplaced models are skipped; terrain remains included automatically. The setting and lighting are saved with the level and synchronized in multiplayer.

When adding `thingflags` in weapon 8, the checkboxes start with the base template's flags. For a dirt platform, keep **Standable** and **Level geometry**, enable **Dirt footsteps**, and disable **Metal footsteps**. Saving template flags clears old inherited flag snapshots on placed instances, while preserving overrides that differ from both the old template and its base.

3DO mesh editing controls:
- Weapon 9 + Fire 1 on a modeled thing: edit at its exact position and rotation. The source thing is hidden until the draft is saved or discarded.
- Select tool (weapon 1) in 3DO mode: mouse wheel halves/doubles the grid (0.00390625 to 1 unit). A horizontal world-aligned grid preview appears around the player for 0.5 seconds. The chosen grid also controls model cleaves, extrusion, center movement, and box placement.
- Ctrl + Fire 1: toggle whole-mesh selection. Fire 2 moves selected meshes.
- Ctrl+C: copy selected meshes. Ctrl+V: paste in place, with the copies selected for moving.
- I: enter invert mode (purple wireframe). X, Y, or Z mirrors the selection around its bounding-box center on that world axis. Escape cancels invert mode without leaving the 3DO editor. Mirroring preserves outward face winding and supports undo.

- 3DO Select tool: highlight an edge or vertex and press Delete to dissolve it. The change is accepted only when the remaining faces stay planar and convex without holes; Ctrl+Z undoes it.

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

In **Spline Shape and Geometry**, **Surface Flags...** opens the surface-flag checkboxes for solid faces in the spline, excluding adjoins. Changed flags apply to solid spline surfaces; untouched flags retain their individual values. Click OK, then Apply. The settings persist when reshaping, saving, or synchronizing the spline.

### Hazard sectors

Select one or more sectors, then open **Sector Attributes → Generate → Lava, Acid, or Electricity** to apply the hazard. Lava uses opaque `00fyfl01.mat` boundary faces and a red tint, without underwater movement. Acid uses opaque `00tfuel.mat` boundary faces and a yellow tint, with underwater movement and ambient sound. Both sides of outer adjoins are textured; internal adjoins between selected liquid sectors stay unchanged. Electricity keeps adjoin faces invisible and clears underwater movement. It emits Force Lightning at random positions across solid upward-facing floors, with `forcefieldhit01.wav` at 0.25 volume.

Hazards use a generated `gs_hazard.cog`, included when saving the level. Its **damage** parameter defaults to 20 per second, applied every **interval** (0.5 seconds by default); electricity bolts can also inflict their normal hit damage. These parameters can be changed in Placed COGs. Normal host-authoritative COG execution replicates damage and lightning in multiplayer. Ctrl+Z undoes generation. Editable spline sectors must first be converted to sectors.

### Edge bevels

In the 3D editor, use weapon 1 **Select** to select one or more edges, then press **B**. A yellow wireframe previews the bevels together at one shared width. The **mouse wheel** changes width in half-grid increments; **Fire 1** or **Enter** applies it, and **Escape** cancels. Invalid widths show a red outline and cannot be applied. **Ctrl+Z** undoes the entire batch. Selecting multiple edges of the same adjoin bevels that opening only once.

For solid sector edges, this creates a flat chamfer. If the edge borders an adjoin, the tool bevels the entire opening: it expands the opening on the selected side and creates a tapered connecting sector reaching into the neighboring sector. Coplanar floors, ceilings and walls stay on their shared planes, including sloping surfaces. Newly created faces use world-mapped UVs. Partial-wall openings retain surrounding geometry; some layouts require supporting sector partitions. Convert editable splines to sectors before beveling. Multiplayer uses the normal host-approved edit transaction; previews remain local until confirmed.

### Spiral staircase generator

Select one six-sided sector and open **Sector Attributes > Generate > Spiral Staircase**. The room needs vertical walls and a horizontal floor and ceiling. The button opens an options menu for step count, vertical gap size, rotation and a solid center post. **Accept** opens the 3D wireframe preview; **Cancel** returns without changing the level. A yellow wireframe previews floating stairs around its center, fitted inside the walls. Each step rises **0.0625 units** (one grid unit), with **16 steps per revolution** and slab thickness equal to the rise minus the chosen gap. The default gap is **0** (closed risers); gaps must be less than 0.0625 units. Adding steps continues the spiral upward through additional turns. Air underneath the treads stays open. Checking **Solid center post** omits the central air sectors, leaving a solid post in game. Invisible ramp adjoins have **solid and floor** flags for smooth walking.

- **Mouse wheel:** change the number of steps (1–64, initially 8).
- **Shift + mouse wheel:** rotate around Z in 5-degree increments.
- **P:** toggle a central post (off initially).
- **Fire 1 / Enter:** generate the staircase.
- **Escape:** cancel without changing the level.
- **Ctrl+Z:** undo the complete generation.

The staircase must fit below the existing ceiling with 0.25 units of headroom. Excessive step counts cannot be applied; reduce the count or enlarge the sector. The generated stairs are editable sectors with world-mapped textures. Multiplayer uses the normal host-approved edit transaction; previews stay local until confirmed.

### Pillar / beam between selected faces

With weapon 1 **Select**, select two faces in the same sector and press **/**. This first clips an enclosing rectangular prism aligned between the two face centers and sized to contain both faces. Detailed beam-side cuts are then confined to the retained enclosure, keeping the surrounding partitions cleaner. It preserves the end faces and surrounding volume. Wall-to-floor connections are supported; warped sides use planar facets. It does not delete the resulting beam sector. **Ctrl+Z** undoes the complete operation.

The 3D selection-cleave shortcut is now **/** (also numpad divide) for two vertices, two edges, or two faces. **C** no longer triggers a 3D selection cleave, leaving it available for crouch. Map-view line cleaving accepts C or / (including numpad divide).
