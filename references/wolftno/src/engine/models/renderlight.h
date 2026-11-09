#pragma once

#include "engine/models/renderworld.h"
#include "engine/models/serializer.h"
#include "engine/renderer/renderlightcommitted.h"
#include "engine/renderer/renderlightparms.h"
#include "shared/idlib/text/atomicstring.h"

class idDeclRenderParm;

class idRenderLight {
public:
  idRenderLight();
  ~idRenderLight();

  int GetIndex() const;
  idStaticParmBlock<32>* GetParmBlock();
  void SetParm(const idDeclRenderParm* parm, const idVec4& value);
  void CommitThisFrame();
  void Delete();
  void Unlink();
  void SerializeSnapshot(idSerializer* serializer);
  void Commit();

  idAtomicString name;
  bool deleteOnSync = false;
  bool unlinked = true;
  bool needWriteToSnapshot = true;
  bool skipAutoReplication = false;
  int serializedChangeCount = 0;
  idRenderLightParms g;
  idStaticParmBlock<32> gameParmBlock;
  idRenderWorldLocal* world = nullptr;
  idRenderLightCommitted* committed = nullptr;
};

static_assert(offsetof(idRenderLight, g) == 16,
              "idRenderLight parms offset drift");
static_assert(offsetof(idRenderLight, gameParmBlock) == 272,
              "idRenderLight parm block offset drift");
static_assert(offsetof(idRenderLight, committed) == 1096,
              "idRenderLight committed offset drift");
static_assert(sizeof(idRenderLight) == 1104, "idRenderLight ABI drift");
