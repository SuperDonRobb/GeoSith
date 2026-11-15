// Custom COG working copies are local files; received resources stay in memory.
#ifdef _WIN32
#include <commdlg.h>
#include <shellapi.h>
#endif
struct CogExternalFile {std::filesystem::path path;std::string seen;};
static std::map<std::string,CogExternalFile> cogExternalFiles;
static int cogExternalPolling;
static unsigned cogExternalRevision;
struct CogCompiled {std::string name,text;SithCogScript *script;};
static std::vector<CogCompiled> cogCompiled;
static SithCogScript *cogCustomCompile(const char *name){
    char *source=cogSourceText(name);if(!source)return NULL;std::string text(source);edFree(source);
    for(auto &c:cogCompiled)if(c.name==name&&c.text==text)return c.script;
    SithCogScript *script=(SithCogScript*)SITH_ALLOC(sizeof(SithCogScript));if(!script)return NULL;memset(script,0,sizeof(*script));
    char path[80];snprintf(path,sizeof(path),"cog\\%s",name);
    if(!sithCogParse_Load(path,script,0)){if(script->pSymbolTable)sithCogParse_FreeSymbolTable(script->pSymbolTable);if(script->pCode)SITH_FREE(script->pCode);SITH_FREE(script);return NULL;}
    cogCompiled.push_back({name,std::move(text),script});return script;
}
static bool cogCustomOnlyChanges(const EdResources &before,const EdResources &after){
    for(auto &f:before.files){auto it=after.files.find(f.first);if(it==after.files.end())return false;if(*it->second!=*f.second&&!f.first.starts_with("cog/"))return false;}
    for(auto &f:after.files){auto it=before.files.find(f.first);if(it!=before.files.end()&&*it->second==*f.second)continue;
        std::string name=f.first.substr(4);EdPlacedCog parsed;std::string text(f.second->begin(),f.second->end());
        if(!f.first.starts_with("cog/")||name.size()>31||name.find('/')!=std::string::npos||text.find('\0')!=std::string::npos||text.size()>2*1024*1024||!edCogParseScript(&parsed,name.c_str(),text.c_str()))return false;
    }return true;
}
static void cogRefreshParams(EdPlacedCog &placed,const EdPlacedCog &parsed){
    EdPlacedCog next=parsed;
    for(int i=0;i<next.count;i++)for(int j=0;j<placed.count;j++)if(next.params[i].type==placed.params[j].type&&!strcmp(next.params[i].name,placed.params[j].name))next.params[i]=placed.params[j];
    placed=next;
}
static int cogPublishCustom(const std::string &name,const std::string &text){
    if(netEditPending()||terrainSessionOpen())return 0;
    if(name.size()>31||EdResources::path("cog/"+name)!="cog/"+name||name.find('/')!=std::string::npos||!name.ends_with(".cog")||text.empty()||text.size()>2*1024*1024||text.find('\0')!=std::string::npos)return 0;
    EdPlacedCog parsed;if(!edCogParseScript(&parsed,name.c_str(),text.c_str()))return 0;
    EdResources previous=customResources,next=customResources;next.add("cog/"+name,EdResources::Bytes(text.begin(),text.end()));setCustomResources(std::move(next));
    if(!cogCustomCompile(name.c_str())){setCustomResources(std::move(previous));return 0;}
    edCopyModel(candidate,model);for(int i=0;i<candidate->cogCount;i++)if(!__strcmpi(candidate->cogs[i].file,name.c_str()))cogRefreshParams(candidate->cogs[i],parsed);
    if(!edValidate(candidate)){setCustomResources(std::move(previous));return 0;}
    int same=edModelsEqual(candidate,model);remember();int ok=netClient()?netPropose(candidate):commit();
    if(!ok){setCustomResources(std::move(previous));return 0;}
    if(!netClient()&&same){pendingUndo=0;if(!active)startPlacedCogs(model);netCommitted();}
    if(cogDraft)for(int i=0;i<cogDraft->count;i++)if(!__strcmpi(cogDraft->cogs[i].file,name.c_str()))cogRefreshParams(cogDraft->cogs[i],parsed);
    cogSourceCount=0;cogExternalRevision++;say(netClient()?"Custom COG submitted to host.":"Custom COG loaded and shared. Add COG to place it.");return 1;
}
static std::string cogReadExternal(const std::filesystem::path &path){
    auto size=std::filesystem::file_size(path);if(!size||size>2*1024*1024)throw std::runtime_error("COG must be between 1 byte and 2 MiB");
    auto bytes=EdResources::read(path,0,(size_t)size);return std::string(bytes.begin(),bytes.end());
}
static bool cogLaunchEditor(const std::filesystem::path &path){
#ifdef _WIN32
    HMODULE library=LoadLibraryW(L"shell32.dll");if(!library)return false;
    auto launch=(HINSTANCE (WINAPI *)(HWND,LPCWSTR,LPCWSTR,LPCWSTR,LPCWSTR,INT))GetProcAddress(library,"ShellExecuteW");
    bool ok=false;if(launch){auto result=launch(NULL,L"edit",path.c_str(),NULL,NULL,SW_SHOWNORMAL);ok=(INT_PTR)result>32;
        if(!ok){std::wstring argument=L"\""+path.wstring()+L"\"";ok=(INT_PTR)launch(NULL,L"open",L"notepad.exe",argument.c_str(),NULL,SW_SHOWNORMAL)>32;}}
    FreeLibrary(library);return ok;
#else
    say("External COG editing is currently supported on Windows.");return false;
#endif
}
static std::filesystem::path cogChooseFile(){
#ifdef _WIN32
    wchar_t path[32768]={};OPENFILENAMEW dialog={};dialog.lStructSize=sizeof(dialog);dialog.lpstrFile=path;dialog.nMaxFile=32768;dialog.lpstrFilter=L"COG scripts (*.cog)\0*.cog\0\0";dialog.lpstrTitle=L"Import COG";dialog.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
    HMODULE library=LoadLibraryW(L"comdlg32.dll");if(!library)return {};auto choose=(BOOL (WINAPI *)(LPOPENFILENAMEW))GetProcAddress(library,"GetOpenFileNameW");bool ok=choose&&choose(&dialog);FreeLibrary(library);if(ok)return path;
#endif
    return {};
}
static std::string cogNewName(){
    char16_t input[32]=u"new_script.cog";
    jkGuiElement e[]={{ELEMENT_TEXT,0,2,u"Create COG",3,{20,40,600,40},1},{ELEMENT_TEXTBOX,0,0,input,31,{70,160,500,35},1},{ELEMENT_TEXTBUTTON,1,2,u"Create",3,{350,330,230,40},1},{ELEMENT_TEXTBUTTON,-1,2,"GUI_CANCEL",3,{60,330,230,40},1},{ELEMENT_END,0}};
    jkGuiMenu menu={e,-1,0xffff,0xffff,15,0,0,jkGui_stdBitmaps,jkGui_stdFonts};jkGui_InitMenu(&menu,jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);jkGuiRend_MenuSetReturnKeyShortcutElement(&menu,&e[2]);jkGuiRend_MenuSetEscapeKeyShortcutElement(&menu,&e[3]);int result=jkGuiRend_DisplayAndReturnClicked(&menu);freeMenuStrings(e);if(result!=1)return {};
    char name[64];stdString_WcharToChar(name,input,64);std::string out=name;for(char &c:out)c=(char)tolower((unsigned char)c);if(!out.ends_with(".cog"))out+=".cog";return out;
}
static void cogExternalMenu(int action)try {
    if(netEditPending()||terrainSessionOpen()){jkGuiDialog_ErrorDialog(u"COG",u"Finish the current edit and wait for host approval first.");return;}
    std::string name,text;std::filesystem::path path;
    if(action==30){path=cogChooseFile();if(path.empty())return;name=path.filename().string();for(char &c:name)c=(char)tolower((unsigned char)c);text=cogReadExternal(path);}
    else if(action==31){name=cogNewName();if(name.empty())return;text="# GeoSith custom COG\nflags=0x0\n\nsymbols\nmessage startup\nend\n\ncode\nstartup:\n    return;\nend\n";
        char *exists=cogSourceText(name.c_str());if(exists){edFree(exists);jkGuiDialog_ErrorDialog(u"Create COG",u"That script already exists. Choose another name or use Edit COG.");return;}}
    else {std::vector<std::string> files;std::vector<std::array<char16_t,128>> names;for(auto &f:customResources.files)if(f.first.starts_with("cog/")){files.push_back(f.first.substr(4));names.push_back({});stdString_CharToWchar(names.back().data(),files.back().c_str(),128);}
        if(files.empty()){jkGuiDialog_ErrorDialog(u"Edit COG",u"No custom COGs. Import or create one, or add one through mods.");return;}
        int selected=chooseList(u"Edit Custom COG",reinterpret_cast<char16_t (*)[128]>(names.data()),(int)names.size(),0);if(selected<0)return;name=files[selected];auto bytes=customResources.files.at("cog/"+name);text.assign(bytes->begin(),bytes->end());}
    if(action!=32&&!cogPublishCustom(name,text)){jkGuiDialog_ErrorDialog(u"COG",u"Could not load the script. Check its filename, syntax and parameter defaults.");return;}
    if(action==30){say("COG imported. Use Add COG to place it.");return;}
    auto directory=std::filesystem::absolute("editor-cogs");std::filesystem::create_directories(directory);path=directory/name;
    // Never overwrite unsaved working-copy changes when reopening the editor.
    if(!std::filesystem::exists(path)){std::ofstream out(path,std::ios::binary);out<<text;if(!out)throw std::runtime_error("Could not write COG working copy");}
    cogExternalFiles[name]={path,text};if(!cogLaunchEditor(path))jkGuiDialog_ErrorDialog(u"COG Editor",u"Could not open the text editor. The working copy is in editor-cogs.");
}catch(const std::exception &){jkGuiDialog_ErrorDialog(u"COG",u"Could not read or write the COG file.");}
static void cogExternalTick(){
    static uint32_t last;if(cogExternalFiles.empty()||!model||(jkMain_bInModalGui&&!cogExternalPolling)||netEditPending()||terrainSessionOpen()||(last&&stdPlatform_GetTimeMsec()-last<1000))return;last=stdPlatform_GetTimeMsec();
    for(auto &f:cogExternalFiles)try{std::string text=cogReadExternal(f.second.path);if(text==f.second.seen)continue;f.second.seen=text;
        if(!cogPublishCustom(f.first,text))say("COG reload failed. Fix the script and save again; the previous version is retained.");break;
    }catch(const std::exception &){/* An editor may briefly replace the file while saving. */}
}
static void cogExternalClose(){cogExternalFiles.clear();for(auto &c:cogCompiled){sithCog_FreeScriptEntry(c.script);SITH_FREE(c.script);}cogCompiled.clear();}

static void cogExternalMenuTick(jkGuiMenu *menu){unsigned before=cogExternalRevision;cogExternalPolling=1;cogExternalTick();cogExternalPolling=0;if(before!=cogExternalRevision)menu->lastClicked=29;}
