#include "engine/models/transparency/rendermodeltransparency.h"

#include "engine/framework/coderesource.h"
#include "engine/models/foliage/foliagemodel.h"
#include "engine/models/renderworld.h"
#include "engine/models/transparency/jobs/transparencybin.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/render.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <new>

extern idCVar r_znear;

idCVar r_useTransparencySort(
    "r_useTransparencySort", "1", CVAR_BOOL,
    "0 = do not sort the transparency quads");
idCVar r_showTransparencyStats("r_showTransparencyStats", "0", CVAR_INTEGER,
                               "1 = print stats");
idCVar r_showVisibleInfluenceSpheres(
    "r_showVisibleInfluenceSpheres", "0", CVAR_BOOL,
    "show the visible influence spheres");
idCVar r_useInfluenceSpheres("r_useInfluenceSpheres", "1", CVAR_BOOL,
                             "enable influence spheres");
idCVar r_useTransparencyEmissivePass(
    "r_useTransparencyEmissivePass", "1", CVAR_INTEGER,
    "render quads marked emissive to the emissive pass");
idCVar r_useTransparencyCollapse("r_useTransparencyCollapse", "1", CVAR_BOOL,
                                 "1 = collapse unused verts");
idCVar r_useTransparencyBinning("r_useTransparencyBinning", "0", CVAR_BOOL,
                                "");
idCVar r_useTransparencySortJobs(
    "r_useTransparencySortJobs", "1", CVAR_INTEGER,
    "0 = off, 1 = threaded", 0.0f, 1.0f);
idCVar r_useInfluenceSphereCullJob(
    "r_useInfluenceSphereCullJob", "1", CVAR_INTEGER,
    "0 = off, 1 = threaded", 0.0f, 1.0f);

idCodeResource<idMaterial> mtrTransSort(
    "textures/particles/transparencySort");
idCodeResource<idMaterial> mtrTransSortGlare(
    "textures/particles/transparencySortGlare");
idCodeResource<idMaterial> mtrTransSortOpaque(
    "textures/particles/transparencySortOpaque");

idVertexBuffer idRenderModelTransparency::vertexBuffer[3];
idIndexBuffer idRenderModelTransparency::indexBuffer[3];
idIndexBuffer idRenderModelTransparency::indexBuffer2[3];
idVertexBuffer idRenderModelTransparency::vertexBufferUnsorted[3];
idIndexBuffer idRenderModelTransparency::indexBufferUnsorted;
idTransparencyVert* idRenderModelTransparency::mappedVerts = nullptr;
std::uint16_t* idRenderModelTransparency::mappedIndices = nullptr;
std::uint16_t* idRenderModelTransparency::mappedIndices2 = nullptr;
int idRenderModelTransparency::mappedBufferIndex = 0;
int idRenderModelTransparency::numVertsAllocated = 0;
float* idRenderModelTransparency::currentQuadDepths = nullptr;
std::uint8_t* idRenderModelTransparency::currentEmissiveQuads = nullptr;
int idRenderModelTransparency::numSegmentAllocations = 0;
transSortSegmentInfo_t* idRenderModelTransparency::segmentInfo = nullptr;
idTransparencyVert* idRenderModelTransparency::mappedVertsUnsorted = nullptr;
int idRenderModelTransparency::numVertsAllocatedUnsorted = 0;
idTransparencyVert* idRenderModelTransparency::binVerts = nullptr;
std::uint16_t* idRenderModelTransparency::binIndices = nullptr;

namespace {

constexpr int kMaximumSortedQuads = 0x4000;
constexpr int kMaximumUnsortedQuads = 0x2000;
constexpr int kMaximumSegments = 0x400;
constexpr int kMaximumSortedIndices = kMaximumSortedQuads * 6;
constexpr int kMaximumEmissiveIndices = 0xC00;
constexpr int kMaximumInfluenceSpheres = 0x400;
constexpr memTag_t kTransparencyTag = static_cast<memTag_t>(0x61);

template <typename Type>
Type* AllocateTransparency(const char* location, std::size_t count,
                           bool clear) {
  return static_cast<Type*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(Type) * count, kTransparencyTag, clear, ALIGN_16,
      HEAP_DEFAULTHEAP));
}

