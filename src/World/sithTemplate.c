#include "Main/jkEditor.h"
#include "sithTemplate.h"

#include "stdPlatform.h" // Added: *_ALLOC/*_FREE macros

#include "World/sithThing.h"
#include "World/sithWorld.h"
#include "Cog/sithCog.h"
#include "General/stdString.h"
#include "General/stdConffile.h"
#include "General/stdHashtbl.h"

#include "jk.h"

int sithTemplate_Startup()
{
    sithTemplate_pHashtable = stdHashtbl_New(512);
    return sithTemplate_pHashtable != 0;
}

void sithTemplate_Shutdown()
{
    if ( sithTemplate_pHashtable )
    {
        stdHashtbl_Free(sithTemplate_pHashtable);
        sithTemplate_pHashtable = 0;
    }
}

int sithTemplate_AllocWorldTemplates(SithWorld *pWorld, unsigned int size)
{
#ifdef TARGET_RETRO_HOMEBREW
    // Added: aThingTemplates are parsed into a stack local and copied in word-safely,
    // then only ever read (spawn copies FROM them; byte reads are fine) -- cold
    // and word-safe, so they can live in word-addressable-only memory.
    int prevSuggest = pSithHS->suggestHeap(HEAP_WORD_ADDRESSABLE);
#endif
    SITH_ASSERTREL(pWorld != NULL); // Added: from OpenJones3D
    pWorld->aThingTemplates = (SithThing*)SITH_ALLOC(sizeof(SithThing) * size);
#ifdef TARGET_RETRO_HOMEBREW
    pSithHS->suggestHeap(prevSuggest);
#endif
    if (!pWorld->aThingTemplates)
        return 0;

    stdPlatform_Memzero32(pWorld->aThingTemplates, sizeof(SithThing) * size); // Added: word-safe
    for (int i = 0; i < size; i++)
    {
        sithThing_Reset(&pWorld->aThingTemplates[i]);
        if ( pWorld->level_type_maybe & 1 )
        {
            pWorld->aThingTemplates[i].idx = 0x8000 | i;
        }
        else
        {
            pWorld->aThingTemplates[i].idx = i;
        }
    }

    pWorld->sizeThingTemplates = size;
    pWorld->numThingTemplates = 0;
    return 1;
}

SithThing* sithTemplate_GetTemplateByIndex(int idx)
{
    SithWorld* world = sithWorld_g_pCurrentWorld;
    if ( idx & 0x8000 )
    {
        world = sithWorld_g_pStaticWorld;
        idx &= ~0x8000; // ?
    }
    
    if ( world && idx > 0 && idx < world->numThingTemplates ) // original doesn't check world, but Cog does?
    {
        return &world->aThingTemplates[idx];
    }

    return NULL;
}

int sithTemplate_ReadThingTemplatesListText(SithWorld *pWorld, int bSkip)
{
    unsigned int sizeThingTemplates;

    if ( bSkip )
        return 0;

    stdConffile_ReadArgs();
    if ( _memcmp(stdConffile_g_entry.aArgs[0].value, "world", 6u) || _memcmp(stdConffile_g_entry.aArgs[1].value, "templates", 0xAu) )
        return 0;

    sizeThingTemplates = _atoi(stdConffile_g_entry.aArgs[2].value);
    sizeThingTemplates = jkEditor_ResourceCapacity(pWorld,sizeThingTemplates,2048);
    if ( !sizeThingTemplates )
        return 1;
    
    sithTemplate_AllocWorldTemplates(pWorld, sizeThingTemplates);
    if(jkEditor_ResourceCapacity(pWorld,0,1)&&!pWorld->aCogs)sithCog_AllocWorldCogs(pWorld,128);
    
    while ( stdConffile_ReadArgs() )
    {
        if ( !_memcmp(stdConffile_g_entry.aArgs[0].value, "end", 4u) )
            break;
        sithTemplate_Parse(pWorld);
    }
    if(jkEditor_ResourceCapacity(pWorld,0,1)) {
        if(stdConffile_Open("misc\\sithed_templates.tpl")||stdConffile_Open("../master.tpl")||stdConffile_Open("../mater.tpl")) {
            while(stdConffile_ReadArgs())if(stdConffile_g_entry.numArgs>=2)sithTemplate_Parse(pWorld);
            stdConffile_Close();
        }
    }
    return 1;
}

int sithTemplate_OldNew(char *fpath)
{
    return 0; // TODO unused but interesting
}

void sithTemplate_OldFree()
{
    // TODO unused but interesting
}

void sithTemplate_FreeWorldTemplates(SithWorld *pWorld)
{
    SITH_ASSERTREL(pWorld != NULL); // Added: from OpenJones3D

    for (int i = 0; i < pWorld->numThingTemplates; i++)
    {
        rdThing_FreeEntry(&pWorld->aThingTemplates[i].renderData);
#ifdef STDHASHTABLE_CRC32_KEYS
        stdHashtbl_FreeKeyCrc32(sithTemplate_pHashtable, pWorld->aThingTemplates[i].templateNameCrc);
#else
        stdHashtbl_Remove(sithTemplate_pHashtable, pWorld->aThingTemplates[i].aName);
#endif
    }

    if ( pWorld->aThingTemplates )
    {
        SITH_FREE(pWorld->aThingTemplates);
        pWorld->aThingTemplates = 0;
        pWorld->sizeThingTemplates = 0;
        pWorld->numThingTemplates = 0;
    }
}

