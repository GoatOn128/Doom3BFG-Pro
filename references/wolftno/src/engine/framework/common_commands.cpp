#include "engine/framework/common_local.h"

#include "engine/decls/declmanager.h"
#include "engine/framework/console.h"
#include "engine/framework/fileresource.h"
#include "engine/framework/gamespawninfo.h"
#include "engine/framework/resourcemanager.h"
#include "engine/renderer/renderersystem.h"
#include "engine/sys/sys_public.h"
#include "engine/mapfile/mapfile.h"
#include "engine/sys/win32/win_crashhandler.h"
#include "shared/idlib/commandlink.h"
#include "shared/idlib/csystems/autocomplete.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/langdict.h"
#include "shared/idlib/sys/sys_time.h"

#include <Windows.h>
#include <cstdlib>

extern idCVar com_showFPS;
extern int time_FPS;
extern int time_mapLoadMS;
extern idResourceList* listOfResourceLists;
bool AssertFailed(const char*,int,const char*,bool);

class idSort_Str {
public:
  virtual ~idSort_Str() = default;
  int Compare(const idStr& first,const idStr& second) const {
    return idStr::Icmp(first.data,second.data);
  }
};

template<typename T,typename Comparator> class idSort_Quick : public Comparator {
public:
  virtual ~idSort_Quick() = default;
  virtual void Sort(T* base,unsigned int num) const;
};

template<>
void idSort_Quick<idStr,idSort_Str>::Sort(idStr* base,unsigned int num) const {
  if(num<=1) return;
  std::int64_t lo[128];
  std::int64_t hi[128];
  std::int64_t left=0;
  std::int64_t right=num-1;
  std::int64_t level=0;
  for(;;) {
    while(right-left<4 || level>=127) {
      if(left<right) {
        for(std::int64_t end=right;end>left;--end) {
          std::int64_t greatest=left;
          for(std::int64_t scan=left+1;scan<=end;++scan)
            if(Compare(base[scan],base[greatest])>0) greatest=scan;
          idStr temporary(base[greatest]);
          base[greatest]=base[end];
          base[end]=temporary;
        }
      }
      --level;
      if(level<0) return;
      left=lo[level];
      right=hi[level];
    }
    idStr pivotSwap(base[right]);
    base[right]=base[(right+left)/2];
    base[(right+left)/2]=pivotSwap;
    std::int64_t low=left;
    std::int64_t high=right-1;
    for(;;) {
      while(low<high && Compare(base[low],base[right])<0) ++low;
      while(high>low && Compare(base[high],base[right])>0) --high;
      if(low<high) {
        idStr temporary(base[low]);
        base[low]=base[high];
        base[high]=temporary;
        ++low;
        --high;
        if(low<high) continue;
      }
      break;
    }
    while(high>left && Compare(base[high],base[right])>=0) --high;
    while(low<right && Compare(base[low],base[right])<=0) ++low;
    idStr pivot(base[right]);
    base[right]=base[low];
    base[low]=pivot;
    if(right-low<=high-left) {
      lo[level]=left;
      hi[level]=high;
      left=low;
    } else {
      lo[level]=low;
      hi[level]=right;
      right=high;
    }
    ++level;
  }
}

#if defined(WOLF_COMMON_COMMANDS_VALIDATION)
extern "C" void CommonCommandsValidationSort(idStr* values,unsigned int count) {
  idSort_Quick<idStr,idSort_Str> sorter;
  sorter.Sort(values,count);
}

idAutoComplete::idAutoComplete()
    :matchLength(0),currentIndex(-1),args(),suggestions() {
  args.argc=0;
  suggestions.Clear();
}
idAutoComplete::~idAutoComplete()=default;

void idCmdSystem::ArgCompletion_FileName(idAutoComplete* completion) {
  cmdSystem->ArgCompletion_FolderExtension(completion,"/","",true);
}
void idCmdSystem::ArgCompletion_SaveGame(idAutoComplete* completion) {
  cmdSystem->ArgCompletion_FolderExtension(completion,"savegame/","save",true);
}
void idCmdSystem::ArgCompletion_EventName(idAutoComplete* completion) {
  cmdSystem->ArgCompletion_FolderExtension(completion,"/","events",true);
}
void idCmdSystem::ArgCompletion_DemoFile(idAutoComplete* completion) {
  cmdSystem->ArgCompletion_FolderExtension(completion,"demos/","demo",false);
}
#endif

