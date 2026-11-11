#ifndef ED_GEOMETRY_H
#define ED_GEOMETRY_H
#include "edStorage.h"
#include <memory>
/* Editor geometry is independent of renderer pointers. All normals point inward.
 * Geometry grows with the document; failed transactions never replace it. */
/* Working storage may exceed the JK format limit during clipping. Only
   surfaces with at most ED_SURFACE_VERTS vertices may be committed/saved. */
#define ED_VERTS 40
#define ED_SURFACE_VERTS 24
#define ED_FACES INT_MAX
#define ED_CELLS INT_MAX
#define ED_EPS 0.00001
/* Legacy JKL coordinates are rounded to six decimals; long edges amplify it. */
#define ED_IMPORT_EPS 0.001
#define ED_GRID 0.0625
#define ED_UV_SCALE 256.0
#define ED_SPAWNS 32
#define ED_SELECTIONS 256
#define ED_OBJECTS (32000-640-32) /* runtime thing table, including gameplay reserve */
#define ED_FRAMES 64
#define ED_LIGHTS INT_MAX
#define ED_EPISODE_ROWS 64
#define ED_COGS (32768-512) /* 15-bit instance IDs, including class-script reserve */
#define ED_COG_PARAMS 128
#define ED_ASSETS INT_MAX
#define ED_MESHES INT_MAX
#define ED_CUSTOM_TEMPLATES 64
#define ED_TEMPLATE_PARAMS 80
typedef struct {char key[32],value[128];} EdTemplateParam;
typedef struct {char name[64],parent[64];int count;EdTemplateParam params[ED_TEMPLATE_PARAMS];} EdTemplate;
typedef struct { double x,y,z; } EdVec;
typedef struct { EdVec point,normal,origin,hit; int valid; double maxDistance; } EdDragTrace;
typedef struct { EdVec p; double u,v,light; } EdVertex;
typedef struct {
    unsigned int surfaceFlags,faceFlags,adjoinFlags;
    int geometryMode,textureMode,lightingMode,portal;
    double uScale,vScale,extraLight,adjoinDistance;
} EdSurfaceAttributes;
typedef struct { int count,material,neighbor,mirror; double uOffset,vOffset; EdVertex v[ED_VERTS]; int hasAttributes;EdSurfaceAttributes attributes; double uvRotation; int customUV; int decalId; } EdFace;
typedef struct { int count; EdBuffer<EdFace> f; double ambientLight,extraLight; unsigned int flags; EdVec tint,thrust; char sound[32]; double soundVolume,tolerance; char colormap[128],layer[64]; int splineId,arrayId; } EdCell;
typedef struct {char name[32];EdVec origin;int saved,terrain;EdVec terrainNormal;int terrainGroup;char paintSlots[2][128];} EdAsset;
#define ED_PAINT_LAYERS 64
typedef struct {char material[128];EdBuffer<double> weights;} EdPaintLayer;
typedef struct {int asset;EdCell geometry;EdBuffer<unsigned char> terrainPinned;EdBuffer<EdVec> terrainNormals;EdBuffer<EdPaintLayer> paint;} EdMesh;
struct EdPaintImage {int width=0,height=0;std::vector<unsigned char> rgba;};
struct EdPaintBake {int size=0;std::vector<unsigned char> mat;std::vector<EdVec> uv;std::vector<uint64_t> faceHashes;std::vector<unsigned char> faceAlpha;int updatedFaces=0;};
extern int (*edPaintLoadImage)(const char*,EdPaintImage*);

