#include "engine/models/water/rendermodelocean2.h"

#include "engine/framework/coderesource.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/image.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <immintrin.h>

extern idCVar r_skipWater;

namespace {
const __m128 vector_float_sign_bit =
    _mm_castsi128_ps(_mm_set1_epi32(static_cast<int>(0x80000000u)));
}

idCVar r_ocean2_jobmode("r_ocean2_jobmode", "1", CVAR_INTEGER,
                        "0 = off, 1 = threaded");
idCodeResource<idDeclRenderParm> r_ocean2_normalmap("r_ocean2_normalmap");
idCVar r_ocean2_debugflags("r_ocean2_debugflags", "0", CVAR_INTEGER,
                           "Ocean2 debug flags.");
idCVar r_ocean2_numwaves("r_ocean2_numwaves", "36", CVAR_INTEGER,
                         "Ocean2 max wave count", 1.0f, 1024.0f);
idCVar r_ocean2_ampmin("r_ocean2_ampmin", "0.5", CVAR_FLOAT,
                       "Ocean2 wave gen min amp");
idCVar r_ocean2_ampmax("r_ocean2_ampmax", "12.0", CVAR_FLOAT,
                       "Ocean2 wave gen max amp");
idCVar r_ocean2_ampdistrexp("r_ocean2_ampdistrexp", "2.2", CVAR_FLOAT,
                            "Ocean2 wave gen amp distribution exponent");
idCVar r_ocean2_kgrav("r_ocean2_kgrav", "512", CVAR_FLOAT,
                      "Ocean2 wave gravity constant");
idCVar r_ocean2_kamp("r_ocean2_kamp", "0.06", CVAR_FLOAT,
                     "Ocean2 amp-displacement constant");
idCVar r_ocean2_anglebase("r_ocean2_anglebase", "0.0", CVAR_FLOAT,
                          "Ocean2 wave base direction in degrees");
idCVar r_ocean2_anglespread("r_ocean2_anglespread", "40.0", CVAR_FLOAT,
                            "Ocean2 wave direction spread in degrees");
idCVar r_ocean2_printwaves("r_ocean2_printwaves", "0", CVAR_INTEGER,
                           "Print ocean2 wave parameters.");
idCVar r_ocean2_spamtimer("r_ocean2_spamtimer", "0", CVAR_FLOAT,
                          "Ocean2 debug spam timer.");

idRenderModelOcean2::COcean2FrameState::COcean2FrameState()
    : m_bValid(false), m_pNormals(nullptr), m_VB(), m_pMappedVB(nullptr) {}

idRenderModelOcean2::COcean2FrameState::~COcean2FrameState() {
  if (m_pNormals != nullptr) {
    m_pNormals->PurgeImage();
    m_pNormals = nullptr;
  }
  if (m_pMappedVB != nullptr) {
    m_VB.UnmapBuffer();
    m_pMappedVB = nullptr;
  }
  m_VB.FreeBufferObject();
  m_bValid = false;
}

idRenderModelOcean2::idRenderModelOcean2(const char* modelName)
    : idRenderModel(), m_lFrames{}, m_IB(), m_WaveParams{} {
  renderModelType = RENDERMODEL_OCEAN2;
  g.flags4 |= 0x40u;
  m_pMaterial = nullptr;
  m_iCurrentFrame = 0;
  m_pJobParams = nullptr;
  m_SpamTimer = 0.0f;
  g.flags0 |= 0x80u;
  g.flags1 |= 0x01u;
  SetName(modelName);
  g.modelDepthSort = MODEL_DEPTH_SORT_AFTER_WORLD;
  (void)vector_float_sign_bit;
}

idRenderModelOcean2::~idRenderModelOcean2() {
  if (m_pJobParams != nullptr) {
    idMem::Singleton()->Free(m_pJobParams, ALIGN_16);
    m_pJobParams = nullptr;
  }
  m_IB.FreeBufferObject();
}

bool idRenderModelOcean2::UpdateInView(const idRenderView*,
                                       const idRenderView*,
                                       idRenderModelUpdateTools*) {
  return r_skipWater.GetInteger() == 0 && surfaces.num != 0;
}