#if !defined(WOLF_COMMON_COMMANDS_VALIDATION)
idCVar com_exitAfterTests("com_exitAfterTests","0",CVAR_BOOL|CVAR_ROM,
    "if set, exits game after regression tests complete",nullptr);

void idStudio_f(const idCmdArgs&) {
  idLib::Warning("idStudio does not run in 32 bit mode!  Use the x64 build instead.");
}

static void Com_Break_f(const idCmdArgs&) {
  AssertFailed("w:\\build\\tech5\\engine\\framework\\Common_commands.cpp",
               359,"Com_Break_f",true);
}

static void Com_Trace_f(const idCmdArgs&) {
  float value=7.0f;
  idStr text("str");
  SampleTrace(2,&value,&text);
}

static void Com_Quit_f(const idCmdArgs&) {
  idLib::Printf("\n You intentionally quit the game \n");
  commonLocal.Quit(0);
}

void Com_TestStatsBegin_f(const idCmdArgs&) {
  *reinterpret_cast<bool*>(reinterpret_cast<std::byte*>(&commonLocal)+688)=true;
  com_showFPS.SetBool(true,true);
  SetErrorMode(3);
}

void R_VidRestart_f(const idCmdArgs&) {renderSystem->Restart();}

void idCmdSystem::ArgCompletion_FileName(idAutoComplete* completion) {
  cmdSystem->ArgCompletion_FolderExtension(completion,"/","",true);
}
void idCmdSystem::ArgCompletion_SaveGame(idAutoComplete* completion) {
  cmdSystem->ArgCompletion_FolderExtension(completion,"savegame/","save",true);
}
void idCmdSystem::ArgCompletion_EventName(idAutoComplete* completion) {
  cmdSystem->ArgCompletion_FolderExtension(completion,"/","events",true);
}
void idCmdSystem::ArgCompletion_DemoFile(idAutoComplete* completion) {
  cmdSystem->ArgCompletion_FolderExtension(completion,"demos/","demo",false);
}

[[noreturn]] static void Com_Error_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  if(args->argc>1) idLib::FatalError("Testing fatal error");
  idLib::Error("Testing drop error");
}

static void Com_Freeze_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  if(args->argc==2) {
    const float seconds=static_cast<float>(std::atof(args->argv[1]));
    const int start=static_cast<int>(Sys_Milliseconds());
    while(static_cast<float>(static_cast<int>(Sys_Milliseconds())-start)*
              0.001f<=seconds) {}
  } else {
    idLib::Printf("freeze <seconds>\n");
  }
}

static void Com_Crash_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  if(args->argc==2 && std::atol(args->argv[1])>0) {
    idCmdArgs newArgs;
    newArgs.AppendArg(args->argc>0?args->argv[0]:"");
    newArgs.AppendArg(va("%d",static_cast<int>(std::atol(
        args->argc>1?args->argv[1]:""))-1).buffer);
    Com_Crash_f(newArgs);
  } else {
    idLib::Printf("\n You intentionally crashed the game \n");
    *static_cast<volatile int*>(nullptr)=0x12345678;
  }
}

void Com_WriteConfig_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  idStr filename;
  if(args->argc==2) {
    filename=args->argv[1];
    filename.DefaultFileExtension(".cfg");
    idLib::Printf("Writing %s.\n",filename.data);
    commonLocal.WriteConfigToFile(filename.data);
  } else {
    idLib::Printf("Usage: writeconfig <filename>\n");
  }
}

void Com_RecordEvents_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  commonLocal.RecordPlayback(true,args->argc<2?nullptr:args->argv[1]);
}
void Com_PlayEvents_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  commonLocal.RecordPlayback(false,args->argc<2?nullptr:args->argv[1]);
}

