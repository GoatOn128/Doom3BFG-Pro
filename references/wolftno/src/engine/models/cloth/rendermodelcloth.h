#pragma once

#include "engine/models/cloth/clothsim.h"
#include "engine/models/rendermodel.h"
#include "engine/renderer/bufferobject.h"

class idCVar;
class idParallelJobList;

class idRenderModelCloth : public idRenderModel {
public:
  enum idClothType_t : int {
    CLOTH_MODEL_INVALID = 0,
    CLOTH_FLAG_SQUARE = 1,
    CLOTH_TARP_SQUARE = 2,
    CLOTH_FLAG_TRIANGLE = 3,
    CLOTH_TARP_TRIANGLE = 5,
  };

  struct clothAttachInfo_t {
    enum idClothAnchorType_t : int {
      ANCHOR_INVALID = 0,
      ANCHOR_CORNER1 = 1,
      ANCHOR_CORNER2 = 2,
      ANCHOR_CORNER3 = 3,
      ANCHOR_CORNER4 = 4,
      ANCHOR_MID1 = 5,
      ANCHOR_MID2 = 6,
      ANCHOR_MID3 = 7,
      ANCHOR_MID4 = 8,
      ANCHOR_CENTER = 9,
      ANCHOR_DEFINED = 10,
    };

    idVec3 point{};
    idClothAnchorType_t clothAnchorType{ANCHOR_DEFINED};
    int row{};
    int col{};
    bool tempAnchor{};
    std::uint8_t recoveredPadding[3]{};
  };

  explicit idRenderModelCloth(const idDeclCloth* clothDecl = nullptr);
  ~idRenderModelCloth() override;

  void BuildClothModel();
  void CreateClothSim(const idDeclCloth* clothDecl, idClothType_t clothType,
                      idList<clothAttachInfo_t, 5>* attachPoints,
                      idList<idVec4, 5>* collisionSpheres,
                      int widthOverride, int heightOverride,
                      float hSpacingOverride, float vSpacingOverride);
  void SerializeSnapshot(idSerializer* serializer, bool serializeParmBlock);
  void Save(idFile* file) const override;
  bool Load(idFile* file) override;
  void SetupClothJob(idParallelJobList* parallelJobList,
                     idDrawVert* vertices, int renderTime);
  void RunSimulation(idParallelJobList* parallelJobList, bool writeVerts);
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;

  idClothSim* clothSimulation{};
  static constexpr int NUM_VERTEX_BUFFERS = 3;
  idVertexBuffer vertexBuffer[NUM_VERTEX_BUFFERS];
  idIndexBuffer indexBuffer;
  idDrawVert* deferredVerts{};
  int numVerts{};
  int currentIndex{};
  idBounds* clothBounds{};
  clothParms_t* clothParms{};
  bool rect{};
  std::uint8_t recoveredPadding0[7]{};
  const idDeclCloth* clothSystem{};
  idClothType_t clothType{CLOTH_MODEL_INVALID};
  std::uint32_t recoveredPadding1{};
  idList<clothAttachInfo_t, 5> attachPoints;
  idList<idVec4, 5> collisionSpheres;
  int width{};
  int height{};
  float vSpacing{};
  float hSpacing{};
  int lastRenderTime{-1};
  std::uint8_t recoveredTail[12]{};
};

extern idCVar clothsim_useJobs;
extern idCVar r_skipCloth;
extern idCVar g_clothTestNormalWind;
extern idCVar g_clothTestOverideIterations;

static_assert(sizeof(idRenderModelCloth::clothAttachInfo_t) == 28,
              "cloth attach-info ABI drift");
static_assert(offsetof(idRenderModelCloth, clothSimulation) == 2176,
              "cloth model simulation offset drift");
static_assert(offsetof(idRenderModelCloth, vertexBuffer) == 2184,
              "cloth model vertex-buffer offset drift");
static_assert(offsetof(idRenderModelCloth, clothParms) == 2304,
              "cloth model parms offset drift");
static_assert(offsetof(idRenderModelCloth, attachPoints) == 2336,
              "cloth model attachment offset drift");
static_assert(offsetof(idRenderModelCloth, lastRenderTime) == 2400,
              "cloth model render-time offset drift");
static_assert(sizeof(idRenderModelCloth) == 2416,
              "idRenderModelCloth ABI drift");