template <typename Type>
class ScopedTempArray {
public:
  ScopedTempArray(const char* location, std::size_t count)
      : pointer(static_cast<Type*>(idMem::Singleton()->AllocWithLocation(
            location, sizeof(Type) * count, TAG_TEMP, false, ALIGN_16,
            HEAP_DEFAULTHEAP))) {}

  ~ScopedTempArray() {
    if (pointer != nullptr)
      idMem::Singleton()->Free(pointer, ALIGN_16);
  }

  Type* Get() const { return pointer; }

private:
  Type* pointer;
};

idTriangles* AllocateSurfaceGeometry() {
  return new (std::nothrow) idTriangles{};
}

void InitializeSurface(idRenderModelTransparency& model,
                       const idMaterial* material, idVertexBuffer& vertices,
                       idIndexBuffer& indices) {
  idRenderModelSurface surface{};
  surface.geometry = AllocateSurfaceGeometry();
  if (surface.geometry == nullptr)
    return;
  surface.geometry->bounds[0] = {-99999.0f, -99999.0f, -99999.0f};
  surface.geometry->bounds[1] = {99999.0f, 99999.0f, 99999.0f};
  surface.material = material;
  surface.geometry->vertexBuffer.Reference(vertices);
  surface.geometry->indexBuffer.Reference(indices);
  surface.geometry->vertexMask = 3167;
  model.AddSurface(surface);
}

} // namespace

void TransparencySortJob(const transSortParms_t* parms) {
  TransparencySortQuads(parms);
  TransparencyTriIndGen(parms, 0);
  TransparencyEmissiveTriIndGen(parms, 0);
}

idParallelJobRegistration register_TransparencySortJob(
    reinterpret_cast<jobRun_t>(TransparencySortJob), "TransparencySortJob");

bool idRenderModelTransparency::AllocateQuads(
    const int numQuads, int& quadOffset, idTransparencyVert*& quadVerts,
    float*& quadDepths) {
  if ((numVertsAllocated >> 2) + numQuads >= kMaximumSortedQuads)
    return false;
  quadOffset = numVertsAllocated >> 2;
  quadVerts = &mappedVerts[numVertsAllocated];
  quadDepths = &currentQuadDepths[numVertsAllocated >> 2];
  numVertsAllocated += 4 * numQuads;
  return true;
}

bool idRenderModelTransparency::AllocateSegment(
    bool hasEmissivePass, bool hasOpaquePass, int*& segmentOffset,
    int*& quadsAllocated, int*& quadsUsed) {
  if (numSegmentAllocations >= kMaximumSegments - 1)
    return false;
  transSortSegmentInfo_t& segment = segmentInfo[numSegmentAllocations++];
  segment.quadsAllocated = 0;
  segment.segmentOffset = 0;
  segment.quadsUsed = 0;
  segment.emissivePass = hasEmissivePass;
  segment.opaquePass = hasOpaquePass;
  segmentOffset = &segment.segmentOffset;
  quadsAllocated = &segment.quadsAllocated;
  quadsUsed = &segment.quadsUsed;
  return true;
}

bool idRenderModelTransparency::AllocateQuadSegment(
    int numQuads, idTransparencyVert*& verts, float*& quadDepths,
    int*& quadsUsed, bool hasEmissivePass) {
  if (numQuads + (numVertsAllocated >> 2) >= kMaximumSortedQuads ||
      numSegmentAllocations >= kMaximumSegments - 1) {
    return false;
  }
  transSortSegmentInfo_t& segment = segmentInfo[numSegmentAllocations++];
  segment.quadsAllocated = numQuads;
  segment.segmentOffset = numVertsAllocated >> 2;
  segment.emissivePass = hasEmissivePass;
  segment.opaquePass = false;
  verts = &mappedVerts[numVertsAllocated];
  quadDepths = &currentQuadDepths[numVertsAllocated >> 2];
  quadsUsed = &segment.quadsUsed;
  numVertsAllocated += 4 * numQuads;
  return true;
}