typedef struct {EdVec position,rotation;} EdFrame;
typedef struct {int frameCount,hasFlags,pathOverride;unsigned int flags;EdFrame frames[ED_FRAMES];int aiFrames,hasVelocity;EdVec velocity;char aiClass[32];} EdThingAttributes;
typedef struct { EdVec position,rotation; EdThingAttributes attributes;} EdSpawn;
typedef struct {char templateName[64];EdVec position,rotation;EdThingAttributes attributes;} EdObject;
typedef struct {int type;EdVec position;double intensity,range;} EdLight;
typedef struct {int line,cd,level,cutscene,lightPower,darkPower,gotoA,gotoB;char file[32],title[96],intro[20][160];} EdEpisodeRow;
typedef struct {char title[96];int type,count,current;EdEpisodeRow rows[ED_EPISODE_ROWS];} EdEpisode;
enum {ED_COG_FLEX=2,ED_COG_THING=3,ED_COG_TEMPLATE=4,ED_COG_SECTOR=5,ED_COG_SURFACE=6,ED_COG_KEYFRAME=7,ED_COG_SOUND=8,ED_COG_COG=9,ED_COG_MATERIAL=10,ED_COG_VECTOR=11,ED_COG_MODEL=12,ED_COG_AI=13,ED_COG_INT=14};
typedef struct {char name[32],value[32];int type,cell,face;} EdCogParam;
typedef struct {char file[32];int count;EdCogParam params[ED_COG_PARAMS];} EdPlacedCog;
typedef struct {
    char title[96],colormap[128],intro[20][160];
    double gravity,ceilingSkyZ,horizonDistance,horizonPixels;
    double horizonOffset[2],ceilingOffset[2],mip[4],lod[4],perspective,gouraud;
} EdLevelHeader;
#define ED_SPLINES 64
#define ED_SPLINE_POINTS 128
#define ED_SPLINE_SECTORS 256
typedef struct {EdVec position,handle;} EdSplinePoint;
typedef struct {int id,count,sides,segments,material,closed,mirrorAxes,groupId;double width,height;unsigned int geometryHash;EdBuffer<EdSplinePoint> points;int invalid,textureHeight;EdBuffer<EdCell> cutBefore,cutAfter,preview;} EdSpline;
struct EdSectorGroup {char name[64]={};int count=0,direction=2,copies=0;EdBuffer<EdCell> cells;};
struct EdDecalMaterial {char name[32]={};std::shared_ptr<const std::vector<unsigned char>> data;};
typedef struct { int count; EdVec spawn; int spawnCount; EdSpawn spawns[ED_SPAWNS]; EdLevelHeader header; int objectCount;EdBuffer<EdObject> objects;int lightCount;EdBuffer<EdLight> lights;EdEpisode episode;int cogCount;EdBuffer<EdPlacedCog> cogs;int assetCount,meshCount;EdBuffer<EdAsset> assets;EdBuffer<EdMesh> meshes;int templateCount;EdBuffer<EdTemplate> templates;EdBuffer<EdCell> c; int splineCount;EdBuffer<EdSpline> splines;int decalCount;EdBuffer<EdDecalMaterial> decals;int groupCount;EdBuffer<EdSectorGroup> groups; } EdModel;
int edGroupsValidate(const EdModel*);
int edGroupCapture(EdModel*,const EdBuffer<unsigned char>&,const char*);
int edGroupArray(EdModel*,const EdModel*,int,int,int);
int edGroupDelete(EdModel*,int);
int edReadGroupData(EdModel*,const unsigned char*,unsigned int);
EdVec edGroupAxis(int);
int edGroupBounds(const EdSectorGroup*,EdVec*,EdVec*);
EdVec edSplinePosition(const EdSpline *s,double t);
int edSplineUpdate(EdModel *out,const EdModel *source,const EdSpline *spline,int textureHeight=64);
int edSplineRebuild(EdModel *out,const EdModel *source,const EdSpline *spline,int textureHeight=64);
void edSplineReconcile(EdModel *m);
int edSplineConvert(EdModel *m,int id);
int edSplineValidate(const EdModel *m);
void edSplineReadMetadata(EdModel *m,const char *text);
/* Unused sector capacity is not part of a document or its equality. */
int edMergeSectors(EdModel *model,const EdBuffer<unsigned char>& selected,int *merged);
void edCopyModel(EdModel *out,const EdModel *source);
unsigned int edCellHash(const EdCell *cell);
int edCellsEqual(const EdCell *a,const EdCell *b);
int edModelsEqual(const EdModel *a,const EdModel *b);
inline int edCompare(const EdModel* a,const EdModel* b,size_t) {return !edModelsEqual(a,b);}
inline int edCompare(const EdLight* a,const EdLight* b,size_t bytes) {for(size_t i=0;i<bytes/sizeof(EdLight);i++)if(a[i].type!=b[i].type||a[i].position.x!=b[i].position.x||a[i].position.y!=b[i].position.y||a[i].position.z!=b[i].position.z||a[i].intensity!=b[i].intensity||a[i].range!=b[i].range)return 1;return 0;}
inline int edCompare(const EdCell* a,const EdCell* b,size_t) {return !edCellsEqual(a,b);}

