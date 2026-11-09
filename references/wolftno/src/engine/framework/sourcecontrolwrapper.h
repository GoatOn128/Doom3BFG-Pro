#pragma once

#include "engine/decls/decl.h"

#include <cstdint>

class idSourceControlWrapper : public idSourceControl {
public:
  idSourceControlWrapper();
  ~idSourceControlWrapper() override;

  bool Init() override;
  void Shutdown() override;
  void SetSilentCheckOut(bool set) override;
  bool GetSilentCheckOut() override;
  void SetSilentCheckIn(bool set) override;
  bool GetSilentCheckIn() override;
  bool IsConnected() const override;
  bool IsInitialized() const override;
  int CheckOut(const idList<idStr>& files) override;
  int UndoCheckOut(const idList<idStr>& files) override;
  int Import(const idList<idStr>& files, bool keepCheckedOut, bool submit,
             scFileType_t fileType) override;
  int GetLatest(const idList<idStr>& files, bool force) override;
  int CheckIn(const idList<idStr>& files, const idStr& comment,
              bool keepCheckedOut) override;
  int Delete(const idList<idStr>& files) override;
  int UpdateFolder(const char* path) override;
  scFileStatus_t GetFileStatus(const idStr& file) override;
  bool GetFileVersion(const idStr& file, int& localVersion,
                      int& serverVersion) override;
  int GetNumCheckOutUsers(const idStr& file) override;
  void GetCheckOutUser(const idStr& file, int userIndex, char* userName,
                       const int maxLength) override;
  void GetUsernameForFileVersion(const idStr& file, int fileVersion,
                                 idStr& username) override;
  void GetCurrentWorkspace(idSourceControlWorkspace& workspace) override;
  void GetAvailableWorkspaces(
      idList<idSourceControlWorkspace>& workspaces) override;
  void SetWorkspace(idSourceControlWorkspace& workspace) override;
  void SetWorkspace(const char* name) override;
  bool IsWorkspaceValid() override;
  idStr GetUsername() override;

  static void SC_Print(const char* text);

  idSourceControl* sc;
  std::uint64_t dllHandle;
  bool loadFailed;
  bool autoInitAttempt;
};

static_assert(sizeof(idSourceControlWrapper) == 0x20,
              "source-control wrapper ABI drift");
static_assert(offsetof(idSourceControlWrapper, sc) == 0x08,
              "source-control wrapper member drift");