bool idRenderModelTransparency::AllocateUnsortedQuadSegment(
    const int numQuads, idTransparencyVert*& verts, int& indexOffset,
    int& vertCount) {
  if (numQuads + (numVertsAllocatedUnsorted >> 2) >=
      kMaximumUnsortedQuads) {
    return false;
  }
  verts = &mappedVertsUnsorted[numVertsAllocatedUnsorted];
  indexOffset = 6 * (numVertsAllocatedUnsorted >> 2);
  vertCount = 4 * numQuads;
  numVertsAllocatedUnsorted += 4 * numQuads;
  return true;
}

void idRenderModelTransparency::ClearInfluenceSpheres() {
  numInfluenceSpheres[currentSphereIndex] = 0;
}

void idRenderModelTransparency::Shutdown() {
  if (!idLib::headless) {
    if (mappedVerts != nullptr && vertexBuffer[mappedBufferIndex].IsMapped())
      vertexBuffer[mappedBufferIndex].UnmapBuffer();
    if (mappedIndices != nullptr && indexBuffer[mappedBufferIndex].IsMapped())
      indexBuffer[mappedBufferIndex].UnmapBuffer();
    if (mappedIndices2 != nullptr && indexBuffer2[mappedBufferIndex].IsMapped())
      indexBuffer2[mappedBufferIndex].UnmapBuffer();
    for (int index = 0; index < 3; ++index) {
      vertexBuffer[index].FreeBufferObject();
      indexBuffer[index].FreeBufferObject();
      indexBuffer2[index].FreeBufferObject();
    }
    if (mappedVertsUnsorted != nullptr &&
        vertexBufferUnsorted[mappedBufferIndex].IsMapped()) {
      vertexBufferUnsorted[mappedBufferIndex].UnmapBuffer();
    }
    for (idVertexBuffer& buffer : vertexBufferUnsorted)
      buffer.FreeBufferObject();
    indexBufferUnsorted.FreeBufferObject();
  }

  idMem* memory = idMem::Singleton();
  if (currentQuadDepths != nullptr)
    memory->Free(currentQuadDepths, ALIGN_16);
  if (currentEmissiveQuads != nullptr)
    memory->Free(currentEmissiveQuads, ALIGN_16);
  if (segmentInfo != nullptr)
    memory->Free(segmentInfo, ALIGN_16);
  if (binVerts != nullptr)
    memory->Free(binVerts, ALIGN_16);
  if (binIndices != nullptr)
    memory->Free(binIndices, ALIGN_16);
  currentQuadDepths = nullptr;
  currentEmissiveQuads = nullptr;
  segmentInfo = nullptr;
  binVerts = nullptr;
  binIndices = nullptr;
}

idRenderModelTransparency::idRenderModelTransparency() {
  renderModelType = RENDERMODEL_TRANSPARENCY;
  g.flags0 |= 0x80u;
  g.flags1 |= 0x21u;
  g.modelDepthSort = MODEL_DEPTH_SORT_LAST;

  InitializeSurface(*this, mtrTransSortOpaque.Get(), vertexBuffer[2],
                    indexBuffer[2]);
  InitializeSurface(*this, mtrTransSort.Get(), vertexBuffer[2],
                    indexBuffer[2]);
  InitializeSurface(*this, mtrTransSortGlare.Get(), vertexBuffer[2],
                    indexBuffer2[2]);

  sortParms = AllocateTransparency<transSortParms_t>(
      "w:\\build\\tech5\\engine\\models\\transparency\\RenderModelTransparency.cpp(269) : TAG_TRANSPARENCY",
      1, false);
  sortResult = AllocateTransparency<transSortResult_t>(
      "w:\\build\\tech5\\engine\\models\\transparency\\RenderModelTransparency.cpp(270) : TAG_TRANSPARENCY",
      1, false);
  influenceSpheres[0] = AllocateTransparency<influenceSphere_t>(
      "w:\\build\\tech5\\engine\\models\\transparency\\RenderModelTransparency.cpp(273) : TAG_TRANSPARENCY",
      kMaximumInfluenceSpheres, false);
  influenceSpheres[1] = AllocateTransparency<influenceSphere_t>(
      "w:\\build\\tech5\\engine\\models\\transparency\\RenderModelTransparency.cpp(274) : TAG_TRANSPARENCY",
      kMaximumInfluenceSpheres, false);
  visibleInfluenceSpheres = AllocateTransparency<visibleInfluenceSpheres_t>(
      "w:\\build\\tech5\\engine\\models\\transparency\\RenderModelTransparency.cpp(275) : TAG_TRANSPARENCY",
      1, true);
  influenceSphereCullParms =
      AllocateTransparency<influenceSphereCullParms_t>(
          "w:\\build\\tech5\\engine\\models\\transparency\\RenderModelTransparency.cpp(276) : TAG_TRANSPARENCY",
          1, false);
  numInfluenceSpheres[0] = 0;
  numInfluenceSpheres[1] = 0;
  currentSphereIndex = 0;
}

