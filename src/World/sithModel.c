#include "Main/jkEditor.h"
#include "sithModel.h"

#include "Primitives/rdModel3.h"
#include "Engine/rdroid.h"
#include "World/sithWorld.h"
#include "World/sithThing.h"
#include "General/stdConffile.h"
#include "stdPlatform.h"
#include "jk.h"

static tHashTable* sithModel_hashtable;

int sithModel_Startup()
{
    sithModel_hashtable = stdHashtbl_New(256);
    return sithModel_hashtable != 0;
}

void sithModel_Shutdown()
{
    if ( sithModel_hashtable )
    {
        stdHashtbl_Free(sithModel_hashtable);
        sithModel_hashtable = 0;
    }
}

int sithModel_ReadStaticModelsListText(SithWorld *pWorld, int bSkip)
{
    int sizeModels;
    flex_t loadStep;
    flex_t loadProgress;

    if ( bSkip )
        return 0;

    SITH_ASSERTREL(pWorld != NULL); // Added
    SITH_ASSERTREL(pWorld->aModels == NULL); // Added
    SITH_ASSERTREL(pWorld->numModels == 0); // Added
    stdConffile_ReadArgs();
    if ( _memcmp(stdConffile_g_entry.aArgs[0].value, "world", 6u) || _memcmp(stdConffile_g_entry.aArgs[1].value, "models", 7u) )
        return 0;
    pWorld->sizeModels = _atoi(stdConffile_g_entry.aArgs[2].value);
    pWorld->sizeModels = jkEditor_ResourceCapacity(pWorld,pWorld->sizeModels,1024);
    if ( !pWorld->sizeModels )
        return 1;

    pWorld->aModels = (rdModel3 *)SITH_ALLOC(sizeof(rdModel3) * pWorld->sizeModels);
    if ( !pWorld->aModels )
    {
        stdPrintf(pSithHS->errorPrint, ".\\World\\sithModel.c", 164, "Memory error while reading models, line %d.\n", stdConffile_linenum, 0, 0, 0);
        return 0;
    }
    pWorld->numModels = 0;
    _memset(pWorld->aModels, 0, sizeof(rdModel3) * pWorld->sizeModels);

    sithWorld_UpdateLoadProgress(60.0);
    loadStep = 10.0 / (flex_d_t)pWorld->sizeModels;
    loadProgress = 60.0;
    while ( stdConffile_ReadArgs() )
    {
        if ( !_memcmp(stdConffile_g_entry.aArgs[0].value, "end", 4u) )
            break;
        sithModel_Load(stdConffile_g_entry.aArgs[1].value, 0);
        loadProgress = loadProgress + loadStep;
        sithWorld_UpdateLoadProgress(loadProgress);
    }
    sithWorld_UpdateLoadProgress(70.0);

    return 1;
}

void sithModel_FreeWorldModels(SithWorld *pWorld)
{
    SITH_ASSERTREL(pWorld != NULL); // Added
    if (!pWorld->sizeModels )
        return;

    SITH_ASSERTREL(pWorld->aModels != NULL); // Added
    for (int i = 0; i < pWorld->numModels; i++)
    {
        stdHashtbl_Remove(sithModel_hashtable, pWorld->aModels[i].filename);
        rdModel3_FreeEntryGeometryOnly(&pWorld->aModels[i]);
    }
    SITH_FREE(pWorld->aModels);
    pWorld->aModels = 0;
    pWorld->numModels = 0;
    pWorld->sizeModels = 0;
}

