GeoSith - Windows x64 redistributable

Getting started
1. Extract this entire folder to a writable location.
   Recommended: Documents\GeoSith. Avoid Program Files and Program Files (x86),
   where Windows normally blocks the editor from writing its catalogs and levels.
2. Install your own copy of Jedi Knight: Dark Forces II. GeoSith does not
   include Jedi Knight game assets.
3. Run "Setup GeoSith.cmd" and enter the folder containing your game's
   Episode and Resource folders. You can also copy your own installation
   into the supplied JK folder. Use a writable game folder: setup generates
   editor catalogs there, and new levels are saved in its Episode folder.
4. Run "Launch GeoSith.cmd". Select Create New Level, then press F8 in game
   to enter the editor. See EDITOR-GUIDE.md for controls and tools.

Setup uses Windows PowerShell and does not require Python, Visual Studio,
or access to the original developer's project. The supplied DLLs belong
beside GeoSith.exe. Keep the licenses and source archives with this package.

The required original data includes JK1.GOB and JK1MP.GOB in Episode,
Res1.gob (or Res1hi.gob) and Res2.gob in Resource, plus your JK_.CD marker.
Do not copy Jedi Knight executable files over GeoSith.exe.

If setup reports Access denied, move the entire GeoSith folder and its JK
subfolder to Documents\GeoSith, then run Setup GeoSith.cmd again. If Windows
offers to replace existing files while copying the game, keep GeoSith's supplied
files. Clear Read-only on generated editor catalogs if necessary. GeoSith does
not need to run as administrator when its folders are writable.

Contents
- GeoSith.exe: the editor integrated with OpenJKDF2.
- OpenAL32.dll: audio runtime.
- GameNetworkingSockets.dll: multiplayer runtime.
- Visual C++ runtime DLLs: app-local Windows x64 runtime dependencies.
- Setup / Launch GeoSith: first-run preparation and launcher.
- licenses: engine and dependency notices.
- source: corresponding project/dependency source snapshots and build notes.

Jedi Knight resources, generated catalogs/templates, saved levels, player
profiles, developer paths, and debug symbols are not part of this package.
They are supplied or generated locally by each user after setup.

Multiplayer setup and troubleshooting: see NETWORKING.md. Both players need the
same GeoSith release and matching game assets. Internet hosting may require UDP
port forwarding or a shared VPN if automatic router mapping is unavailable.

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
- Multiplayer participants must use this updated build (editor protocol 22).

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
  sculpt/smooth intensity (1-100%, default 25%); middle mouse also offers a
  numeric intensity setting. Terrain uses a shared tiled UV map, recalculated
  after edits to reduce stretching and keep linked seams aligned.
- Middle mouse opens texture selection and conversion options. Texture changes
  apply to the whole terrain. Convert to 3DO keeps a model in the custom library;
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

Surface texture painting in multiplayer (weapon 2): the first brush stroke locks
the surface to you. You see a red border; other editors see a red tint and cannot
change its texture. Strokes stay local until you press Escape, change weapons or
texture mode, or begin painting another surface. The completed painting is sent
once and becomes one shared undo step.
