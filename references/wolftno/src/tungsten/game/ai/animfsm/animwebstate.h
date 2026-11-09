#pragma once

#include "engine/animation/md6.h"
#include "engine/decls/animwebs/declanimweb.h"

class idAI2;

class idAnimWebState {
public:
  bool operator==(const idAnimWebState& other) const;
  bool operator!=(const idAnimWebState& other) const {
    return !(*this == other);
  }
  void UpdateDebugInfo(const char* defaultSubWebName,
                       const char* defaultStateName);
  void Init(const idDeclAnimWeb* declaration, idAnimWebPath* path);
  void Init(const idDeclAnimWeb* declaration, const char* subWebName,
            const char* stateName);

  const idDeclAnimWeb* GetOwner() const { return owner; }
  animWebSubWebIndex_t GetSubWeb() const { return subWeb; }
  animWebStateIndex_t GetState() const { return state; }
  animWebNodeIndex_t GetNode() const { return node; }
  bool IsPathConstructed() const { return pathConstructed; }
  const idStr& GetDebugSubWeb() const { return debugSubWeb; }
  const idStr& GetDebugState() const { return debugState; }

protected:
  const idDeclAnimWeb* owner{nullptr};
  animWebSubWebIndex_t subWeb{};
  animWebStateIndex_t state{};
  animWebNodeIndex_t node{};
  bool pathConstructed{false};
  idStr debugSubWeb;
  idStr debugState;
};

class idAIAnimWebState : public idAnimWebState {
public:
  idAIAnimWebState(idAI2* ai, const char* subWebName,
                   const char* stateName);
  idAIAnimWebState(const idAI2* ai, const char* path);
};

class idAIAnimWebState_Current : public idAnimWebState {
public:
  explicit idAIAnimWebState_Current(const idAI2* ai);
};