void Com_TestStatsEnd_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  *reinterpret_cast<bool*>(reinterpret_cast<std::byte*>(&commonLocal)+688)=false;
  idGame* const game=*reinterpret_cast<idGame**>(
      reinterpret_cast<std::byte*>(&commonLocal)+464);
  const idGameSpawnInfo* const spawnInfo=game->GetGameSpawnInfo();
  idStr mapName(spawnInfo->mapName.data);
  mapName.BackSlashesToSlashes();
  mapName.ToLower();
  idStr fileName;
  idStr groupName;
  mapName.ExtractFileName(fileName);
  if(fileName.Length()==0) fileName="unknown";
  if(mapName.Find("wasteland1",true,0,-1)>=0) groupName="wasteland1";
  else if(mapName.Find("wasteland2",true,0,-1)>=0) groupName="wasteland2";
  else if(mapName.Find("tracks",true,0,-1)>=0) groupName="tracks";
  else groupName="other";

  idStr platformName("win32");
  platformName="win64";
  idStr buildNumber(va("1.%i.%i",1683,12143));
  idStr outputPath(va("testoutput/%s/%s/",buildNumber.data,
                      platformName.data));
  idStr outputName(va("%s%s",outputPath.data,fileName.data));
  if(args->argc>0) {
    idFile* output=fileSystem->OpenFileWriteRetail(
        va("%s_%s.mapstats",outputName.data,platformName.data).buffer,
        FSPATH_SAVE);
    if(output!=nullptr) {
      output->WriteFloatString(
          "{\r\t\"name\":\"%s\",\r\t\"buildnum\":\"%s\",\r\t\"date\":\"%s\",\r\t\"map\":\"%s\",\r\t\"groupname\":\"%s\",\r\t\"texturesize_mb\":\"%s\",\r\t\"platformstats\": [\r\t{\r\t\t\"platform\":\"%s\",\r\t\t\"loadtime_ms\":%i,\r\t\t\"fps\":%i,\r\t\t\"loaded\":%i\r\\t}\r\t]\r}\r",
          fileName.data,buildNumber.data,Sys_DateStr(true),mapName.data,
          groupName.data,"n/a",platformName.data,time_mapLoadMS,time_FPS,2,-2);
      delete output;
    }
  }
  console->Close(true);
  mapName=va("Platform: %s\nDate: %s\nBuild: %s\nMap: %s\nLoad time(secs) %1.2f\nFps: %i\n",
             platformName.data,Sys_DateStr(true),buildNumber.data,fileName.data,
             static_cast<float>(time_mapLoadMS)/1000.0f,time_FPS);
  static idOverlayHandle handle;
  console->PrintOverlay(handle,JUSTIFY_RIGHT,1.0f,1.0f,mapName.data);
  cmdSystem->ExecuteCommandText("wait 120");
  cmdSystem->ExecuteCommandText(va("screenshot %s.jpg",outputName.data).buffer);
  cmdSystem->ExecuteCommandText("wait 5");
  console->Dump(va("%s.log",outputName.data).buffer,nullptr);
}

void DumpWarnings_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  if(args->argc==2) commonLocal.DumpWarnings(args->argv[1]);
  else idLib::Printf("USAGE: com_dumpWarnings <file name>\n");
}

void writeEntitiesFile_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  if(args->argc!=2) {
    idLib::Printf("USAGE: writeEntitiesFile <mapname>\n");
    return;
  }
  idStr mapName(args->argv[1]);
  mapName.BackSlashesToSlashes();
  if(idStr::Icmpn(mapName.data,"maps/",5)!=0) mapName="maps/"+mapName;
  idMapFile mapFile;
  mapFile.game=common->Game();
  if(!mapFile.Parse(mapName.data,false,nullptr)) {
    idLib::Printf("Could not load %s\n",mapName.data);
    return;
  }
  if(mapFile.GetNumEntities()==0) {
    idLib::Printf("%s has no map entities\n",mapName.data);
    return;
  }
  idStr entitiesName(mapName);
  entitiesName.SetFileExtension("entities");
  if(mapFile.WriteEntities(entitiesName.data,false)) idLib::Printf("Done.\n");
}

