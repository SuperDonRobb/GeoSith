static int materialDecalMode;
static double materialDecalSize=.5;
static int materialPaintStroke;
static unsigned materialPaintTime;
static void line(EdVec,EdVec,int);
static void materialWheel(int wheel,int shift,int ctrl){
    if(!wheel)return;
    if(shift&&paletteCount){
        int direction=wheel>0?1:-1,steps=abs(wheel%paletteCount);
        for(int step=0;step<steps;step++)for(int scanned=0;scanned<paletteCount;scanned++){
            currentMaterial=(currentMaterial+direction+paletteCount)%paletteCount;
            if(materialBrowsable(currentMaterial))break;
        }
    }
    else if(ctrl&&materialDecalMode)materialDecalSize=fmax(ED_GRID/4,fmin(128,materialDecalSize*pow(1.2,wheel>0?1:-1)));
    else if(!ctrl&&!grabbing){terrainSessionFinish();materialDecalMode=(materialDecalMode+(wheel>0?1:2))%3;say(materialDecalMode==2?"Texture Paint: hold Fire 1 to paint; Ctrl+wheel: brush size.":materialDecalMode?"Decal mode. Fire 1 places; Shift+wheel: texture; Ctrl+wheel: size.":"Texture mode. Shift+wheel: texture; Fire 1 applies.");}
}
static int placeMaterialDecal(EdHit hit,int mesh){
    if(hit.cell<0||netEditPending())return 0;
    if(mesh&&edPainted(model,model->meshes[hit.cell].asset)){say("Use terrain texture painting on painted terrain models.");return 0;}
    const EdFace *face=mesh?&model->meshes[hit.cell].geometry.f[hit.face]:&model->c[hit.cell].f[hit.face];EdPaintImage base,decal;
    if(!(face->decalId?edDecalImage(model,face->decalId,&base):terrainLoadImage(paletteNames[face->material],&base))||!terrainLoadImage(paletteNames[currentMaterial],&decal)){say("Could not load the decal or surface texture.");return 0;}
    edCopyModel(candidate,model);EdFace *target=mesh?&candidate->meshes[hit.cell].geometry.f[hit.face]:&candidate->c[hit.cell].f[hit.face];
    if(!edPlaceDecal(candidate,target,hit.point,materialDecalSize,&base,&decal)){say("Decal could not be placed: empty image, no surface overlap, or texture storage limit.");return 0;}
    remember();if(!commit())return 0;if(!netClient())say("Decal baked onto surface. Ctrl+Z: undo.");return 1;
}
// Only whole-face picks restrict painting; vertex and edge selections do not
// implicitly select the surfaces that happen to contain those handles.
static int materialPaintAllowed(EdHit hit){
    if(hit.cell<0||hit.cell>=model->count||hit.face<0||hit.face>=model->c[hit.cell].count)return 0;
    int hasSurfaces=0;
    for(int i=0;i<picks.count;i++){
        EdHit selected=picks.hits[i];
        if(selected.vertex>=0||selected.edge>=0||selected.cell<0||selected.cell>=model->count||selected.face<0||selected.face>=model->c[selected.cell].count)continue;
        hasSurfaces=1;if(selected.cell==hit.cell&&selected.face==hit.face)return 1;
    }
    return !hasSurfaces;
}
static void materialPaintControl(EdHit hit,int fire,int grab){
    if(materialPaintStroke&&!grabbing)materialPaintStroke=0;
    if(!fire||grab){if(materialPaintStroke)grabbing=0;materialPaintStroke=0;return;}
    if(!materialPaintAllowed(hit)||netEditPending())return;
    if(!surfaceSessionBegin(hit)){materialPaintStroke=grabbing=0;return;}
    unsigned now=stdPlatform_GetTimeMsec();if(materialPaintStroke&&now-materialPaintTime<80)return;
    const EdFace *face=&model->c[hit.cell].f[hit.face];EdPaintImage base,paint;
    if(!(face->decalId?edDecalImage(model,face->decalId,&base):terrainLoadImage(paletteNames[face->material],&base))||!terrainLoadImage(paletteNames[currentMaterial],&paint))return;
    edCopyModel(candidate,model);
    if(!edPlaceDecal(candidate,&candidate->c[hit.cell].f[hit.face],hit.point,materialDecalSize,&base,&paint,.35))return;
    if(!materialPaintStroke)remember();
    if(commit()){materialPaintStroke=grabbing=1;selection=hit;materialPaintTime=now;}
}
static void drawSurfaceLocks(int color){
    EdHit hit={};if(!surfaceSessionOwnedHit(&hit))return;
    const EdFace &face=model->c[hit.cell].f[hit.face];EdVec offset=edScale(edNormal(&face),.003);for(int v=0;v<face.count;v++)line(edAdd(face.v[v].p,offset),edAdd(face.v[(v+1)%face.count].p,offset),color);
}
static void drawMaterialPaintBrush(int color){
    if(tool!=2||materialDecalMode!=2||!materialPaintAllowed(hover))return;
    const EdFace *face=&model->c[hover.cell].f[hover.face];EdVec n=edNormal(face),u=edCross(fabs(n.z)<.9?edV(0,0,1):edV(0,1,0),n);u=edScale(u,1/sqrt(edDot(u,u)));EdVec v=edCross(n,u);
    for(int ring=0;ring<2;ring++){
        EdVec previous={};int previousValid=0;
        for(int i=0;i<=64;i++){
            double angle=i*6.283185307179586/64,radius=materialDecalSize*(ring?.25:.5);
            EdVec p=edAdd(hover.point,edAdd(edScale(u,cos(angle)*radius),edScale(v,sin(angle)*radius)));int valid=1;
            for(int e=0;e<face->count;e++){EdVec edge=edSub(face->v[(e+1)%face->count].p,face->v[e].p);if(edDot(edSub(p,face->v[e].p),edCross(edge,n))>1e-7){valid=0;break;}}
            p=edAdd(p,edScale(n,.003));if(valid&&previousValid)line(previous,p,color);previous=p;previousValid=valid;
        }
    }
}
static void drawMaterialDecal(const EdFace *surface,EdVec point){
    if(tool!=2||materialDecalMode!=1||grabbing)return;rdMaterial *material=editorMaterial(currentMaterial);
    if(!material||!rdMaterial_EnsureDataForced(material)||!material->num_texinfo||!material->texinfos[0])return;
    rdTexinfo *info=material->texinfos[0];tVBuffer *buffer=info->texture_ptr?info->texture_ptr->texture_struct[0]:NULL;int width=buffer?buffer->format.width:1,height=buffer?buffer->format.height:1;
    EdFace outline;if(!edDecalOutline(surface,point,materialDecalSize,(double)height/width,&outline))return;
    rdVector3 positions[ED_VERTS]={},normals[ED_VERTS]={};rdVector2 uv[ED_VERTS]={};flex_t lights[ED_VERTS]={},dynamic[ED_VERTS]={};int indices[ED_VERTS]={};
    rdFace face={};face.numVertices=outline.count;face.vertexPosIdx=face.vertexUVIdx=indices;face.material=material;face.wallCel=-1;face.geometryMode=RD_GEOMETRY_FULL;face.lightingMode=RD_LIGHTMODE_FULLYLIT;face.textureMode=RD_TEXTUREMODE_PERSPECTIVE;face.type=1|32;face.normal=rdvec(edNormal(surface));
    double radius=0;for(int i=0;i<outline.count;i++){EdVec p=edSub(outline.v[i].p,point);positions[i]=rdvec(p);normals[i]=face.normal;uv[i]={(flex_t)(outline.v[i].u*(std::min)(width,256)),(flex_t)(outline.v[i].v*(std::min)(height,256))};indices[i]=i;radius=fmax(radius,sqrt(edDot(p,p)));}
    rdMesh mesh={};mesh.geometryMode=RD_GEOMETRY_FULL;mesh.lightingMode=RD_LIGHTMODE_FULLYLIT;mesh.textureMode=RD_TEXTUREMODE_PERSPECTIVE;mesh.numVertices=mesh.numUVs=outline.count;mesh.numFaces=1;mesh.aVertices=positions;mesh.aTexVerticies=uv;mesh.vertexNormals=normals;mesh.vertices_i=lights;mesh.vertices_unk=dynamic;mesh.faces=&face;mesh.radius=radius;
    rdHierarchyNode node={};rdModel3 preview={};preview.numGeos=preview.numHNodes=1;preview.aGeos[0].numMeshes=1;preview.aGeos[0].aMeshes=&mesh;preview.aHierarchyNodes=&node;preview.radius=radius;
    rdThing thing={};rdThing_NewEntry(&thing,NULL);if(rdThing_SetModel3(&thing,&preview)){thing.desiredLightMode=thing.curLightMode=RD_LIGHTMODE_FULLYLIT;rdMatrix34 transform;rdMatrix_Identity34(&transform);transform.scale=rdvec(edAdd(point,edScale(edNormal(surface),.001)));rdCache_Flush();rdThing_Draw(&thing,&transform);rdCache_Flush();}rdThing_FreeEntry(&thing);
}