typedef struct { int cell,face,vertex,edge; double distance; EdVec point; } EdHit;
typedef struct { int count; EdHit hits[ED_SELECTIONS]; } EdSelection;
int edTerrainCreate(EdModel *out,const EdModel *source,const EdSelection *surfaces,const char *name);
int edTerrainPaint(EdModel*,int,EdVec,double,double,const char*,const char*(*)(int));
int edPaintValid(const EdModel*);
int edDecalsValid(const EdModel*);
int edDecalOutline(const EdFace*,EdVec,double,double,EdFace*);
int edPlaceDecal(EdModel*,EdFace*,EdVec,double,const EdPaintImage*,const EdPaintImage*,double paintStrength=0);
int edDecalImage(const EdModel*,int,EdPaintImage*);
void edCompactDecals(EdModel*);
int edSurfacePaintOnlyChange(const EdModel*,const EdModel*);
int edPaintOnlyChange(const EdModel*,const EdModel*);
unsigned int edPaintHash(const EdModel*,int);
int edPaintBakeAsset(const EdModel*,int,EdPaintBake*,int incremental=0);
int edPainted(const EdModel*,int);
void edPaintFilename(const EdModel*,int,char*);
int edTerrainSculpt(EdModel *m,int asset,EdVec center,double radius,double amount,EdVec normal={});
int edTerrainGrab(EdModel *out,const EdModel *source,int asset,EdVec center,double radius,EdVec delta);
int edTerrainSmooth(EdModel *m,int asset,EdVec center,double radius,double strength);
int edTerrainObject(const EdModel *m,int object);
int edTemplatesValidate(const EdModel *m);
int edTemplateIndex(const EdModel *m,const char *name);
int edMeshValidate(const EdModel *m);
int edMeshPrimitiveDetail(EdModel *m,int asset,int primitive,EdVec center,int material,int detail);
int edMeshPrimitiveTransformedDetail(EdModel *m,int asset,int primitive,EdVec center,int material,double size,double degrees,int detail);
int edMeshPrimitive(EdModel *m,int asset,int primitive,EdVec center,int material);
int edMeshPrimitiveTransformed(EdModel *m,int asset,int primitive,EdVec center,int material,double size,double degrees);
int edMeshRay(const EdModel *m,int asset,EdVec origin,EdVec direction,int handles,EdHit *hit);
int edMeshCleave(EdModel *m,EdHit hit,EdVec a,EdVec b);
int edMeshExtrude(EdModel *m,EdHit hit);
int edMeshMoveConstrained(EdModel *m,const EdModel *source,const EdSelection *selection,EdVec delta,int freeMovement);
int edMeshMove(EdModel *m,const EdModel *source,const EdSelection *selection,EdVec delta);
EdVec edV(double x,double y,double z);
EdVec edAdd(EdVec a,EdVec b);
EdVec edSub(EdVec a,EdVec b);
EdVec edScale(EdVec a,double s);
EdVec edCross(EdVec a,EdVec b);
double edDot(EdVec a,EdVec b);
EdVec edNormal(const EdFace *f);
EdSurfaceAttributes edSurfaceAttributes(const EdFace *face);
EdVec edSnapPoint(EdVec p);
EdVec edSnapFacePoint(const EdFace *face,EdVec p);
EdVec edSnapMeshFacePoint(const EdFace *face,EdVec p);
void edMapFace(EdFace *face);
int edStitchTexture(EdFace *target,const EdFace *source);
void edShiftFaceUV(EdFace *face,EdVec movement);
void edPanFaceTexture(EdFace *face,EdVec movement);
void edPanFaceTextureGrid(EdFace *face,EdVec movement,double grid);
void edRotateFaceTexture(EdFace *face,double degrees,EdVec pivot);
void edRotateTextureDrag(EdFace *face,EdVec start,EdVec movement,int edge);
void edMapWorld(EdModel *model);
int edExtrudeStep(EdModel *model,int cell,int face);
int edExtrusionCap(const EdFace *face,int horizontal,EdFace *cap);
int edExtrudeHorizontal(EdModel *model,int cell,int face);
/* Trace the original grab plane; preserve player travel along its normal. */
EdVec edDragDelta(EdVec startOrigin,EdVec startDirection,EdVec origin,EdVec direction,double depth,const EdVec *normal);
/* Choose the dominant axis in the surface's normal/right/up coordinate frame. */
EdVec edSurfaceDragDelta(EdVec delta,EdVec normal);
void edDefault(EdModel *m);
int edContains(const EdCell *c,EdVec p);
int edValidate(const EdModel *m);
void edClearGeometryError(void);
const char *edGeometryFailure(void);
int edVertexLimitHit(void);
int edSurfaceVertexCountAllowed(int count);
int edRay(const EdModel *m,EdVec origin,EdVec direction,EdHit *hit);
int edRaySelection(const EdModel *m,EdVec origin,EdVec direction,EdHit *hit);
/* Paint visible adjoins while passing through invisible portal faces. */
int edRayMaterial(const EdModel *m,EdVec origin,EdVec direction,EdHit *hit);
double edVertexLight(const EdModel *model,int cell,int face,int vertex);
int edCleave(EdModel *m,int cell,int face,EdVec start,EdVec end);
int edShapePoints(const EdFace *face,EdVec center,double radius,int sides,double degrees,EdVec points[32]);
/* Shared by preview and cleave. Returns 1 for an area intersection, 0 for no
   intersection/invalid input, -1 if clipping exhausts working storage. */
