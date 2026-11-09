#include "engine/framework/common_local.h"

#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <ctime>

extern idCVar fs_readOnly;

idCVar com_logFile("logFile","1",0,"1 = buffer log, 2 = flush after each print",
                   0.0f,2.0f,idCmdSystem::ArgCompletion_Integer<0,2>);
idCVar com_logFileName("logFileName","qconsole.log",0,
                       "name of log file, if empty, qconsole.log will be used");

void idCommonLocal::idLogFileWriter::CloseLogFile() {
  if(logFile!=nullptr) {
    com_logFile.SetBool(false,true);
    delete logFile;
    logFile=nullptr;
  }
}

void idCommonLocal::SetRefreshOnPrint(bool value) {
  refreshOnPrint.cl=this;
  refreshOnPrint.set=value;
}

void idCommonLocal::idRefreshOnPrint::Print(const char*) {
  if(set&&cl!=nullptr) cl->UpdateConsoleDisplay(true);
}

void idCommonLocal::idLogFileWriter::Print(const char* message) {
  const int logMode=com_logFile.valueInteger;
  if(logMode==0) return;
  idScopedCriticalSection scope(lock);
  const char* fileName=!com_logFileName.valueString.IsEmpty()
    ?com_logFileName.valueString.data:"qconsole.log";
  if(logFile!=nullptr&&idStr::Icmp(fileName,logFile->GetName())!=0) {
    delete logFile;
    logFile=nullptr;
  }
  if(logFile==nullptr&&!recursing) {
    recursing=true;
    const bool readOnly=fs_readOnly.valueInteger!=0;
    fs_readOnly.SetBool(false,true);
    idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
    fileSystem->RemoveFile(fileName,FSPATH_SAVE);
    logFile=fileSystem->OpenFileAppend(fileName,FSPATH_SAVE);
    idMem::Singleton()->PopHeap();
    if(logFile==nullptr) idLib::FatalError("failed to open log file '%s'\n",fileName);
    recursing=false;
    fs_readOnly.SetBool(readOnly,true);
    const std::time_t now=std::time(nullptr);
    const std::tm* local=std::localtime(&now);
    idLib::Printf("log file '%s' opened on %s\n",fileName,std::asctime(local));
  }
  if(logFile!=nullptr) {
    logFile->Write(message,static_cast<std::size_t>(idStr::Length(message)));
    if(logMode==2) logFile->Flush();
  }
}

void idCommonLocal::PrintWarnings() {
  idLib::Printf("------------- Warnings ---------------\n");
  idLib::Printf("during %s...\n",warningList.caption.data);
  for(int index=0;index<warningList.list.num;++index)
    idLib::Printf("%s",warningList.list[index].message.data);
  idLib::Printf("%d warnings\n",warningList.list.num);
  idLib::Printf("--------------------------------------\n");
  ClearWarnings("");
}

void idCommonLocal::DumpWarnings(const char* fileName) {
  const fsPath_t path=idLib::production>=2?FSPATH_SAVE:FSPATH_BASE;
  idFile* file=fileSystem->OpenFileWrite(fileName,path);
  if(file==nullptr) return;
  for(int index=0;index<warningList.list.num;++index) {
    const idStr& message=warningList.list[index].message;
    file->Write(message.data,static_cast<std::size_t>(message.len));
    if(message.data[message.len-1]!='\n') file->Write("\n",1);
  }
  delete file;
}

void idCommonLocal::ClearWarnings(const char* reason) {
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  warningList.caption=reason;
  warningList.list.ClearFree();
  warningList.hash.Clear();
  idMem::Singleton()->PopHeap();
}

void idCommonLocal::idWarningList::Print(const char* message) {
  const int hashKey=idStr::IHash(message)&hash.hashMask;
  for(int index=hash.First(hashKey);index!=-1;index=hash.Next(index)) {
    if(list[index].message.Icmp(message)==0) {
      ++list[index].count;
      return;
    }
  }
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  const int index=list.Append(idUniqueWarning(message));
  hash.Add(hashKey,index);
  idMem::Singleton()->PopHeap();
}