rdModel3* sithModel_Load(const char *pName, int bSkipDefault)
{
    rdModel3 *model;
    char model_fpath[128];

    SITH_ASSERTREL(sithWorld_g_pLastLoadedWorld != NULL); // Added
    SITH_ASSERTREL(pName != NULL); // Added
    model = (rdModel3 *)stdHashtbl_Find(sithModel_hashtable, pName);
    if ( model ) {
        //stdPlatform_Printf("OpenJKDF2: %s: Load %s from static jkl.\n", __func__, model_3do_fname); // Added
        return model;
    }

    SITH_ASSERTREL(sithWorld_g_pLastLoadedWorld->numModels <= sithWorld_g_pLastLoadedWorld->sizeModels); // Added
    if ( sithWorld_g_pLastLoadedWorld->numModels >= sithWorld_g_pLastLoadedWorld->sizeModels ) {
        stdPlatform_Printf("OpenJKDF2: %s: Too many models already loaded!\n", __func__); // Added
        return 0;
    }
    model = &sithWorld_g_pLastLoadedWorld->aModels[sithWorld_g_pLastLoadedWorld->numModels];

    SITH_ASSERTREL(_strlen(pName) < sizeof(model_fpath)); // Added
    _sprintf(model_fpath, "%s%c%s", "3do", '\\', pName);
    if ( !rdModel3_LoadEntry(model_fpath, model) )
    {
        if ( !bSkipDefault ) {
            stdPlatform_Printf("OpenJKDF2: %s: rdModel3_LoadEntry failed for `%s`, loading dflt.3do!\n", __func__, pName); // Added
            return sithModel_Load("dflt.3do", 1);
        }
        return 0;
    }
    
    model->id = sithWorld_g_pLastLoadedWorld->numModels;
    if (sithWorld_g_pLastLoadedWorld->level_type_maybe & 1)
        model->id |= 0x8000;
    
    stdHashtbl_Add(sithModel_hashtable, model->filename, model);
    sithWorld_g_pLastLoadedWorld->numModels += 1;

    return model;
}

uint32_t sithModel_GetModelMemUsage(rdModel3 *pModel)
{
    unsigned int result; // eax
    rdGeoset *v2; // ebx
    int v3; // edi
    rdMesh* v4; // edx
    int v5; // esi
    rdFace* v6; // ecx
    int modela; // [esp+8h] [ebp+4h]

    result = (sizeof(void*) * pModel->sizeMaterials) + (sizeof(rdHierarchyNode) * pModel->numHNodes) + sizeof(rdModel3);
    if ( pModel->numGeos )
    {
        v2 = pModel->aGeos;
        modela = pModel->numGeos;
        do
        {
            result += 8;
            if ( v2->numMeshes )
            {
                v3 = v2->numMeshes;
                v4 = v2->aMeshes;
                do
                {
                    v5 = v4->numFaces;
                    result += (sizeof(rdVector2) * v4->numUVs) + ((sizeof(rdVector3) + sizeof(rdVector3) + sizeof(rdVector2)) * v4->numVertices) + (sizeof(rdFace) * v4->numFaces) + sizeof(rdMesh);
                    if ( v4->numFaces )
                    {
                        v6 = v4->faces;
                        do
                        {
                            result += (sizeof(int) * 2) * v6->numVertices;
                            v6++;
                            --v5;
                        }
                        while ( v5 );
                    }
                    ++v4;
                    --v3;
                }
                while ( v3 );
            }
            ++v2;
            --modela;
        }
        while ( modela );
    }
    return result;
}

int sithModel_AllocWorldModels(SithWorld *pWorld, int size)
{
    SITH_ASSERTREL(pWorld->aModels == NULL); // Added
    pWorld->aModels = (rdModel3 *)SITH_ALLOC(sizeof(rdModel3) * size);
    if ( !pWorld->aModels )
        return 0;

    pWorld->sizeModels = size;
    pWorld->numModels = 0;
    _memset(pWorld->aModels, 0, sizeof(rdModel3) * size);

    return 1;
}

rdModel3* sithModel_GetModelByIndex(int modelIdx)
{
    SithWorld *world;
    rdModel3 *result;

    world = sithWorld_g_pCurrentWorld;
    if ( (modelIdx & 0x8000) != 0 )
    {
        world = sithWorld_g_pStaticWorld;
        modelIdx &= 0x7FFF;
    }
    if ( world && modelIdx >= 0 && modelIdx < world->numModels )
        return &world->aModels[modelIdx];

    return NULL;
}

/* Transfer editor geometry into world-owned stable storage. Materials are
   borrowed, exactly as for ordinary sithModel_Load models. */
