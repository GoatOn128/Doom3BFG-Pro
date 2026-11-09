#pragma once

#include "tungsten/game/game_runtime.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

class idInfo_TraversalPoint : public idEntity {
public:
  idInfo_TraversalPoint();
  ~idInfo_TraversalPoint() override;
  void PostSpawn() override;
};

class idInfoTraversalEndPoint : public idInfo_TraversalPoint {
public:
  void Spawn();
};

class idInfoTraversal : public idEntity {
public:
  idInfoTraversal();
  ~idInfoTraversal() override;
  void Spawn();
  void OnActivate(idEntity* activator) override;
  void AddTarget(idEntity* entity) { targets.Append(entity); }
  int GetTargetCount() const { return targets.Num(); }

  int aasType{48};
  idStr traversalAnim;
  bool overrideAnim{false};
  bool classE{false};
  bool aasTraversal{true};
  bool longTraversal{false};
  float defaultEndpointOffset{16.0F};
  idVec3 traversalEndpoint{16.0F, 16.0F, 0.0F};
  idVec3 extrusionEndpoint{0.0F, -16.0F, 0.0F};

private:
  idList<idEntity*, 5> targets;
};

class idInfo_TraversalChain : public idInfo_TraversalPoint {
public:
  struct idLink {
    idInfo_TraversalPoint* from{nullptr};
    idInfo_TraversalPoint* to{nullptr};
    float cost{0.0F};
  };
  struct data_t {
    int aasType{3};
    int flags{60};
    idVec3 extrusionEndpoint{};
    const void* animWeb{nullptr};
  };

  idInfo_TraversalChain();
  ~idInfo_TraversalChain() override;
  void PostSpawn() override;
  void AddLink(idInfo_TraversalPoint* from, idInfo_TraversalPoint* to,
               float cost);
  int GetLinkCount() const { return links.Num(); }
  const idLink& GetLink(int index) const { return links[index]; }
  const data_t& GetData() const { return data; }

private:
  data_t data;
  idList<idLink, 5> links;
};