idRenderModelTransparency::~idRenderModelTransparency() {
  idMem* memory = idMem::Singleton();
  memory->Free(sortParms, ALIGN_16);
  memory->Free(sortResult, ALIGN_16);
  memory->Free(influenceSpheres[0], ALIGN_16);
  memory->Free(influenceSpheres[1], ALIGN_16);
  memory->Free(visibleInfluenceSpheres, ALIGN_16);
  memory->Free(influenceSphereCullParms, ALIGN_16);
}

void idRenderModelTransparency::GenerateVisibleInfluenceSphereList(
    const idRenderView* currentRenderView,
    const idRenderView* nextRenderView,
    idRenderModelUpdateTools* updateTools) {
  (void)currentRenderView;
  if (r_showVisibleInfluenceSpheres.valueInteger != 0) {
    for (int index = 0; index < visibleInfluenceSpheres->numSpheres; ++index) {
      const influenceSphere_t& influence =
          visibleInfluenceSpheres->spheres[index];
      const float red = std::clamp(influence.angle * 0.022222223f, 0.0f, 1.0f);
      const float blue =
          std::clamp(influence.angle * -0.022222223f, 0.0f, 1.0f);
      const float green = std::clamp(0.5f - red - blue, 0.0f, 1.0f);
      idSphere sphere{};
      sphere.origin = influence.center;
      sphere.radius = influence.outerRadius;
      GetRenderDebug()->DebugSphere({red, green, blue, 1.0f}, sphere, 12, 0,
                                    false);
    }
  }

  if (r_useInfluenceSpheres.valueInteger == 0) {
    visibleInfluenceSpheres->numSpheres = 0;
    return;
  }

  influenceSphereCullParms->nearDist = r_znear.valueFloat;
  influenceSphereCullParms->farDist = r_foliageBigFadeDistMax.valueFloat;
  influenceSphereCullParms->fov.x =
      std::tan(nextRenderView->r.fov_x * 0.017453292f * 0.5f);
  influenceSphereCullParms->fov.y =
      std::tan(nextRenderView->r.fov_y * 0.017453292f * 0.5f);
  influenceSphereCullParms->viewFwd = nextRenderView->r.viewaxis[0];
  influenceSphereCullParms->viewLeft = nextRenderView->r.viewaxis[1];
  influenceSphereCullParms->viewUp = nextRenderView->r.viewaxis[2];
  influenceSphereCullParms->viewOrg = nextRenderView->r.vieworg;
  influenceSphereCullParms->influenceSpheres =
      influenceSpheres[currentSphereIndex];
  influenceSphereCullParms->numInfluenceSpheres =
      numInfluenceSpheres[currentSphereIndex];
  influenceSphereCullParms->visibleInfluenceSpheres = visibleInfluenceSpheres;
  currentSphereIndex ^= 1u;

  if (r_useInfluenceSphereCullJob.valueInteger == 1) {
    updateTools->renderModelJobList->AddJob(
        reinterpret_cast<jobRun_t>(CullInfluenceSpheresForView),
        influenceSphereCullParms);
  } else {
    CullInfluenceSpheresForView(influenceSphereCullParms);
  }
  updateTools->renderModelJobList->InsertSyncPoint(SYNC_SIGNAL);
  updateTools->renderModelJobList->InsertSyncPoint(SYNC_SYNCHRONIZE);
}