void idCommonLocal::InitLanguageDict() {
  idStrStatic<64> fileName;
  fileName.Format("strings/%s.lang",sys_lang.GetString());
  const unsigned previousPlatform=resourceManager->GetResourceFilePlatform();
  resourceManager->SetResourceFilePlatform(63);
  idResource* loaded=nullptr;
  try {
    if(idLib::production>=PROD_PRODUCTION) resourceManager->SetFileHook();
    loaded=idFileResource::resourceList.Load(fileName.data,true,false);
    if(idLib::production>=PROD_PRODUCTION) resourceManager->ReleaseFileHook();
  } catch(...) {
    resourceManager->SetResourceFilePlatform(previousPlatform);
    throw;
  }
  resourceManager->SetResourceFilePlatform(previousPlatform);
  idFileResource* file=static_cast<idFileResource*>(loaded);
  if(file==nullptr || file->GetLength()<=0) {
    sys_lang.Reset();
    fileName.Format("strings/%s.lang",sys_lang.GetString());
    file=static_cast<idFileResource*>(
        idFileResource::resourceList.Load(fileName.data,true,false));
  }
  if(file!=nullptr && file->GetLength()>0) {
    if(!idLocalization::languageDict.Load(
           static_cast<const char*>(file->GetData()),file->GetLength(),
           fileName.data))
      idLib::Warning("Unable to load language data from file %s!",fileName.data);
    delete file;
  } else {
    idLib::Warning("Unable to load default language file %s!",fileName.data);
  }
}

void Com_ReloadLanguage_f(const idCmdArgs&) { commonLocal.InitLanguageDict(); }

void Com_StripStrings_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  if(args->argc!=2) {
    idLib::Printf("USAGE: stripStrings <match string>\n");
    return;
  }
  const char* const match=args->argv[1];
  const int matchLength=idStr::Length(match);
  int removed=0;
  if(idStr::Cmpn(match,"#str_",5)==0) {
    idLib::Printf("Finding keys starting with '%s'...\n",match);
    if(idLocalization::languageDict.dictionary.Num()<=0) return;
    for(int index=0;index<idLocalization::languageDict.dictionary.Num();++index) {
      if(idStr::Cmpn(idLocalization::languageDict.dictionary[index].key.str,
                     match,matchLength)==0) {
        idLocalization::languageDict.dictionary.RemoveIndex(index--);
        ++removed;
      }
    }
  } else {
    idLib::Printf("Finding substrings matching '%s'...\n",match);
    if(idLocalization::languageDict.dictionary.Num()<=0) return;
    for(int index=0;index<idLocalization::languageDict.dictionary.Num();++index) {
      if(idStr::Find(idLocalization::languageDict.dictionary[index].value.str,
                     match,true,0,-1)!=-1) {
        idLocalization::languageDict.dictionary.RemoveIndex(index--);
        ++removed;
      }
    }
  }
  if(removed>0) {
    idLib::Printf("Removed %d strings.\n",removed);
    idStr fileName(va("strings/%s.lang",sys_lang.GetString()));
    if(idLocalization::languageDict.Save(fileName.data))
      idLib::Printf("Saved strings file '%s'.\n",fileName.data);
  }
}

