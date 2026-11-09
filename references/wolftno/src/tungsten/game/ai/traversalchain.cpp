#include "tungsten/game/ai/traversalchain.h"

#include <algorithm>

// DIA: idInfo_TraversalPoint::PostSpawn
void idInfo_TraversalPoint::PostSpawn() {
  idEntity::PostSpawn();
  Remove();
}

// DIA: idInfo_TraversalChain::PostSpawn
void idInfo_TraversalChain::PostSpawn() {
  idEntity::PostSpawn();
  Remove();
}

// DIA: idInfoTraversalEndPoint::Spawn
void idInfoTraversalEndPoint::Spawn() { Remove(); }

// DIA: idInfo_TraversalPoint::idInfo_TraversalPoint
idInfo_TraversalPoint::idInfo_TraversalPoint() = default;

// DIA: idInfo_TraversalPoint::~idInfo_TraversalPoint
idInfo_TraversalPoint::~idInfo_TraversalPoint() = default;

// DIA: idInfoTraversal::idInfoTraversal
idInfoTraversal::idInfoTraversal() = default;

idInfoTraversal::~idInfoTraversal() = default;

// DIA: idInfo_TraversalPoint::`scalar deleting destructor'

// DIA: idInfoTraversal::`scalar deleting destructor'

// DIA: idInfoTraversal::Spawn
void idInfoTraversal::Spawn() {
  bool allEndpoints = true;
  for (int i = 0; i < targets.Num(); ++i)
    allEndpoints &= dynamic_cast<idInfoTraversalEndPoint*>(targets[i]) != nullptr;
  if (aasTraversal && allEndpoints) Remove();
}

// DIA: idInfoTraversal::OnActivate
void idInfoTraversal::OnActivate(idEntity* activator) {
  for (int i = 0; i < targets.Num(); ++i)
    if (targets[i] != nullptr) targets[i]->OnActivate(activator);
}

// DIA: idInfo_TraversalChain::idInfo_TraversalChain
idInfo_TraversalChain::idInfo_TraversalChain() = default;

// DIA: `idInfo_TraversalChain::idInfo_TraversalChain'::`1'::dtor$0

// DIA: idInfo_TraversalChain::~idInfo_TraversalChain
idInfo_TraversalChain::~idInfo_TraversalChain() = default;

// DIA: idInfo_TraversalChain::`scalar deleting destructor'

void idInfo_TraversalChain::AddLink(idInfo_TraversalPoint* from,
                                    idInfo_TraversalPoint* to, float cost) {
  if (from == nullptr || to == nullptr) return;
  for (int i = 0; i < links.Num(); ++i) {
    if (links[i].from == from && links[i].to == to) {
      links[i].cost = std::max(0.0F, cost);
      return;
    }
  }
  links.Append({from, to, std::max(0.0F, cost)});
}