void idRenderModelTransparency::StartFrame() {
  numVertsAllocated = 0;
  numSegmentAllocations = 0;
  numVertsAllocatedUnsorted = 0;
  if (mappedVerts != nullptr && vertexBuffer[mappedBufferIndex].IsMapped())
    vertexBuffer[mappedBufferIndex].UnmapBuffer();
  if (mappedIndices != nullptr && indexBuffer[mappedBufferIndex].IsMapped())
    indexBuffer[mappedBufferIndex].UnmapBuffer();
  if (mappedIndices2 != nullptr && indexBuffer2[mappedBufferIndex].IsMapped())
    indexBuffer2[mappedBufferIndex].UnmapBuffer();
  if (mappedVertsUnsorted != nullptr &&
      vertexBufferUnsorted[mappedBufferIndex].IsMapped()) {
    vertexBufferUnsorted[mappedBufferIndex].UnmapBuffer();
  }
  mappedBufferIndex = (mappedBufferIndex + 1) % 3;

  if (r_useTransparencyBinning.valueInteger != 0) {
    if (binVerts == nullptr) {
      binVerts = AllocateTransparency<idTransparencyVert>(
          "w:\\build\\tech5\\engine\\models\\transparency\\RenderModelTransparency.cpp(509) : TAG_TRANSPARENCY",
          0x10000, false);
    }
    if (binIndices == nullptr) {
      binIndices = AllocateTransparency<std::uint16_t>(
          "w:\\build\\tech5\\engine\\models\\transparency\\RenderModelTransparency.cpp(512) : TAG_TRANSPARENCY",
          kMaximumSortedIndices, false);
    }
    mappedVerts = binVerts;
    mappedIndices = binIndices;
  } else {
    mappedVerts = static_cast<idTransparencyVert*>(
        vertexBuffer[mappedBufferIndex].MapBuffer(BM_WRITE_NOSYNC));
    mappedIndices = static_cast<std::uint16_t*>(
        indexBuffer[mappedBufferIndex].MapBuffer(BM_WRITE_NOSYNC));
    mappedIndices2 = static_cast<std::uint16_t*>(
        indexBuffer2[mappedBufferIndex].MapBuffer(BM_WRITE_NOSYNC));
  }
  mappedVertsUnsorted = static_cast<idTransparencyVert*>(
      vertexBufferUnsorted[mappedBufferIndex].MapBuffer(BM_WRITE_NOSYNC));
}

void idRenderModelTransparency::AddTransparencySortJob(
    idRenderModelUpdateTools* updateTools) {
  sortParms->collapse = r_useTransparencyCollapse.valueInteger != 0;
  sortParms->sort = r_useTransparencySort.valueInteger != 0;
  sortParms->numSegmentAllocations = numSegmentAllocations;
  sortParms->segmentInfo = segmentInfo;
  sortParms->verts = mappedVerts;
  sortParms->numQuadsToSort = numVertsAllocated >> 2;
  sortParms->quadDepths = currentQuadDepths;
  sortParms->sortResult = sortResult;
  sortParms->maxIndices = kMaximumSortedIndices;
  sortParms->indices = mappedIndices;
  sortParms->maxIndices2 = kMaximumEmissiveIndices;
  sortParms->indices2 = mappedIndices2;
  std::memset(currentEmissiveQuads, 0, kMaximumSortedQuads);
  sortParms->emissiveQuads = currentEmissiveQuads;
  updateTools->renderModelJobList->InsertSyncPoint(SYNC_SIGNAL);
  updateTools->renderModelJobList->InsertSyncPoint(SYNC_SYNCHRONIZE);
  if (r_useTransparencySortJobs.valueInteger == 1) {
    updateTools->renderModelJobList->AddJob(
        reinterpret_cast<jobRun_t>(TransparencySortJob), sortParms);
  } else {
    TransparencySortJob(sortParms);
  }
}