void Com_Find_f(const idCmdArgs& argsRef) {
  const idCmdArgs* args=&argsRef;
  if(args->argc!=2) {
    idLib::Printf("Usage: find <search string>\n");
    return;
  }
  const idStr searchString(args->argv[1]);
  idList<idStr> found;
  for(int typeIndex=0;typeIndex<declManager->GetNumDeclTypes();++typeIndex) {
    idDeclInfo* const type=declManager->GetDeclType(typeIndex);
    for(int sourceIndex=0;sourceIndex<type->declSources.Num();++sourceIndex) {
      const idDeclSource* const source=type->declSources[sourceIndex];
      if(idStr::Find(source->name.str,searchString.data,false,0,-1)>-1) {
        idStr entry;
        entry.Format("^3[^2%s^3] ^0%s\n",type->GetResourceTypeName(),
                     source->name.str);
        found.Append(entry);
      }
    }
  }
  for(idResourceList* list=listOfResourceLists;list!=nullptr;
      list=list->GetNextList()) {
    if(dynamic_cast<idDeclInfo*>(list)!=nullptr) continue;
    for(int index=0;index<list->Num();++index) {
      idResource* const resource=list->Index(index);
      if(idStr::Find(resource->name.str,searchString.data,false,0,-1)>-1) {
        idStr entry;
        entry.Format("^3[^2%s^3] ^0%s\n",list->GetResourceTypeName(),
                     resource->name.str);
        found.Append(entry);
      }
    }
  }
  for(int index=0;index<cvarSystem->NumCVars();++index) {
    const idCVar* const cvar=cvarSystem->FindByIndex(index);
    if(cvar!=nullptr &&
       idStr::Find(cvar->name,searchString.data,false,0,-1)>-1) {
      idStr entry;
      entry.Format("^4[^2%s^4] ^0%s","cvar",cvar->name);
      while(entry.Length()<40) entry.Append(' ');
      entry.Append(cvar->description);
      entry.Append('\n');
      found.Append(entry);
    }
  }
  idList<idStr> commands;
  cmdSystem->FindCommands(searchString.data,commands);
  for(int index=0;index<commands.Num();++index) {
    idStr entry;
    entry.Format("^5[^2%s^5] ^0%s","command",commands[index].data);
    while(entry.Length()<40) entry.Append(' ');
    entry.Append(cmdSystem->GetCommandDescription(commands[index].data));
    entry.Append('\n');
    found.Append(entry);
  }
  idSort_Quick<idStr,idSort_Str> sorter;
  if(found.list!=nullptr) sorter.Sort(found.list,found.num);
  if(found.Num()>0) {
    idLib::Printf("Found %d item(s):\n\n",found.Num());
    for(int index=0;index<found.Num();++index) idLib::Printf(found[index].data);
  } else {
    idLib::Printf("^1No items found^0\n");
  }
}

static void EmptyCommand_f(const idCmdArgs*) {}

