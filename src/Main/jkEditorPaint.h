static const char *terrainMaterialName(int id){return id>=0&&id<paletteCount?paletteNames[id]:NULL;}
static int terrainLoadImage(const char *name,EdPaintImage *out){
    int id=addMaterialName(name);if(id<0)return 0;rdMaterial *mat=editorMaterial(id);
    if(!mat||!rdMaterial_EnsureDataForced(mat)||!mat->num_texinfo||!mat->texinfos[0])return 0;
    rdTexinfo *info=mat->texinfos[0];tVBuffer *source=info->texture_ptr?info->texture_ptr->texture_struct[0]:NULL;
    rdColor24 *colors=mat->palette_alloc?mat->palette_alloc:colormapColors(materialColormap[id]);
    if(!source){rdColor24 color=colors[info->header.solidColor&255];out->width=out->height=1;out->rgba={color.r,color.g,color.b,255};return 1;}
    if(source->format.width<1||source->format.height<1||source->format.width>4096||source->format.height>4096)return 0;
    if(!stdDisplay_VBufferLock(source)||!source->surface_lock_alloc){stdDisplay_VBufferUnlock(source);return 0;}
    out->width=source->format.width;out->height=source->format.height;out->rgba.resize((size_t)out->width*out->height*4);
    for(int y=0;y<out->height;y++)for(int x=0;x<out->width;x++){
        const unsigned char *p=(unsigned char*)source->surface_lock_alloc+y*source->format.rowSize+x*(source->format.format.bpp/8);unsigned char *dest=&out->rgba[((size_t)y*out->width+x)*4];unsigned value;
        if(source->format.format.bpp==8){value=*p;dest[0]=colors[value].r;dest[1]=colors[value].g;dest[2]=colors[value].b;}
        else if(source->format.format.bpp==16){uint16_t pixel;memcpy(&pixel,p,2);value=pixel;rdTexFormat *fmt=&source->format.format;
            dest[0]=fmt->r_bits?((value>>fmt->r_shift)&((1u<<fmt->r_bits)-1))*255/((1u<<fmt->r_bits)-1):0;
            dest[1]=fmt->g_bits?((value>>fmt->g_shift)&((1u<<fmt->g_bits)-1))*255/((1u<<fmt->g_bits)-1):0;
            dest[2]=fmt->b_bits?((value>>fmt->b_shift)&((1u<<fmt->b_bits)-1))*255/((1u<<fmt->b_bits)-1):0;
        }else{stdDisplay_VBufferUnlock(source);return 0;}
        dest[3]=info->texture_ptr->alpha_en&&value==info->texture_ptr->color_transparent?0:255;
    }
    stdDisplay_VBufferUnlock(source);return 1;
}
static std::map<std::string,rdMaterial*> terrainBakedMaterials;
static std::map<std::string,EdPaintBake> terrainBakeCache;
static int terrainDeferBake;
static std::set<std::string> terrainDeferredBakes;
static rdMaterial *terrainBakeLive(const EdModel *source,int asset,EdPaintBake *bake){
    char name[32],path[64];edPaintFilename(source,asset,name);snprintf(path,sizeof(path),"mat/%s",name);
    EdPaintBake &cached=terrainBakeCache[path];
    // Sculpting changes source UVs, but never the triangle atlas layout. Keep
    // the existing pixels during the stroke; rebuild once when it ends.
    size_t vertices=0;for(int i=0;i<source->meshCount;i++)if(source->meshes[i].asset==asset)vertices+=source->meshes[i].geometry.count*3;
    if(terrainDeferBake&&terrainBakedMaterials[path]&&cached.uv.size()==vertices){
        terrainDeferredBakes.insert(source->assets[asset].name);bake->size=cached.size;bake->uv=cached.uv;return terrainBakedMaterials[path];
    }
    edPaintLoadImage=terrainLoadImage;if(!edPaintBakeAsset(source,asset,&cached,1))return NULL;
    terrainDeferredBakes.erase(source->assets[asset].name);
    // Live model construction only needs UVs, not a copy of the large bitmap.
    bake->size=cached.size;bake->uv=cached.uv;bake->updatedFaces=cached.updatedFaces;
    if(!cached.updatedFaces&&terrainBakedMaterials[path])return terrainBakedMaterials[path];
    terrainBakedResources.add(path,cached.mat);
    rdMaterial *&mat=terrainBakedMaterials[path];rdCache_Flush();
    if(mat){rdMaterial_FreeEntry(mat);if(!rdMaterial_LoadEntry(path,mat,0,0))return NULL;}
    else{rdMaterialLoader_t previous=rdMaterial_RegisterLoader(NULL);mat=rdMaterial_Load(path,0,0);rdMaterial_RegisterLoader(previous);}
    return mat;
}
static void terrainFreePaint(void){for(auto &entry:terrainBakedMaterials)rdMaterial_Free(entry.second);terrainBakedMaterials.clear();terrainBakedResources.files.clear();terrainBakeCache.clear();terrainDeferredBakes.clear();terrainDeferBake=0;}