void idRenderModelTransparency::AddInfluenceSphere(
    const idVec3& center, const idVec3& directionalForce, float innerRadius,
    float outerRadius, float angle) {
  int& count = numInfluenceSpheres[currentSphereIndex];
  if (count >= kMaximumInfluenceSpheres)
    return;
  influenceSphere_t& sphere = influenceSpheres[currentSphereIndex][count++];
  sphere.center = center;
  sphere.directionalForce = directionalForce;
  sphere.innerRadius = innerRadius;
  sphere.outerRadius = outerRadius;
  sphere.angle = angle;
}

void idRenderModelTransparency::Init() {
  for (int index = 0; index < 3; ++index) {
    vertexBuffer[index].AllocBufferObject(nullptr, 0x200000, 3167,
                                          BU_DYNAMIC);
    indexBuffer[index].AllocBufferObject(nullptr, 196608, BU_DYNAMIC);
    indexBuffer2[index].AllocBufferObject(nullptr, 6144, BU_DYNAMIC);
  }
  segmentInfo = AllocateTransparency<transSortSegmentInfo_t>(
      "w:\\build\\tech5\\engine\\models\\transparency\\RenderModelTransparency.cpp(113) : TAG_TRANSPARENCY",
      kMaximumSegments, true);
  currentQuadDepths = AllocateTransparency<float>(
      "w:\\build\\tech5\\engine\\models\\transparency\\RenderModelTransparency.cpp(114) : TAG_TRANSPARENCY",
      kMaximumSortedQuads, true);
  currentEmissiveQuads = AllocateTransparency<std::uint8_t>(
      "w:\\build\\tech5\\engine\\models\\transparency\\RenderModelTransparency.cpp(115) : TAG_TRANSPARENCY",
      kMaximumSortedQuads, true);
  mappedVerts = nullptr;
  mappedIndices = nullptr;
  mappedIndices2 = nullptr;
  mappedBufferIndex = 0;
  numSegmentAllocations = 0;
  numVertsAllocated = 0;
  binVerts = nullptr;
  binIndices = nullptr;

  for (idVertexBuffer& buffer : vertexBufferUnsorted)
    buffer.AllocBufferObject(nullptr, 0x100000, 3167, BU_DYNAMIC);

  ScopedTempArray<std::uint16_t> indices(
      "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP",
      49152);
  std::uint16_t* write = indices.Get();
  for (int quad = 0; quad < kMaximumUnsortedQuads; ++quad) {
    write[quad * 6 + 0] = static_cast<std::uint16_t>(quad * 4 + 0);
    write[quad * 6 + 1] = static_cast<std::uint16_t>(quad * 4 + 2);
    write[quad * 6 + 2] = static_cast<std::uint16_t>(quad * 4 + 3);
    write[quad * 6 + 3] = static_cast<std::uint16_t>(quad * 4 + 0);
    write[quad * 6 + 4] = static_cast<std::uint16_t>(quad * 4 + 3);
    write[quad * 6 + 5] = static_cast<std::uint16_t>(quad * 4 + 1);
  }
  indexBufferUnsorted.AllocBufferObject(write, 98304, BU_DEFAULT);
}

