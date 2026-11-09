#pragma once

#include "tungsten/game/game_runtime.h"

#include <string>

struct idDeclPonytail {
  std::string modelDefName;
  std::string tagName;
};

class idPonytail {
public:
  idPonytail();
  ~idPonytail();

  void Update();
  void Shutdown();
  bool Init(idEntity* parent);
  void SetDecl(const idDeclPonytail* value) { declPonytail = value; }

  bool IsInitialized() const { return initialized; }
  bool IsUsingParentAnimStack() const { return usingParentAnimStack; }
  int GetAttachmentHandle() const { return attachmentHandle; }
  void ConfigureAttachmentForGoreTest(int value) {
    attachmentHandle = value;
    initialized = true;
  }
  int GetBlendCount() const { return blendCount; }
  idEntity* GetParent() const { return parent; }

private:
  const idDeclPonytail* declPonytail{nullptr};
  idEntity* parent{nullptr};
  int attachmentHandle{-1};
  int blendCount{0};
  bool usingParentAnimStack{false};
  bool initialized{false};
};