rdModel3 *sithModel_Find(const char *name){return stdHashtbl_Find(sithModel_hashtable,name);}
rdModel3 *sithModel_RegisterEditor(const char *name,rdModel3 *owned){
    SithWorld *w=sithWorld_g_pCurrentWorld;rdModel3 *existing=sithModel_Find(name);if(existing)return existing;
    if(!w||!w->aModels||w->numModels>=w->sizeModels||strlen(name)>=sizeof(owned->filename))return NULL;
    rdModel3 *m=&w->aModels[w->numModels];*m=*owned;memset(owned,0,sizeof(*owned));m->id=w->numModels++;snprintf(m->filename,sizeof(m->filename),"%s",name);stdHashtbl_Add(sithModel_hashtable,m->filename,m);return m;
}

/* Sculpting changes vertex data repeatedly. Reuse stable model storage when
   its hierarchy size is unchanged, so every brush tick does not consume a slot.
   Existing instances keep correctly sized joint arrays; editor transactions
   rebuild changed templates/things before rendering resumes. */
static void sithModel_RemapEditorContacts(SithWorld *world,rdModel3 *old,rdModel3 *replacement)
{
    for(unsigned int i=0;i<world->numThingsLoaded;i++) {
        SithThing *thing=&world->aThings[i];
        if(!(thing->attach_flags&SITH_ATTACH_THINGFACE)||!thing->attachedThing
            ||thing->attachedThing->renderData.type!=RD_THING_MODEL3
            ||thing->attachedThing->renderData.model3!=old)continue;
        rdFace *support=NULL;rdVector3 anchor={0};
        for(int g=0;g<old->numGeos&&!support;g++)for(int m=0;m<old->aGeos[g].numMeshes&&!support;m++) {
            rdMesh *mesh=&old->aGeos[g].aMeshes[m];
            for(int f=0;f<mesh->numFaces;f++) {
                rdFace *face=&mesh->faces[f];
                if((rdFace*)thing->attachedSufaceInfo!=face)continue;
                if(!replacement||g>=replacement->numGeos||m>=replacement->aGeos[g].numMeshes)break;
                rdMesh *next=&replacement->aGeos[g].aMeshes[m];
                if(f>=next->numFaces)break;
                rdFace *target=&next->faces[f];
                if(face->numVertices<3||face->numVertices!=target->numVertices
                    ||memcmp(&face->normal,&target->normal,sizeof(rdVector3)))break;
                int same=1;
                for(int v=0;v<face->numVertices;v++) {
                    int a=face->vertexPosIdx[v],b=target->vertexPosIdx[v];
                    if(a<0||a>=mesh->numVertices||b<0||b>=next->numVertices
                        ||memcmp(&mesh->aVertices[a],&next->aVertices[b],sizeof(rdVector3))){same=0;break;}
                }
                if(same){support=target;anchor=next->aVertices[target->vertexPosIdx[0]];}
                break;
            }
        }
        /* Paint/UV changes preserve contact without impact sounds or velocity
           changes. A sculpted/deleted support must detach before its face dies. */
        if(support){thing->attachedSufaceInfo=(sithSurfaceInfo*)support;thing->field_38=anchor;}
        else sithThing_DetachThing(thing);
    }
}
rdModel3 *sithModel_ReplaceEditor(const char *name,rdModel3 *owned){
    SithWorld *w=sithWorld_g_pCurrentWorld;rdModel3 *existing=sithModel_Find(name);
    if(!w||!w->aModels||strlen(name)>=sizeof(owned->filename))return NULL;
    if(existing && existing->numHNodes==owned->numHNodes){
        sithModel_RemapEditorContacts(w,existing,owned);
        int id=existing->id;existing->sizeMaterials=0;rdModel3_FreeEntry(existing);
        *existing=*owned;memset(owned,0,sizeof(*owned));existing->id=id;snprintf(existing->filename,sizeof(existing->filename),"%s",name);return existing;
    }
    if(w->numModels>=w->sizeModels)return NULL;
    if(existing)sithModel_RemapEditorContacts(w,existing,NULL);
    stdHashtbl_Remove(sithModel_hashtable,name);return sithModel_RegisterEditor(name,owned);
}
