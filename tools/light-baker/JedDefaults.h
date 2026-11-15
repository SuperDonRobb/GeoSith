// Compatibility stubs for the JED 0.951 ABI. Implemented operations override these below.
#pragma once
#include "JedAbi.h"
struct IJEDLevelDefaults : IJEDLevel {
HRESULT __stdcall QueryInterface(REFIID,void **out) override {if(out)*out=nullptr;return E_NOINTERFACE;}
ULONG __stdcall AddRef() override {return 1;}
ULONG __stdcall Release() override {return 1;}
HRESULT __stdcall GetLevelHeader(tlevelheader* lh, int flags) override {return E_NOTIMPL;}
HRESULT __stdcall SetLevelHeader(tlevelheader* lh, int flags) override {return E_NOTIMPL;}
int __stdcall NSectors() override {return 0;}
int __stdcall NThings() override {return 0;}
int __stdcall NLights() override {return 0;}
int __stdcall NCOgs() override {return 0;}
int __stdcall AddSector() override {return -1;}
HRESULT __stdcall DeleteSector(int n) override {return E_NOTIMPL;}
HRESULT __stdcall GetSector(int sec, tjedsectorrec* rec, int flags) override {return E_NOTIMPL;}
HRESULT __stdcall SetSector(int sec, tjedsectorrec* rec, int flags) override {return E_NOTIMPL;}
int __stdcall SectorNVertices(int sec) override {return -1;}
int __stdcall SectorNSurfaces(int sec) override {return -1;}
HRESULT __stdcall SectorGetVertex(int sec, int , double* x, double* , double* ) override {return E_NOTIMPL;}
HRESULT __stdcall SectorSetVertex(int sec, int , double x, double , double ) override {return E_NOTIMPL;}
int __stdcall SectorAddVertex(int sec, double x, double , double ) override {return -1;}
int __stdcall SectorFindVertex(int sec, double x, double , double ) override {return -1;}
int __stdcall SectorDeleteVertex(int sec, int n) override {return -1;}
int __stdcall SectorAddSurface(int sec) override {return -1;}
HRESULT __stdcall SectorDeleteSurface(int sc, int ) override {return E_NOTIMPL;}
HRESULT __stdcall SectorUpdate(int sec) override {return E_NOTIMPL;}
HRESULT __stdcall GetSurface(int sc, int , tjedsurfacerec* rec, int flags) override {return E_NOTIMPL;}
HRESULT __stdcall SetSurface(int sc, int , tjedsurfacerec* rec, int flags) override {return E_NOTIMPL;}
HRESULT __stdcall GetSurfaceNormal(int sc, int , tjedvector* n) override {return E_NOTIMPL;}
HRESULT __stdcall SurfaceUpdate(int sc, int , int how) override {return E_NOTIMPL;}
int __stdcall SurfaceNVertices(int sc, int ) override {return -1;}
int __stdcall SurfaceGetVertexNum(int sc, int , int ) override {return -1;}
HRESULT __stdcall SurfaceSetVertexNum(int sc, int , int , int secvx) override {return E_NOTIMPL;}
int __stdcall SurfaceAddVertex(int sc, int , int secvx) override {return -1;}
int __stdcall SurfaceInsertVertex(int sc, int , int at, int secvx) override {return -1;}
HRESULT __stdcall SurfaceDeleteVertex(int sc, int , int n) override {return E_NOTIMPL;}
HRESULT __stdcall SurfaceGetVertexUV(int sc, int , int , float* u, float* ) override {return E_NOTIMPL;}
HRESULT __stdcall SurfaceSetVertexUV(int sc, int , int , float u, float ) override {return E_NOTIMPL;}
HRESULT __stdcall SurfaceGetVertexLight(int sc, int , int , float* white, float* , float* , float* ) override {return E_NOTIMPL;}
HRESULT __stdcall SurfaceSetVertexLight(int sc, int , int , float white, float , float , float ) override {return E_NOTIMPL;}
int __stdcall AddThing() override {return -1;}
HRESULT __stdcall DeleteThing(int th) override {return E_NOTIMPL;}
HRESULT __stdcall GetThing(int th, tjedthingrec* rec, int flags) override {return E_NOTIMPL;}
HRESULT __stdcall SetThing(int th, tjedthingrec* rec, int flags) override {return E_NOTIMPL;}
HRESULT __stdcall ThingUpdate(int th) override {return E_NOTIMPL;}
int __stdcall AddLight() override {return -1;}
HRESULT __stdcall DeleteLight(int lt) override {return E_NOTIMPL;}
HRESULT __stdcall GetLight(int lt, tjedlightrec* rec, int flags) override {return E_NOTIMPL;}
HRESULT __stdcall SetLight(int lt, tjedlightrec* rec, int flags) override {return E_NOTIMPL;}
int __stdcall NLayers() override {return 0;}
char* __stdcall GetLayerName(int n) override {return nullptr;}
int __stdcall AddLayer(char* name) override {return -1;}
int __stdcall NTHingValues(int th) override {return 0;}
char* __stdcall GetThingValueName(int th, int ) override {return nullptr;}
char* __stdcall GetThingValueData(int th, int ) override {return nullptr;}
HRESULT __stdcall SetThingValueData(int th, int , char* val) override {return E_NOTIMPL;}
HRESULT __stdcall GetThingFrame(int th, int , double* x, double* , double* , double* , double* , double* ) override {return E_NOTIMPL;}
HRESULT __stdcall SetThingFrame(int th, int , double x, double , double , double , double , double ) override {return E_NOTIMPL;}
int __stdcall AddThingValue(int th, char* name, char* ) override {return -1;}
HRESULT __stdcall InsertThingValue(int th, int , char* name, char* ) override {return E_NOTIMPL;}
HRESULT __stdcall DeleteThingValue(int th, int ) override {return E_NOTIMPL;}
int __stdcall AddCOG(char* name) override {return -1;}
HRESULT __stdcall DeleteCOG(int n) override {return E_NOTIMPL;}
HRESULT __stdcall UpdateCOG(int cg) override {return E_NOTIMPL;}
char* __stdcall GetCOGFileName(int cg) override {return nullptr;}
int __stdcall NCOGValues(int cg) override {return 0;}
char* __stdcall GetCOGValueName(int cg, int ) override {return nullptr;}
int __stdcall GetCOGValueType(int cg, int ) override {return -1;}
char* __stdcall GetCOGValue(int cg, int ) override {return nullptr;}
bool __stdcall SetCOGValue(int cg, int , char* val) override {return false;}
int __stdcall AddCOGValue(int cg, char* name, char* , int vtype) override {return -1;}
HRESULT __stdcall InsertCOGValue(int cg, int , char* name, char* , int vtype) override {return E_NOTIMPL;}
HRESULT __stdcall DeleteCOGValue(int cg, int ) override {return E_NOTIMPL;}
};
struct IJEDDefaults : IJED {
HRESULT __stdcall QueryInterface(REFIID,void **out) override {if(out)*out=nullptr;return E_NOINTERFACE;}
ULONG __stdcall AddRef() override {return 1;}
ULONG __stdcall Release() override {return 1;}
double __stdcall GetVersion() override {return 0;}
IJEDLevel * __stdcall GetLevel() override {return nullptr;}
int __stdcall GetMapMode() override {return -1;}
HRESULT __stdcall SetMapMode(int mode) override {return E_NOTIMPL;}
int __stdcall GetCurSC() override {return -1;}
HRESULT __stdcall SetCurSC(int sc) override {return E_NOTIMPL;}
int __stdcall GetCurTH() override {return -1;}
HRESULT __stdcall SetCurTH(int th) override {return E_NOTIMPL;}
int __stdcall GetCurLT() override {return -1;}
HRESULT __stdcall SetCurLT(int lt) override {return E_NOTIMPL;}
HRESULT __stdcall GetCurVX(int* sc, int* ) override {return E_NOTIMPL;}
HRESULT __stdcall SetCurVX(int sc, int ) override {return E_NOTIMPL;}
HRESULT __stdcall GetCurSF(int* sc, int* ) override {return E_NOTIMPL;}
HRESULT __stdcall SetCurSF(int sc, int ) override {return E_NOTIMPL;}
HRESULT __stdcall GetCurED(int* sc, int* , int* ) override {return E_NOTIMPL;}
HRESULT __stdcall SetCurED(int sc, int , int ) override {return E_NOTIMPL;}
HRESULT __stdcall GetCurFR(int* th, int* ) override {return E_NOTIMPL;}
HRESULT __stdcall SetCurFR(int th, int ) override {return E_NOTIMPL;}
HRESULT __stdcall NewLevel(bool mots) override {return E_NOTIMPL;}
HRESULT __stdcall LoadLevel(char* name) override {return E_NOTIMPL;}
HRESULT __stdcall FindBBox(int sec, tjedbox* box) override {return E_NOTIMPL;}
HRESULT __stdcall FindBoundingSphere(int sec, double* cx, double* , double* , double* ) override {return E_NOTIMPL;}
bool __stdcall FindCollideBox(int sec, tjedbox* bbox, double cx, double , double , tjedbox* cbox) override {return false;}
HRESULT __stdcall FindSurfaceCenter(int sc, int , double* cx, double* , double* ) override {return E_NOTIMPL;}
HRESULT __stdcall RotateVector(tjedvector* vec, double pch, double , double ) override {return E_NOTIMPL;}
HRESULT __stdcall RotatePoint(double ax1, double , double , double , double , double , double angle, double* x, double* , double* ) override {return E_NOTIMPL;}
HRESULT __stdcall GetJKPYR(tjedvector* x, tjedvector* , tjedvector* , double* pch, double* , double* ) override {return E_NOTIMPL;}
bool __stdcall IsSurfaceConvex(int sc, int ) override {return false;}
bool __stdcall IsSurfacePlanar(int sc, int ) override {return false;}
bool __stdcall IsSectorConvex(int sec) override {return false;}
bool __stdcall IsInSector(int sec, double x, double , double ) override {return false;}
bool __stdcall DoSectorsOverlap(int sec1, int ) override {return false;}
bool __stdcall IsPointOnSurface(int sc, int , double x, double , double ) override {return false;}
int __stdcall FindSectorForThing(int th) override {return -1;}
int __stdcall FindSectorForXYZ(double x, double , double ) override {return -1;}
int __stdcall ExtrudeSurface(int sc, int , double by) override {return -1;}
int __stdcall CleaveSurface(int sc, int , tjedvector* cnormal, double cx, double , double ) override {return -1;}
int __stdcall CleaveSector(int sec, tjedvector* cnormal, double cx, double , double ) override {return -1;}
bool __stdcall CleaveEdge(int sc, int , int , tjedvector* cnormal, double cx, double , double ) override {return false;}
bool __stdcall JoinSurfaces(int sc1, int , int , int ) override {return false;}
bool __stdcall PlanarizeSurface(int sc, int ) override {return false;}
int __stdcall MergeSurfaces(int sc, int , int ) override {return -1;}
int __stdcall MergeSectors(int sec1, int ) override {return -1;}
HRESULT __stdcall CalculateDefaultUVNormals(int sc, int , int orgvx, tjedvector* un, tjedvector* ) override {return E_NOTIMPL;}
HRESULT __stdcall CalcUVNormals(int sc, int , tjedvector* un, tjedvector* ) override {return E_NOTIMPL;}
HRESULT __stdcall ArrangeTexture(int sc, int , int orgvx, tjedvector* un, tjedvector* ) override {return E_NOTIMPL;}
HRESULT __stdcall ArrangeTextureBy(int sc, int , tjedvector* un, tjedvector* , double refx, double , double , double , double ) override {return E_NOTIMPL;}
bool __stdcall IsTextureFlipped(int sc, int ) override {return false;}
HRESULT __stdcall RemoveSurfaceReferences(int sc, int ) override {return E_NOTIMPL;}
HRESULT __stdcall RemoveSectorReferences(int sec, bool surfs) override {return E_NOTIMPL;}
bool __stdcall StitchSurfaces(int sc1, int , int , int ) override {return false;}
bool __stdcall FindCommonEdges(int sc1, int , int , int , int* v11, int* , int* , int* ) override {return false;}
bool __stdcall DoSurfacesOverlap(int sc1, int , int , int ) override {return false;}
bool __stdcall MakeAdjoin(int sc, int ) override {return false;}
bool __stdcall MakeAdjoinFromSectorUp(int sc, int , int firstsc) override {return false;}
bool __stdcall UnAdjoin(int sc, int ) override {return false;}
int __stdcall CreateCubicSector(double x, double , double , tjedvector* pnormal, tjedvector* ) override {return -1;}
HRESULT __stdcall StartUndo(char* name) override {return E_NOTIMPL;}
HRESULT __stdcall SaveUndoForThing(int n, int change) override {return E_NOTIMPL;}
HRESULT __stdcall SaveUndoForLight(int n, int change) override {return E_NOTIMPL;}
HRESULT __stdcall SaveUndoForSector(int n, int change, int whatpart) override {return E_NOTIMPL;}
HRESULT __stdcall ClearUndoBuffer() override {return E_NOTIMPL;}
HRESULT __stdcall ApplyUndo() override {return E_NOTIMPL;}
int __stdcall GetApplicationHandle() override {return -1;}
bool __stdcall JoinSectors(int sec1, int ) override {return false;}
int __stdcall GetNMultiselected(int what) override {return -1;}
HRESULT __stdcall ClearMultiselection(int what) override {return E_NOTIMPL;}
HRESULT __stdcall RemoveFromMultiselection(int what, int ) override {return E_NOTIMPL;}
int __stdcall GetSelectedSC(int n) override {return -1;}
int __stdcall GetSelectedTH(int n) override {return -1;}
int __stdcall GetSelectedLT(int n) override {return -1;}
HRESULT __stdcall GetSelectedSF(int n, int* sc, int* ) override {return E_NOTIMPL;}
HRESULT __stdcall GetSelectedED(int n, int* sc, int* , int* ) override {return E_NOTIMPL;}
HRESULT __stdcall GetSelectedVX(int n, int* sc, int* ) override {return E_NOTIMPL;}
HRESULT __stdcall GetSelectedFR(int n, int* th, int* ) override {return E_NOTIMPL;}
int __stdcall SelectSC(int sc) override {return -1;}
int __stdcall SelectSF(int sc, int ) override {return -1;}
int __stdcall SelectED(int sc, int , int ) override {return -1;}
int __stdcall SelectVX(int sc, int ) override {return -1;}
int __stdcall SelectTH(int th) override {return -1;}
int __stdcall SelectFR(int th, int ) override {return -1;}
int __stdcall SelectLT(int lt) override {return -1;}
int __stdcall FindSelectedSC(int sc) override {return -1;}
int __stdcall FindSelectedSF(int sc, int ) override {return -1;}
int __stdcall FindSelectedED(int sc, int , int ) override {return -1;}
int __stdcall FindSelectedVX(int sc, int ) override {return -1;}
int __stdcall FindSelectedTH(int th) override {return -1;}
int __stdcall FindSelectedFR(int th, int ) override {return -1;}
int __stdcall FindSelectedLT(int lt) override {return -1;}
HRESULT __stdcall SaveJED(char* name) override {return E_NOTIMPL;}
HRESULT __stdcall SaveJKL(char* name) override {return E_NOTIMPL;}
HRESULT __stdcall UpdateMap() override {return E_NOTIMPL;}
HRESULT __stdcall SetPickerCMP(char* cmp) override {return E_NOTIMPL;}
char* __stdcall PickResource(int what, char* cur) override {return nullptr;}
long __stdcall EditFlags(int what, long flags) override {return -1;}
HRESULT __stdcall ReloadTemplates() override {return E_NOTIMPL;}
HRESULT __stdcall PanMessage(int mtype, char* msg) override {return E_NOTIMPL;}
HRESULT __stdcall SendKey(int shift, int key) override {return E_NOTIMPL;}
bool __stdcall ExecuteMenu(char* itemref) override {return false;}
void * __stdcall GetJEDSetting(char* name) override {return nullptr;}
bool __stdcall IsLayerVisible(int n) override {return false;}
HRESULT __stdcall CheckConsistencyErrors() override {return E_NOTIMPL;}
HRESULT __stdcall CheckResources() override {return E_NOTIMPL;}
int __stdcall NConsistencyErrors() override {return 0;}
char* __stdcall GetConsErrorString(int n) override {return nullptr;}
int __stdcall GetConsErrorType(int n) override {return -1;}
int __stdcall GetConsErrorSector(int n) override {return -1;}
int __stdcall GetConsErrorSurface(int n) override {return -1;}
int __stdcall GetConsErrorThing(int n) override {return -1;}
int __stdcall GetConsErrorCog(int n) override {return -1;}
bool __stdcall IsPreviewActive() override {return false;}
char* __stdcall GetJEDString(int what) override {return nullptr;}
bool __stdcall GetIsMOTS() override {return false;}
HRESULT __stdcall SetIsMOTS(bool mots) override {return E_NOTIMPL;}
int __stdcall GetJedWindow(int whichone) override {return -1;}
char* __stdcall FileExtractExt(char* path) override {return nullptr;}
char* __stdcall FileExtractPath(char* path) override {return nullptr;}
char* __stdcall FileExtractName(char* path) override {return nullptr;}
char* __stdcall FindProjectDirFile(char* name) override {return nullptr;}
bool __stdcall IsFileContainer(char* path) override {return false;}
bool __stdcall IsFileInContainer(char* path) override {return false;}
char* __stdcall FileOpenDialog(char* fname, char* filter) override {return nullptr;}
int __stdcall OpenFile(char* name) override {return -1;}
int __stdcall OpenGameFile(char* name) override {return -1;}
int __stdcall ReadFile(int handle, void* buf, long size) override {return -1;}
HRESULT __stdcall SetFilePos(int handle, long pos) override {return E_NOTIMPL;}
long __stdcall GetFilePos(int handle) override {return -1;}
long __stdcall GetFileSize(int handle) override {return -1;}
char* __stdcall GetFileName(int handle) override {return nullptr;}
HRESULT __stdcall CloseFile(int handle) override {return E_NOTIMPL;}
int __stdcall OpenTextFile(char* name) override {return -1;}
int __stdcall OpenGameTextFile(char* name) override {return -1;}
char* __stdcall GetTextFileName(int handle) override {return nullptr;}
char* __stdcall ReadTextFileString(int handle) override {return nullptr;}
bool __stdcall TextFileEof(int handle) override {return false;}
int __stdcall TextFileCurrentLine(int handle) override {return -1;}
HRESULT __stdcall CloseTextFile(int handle) override {return E_NOTIMPL;}
int __stdcall OpenGOB(char* name) override {return -1;}
int __stdcall GOBNFiles(int handle) override {return -1;}
char* __stdcall GOBNFileName(int handle, int n) override {return nullptr;}
char* __stdcall GOBNFullFileName(int handle, int n) override {return nullptr;}
long __stdcall GOBGetFileSize(int handle, int n) override {return -1;}
long __stdcall GOBGetFileOffset(int handle, int n) override {return -1;}
HRESULT __stdcall CloseGOB(int handle) override {return E_NOTIMPL;}
IJEDWFRenderer * __stdcall CreateWFRenderer(int wnd, int whichone) override {return nullptr;}
};