static std::map<std::string,rdMaterial*> decalMaterials;
static rdMaterial *editorFaceMaterial(const EdModel *source,const EdFace *face){
    if(!face->decalId)return editorMaterial(face->material);
    if(face->decalId<1||face->decalId>source->decalCount)return NULL;
    const EdDecalMaterial &d=source->decals[face->decalId-1];std::string path=std::string("mat/")+d.name;
    auto found=decalMaterials.find(path);if(found!=decalMaterials.end())return found->second;
    terrainBakedResources.add(path,*d.data);rdMaterialLoader_t previous=rdMaterial_RegisterLoader(NULL);
    rdMaterial *material=rdMaterial_Load((char*)path.c_str(),0,0);rdMaterial_RegisterLoader(previous);if(material)decalMaterials[path]=material;return material;
}
static void decalFreeMaterials(void){for(auto &p:decalMaterials)rdMaterial_Free(p.second);decalMaterials.clear();}

static unsigned int decalAssetHash(const EdModel *source,int asset){
    unsigned int hash=0;
    for(int i=0;i<source->meshCount;i++)if(source->meshes[i].asset==asset)for(int f=0;f<source->meshes[i].geometry.count;f++){
        int id=source->meshes[i].geometry.f[f].decalId;
        if(id>0&&id<=source->decalCount)hash=(hash^edNetHash(source->decals[id-1].name,32))*16777619u;
    }return hash;
}

static int applySurfacePaint(const EdModel *source){
    SithWorld *world=sithWorld_g_pCurrentWorld;
    if(!world||world->numSectors!=source->count)return 0;
    std::vector<rdMaterial*> materials;
    for(int c=0;c<source->count;c++){
        if(world->aSectors[c].numSurfaces!=source->c[c].count)return 0;
        for(int f=0;f<source->c[c].count;f++){
            const EdFace &src=source->c[c].f[f];rdFace *face=&world->aSectors[c].surfaces[f].surfaceInfo.face;
            if(face->numVertices!=src.count||!face->vertexUVIdx)return 0;
            for(int v=0;v<src.count;v++)if(face->vertexUVIdx[v]<0||face->vertexUVIdx[v]>=world->numTexVertices)return 0;
            rdMaterial *mat=editorFaceMaterial(source,&src);if(!mat)return 0;materials.push_back(mat);
        }
    }
    rdCache_Flush();size_t index=0;
    for(int c=0;c<source->count;c++)for(int f=0;f<source->c[c].count;f++){
        const EdFace &src=source->c[c].f[f];rdFace *face=&world->aSectors[c].surfaces[f].surfaceInfo.face;face->material=materials[index++];face->texVertOffset={0,0};
        for(int v=0;v<src.count;v++)world->aTexVerticies[face->vertexUVIdx[v]]={(flex_t)src.v[v].u,(flex_t)src.v[v].v};
    }
    // Past stamps live in undo as immutable bytes, not as resident GPU textures.
    for(auto it=decalMaterials.begin();it!=decalMaterials.end();){
        bool used=std::find(materials.begin(),materials.end(),it->second)!=materials.end();
        for(int i=0;!used&&i<source->meshCount;i++)for(int f=0;f<source->meshes[i].geometry.count;f++){int id=source->meshes[i].geometry.f[f].decalId;if(id&&it->first==std::string("mat/")+source->decals[id-1].name)used=true;}
        if(used){++it;continue;}rdMaterial_Free(it->second);terrainBakedResources.files.erase(it->first);it=decalMaterials.erase(it);
    }
    return 1;
}

// Baked materials use a full-face atlas. A regular material must return to
// world tiling, including baked faces imported from a saved level (no decalId).
static void applyFaceMaterial(EdFace *face,int material){
    const char *name=terrainMaterialName(face->material);
    int baked=face->decalId||(name&&!strncmp(name,"gsd_",4));
    face->material=material;face->decalId=0;
    if(baked){face->customUV=0;face->uOffset=face->vOffset=face->uvRotation=0;edMapFace(face);}
}