void idCommonLocal::InitCommands() {
  cmdSystem->AddCommand("error",Com_Error_f,"causes an error",nullptr);
  cmdSystem->AddCommand("crash",Com_Crash_f,"causes a crash",nullptr);
  cmdSystem->AddCommand("trace",Com_Trace_f,"forces a trace",nullptr);
  cmdSystem->AddCommand("break",Com_Break_f,"causes a debug breakpoint",nullptr);
  cmdSystem->AddCommand("freeze",Com_Freeze_f,
                        "freezes the game for a number of seconds",nullptr);
  cmdSystem->AddCommand("hitch",Hitch_f,"hitches the game",nullptr);
  cmdSystem->AddCommand("quit",Com_Quit_f,"quits the game",nullptr);
  cmdSystem->AddCommand("exit",Com_Quit_f,"exits the game",nullptr);
  cmdSystem->AddCommand("writeConfig",Com_WriteConfig_f,"writes a config file",nullptr);
  cmdSystem->AddCommand("find",Com_Find_f,
                        "finds all system elements containing the given search string",nullptr);
  cmdSystem->AddCommand("dumpWarnings",DumpWarnings_f,
                        "Dump all warnings to a text file.",nullptr);
  cmdSystem->AddCommand("vid_restart",R_VidRestart_f,
                        "resizes/fullscreens the game window",nullptr);
  cmdSystem->AddCommand("testLCP",EmptyCommand_f,"test LCP solver code",nullptr);
  cmdSystem->AddCommand("reloadLanguage",Com_ReloadLanguage_f,
                        "reload language dict",nullptr);
  cmdSystem->AddCommand("stripStrings",Com_StripStrings_f,
                        "strip strings containing the specified substring or prefix",nullptr);
  cmdSystem->AddCommand("map",ComboMap_f,"loads a combo map",
                        idCmdSystem::ArgCompletion_MapName);
  cmdSystem->AddCommand("cmap",ComboMap_f,"loads a combo map",
                        idCmdSystem::ArgCompletion_MapName);
  cmdSystem->AddCommand("devmap",DevComboMap_f,
                        "loads a combo map in developer mode",
                        idCmdSystem::ArgCompletion_MapName);
  cmdSystem->AddCommand("devcmap",DevComboMap_f,
                        "loads a combo map in developer mode",
                        idCmdSystem::ArgCompletion_MapName);
  cmdSystem->AddCommand("checkmap",CheckLoad_f,
                        "loads a combo map in developer mode from checkpoint load",
                        idCmdSystem::ArgCompletion_MapName);
  cmdSystem->AddCommand("slowmap",DevMap_f,
                        "loads a map using slow non shipping render path in developer mode",
                        idCmdSystem::ArgCompletion_MapName);
  cmdSystem->AddCommand("restartmap",RestartMap_f,"restarts the current map",nullptr);
  cmdSystem->AddCommand("restartmaphere",RestartMapHere_f,
                        "restarts the current map and teleports the player back to the current position",
                        nullptr);
  cmdSystem->AddCommand("nextmap",NextMap_f,
                        "transitions to the next map, maintaining inventory",nullptr);
  cmdSystem->AddCommand("restart_loadgame",RestartLoadGame_f,
                        "loads a game after rebooting due to a disc swap",nullptr);
  cmdSystem->AddCommand("restart_passMainMenu",RestartPassMainMenu_f,
                        "continues after a disc swap to somewhere on the main menu",nullptr);
  cmdSystem->AddCommand("restart_mapAfterSwap",RestartMapAfterSwap_f,
                        "continues after a disc swap to a map",nullptr);
  cmdSystem->AddCommand("restart_multiplayerInvite",RestartMultiplayerInvite_f,
                        "used after accepting a MP invite from disc 1 or 2",nullptr);
  cmdSystem->AddCommand("disconnect",Disconnect_f,"disconnects from a game",nullptr);
  cmdSystem->AddCommand("leaveGame",LeaveGame_f,"disconnects from a game",nullptr);
  cmdSystem->AddCommand("saveGame",SaveGame_f,"saves a game",nullptr);
  cmdSystem->AddCommand("loadGame",LoadGame_f,"loads a game",
                        idCmdSystem::ArgCompletion_SaveGame);
  cmdSystem->AddCommand("demorecord",RecordDemo_f,"Record a demo",nullptr);
  cmdSystem->AddCommand("demorecord_playtest",EmptyCommand_f,
                        "Record a demo for playtest, gets argurments from cvars.  temp...",nullptr);
  cmdSystem->AddCommand("demoplay",PlayDemo_f,"Play a recorded demo",
                        idCmdSystem::ArgCompletion_DemoFile);
  cmdSystem->AddCommand("demostop",StopDemo_f,
                        "Stop recording or playing a demo",nullptr);
  cmdSystem->AddCommand("demoCaptureFrames",CaptureDemoFrames_f,
                        "Capture demo frames at 1280x720",
                        idCmdSystem::ArgCompletion_DemoFile);
  cmdSystem->AddCommand("demoCaptureSounds",CaptureDemoSounds_f,
                        "Capture demo sound",idCmdSystem::ArgCompletion_DemoFile);
  cmdSystem->AddCommand("compressFile",CompressFile_f,"compresses a file",
                        idCmdSystem::ArgCompletion_FileName);
  cmdSystem->AddCommand("decompressFile",DecompressFile_f,"decompresses a file",
                        idCmdSystem::ArgCompletion_FileName);
  cmdSystem->AddCommand("deleteGenerated",DeleteGenerated_f,
                        "deletes the generated path",nullptr);
  cmdSystem->AddCommand("recordEvents",Com_RecordEvents_f,
                        "recordEvents <name> start recording sys events",nullptr);
  cmdSystem->AddCommand("playEvents",Com_PlayEvents_f,
                        "playEvents <name> playback sys events F12 will stop playback",
                        idCmdSystem::ArgCompletion_EventName);
  cmdSystem->AddCommand("testStatsBegin",Com_TestStatsBegin_f,
                        "starts a test session for recording stats",nullptr);
  cmdSystem->AddCommand("testStatsEnd",Com_TestStatsEnd_f,
                        "ends a test session for recording stats and exits the game",nullptr);
}

idAutoComplete::idAutoComplete()
    :matchLength(0),currentIndex(-1),args(),suggestions() {
  args.argc=0;
  suggestions.Clear();
}
idAutoComplete::~idAutoComplete()=default;

idCommandLink idStudio_v("idStudio",idStudio_f,"design studio for the game",nullptr);
idCommandLink writeEntitiesFile_v(
    "writeEntitiesFile",writeEntitiesFile_f,
    "writes out a production build .entities file for the specified map",
    idCmdSystem::ArgCompletion_MapName);
#endif
