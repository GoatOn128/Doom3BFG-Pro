#include "engine/models/renderworld.h"

const char* idRenderWorld::GetName() const { return ""; }
bool idRenderWorld::IsComboMap() const { return false; }
bool idRenderWorld::InitFromMap(const char*, bool, bool) { return false; }
void idRenderWorld::SpecificationComplete(bool) {}
dynamicEnvironmentHandle_t idRenderWorld::AddEnvironment(
    const idBounds&, const idDeclEnv*, const idPlane*, int, float) {
  return {};
}
void idRenderWorld::RemoveEnvironment(const dynamicEnvironmentHandle_t&) {}
void idRenderWorld::SetEnvironmentEffects(const dynamicEnvironmentHandle_t&,
                                          const idDeclEnv*) {}
void idRenderWorld::SetEnvironmentBounds(const dynamicEnvironmentHandle_t&,
                                         const idBounds&) {}
int idRenderWorld::FindEnvironment(const dynamicEnvironmentHandle_t&) const {
  return -1;
}
const idDeclEnv* idRenderWorld::FindEnvironmentDecl(const idVec3&) const {
  return nullptr;
}
idOceanSharedTileMemory* idRenderWorld::GetOceanSharedTileMemory() {
  return nullptr;
}
void idRenderWorld::Ocean_Init() {}
idRenderLight* idRenderWorld::AllocRenderLight(bool) { return nullptr; }
idRenderModel* idRenderWorld::AllocBaseRenderModel() { return nullptr; }
idRenderModel* idRenderWorld::AllocStaticModel(const char*, bool) {
  return nullptr;
}
bool idRenderWorld::AddRenderModel(idRenderModel*) { return false; }
idRenderModel* idRenderWorld::StampModel() { return nullptr; }
idRenderModelFoliageEdit* idRenderWorld::FoliageEditRenderModel() {
  return nullptr;
}
idRenderModelFoliage* idRenderWorld::FoliageRenderModel() { return nullptr; }
idRenderModelDetailEdit* idRenderWorld::DetailEditRenderModel() {
  return nullptr;
}
idRenderModelDetail* idRenderWorld::DetailRenderModel() { return nullptr; }
idRenderModelTransparency* idRenderWorld::TransparencyRenderModel() {
  return nullptr;
}
idRenderModelDecal* idRenderWorld::DecalModel() { return nullptr; }
void idRenderWorld::SetBinaryModelState(const char*, bool) {}
void idRenderWorld::ApproximateLightingForBounds(const idBounds*,
                                                 approximateLighting_t*) {}
void idRenderWorld::ApproximateAmbientForBounds(const idBounds*,
                                                approximateLighting_t*) {}
void idRenderWorld::SetNumRenderViews(int) {}
idRenderView* idRenderWorld::RenderViewForIndex(int) const { return nullptr; }
void idRenderWorld::UpdateInView(const idRenderView*, const idRenderView*) {}
void idRenderWorld::Commit(bool) {}
void idRenderWorld::UpdateDeferredPositions() {}
void idRenderWorld::MarkLightSerialized(idRenderLight*, bool, bool) {}
void idRenderWorld::MarkAllLightsAsSerialized() {}
void idRenderWorld::WriteToSnapshot(idSnapShot&, const idSnapShot*) const {}
void idRenderWorld::ReadFromSnapshot(idSnapShot&, const idSnapShot*) {}
void idRenderWorld::ResetSnapshots() {}
idRenderDebug* idRenderWorld::GetRenderDebug() { return nullptr; }
bool idRenderWorld::InitForTools(const char*) { return false; }
void idRenderWorld::TestEnvironment(const idDeclEnv*) {}
const idDeclEnv* idRenderWorld::GetTestEnvironment() const { return nullptr; }
int idRenderWorld::NumEnvironments() const { return 0; }
void idRenderWorld::ClearEnvironments() {}
const idParmBlock* idRenderWorld::GetCurrentBlendedParms() { return nullptr; }
const idDeclEnv* idRenderWorld::GetEnvironmentDeclForCurrentViewPoint() const {
  return nullptr;
}
int idRenderWorld::GetNumRenderModels() const { return 0; }
idRenderModel* idRenderWorld::RenderModelForIndex(int) const { return nullptr; }
int idRenderWorld::GetNumRenderLights() const { return 0; }
idRenderLight* idRenderWorld::RenderLightForIndex(int) const { return nullptr; }
int idRenderWorld::GetNumVirtualTextures() const { return 0; }
idVirtualTexture* idRenderWorld::VirtualTextureForIndex(int) const {
  return nullptr;
}
int idRenderWorld::GetNumMegaTextures() const { return 0; }
idMegaTexture* idRenderWorld::MegaTextureForIndex(int) const { return nullptr; }
const idStaticModel* idRenderWorld::GetWorldStaticModel() const {
  return nullptr;
}
const idRenderModel* idRenderWorld::GetWorldRenderModel() const {
  return nullptr;
}
void idRenderWorld::GetStaticWorldBounds(idBounds*) {}
idRenderWorldCommitted* idRenderWorld::GetCommittedWorld() const {
  return nullptr;
}
bool idRenderWorld::Trace(modelTrace_t*, const idVec3*, const idVec3*,
                          const idTraceOpts*) {
  return false;
}
