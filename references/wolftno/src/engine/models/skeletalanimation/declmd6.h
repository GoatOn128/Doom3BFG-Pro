#pragma once

#include "engine/models/skeletalanimation/md6animevent.h"
#include "shared/idlib/containers/hashindex.h"

#include <cstddef>

class idDeclMD6;
struct idJointMat;

struct idDeclMD6RuntimeCallbacks {
  bool (*getJointsForAnimTime)(const idMD6Model* model, idJointMat* joints,
                               const idMD6Anim* anim, int timeMS,
                               const idVec3& offset,
                               bool removeOriginOffset){};
  const idMD6Anim* (*loadAnimation)(const char* animationName){};
  const idDeclMD6* (*findDeclaration)(const char* declarationName){};
  idMD6Model* (*loadModel)(const char* modelName){};
  int (*getNumUserChannels)(const idMD6Skel* skeleton){};
  void (*initHeadTrackGroups)(idDeclMD6* declaration){};
  idDeclInfo* (*getDeclInfo)(){};
};

void SetDeclMD6RuntimeCallbacks(const idDeclMD6RuntimeCallbacks& callbacks);

using idAnimEventTransformCacheCallback =
    bool (*)(const idDeclMD6* declaration, const char* animationName,
             idMD6AnimEvent& event);
using idAnimEventSoundPinCallback = void (*)(const void* soundResource);

void SetAnimEventTransformCacheCallback(
    idAnimEventTransformCacheCallback callback);
void SetAnimEventSoundPinCallback(idAnimEventSoundPinCallback callback);

class idAnimEvents {
public:
  struct animEventInfo_t {
    animEventInfo_t() = default;
    animEventInfo_t(const animEventInfo_t&) = default;
    animEventInfo_t& operator=(const animEventInfo_t&) = default;
    ~animEventInfo_t() = default;

    bool GetCachedJoints(int eventNum,
                         idList<const idCachedJoint*, 5>& joints) const;

    idAtomicString animRef;
    idList<idMD6AnimEvent, 28> events;
  };

  idAnimEvents();
  virtual ~idAnimEvents();

  static idAnimEventId GetNextEventId();
  bool CacheTransforms(const idDeclMD6* declaration);
  void PinStreamedSound(int eventNum, int maxFrame) const;
  bool FindAnimEvents(const idAtomicString& animation, int startFrame,
                      int endFrame,
                      idList<const idMD6AnimEvent*, 5>& events) const;
  bool FindAnimEvents(const idAtomicString& animation, int startFrame,
                      int endFrame, int eventNum,
                      idList<const idMD6AnimEvent*, 5>& events) const;
  bool GetCachedJoints(int eventNum, const idMD6Anim* animation,
                       idList<const idCachedJoint*, 5>& joints) const;
  void Parse(const idDeclMD6* declaration, idParser& source,
             int& loadErrors);

  animEventInfo_t& FindOrAdd(const char* animationName);
  const animEventInfo_t* Find(const char* animationName) const;
  void Clear();

  idList<animEventInfo_t, 28> list;
  idHashIndex hashIndex;
  static idAnimEventId curEventId;
};

void InsertEventSorted(idList<const idMD6AnimEvent*, 5>& events,
                       const idMD6AnimEvent* event);
void MergeEvents(const idList<const idMD6AnimEvent*, 5>& newEvents,
                 idList<const idMD6AnimEvent*, 5>& events);

static_assert(sizeof(idAnimEvents::animEventInfo_t) == 0x20);
static_assert(offsetof(idAnimEvents::animEventInfo_t, events) == 0x08);
static_assert(sizeof(idAnimEvents) == 0x48);
static_assert(offsetof(idAnimEvents, list) == 0x08);
static_assert(offsetof(idAnimEvents, hashIndex) == 0x20);
static_assert(sizeof(idVarArgs<6>) == 0x30);
static_assert(sizeof(idAliasHandle_t) == 0x02);
