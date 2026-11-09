#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI

#include "engine/gamelib/physics/physics_parametricdoor.h"

idPhysics_ParametricDoor::idPhysics_ParametricDoor() {
  type = PHYSICS_PARAMETRICDOOR;
  clipModels.ptr[0] = nullptr;
  clipModels.ptr[1] = nullptr;
}

idPhysics_ParametricDoor::~idPhysics_ParametricDoor() {
  idPhysics_Parametric::SetClipModel(nullptr, 1.0f, 0, false);
  idClipModel** currentModel = clipModels.ptr;
  for (std::int64_t remaining = 2; remaining != 0;
       --remaining, ++currentModel) {
    if (*currentModel != nullptr) {
      (*currentModel)->Delete();
      *currentModel = nullptr;
    }
  }
}

void idPhysics_ParametricDoor::SetClipModel(idClipModel* model, float density,
                                            int id, bool freeOld) {
  idClipModel* oldModel = clipModels.ptr[id];
  if (oldModel != nullptr && oldModel != model && freeOld) {
    oldModel->Delete();
  }

  clipModels.ptr[id] = model;
  if (id != 0) {
    if (model != nullptr) {
      model->Link(entityNumber, physicsId, id, current.worldOrigin,
                  current.worldAxis);
    }
  } else {
    idPhysics_Parametric::SetClipModel(model, density, 0, false);
  }
}

idClipModel* idPhysics_ParametricDoor::GetClipModel(int id) {
  idClipModel* model = clipModels.ptr[id];
  return model != nullptr ? model : clip->defaultClipModel;
}

int idPhysics_ParametricDoor::GetNumClipModels() {
  return (clipModels.ptr[0] != nullptr) + (clipModels.ptr[1] != nullptr);
}

void idPhysics_ParametricDoor::SetContents(int contents, int id) {
  if (id == -1) {
    id = 0;
  }
  idClipModel* model = clipModels.ptr[id];
  if (model != nullptr) {
    model->SetContents(contents);
  }
}

int idPhysics_ParametricDoor::GetContents(int id) {
  if (id == -1) {
    id = 0;
  }
  idClipModel* model = clipModels.ptr[id];
  return model != nullptr ? model->contents : 0;
}

void idPhysics_ParametricDoor::RestoreState() {
  idPhysics_Parametric::RestoreState();
  idClipModel* model = clipModels.ptr[1];
  if (model != nullptr) {
    model->Link(entityNumber, physicsId, 1, current.worldOrigin,
                current.worldAxis);
  }
}

void idPhysics_ParametricDoor::SetOrigin(const idVec3& newOrigin, int id) {
  idPhysics_Parametric::SetOrigin(newOrigin, id);
  idClipModel* model = clipModels.ptr[1];
  if (model != nullptr) {
    model->Link(entityNumber, physicsId, 1, current.worldOrigin,
                current.worldAxis);
  }
}

void idPhysics_ParametricDoor::SetAxis(const idMat3& newAxis, int id) {
  idPhysics_Parametric::SetAxis(newAxis, id);
  idClipModel* model = clipModels.ptr[1];
  if (model != nullptr) {
    model->Link(entityNumber, physicsId, 1, current.worldOrigin,
                current.worldAxis);
  }
}

void idPhysics_ParametricDoor::DisableClip() {
  idClipModel** currentModel = clipModels.ptr;
  for (std::int64_t remaining = 2; remaining != 0;
       --remaining, ++currentModel) {
    idClipModel* model = *currentModel;
    if (model != nullptr) {
      model->flags &= ~1u;
    }
  }
}

void idPhysics_ParametricDoor::EnableClip() {
  idClipModel** currentModel = clipModels.ptr;
  for (std::int64_t remaining = 2; remaining != 0;
       --remaining, ++currentModel) {
    idClipModel* model = *currentModel;
    if (model != nullptr) {
      model->flags |= 1u;
    }
  }
}

void idPhysics_ParametricDoor::UnlinkClip() {
  idClipModel** currentModel = clipModels.ptr;
  for (std::int64_t remaining = 2; remaining != 0;
       --remaining, ++currentModel) {
    idClipModel* model = *currentModel;
    if (model != nullptr) {
      model->Unlink();
    }
  }
}

void idPhysics_ParametricDoor::LinkClip() {
  int id = 0;
  idClipModel** currentModel = clipModels.ptr;
  do {
    idClipModel* model = *currentModel;
    if (model != nullptr) {
      model->Link(entityNumber, physicsId, id, current.worldOrigin,
                  current.worldAxis);
    }
    ++id;
    ++currentModel;
  } while (id < 2);
}

void idPhysics_ParametricDoor::SetLocalOrigin(const idVec3& newOrigin,
                                              int id) {
  idPhysics_Parametric::SetLocalOrigin(newOrigin, id);
  idClipModel* model = clipModels.ptr[1];
  if (model != nullptr) {
    model->Link(entityNumber, physicsId, 1, current.worldOrigin,
                current.worldAxis);
  }
}

void idPhysics_ParametricDoor::SetLocalAxis(const idMat3& newAxis, int id) {
  idPhysics_Parametric::SetLocalAxis(newAxis, id);
  idClipModel* model = clipModels.ptr[1];
  if (model != nullptr) {
    model->Link(entityNumber, physicsId, 1, current.worldOrigin,
                current.worldAxis);
  }
}

bool idPhysics_ParametricDoor::Evaluate(int timeStepMSec, int endTimeMSec) {
  const bool result =
      idPhysics_Parametric::Evaluate(timeStepMSec, endTimeMSec);
  idClipModel* model = clipModels.ptr[1];
  if (model != nullptr) {
    model->Link(entityNumber, physicsId, 1, current.worldOrigin,
                current.worldAxis);
  }
  return result;
}