int edClipShape(const EdFace *face,EdVec center,double radius,int sides,double degrees,EdFace *outline);
int edCleaveShape(EdModel *m,int cell,int face,EdVec center,double radius,int sides);
int edCleaveShapeRotated(EdModel *m,int cell,int face,EdVec center,double radius,int sides,double degrees);
int edCleaveSector(EdModel *m,int cell,int face,EdVec start,EdVec end);
int edCleaveSectorsPlane(EdModel *model,const EdBuffer<unsigned char>& selected,EdVec normal,EdVec point);
int edTracePlane(EdVec origin,EdVec direction,EdVec point,EdVec normal,EdVec *hit);
void edDragTraceBegin(EdDragTrace *trace,EdVec origin,EdVec direction,EdVec point,EdVec normal,double maxDistance);
/* Carry the reference plane with player travel so walking past the original
   surface never loses the grab. Ray intersections supply cursor motion. */
EdVec edDragTraceStep(EdDragTrace *trace,EdVec origin,EdVec direction);
EdVec edVertexEdgeDelta(const EdModel *m,EdHit hit,EdVec delta);
int edVertexSlideAxis(const EdModel *m,EdHit hit,EdVec *axis);
int edExtrude(EdModel *m,int cell,int face,EdVec delta);
int edMove(EdModel *m,const EdModel *original,EdHit selection,EdVec delta);
int edMoveSelection(EdModel *m,const EdModel *original,const EdSelection *selection,EdVec delta);
int edScaleSelection(EdModel *out,const EdModel *source,const EdSelection *selection,const EdBuffer<unsigned char>& sectors,double factor,EdVec *pivot);
int edMoveSelectionExact(EdModel *m,const EdModel *original,const EdSelection *selection,EdVec delta);
int edMoveSelectionXY(EdModel *m,const EdModel *original,const EdSelection *selection,EdVec delta);
EdVec edSelectionDeltaXY(const EdModel *original,const EdSelection *selection,EdVec delta);
EdVec edSelectionDeltaXYGrid(const EdModel *original,const EdSelection *selection,EdVec delta,double grid);
EdVec edSelectionDelta(const EdModel *original,const EdSelection *selection,EdVec delta);
int edExtrudeSelection(EdModel *m,const EdSelection *selection);
int edExtrudeSelectionHorizontal(EdModel *m,const EdSelection *selection);
int edSelectionToggle(const EdModel *m,EdSelection *selection,EdHit hit);
void edSelectionRemap(const EdModel *from,const EdModel *to,const EdSelection *selection,EdVec delta,EdSelection *result);
int edResolve(EdModel *m);
int edDeleteSector(EdModel *model,int sector);
/* Dissolve a redundant coplanar edge or vertex, preserving a closed convex
   boundary. Failure leaves the model unchanged. */
int edDeleteHandle(EdModel *model,EdHit hit);
int edCleanupSector(EdModel *model,int sector);
int edStairsDimensions(const EdModel *model,EdHit top,EdHit bottom,double *rise,double *run);
int edCreateStairs(EdModel *model,EdHit top,EdHit bottom,int steps);
const char *edCogTypeName(int type);
int edCogParseScript(EdPlacedCog *cog,const char *filename,const char *text);
int edCogParamValue(const EdModel *model,const EdCogParam *param,char value[32]);
int edCogSetValue(const EdModel *model,EdCogParam *param,const char *value);
void edCogThingChanged(EdModel *model,int index,int inserted);
int edCogDelete(EdModel *model,int index);
#endif