SithThing* sithTemplate_GetTemplate(const char *pName)
{
    SithThing *result;

    SITH_ASSERTREL(pName); // Added: from OpenJones3D
    if ( !_memcmp(pName, "none", 5u) )
        return 0;
    result = (SithThing *)stdHashtbl_Find(sithTemplate_pHashtable, pName);
    if ( result )
        return result;

    if ( !sithTemplate_masterFileCount )
        return 0;

    // TODO interesting, but this pHashtbl is never initialized
#if 0
    char v6[0x400];
    const char** v3 = (const char **)stdHashtbl_Find(sithTemplate_pMasterHashtable, pName);
    if ( !v3 )
        return 0;
    if ( v3[3] )
        sithTemplate_GetTemplate(v3[3]);
    stdConffile_Open("none");

    _strncpy(v6, v3[2], 0x3FFu);
    v6[0x3FF] = 0;

    stdConffile_ReadArgsFromStr(&v6);
    result = sithTemplate_Parse(sithWorld_g_pLastLoadedWorld);
    stdConffile_Close();
    return result;
#endif
    return 0;
}

static SithThing* sithTemplate_ParseInternal(SithWorld *pWorld,int strict)
{
    // Live editor templates are parsed without an open conffile. Resource
    // loaders can replace the global argument entry; retain this template
    // line while loading its puppet, sprite or particle dependencies.
    StdConffileEntry templateEntry = stdConffile_g_entry;
    SithThing *result;
    SithThing tmp;
    const char* aName;

    result = (SithThing *)stdHashtbl_Find(sithTemplate_pHashtable, (const char*)templateEntry.aArgs[0].value);
    SithThing *replacement=strict==2?result:NULL;
    if ( result && !replacement )return result;

    if(strict&&!replacement&&pWorld->numThingTemplates>=pWorld->sizeThingTemplates)return NULL;
    // Added: memset for consistent behavior
    memset(&tmp, 0, sizeof(tmp));

    sithThing_Reset(&tmp);
    result = (SithThing *)stdHashtbl_Find(sithTemplate_pHashtable, (const char*)templateEntry.aArgs[1].value);
    sithThing_SetThingBasedOn(&tmp, result);

    aName = templateEntry.aArgs[0].value;
#ifdef SITH_DEBUG_STRUCT_NAMES
    stdString_SafeStrCopy(tmp.aName, aName, sizeof(tmp.aName));
#endif
#ifdef STDHASHTABLE_CRC32_KEYS
    tmp.templateNameCrc = stdCrc32(aName, strlen(aName));
#endif

    for (int i = 2; i < templateEntry.numArgs; i++)
    {
        if (!sithThing_ParseArg(&templateEntry.aArgs[i], &tmp) && strict) {
            rdThing_FreeEntry(&tmp.renderData);
            if(tmp.moveType==SITH_MT_PATH&&tmp.trackParams.aFrames)SITH_FREE(tmp.trackParams.aFrames);
            return NULL;
        }
    }

    if (!tmp.type ) {
        if(strict){rdThing_FreeEntry(&tmp.renderData);if(tmp.moveType==SITH_MT_PATH&&tmp.trackParams.aFrames)SITH_FREE(tmp.trackParams.aFrames);}
        return 0;
    }

    if (!replacement&&pWorld->numThingTemplates >= pWorld->sizeThingTemplates)return 0;
    result = replacement?replacement:&pWorld->aThingTemplates[pWorld->numThingTemplates++];
    if(replacement){rdThing_FreeEntry(&replacement->renderData);if(replacement->moveType==SITH_MT_PATH&&replacement->trackParams.aFrames)SITH_FREE(replacement->trackParams.aFrames);}
    tmp.idx = result->idx;
    stdPlatform_Memcpy32(result, &tmp, sizeof(SithThing)); // Added: word-safe (array may be word-addressable-only)
#ifdef SITH_DEBUG_STRUCT_NAMES
    // The copies of names are load-bearing, SetKeyVal stores a reference
    if(!replacement)stdHashtbl_Add(sithTemplate_pHashtable, result->aName, result);
#else
    stdHashtbl_Add(sithTemplate_pHashtable, aName, result);
#endif

    return result;
}

SithThing *sithTemplate_Parse(SithWorld *world){return sithTemplate_ParseInternal(world,0);}
SithThing *sithTemplate_ParseEditor(SithWorld *world){return sithTemplate_ParseInternal(world,1);}

SithThing *sithTemplate_UpdateEditor(SithWorld *world){return sithTemplate_ParseInternal(world,2);}