void idRenderModelTransparency::GenerateTransparencyRenderList(
    const idRenderView* currentView) {
  int numSortedQuads = sortResult->numSortedQuads;
  const int numOpaqueQuads = sortResult->numOpaqueQuads;
  int numEmissiveQuads = r_useTransparencyEmissivePass.valueInteger != 0
                             ? sortResult->numEmissiveQuads
                             : 0;
  int numCollapsedVerts = sortResult->numCollapsedVerts;

  if (r_useTransparencyBinning.valueInteger != 0 && binVerts != nullptr &&
      binIndices != nullptr) {
    deferredTransparencyBinParms_t parms{};
    parms.verts = binVerts;
    parms.indices = binIndices;
    parms.numVerts = sortResult->numCollapsedVerts;
    parms.numIndices = 6 * sortResult->numSortedQuads;
    parms.viewOrigin = currentView->r.vieworg;
    parms.viewAxis = currentView->r.viewaxis;
    parms.fovX = std::tan(currentView->r.fov_x * 0.017453292f * 0.5f);
    parms.fovY = std::tan(currentView->r.fov_y * 0.017453292f * 0.5f);

    ScopedTempArray<bin_t> bins(
        "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP",
        1800);
    parms.bins = bins.Get();
    parms.numBins = 0;
    numEmissiveQuads = 0;
    ScopedTempArray<node_t> nodes(
        "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP",
        1804);
    parms.nodes = nodes.Get();
    parms.numNodes = 0;
    ScopedTempArray<idTransparencyVert> tempBinVerts(
        "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP",
        0x10000);
    parms.tempBinVerts = tempBinVerts.Get();
    ScopedTempArray<int> vertexRemap(
        "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP",
        0x10000);
    parms.vertexRemap = vertexRemap.Get();
    parms.binVerts = static_cast<idTransparencyVert*>(
        vertexBuffer[mappedBufferIndex].MapBuffer(BM_WRITE_NOSYNC));
    parms.numBinVerts = 0;
    parms.maxBinVerts = 0x10000;
    parms.binIndices = static_cast<std::uint16_t*>(
        indexBuffer[mappedBufferIndex].MapBuffer(BM_WRITE_NOSYNC));
    parms.numBinIndices = 0;
    parms.maxBinIndices = kMaximumSortedIndices;
    BinQuads(&parms);
    indexBuffer[mappedBufferIndex].UnmapBuffer();
    vertexBuffer[mappedBufferIndex].UnmapBuffer();
    numCollapsedVerts = parms.numBinVerts;
  }

  const int transparentIndices = 6 * (numSortedQuads - numOpaqueQuads);
  idRenderModelSurface& opaqueSurface = surfaces[0];
  opaqueSurface.material = mtrTransSortOpaque.Get();
  opaqueSurface.geometry->numVerts = numCollapsedVerts;
  opaqueSurface.geometry->numIndexes = 6 * numOpaqueQuads;
  opaqueSurface.geometry->indexBuffer.Reference(indexBuffer[mappedBufferIndex]);
  opaqueSurface.geometry->vertexBuffer.Reference(vertexBuffer[mappedBufferIndex]);

  idRenderModelSurface& transparentSurface = surfaces[1];
  transparentSurface.material = mtrTransSort.Get();
  transparentSurface.geometry->numVerts = numCollapsedVerts;
  transparentSurface.geometry->numIndexes = transparentIndices;
  transparentSurface.geometry->indexBuffer.Reference(
      indexBuffer[mappedBufferIndex], 12 * numOpaqueQuads,
      2 * transparentIndices);
  transparentSurface.geometry->vertexBuffer.Reference(
      vertexBuffer[mappedBufferIndex]);

  idRenderModelSurface& emissiveSurface = surfaces[2];
  emissiveSurface.material = mtrTransSortGlare.Get();
  emissiveSurface.geometry->numVerts = 4 * numEmissiveQuads;
  emissiveSurface.geometry->numIndexes = 6 * numEmissiveQuads;
  emissiveSurface.geometry->indexBuffer.Reference(indexBuffer2[mappedBufferIndex]);
  emissiveSurface.geometry->vertexBuffer.Reference(vertexBuffer[mappedBufferIndex]);

  if (r_showTransparencyStats.valueInteger > 0) {
    idLib::Printf("Transparency Quads: %d\n", numCollapsedVerts >> 2);
    idLib::Printf("Transparency Emissive Quads: %d\n", numEmissiveQuads);
  }
  sortResult->numSortedQuads = 0;
  sortResult->numCollapsedVerts = 0;
  sortResult->numEmissiveQuads = 0;
}
