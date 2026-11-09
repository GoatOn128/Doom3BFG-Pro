#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/physics/physics.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include "shared/idlib/lib.h"

#include <cstring>

int idPhysics::currentPhysicsId = 0;
idPhysics* idPhysics::hash[1024]{};

void idPhysics::InitPhysics(idClip* clip_, idPhysicsCallbacks* callbacks_,
                            int entityNumber_) {
  clip = clip_;
  callbacks = callbacks_;
  entityNumber = entityNumber_;
}

void idPhysics::ClearHash() {
  std::memset(hash, 0, sizeof(hash));
  currentPhysicsId = 0;
}

idPhysics* idPhysics::GetPhysicsForId(int physicsId_) {
  if (physicsId_ == -1) {
    return nullptr;
  }

  idPhysics* physics = hash[physicsId_ & 0x3FF];
  while (physics != nullptr && physics->physicsId != physicsId_) {
    physics = physics->nextInHash;
  }
  return physics;
}

void idPhysics::UpdateCollisionResidency(const contactInfo_t& contact) {
  if ((contact.flags & 1) != 0) {
    collisionResidency = RESIDENCY_NO;
  }
}

void idPhysics::Link() {
  int id = physicsId;
  if (id != -1) {
    idPhysics* physics = hash[id & 0x3FF];
    while (physics != nullptr && physics->physicsId != id) {
      physics = physics->nextInHash;
    }

    if (physics != nullptr) {
      idLib::Printf(" duplicate physicsId = %d\n", id);
      for (;;) {
        physicsId = currentPhysicsId;
        ++currentPhysicsId;
        id = physicsId;
        if (id == -1) {
          break;
        }

        physics = hash[id & 0x3FF];
        while (physics != nullptr && physics->physicsId != id) {
          physics = physics->nextInHash;
        }
        if (physics == nullptr) {
          break;
        }
      }
    }
  }

  idPhysics*& bucket = hash[physicsId & 0x3FF];
  nextInHash = bucket;
  bucket = this;
}

idPhysics::idPhysics() {
  type = PHYSICS_INVALID;
  ++currentPhysicsId;
  nextInHash = nullptr;
  clip = nullptr;
  callbacks = nullptr;
  collisionResidency = RESIDENCY_NOT_TESTED;
  physicsId = currentPhysicsId;
  entityNumber = 0x1FFF;
  Link();
}

idPhysics::~idPhysics() {
  idPhysics** link = &hash[physicsId & 0x3FF];
  while (*link != nullptr) {
    idPhysics* const physics = *link;
    if (physics->physicsId == physicsId && physics == this) {
      *link = physics->nextInHash;
      break;
    }
    link = &physics->nextInHash;
  }

  nextInHash = nullptr;
  clip = nullptr;
  callbacks = nullptr;
}

bool idPhysics::IsInNonResidentCollisionArea(bool assumeNonResident) {
  idClip* const ownerClip = clip;
  const std::uint64_t index = residencyQuery.index;
  if (index >= ownerClip->residencyQueryFirstSubmittedIndex &&
      index <= ownerClip->residencyQueryLastSubmittedIndex) {
    if (index < ownerClip->residencyQueryFirstSubmittedIndex ||
        index > ownerClip->residencyQueryLastSubmittedIndex ||
        ownerClip->residencyQueries[index & 0x3FF].resident) {
      collisionResidency = RESIDENCY_YES;
    } else {
      collisionResidency = RESIDENCY_NO;
    }
  }

  if (residencyQuery.index <=
      ownerClip->residencyQueryLastSubmittedIndex) {
    residencyQuery = ownerClip->TestResidency(GetAbsBounds(-1));
  }

  return collisionResidency == RESIDENCY_NO ||
         (collisionResidency == RESIDENCY_NOT_TESTED &&
          assumeNonResident);
}