static int decalSelfTest(void){
    clearHistory();tool=2;materialDecalMode=0;grabbing=0;
    materialWheel(1,0,0);if(!materialDecalMode)return 0;
    double size=materialDecalSize;materialWheel(1,0,1);if(materialDecalSize<=size)return 0;
    int material=currentMaterial;materialWheel(1,1,0);if(paletteCount>1&&currentMaterial==material)return 0;
    if(!edRay(model,edV(0,0,.5),edV(0,1,0),&hover))return 0;
    EdHit hit=hover;EdModel before=*model;if(!placeMaterialDecal(hit,0)||!model->decalCount)return 0;
    EdModel after=*model;if(!historyStep(0)||!edModelsEqual(model,&before)||!historyStep(1)||!edModelsEqual(model,&after))return 0;
    rdMaterial *mat=editorFaceMaterial(model,&model->c[hit.cell].f[hit.face]);
    if(!mat||!rdMaterial_EnsureDataForced(mat)||!mat->textures||!mat->textures[0].texture_struct[0]||mat->textures[0].texture_struct[0]->format.format.bpp!=16)return 0;
    if(getenv("GEOSITH_SURFACE_PAINT_TEST")){
        materialWheel(1,0,0);if(materialDecalMode!=2)return 0;
        EdModel unpainted=*model;SithSector *sectors=sithWorld_g_pCurrentWorld->aSectors;int undos=undoCount;
        EdHit excluded={},second={};int restricted=getenv("GEOSITH_PAINT_SELECTION_TEST")!=NULL;
        if(restricted){
            if(!edRay(model,edV(0,0,.5),edV(0,0,-1),&second)||!edRay(model,edV(0,0,.5),edV(1,0,0),&excluded))return 0;
            picks.count=2;picks.hits[0]=hit;picks.hits[1]=second;for(int i=0;i<2;i++)picks.hits[i].vertex=picks.hits[i].edge=-1;
            if(!materialPaintAllowed(hit)||!materialPaintAllowed(second)||materialPaintAllowed(excluded))return 0;
            materialPaintControl(excluded,1,0);if(materialPaintStroke||grabbing||undoCount!=undos||!edModelsEqual(&unpainted,model))return 0;
        }
        materialPaintControl(hit,1,0);
        if(restricted){EdModel during=*model;int count=undoCount;materialPaintTime-=100;materialPaintControl(excluded,1,0);if(undoCount!=count||!edModelsEqual(&during,model))return 0;}
        materialPaintTime-=100;materialPaintControl(hit,1,0);materialPaintControl(hit,0,0);
        if(restricted){picks.count=0;if(!materialPaintAllowed(excluded))return 0;puts("GEOSITH_PAINT_SELECTION PASS: multiple selected faces allowed, unselected face cannot begin or extend a stroke, clearing selection removes restriction");}
        if(materialPaintStroke||grabbing||undoCount!=undos+1||sectors!=sithWorld_g_pCurrentWorld->aSectors||edModelsEqual(&unpainted,model))return 0;
        EdModel painted=*model;if(!historyStep(0)||!edModelsEqual(model,&unpainted)||!historyStep(1)||!edModelsEqual(model,&painted)||sectors!=sithWorld_g_pCurrentWorld->aSectors)return 0;
        puts("GEOSITH_SURFACE_PAINT PASS: held stroke, grouped undo, redo and stable live sector storage");
    }
    if(getenv("GEOSITH_PAINT_UV_RESET_TEST")){
        EdModel painted=*model;EdFace expected=model->c[hit.cell].f[hit.face];expected.customUV=0;expected.uOffset=expected.vOffset=expected.uvRotation=0;edMapFace(&expected);
        remember();edCopyModel(candidate,model);applyFaceMaterial(&candidate->c[hit.cell].f[hit.face],currentMaterial);edCompactDecals(candidate);if(!commit())return 0;
        const EdFace &reset=model->c[hit.cell].f[hit.face];if(reset.customUV||reset.decalId)return 0;
        for(int i=0;i<reset.count;i++)if(fabs(reset.v[i].u-expected.v[i].u)>1e-6||fabs(reset.v[i].v-expected.v[i].v)>1e-6)return 0;
        if(!historyStep(0)||!edModelsEqual(model,&painted)||!historyStep(1))return 0;
        EdFace imported=painted.c[hit.cell].f[hit.face];imported.material=addMaterialName(painted.decals[imported.decalId-1].name);imported.decalId=0;applyFaceMaterial(&imported,currentMaterial);if(imported.customUV)return 0;
        for(int i=0;i<imported.count;i++)if(fabs(imported.v[i].u-expected.v[i].u)>1e-6||fabs(imported.v[i].v-expected.v[i].v)>1e-6)return 0;
        puts("GEOSITH_PAINT_UV_RESET PASS: regular texture restores tiling, undo/redo, and imported baked material");
    }
    return writeEditorGob(model,"Episode/sithed_decal_test.gob","Decal test",0);
}
