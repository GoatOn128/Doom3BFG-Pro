#include "engine/framework/sourcecontrolwrapper.h"

#include "engine/sys/win32/win_misc.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

idCVar com_sourceControl("com_sourceControl", "perforce", CVAR_ARCHIVE,
                         "name of the source control dll to load");
idCVar sc_maxVerbosity(
    "sc_maxVerbosity", "1", CVAR_INTEGER,
    "0 = errors only, 1 = minimal output, 2 = maximum verbosity from perforce api commands");
idCVar sc_serverAddress("sc_serverAddress", "perforce:1666", 0,
                        "address used to access perforce server");

idSourceControlWrapper::idSourceControlWrapper()
    : sc(nullptr), dllHandle(0), loadFailed(false), autoInitAttempt(true) {}

idSourceControlWrapper::~idSourceControlWrapper() { Shutdown(); }

bool idSourceControlWrapper::Init() { return false; }

void idSourceControlWrapper::Shutdown() {
  if (sc != nullptr)
    sc->Shutdown();
  if (dllHandle != 0)
    Sys_DLL_Unload(reinterpret_cast<HMODULE>(dllHandle));
  dllHandle = 0;
  sc = nullptr;
  loadFailed = false;
}

void idSourceControlWrapper::SetSilentCheckOut(bool set) {
  if (Init())
    sc->SetSilentCheckOut(set);
}

bool idSourceControlWrapper::GetSilentCheckOut() {
  return Init() && sc->GetSilentCheckOut();
}

void idSourceControlWrapper::SetSilentCheckIn(bool set) {
  if (Init())
    sc->SetSilentCheckIn(set);
}

bool idSourceControlWrapper::GetSilentCheckIn() {
  return Init() && sc->GetSilentCheckIn();
}

bool idSourceControlWrapper::IsConnected() const {
  return sc != nullptr && sc->IsConnected();
}

bool idSourceControlWrapper::IsInitialized() const {
  return sc != nullptr && sc->IsInitialized();
}

int idSourceControlWrapper::CheckOut(const idList<idStr>& files) {
  return Init() ? sc->CheckOut(files) : 0;
}

int idSourceControlWrapper::UndoCheckOut(const idList<idStr>& files) {
  return Init() ? sc->UndoCheckOut(files) : 0;
}

int idSourceControlWrapper::Import(const idList<idStr>& files,
                                   bool keepCheckedOut, bool submit,
                                   scFileType_t fileType) {
  return Init() ? sc->Import(files, keepCheckedOut, submit, fileType) : 0;
}

int idSourceControlWrapper::GetLatest(const idList<idStr>& files, bool force) {
  return Init() ? sc->GetLatest(files, force) : 0;
}

int idSourceControlWrapper::CheckIn(const idList<idStr>& files,
                                    const idStr& comment,
                                    bool keepCheckedOut) {
  return Init() ? sc->CheckIn(files, comment, keepCheckedOut) : 0;
}

int idSourceControlWrapper::Delete(const idList<idStr>& files) {
  return Init() ? sc->Delete(files) : 0;
}

scFileStatus_t idSourceControlWrapper::GetFileStatus(const idStr& file) {
  return Init() ? sc->GetFileStatus(file) : SCF_NOTMANAGED;
}

bool idSourceControlWrapper::GetFileVersion(const idStr& file,
                                            int& localVersion,
                                            int& serverVersion) {
  return Init() && sc->GetFileVersion(file, localVersion, serverVersion);
}

int idSourceControlWrapper::GetNumCheckOutUsers(const idStr& file) {
  return Init() ? sc->GetNumCheckOutUsers(file) : 0;
}

int idSourceControlWrapper::UpdateFolder(const char* path) {
  return Init() ? sc->UpdateFolder(path) : 0;
}

void idSourceControlWrapper::GetCheckOutUser(const idStr& file, int userIndex,
                                             char* userName,
                                             const int maxLength) {
  if (Init())
    sc->GetCheckOutUser(file, userIndex, userName, maxLength);
}

void idSourceControlWrapper::GetUsernameForFileVersion(const idStr& file,
                                                       int fileVersion,
                                                       idStr& username) {
  if (Init())
    sc->GetUsernameForFileVersion(file, fileVersion, username);
}

void idSourceControlWrapper::GetCurrentWorkspace(
    idSourceControlWorkspace& workspace) {
  if (Init())
    sc->GetCurrentWorkspace(workspace);
}

void idSourceControlWrapper::GetAvailableWorkspaces(
    idList<idSourceControlWorkspace>& workspaces) {
  if (Init())
    sc->GetAvailableWorkspaces(workspaces);
}

void idSourceControlWrapper::SetWorkspace(idSourceControlWorkspace& workspace) {
  if (Init())
    sc->SetWorkspace(workspace);
}

void idSourceControlWrapper::SetWorkspace(const char* name) {
  if (Init())
    sc->SetWorkspace(name);
}

bool idSourceControlWrapper::IsWorkspaceValid() {
  return Init() && sc->IsWorkspaceValid();
}

idStr idSourceControlWrapper::GetUsername() {
  return Init() ? sc->GetUsername() : idStr("*unknown*");
}

void idSourceControlWrapper::SC_Print(const char* text) {
  idLib::Printf("%s", text);
}
