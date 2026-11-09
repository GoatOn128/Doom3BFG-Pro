#pragma once

#include "animprops.h"
#include "animwebscalar.h"
#include "blendparms.h"
#include "engine/decls/decl.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/math/vector.h"

#include <cstdint>

template <typename Tag> struct animWebIndex_t {
  std::int16_t value=-1;
  animWebIndex_t()=default; explicit animWebIndex_t(int v):value(static_cast<std::int16_t>(v)){}
#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
  template <typename OtherIndex>
    requires requires(const OtherIndex& index) { index.value; }
  animWebIndex_t(const OtherIndex& other)
      : value(static_cast<std::int16_t>(other.value)) {}
#endif
  operator int()const{return value;}
};
struct animWebStateTag{}; struct animWebSubWebTag{}; struct animWebNodeTag{}; struct animWebModelTag{}; struct animWebEdgeTag{};
using animWebStateIndex_t=animWebIndex_t<animWebStateTag>;
using animWebSubWebIndex_t=animWebIndex_t<animWebSubWebTag>;
using animWebNodeIndex_t=animWebIndex_t<animWebNodeTag>;
using animWebModelIndex_t=animWebIndex_t<animWebModelTag>;
using animWebEdgeIndex_t=animWebIndex_t<animWebEdgeTag>;
struct animWebStateList_t {
  animWebStateIndex_t stateIndex[4];
  std::uint16_t numStates = 0;
};

struct idDeclAnimWebTag { idAtomicString name; bool isDefault=false; };
struct idDeclAnimWebTagGroup { idAtomicString name; idList<idDeclAnimWebTag,32> tags; };
static_assert(sizeof(idDeclAnimWebTag)==16,"AnimWeb tag ABI drift");
static_assert(sizeof(idDeclAnimWebTagGroup)==32,"AnimWeb tag-group ABI drift");

class idDeclAnimWeb;
class idDeclAnimSubWeb;
class idDeclAnimWebNode;
struct idAnimWebEdgeCache { animWebNodeIndex_t destNodeIndex; std::uint8_t weightScale=255; std::uint8_t flags=0; };
class idDeclAnimWebEdge {
public:
  idDeclAnimWebEdge();
  virtual ~idDeclAnimWebEdge();
  void Parse(idDeclAnimWeb& decl,idDeclAnimWebNode& node,idAnimWebEdgeCache& edgeCache,idParser& source);
  void Parse(idDeclAnimSubWeb& decl,idDeclAnimWebNode& node,idAnimWebEdgeCache& edgeCache,idParser& source);
  animWebSubWebIndex_t destSubWebIndex;
  blendParms_t blendParms;
  std::uint16_t customFlags;
};

class idDeclAnimWeb final : public idDecl {
public:
  enum invalidEdgeIndex_t : int { INVALID_EDGE_INDEX = -1 };
  enum invalidSubWebIndex_t : int { INVALID_SUBWEB_INDEX = -1 };
  enum invalidStateIndex_t : int { INVALID_STATE_INDEX = -1 };
  enum invalidNodeIndex_t : int { INVALID_NODE_INDEX = -1 };
  enum invalidModelIndex_t : int { INVALID_MODEL_INDEX = -1 };
  enum invalidLayerIndex_t : int { INVALID_LAYER_INDEX = -1 };

  // This is a real nested retail type, not a typedef.  The animator compiland
  // uses the retail idIndex fields; declaration parser compilands retain the
  // layout-compatible parser representation.
#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
  class stateList_t {
  public:
    idIndex<std::int16_t, invalidStateIndex_t> stateIndex[4];
    std::uint16_t numStates = 0;
  };
#else
  class stateList_t : public animWebStateList_t {};
#endif
  struct idStateInfo { idAtomicString name; };
  struct idSubWebInfo {
#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
    idList<idIndex<std::int16_t, invalidNodeIndex_t>,32> nodes;
#else
    idList<animWebNodeIndex_t,32> nodes;
#endif
    idAtomicString name;
    idVec4 color{-1,-1,-1,-1};
    bool isHub=false;
    bool isVisible=true;
    bool used=false;
  };
  class idEdgeCache
#if !defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
      : public idAnimWebEdgeCache
#endif
  {
  public:
#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
    idIndex<std::int16_t, invalidNodeIndex_t> destNodeIndex;
    std::uint8_t weightScale = 255;
    std::uint8_t flags = 0;
#else
    idEdgeCache& operator=(const idAnimWebEdgeCache& other) {
      destNodeIndex = other.destNodeIndex;
      weightScale = other.weightScale;
      flags = other.flags;
      return *this;
    }
#endif
  };
  class idNodeCache {
  public:
    std::uint16_t numEdges = 0;
    std::uint8_t flags = 0;
    std::uint8_t pad2 = 0;
    std::uint32_t edgeIndexOffset = 0;
  };
  struct idLayerInfo { idAtomicString name; idList<idAtomicString,5> subWebsOnLayer; bool isVisible=true; };
  struct idSettings {
    idVec3 editPos{0.0f,0.0f,1.0f};
    int gridSize=32;
    bool snapToGrid=true;
    bool showGrid=true;
    bool alwaysShowIntraSubwebLinks=true;
    bool alwaysShowNodeProps=false;
    bool showAbbreviatedNodeProps=false;
    bool showIncomingLinks=false;
    bool isComposite=false;
    bool mergeModels=true;
    bool nodeProps[6]{true,true,false,true,true,true};
    int defaultBlendOutWindow=10;
    int defaultBlendDuration=10;
    int defaultBlendType=0;
    int subWebBlend=0;
  };

  idDeclAnimWeb();
  ~idDeclAnimWeb() override;
  const char* DefaultDefinition() const override;
  idDeclInfo* GetDeclInfo() const override;
  void Parse(idParser& source) override;
  void FreeData() override;
  std::uint64_t Size() const override;
  void SetupHashes();
#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
  const char* GetStateName(
      idIndex<std::int16_t, invalidStateIndex_t> index) const;
  __forceinline const char* GetStateName(animWebStateIndex_t index) const {
    return GetStateName(
        idIndex<std::int16_t, invalidStateIndex_t>(index.value));
  }
#else
  const char* GetStateName(animWebStateIndex_t index) const;
  const char* GetStateName(
      idIndex<std::int16_t, invalidStateIndex_t> index) const;
#endif
  idAtomicString GetStateNameAtomic(animWebStateIndex_t index) const;
#if defined(WOLFTNO_RETAIL_DRAGJOINTS_ANIMWEB_INDEX_ABI) || \
    defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
  idIndex<std::int16_t, invalidStateIndex_t>
  GetStateIndexForName(const char* name) const;
  idIndex<std::int16_t, invalidSubWebIndex_t>
  GetSubWebIndexForName(const char* name) const;
#else
  animWebStateIndex_t GetStateIndexForName(const char* name) const;
  animWebSubWebIndex_t GetSubWebIndexForName(const char* name) const;
#endif
#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
  const char* GetSubWebName(
      idIndex<std::int16_t, invalidSubWebIndex_t> index) const;
  __forceinline const char* GetSubWebName(animWebSubWebIndex_t index) const {
    return GetSubWebName(
        idIndex<std::int16_t, invalidSubWebIndex_t>(index.value));
  }
#else
  const char* GetSubWebName(animWebSubWebIndex_t index) const;
  const char* GetSubWebName(
      idIndex<std::int16_t, invalidSubWebIndex_t> index) const;
#endif
  idAtomicString GetSubWebNameAtomic(animWebSubWebIndex_t index) const;
#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
  idIndex<std::int16_t, invalidNodeIndex_t>
  FindNodeIndex(idIndex<std::int16_t, invalidSubWebIndex_t> subWeb,
                const stateList_t& stateValues, bool exactMatch) const;
  idIndex<std::int16_t, invalidNodeIndex_t>
  FindNodeIndex(idIndex<std::int16_t, invalidSubWebIndex_t> subWeb,
                idIndex<std::int16_t, invalidStateIndex_t> state) const;
  __forceinline animWebNodeIndex_t
  FindNodeIndex(animWebSubWebIndex_t subWeb, const stateList_t& stateValues,
                bool exactMatch) const {
    const auto result = FindNodeIndex(
        idIndex<std::int16_t, invalidSubWebIndex_t>(subWeb.value), stateValues,
        exactMatch);
    return animWebNodeIndex_t(result.value);
  }
  __forceinline animWebNodeIndex_t
  FindNodeIndex(animWebSubWebIndex_t subWeb,
                animWebStateIndex_t state) const {
    const auto result = FindNodeIndex(
        idIndex<std::int16_t, invalidSubWebIndex_t>(subWeb.value),
        idIndex<std::int16_t, invalidStateIndex_t>(state.value));
    return animWebNodeIndex_t(result.value);
  }
#else
  animWebNodeIndex_t FindNodeIndex(animWebSubWebIndex_t subWeb,const stateList_t& states,bool exactMatch) const;
  animWebNodeIndex_t FindNodeIndex(animWebSubWebIndex_t subWeb,animWebStateIndex_t state) const;
#endif
  animWebModelIndex_t FindModelIndex(const idDeclMD6* model) const;
#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
  const idList<idAnimWebScalar, 5>& GetScalars() const;
#else
  idList<idAnimWebScalar,32>& GetScalars();
#endif
#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
  idIndex<std::int16_t, invalidNodeIndex_t>
  GetNodeIndexForName(const char* subWeb, const char* state) const;
  idIndex<std::int16_t, invalidNodeIndex_t>
  FindNodeIndices(idIndex<std::int16_t, invalidSubWebIndex_t> subWeb,
                  const stateList_t& stateValues,
                  idList<idIndex<std::int16_t, invalidNodeIndex_t>>& result)
      const;
  __forceinline animWebNodeIndex_t
  FindNodeIndices(animWebSubWebIndex_t subWeb, const stateList_t& stateValues,
                  idList<animWebNodeIndex_t>& result) const {
    const auto exactResult = FindNodeIndices(
        idIndex<std::int16_t, invalidSubWebIndex_t>(subWeb.value), stateValues,
        reinterpret_cast<
            idList<idIndex<std::int16_t, invalidNodeIndex_t>>&>(result));
    return animWebNodeIndex_t(exactResult.value);
  }
#else
  animWebNodeIndex_t GetNodeIndexForName(const char* subWeb,const char* state) const;
  animWebNodeIndex_t FindNodeIndices(animWebSubWebIndex_t subWeb,const stateList_t& states,idList<animWebNodeIndex_t>& result) const;
#endif
  animWebNodeIndex_t AddNode(animWebSubWebIndex_t subWeb,const stateList_t& states);
  void AddNode(const char* subWebName,const idStaticList<idStr,4>& stateNames,animWebNodeIndex_t& nodeIndex,animWebSubWebIndex_t& subWebIndex,stateList_t& stateIndices);
  animWebSubWebIndex_t AddSubWeb(const char* name,bool hub=false);
  animWebEdgeIndex_t AllocEdge();
  static void Composite(const idList<const idDeclAnimWeb*,5>& mergeDecls,idDeclAnimWeb& output);
  void MergeSubWebs();
  void ParseSubWeb(idParser& source);

  idList<idDeclAnimWebNode*,32> nodes;
  idList<idSubWebInfo,32> subWebs;
  idList<idStateInfo,32> states;
  idList<const idDeclMD6*,32> models;
  idList<idDeclAnimWebEdge,32> edges;
  idList<idEdgeCache,32> edgeCache;
  idList<idNodeCache,32> nodeCache;
  idList<idAnimWebScalar,32> scalars;
  idList<idAtomicString,32> editorModels;
  idList<idLayerInfo,32> layers;
  idList<const idDeclAnimSubWeb*,32> subWebDecls;
  std::int16_t* edgeIndexCache;
  unsigned edgeIndexCacheSize;
  int changeId;
  bool expandSubWebLinks;
  idSettings settings;
  idHashIndex stateHash;
  idHashIndex subWebHash;
  int hashChangeId;
  static idDeclInfoTemplate<idDeclAnimWeb> resourceList;
};

class idDeclAnimWebNode {
public:
  enum animDelta_t : int {
    ANIMDELTA_DEFAULT = 0,
    ANIMDELTA_FULL = 1,
    ANIMDELTA_FULL_NOCLIP = 2,
    ANIMDELTA_FULL_GRAVITY = 3,
    ANIMDELTA_TURN = 4,
    ANIMDELTA_FULL_VELOCITY = 5,
    ANIMDELTA_FULL_DRIVE_AI = 6,
    ANIMDELTA_IGNORE = 7,
    ANIMDELTA_MAX = 8,
  };
  enum nodeFlag_t : int {
    NODEFLAG_START_TRANSITION = 0x01,
    NODEFLAG_STOP_TRANSITION = 0x02,
    NODEFLAG_STEP_TRANSITION = 0x04,
    NODEFLAG_TURN_TRANSITION = 0x08,
    NODEFLAG_BLENDOUT = 0x10,
    NODEFLAG_ADDITIVECAMERA = 0x20,
    NODEFLAG_GENERIC_TRANSITION = 0x40,
    NODEFLAG_HIDE = 0x80,
  };
  class idModelInfo {
  public:
    idModelInfo();
    idModelInfo(const idModelInfo& other);
    idModelInfo& operator=(const idModelInfo& other);
#if defined(WOLFTNO_RETAIL_BLEND_EQ_MODELINFO_DTOR_ABI)
    __forceinline ~idModelInfo() {
      SetNumCoordinateDimensions(0);
      UpdateTagGroupsList(0);
    }
#else
    ~idModelInfo();
#endif
    void SetNumCoordinateDimensions(int count);
    void SetCoordinate(int propIndex,const idList<float,5>& value);
    void AddTagGroup();
    void UpdateTagGroupsList(int count);
    void CopyTagGroups(const idModelInfo& other);
    void FixUpAnimVars();
    void Parse(idDeclAnimWeb& web,idParser& source);
    void Parse(idDeclAnimWeb& web,idDeclAnimWebNode& node,idParser& source);
    void Parse(idDeclAnimSubWeb& web,idDeclAnimWebNode& node,idParser& source);
    int NumTagGroups() const;
#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
    idIndex<std::int16_t, idDeclAnimWeb::invalidModelIndex_t> modelIndex;
#else
    animWebModelIndex_t modelIndex;
#endif
    idAtomicString blendEq;
    idAtomicString blendEqFixed;
    idList<idMD6AnimProps,32> animProps;
    idList<float,32>* coordinates;
    idList<idDeclAnimWebTagGroup,32>* tagGroups;
  };
  struct toolSettings_t { idVec3 pos{0.0f,0.0f,0.0f}; };

  idDeclAnimWebNode();
  idDeclAnimWebNode(animWebNodeIndex_t nodeIndex,animWebSubWebIndex_t subWebIndex,const animWebStateList_t& states);
  ~idDeclAnimWebNode();
  int MatchStates(const animWebStateList_t& states) const;
  void GetName(
#if defined(WOLFTNO_RETAIL_ANIMWEB_GETNAME_POINTER_ABI)
      const idDeclAnimWeb* web,
#else
      const idDeclAnimWeb& web,
#endif
      idStr& name) const;
  void GetName(const idDeclAnimSubWeb& web,idStr& name) const;
  void GetPath(const idDeclAnimWeb& web,idAnimWebPath& path) const;
  void GetStateName(const idDeclAnimWeb& web,idStr& name) const;
  void GetStateName(const idDeclAnimWeb& web,int state,idStr& name) const;
#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
  idModelInfo* FindModelInfo(
      idIndex<std::int16_t, idDeclAnimWeb::invalidModelIndex_t> model);
  const idModelInfo* FindModelInfo(
      idIndex<std::int16_t, idDeclAnimWeb::invalidModelIndex_t> model) const;
#else
  idModelInfo* FindModelInfo(animWebModelIndex_t model);
  const idModelInfo* FindModelInfo(animWebModelIndex_t model) const;
#endif
  void AddModelInfo(const idModelInfo& info,animWebModelIndex_t model);
  void CreateSubWebEdges(idDeclAnimWeb& web);
  bool Parse(idDeclAnimWeb& web,idParser& source);
  bool Parse(idDeclAnimSubWeb& web,idParser& source);
  int NumAnims(animWebModelIndex_t model) const;
  std::size_t Size() const;

#if defined(WOLFTNO_RETAIL_ANIMATOR_ANIMWEB_INDEX_ABI)
  idIndex<std::int16_t, idDeclAnimWeb::invalidNodeIndex_t> nodeIndex;
  idIndex<std::int16_t, idDeclAnimWeb::invalidSubWebIndex_t> subWebIndex;
  idDeclAnimWeb::stateList_t states;
#else
  animWebNodeIndex_t nodeIndex;
  animWebSubWebIndex_t subWebIndex;
  animWebStateList_t states;
#endif
  std::uint8_t animDelta;
  std::uint8_t flags;
  std::uint16_t customFlags;
  idList<std::int16_t,32> edges;
  idList<idModelInfo,32> modelInfo;
  toolSettings_t settings;
};

static_assert(sizeof(idDeclAnimWebNode::idModelInfo)==64);

class idDeclAnimSubWeb final : public idDecl {
public:
  struct idSubWebInfo_t { idAtomicString name; idList<animWebNodeIndex_t,32> nodes; };
  idDeclAnimSubWeb();
  ~idDeclAnimSubWeb() override;
  const char* DefaultDefinition() const override;
  idDeclInfo* GetDeclInfo() const override;
  void Parse(idParser& source) override;
  void FreeData() override;
  animWebNodeIndex_t AddNode(animWebSubWebIndex_t subWeb,const idDeclAnimWeb::stateList_t& states);
  void AddNode(const char* subWebName,const idStaticList<idStr,4>& stateNames,animWebNodeIndex_t& nodeIndex,animWebSubWebIndex_t& subWebIndex,idDeclAnimWeb::stateList_t& stateIndices);
  animWebSubWebIndex_t AddSubWeb(const char* name);
  animWebEdgeIndex_t AllocEdge();
  int changeId;
  idVec4 color;
  bool isHub;
  bool isVisible;
  idList<idAtomicString,32> models;
  idList<idAtomicString,32> states;
  idList<idSubWebInfo_t,32> subWebs;
  idList<idDeclAnimWebNode*,32> nodes;
  idList<idDeclAnimWebEdge,32> edges;
  idList<idDeclAnimWeb::idEdgeCache,32> edgeCache;
  static idDeclInfoTemplate<idDeclAnimSubWeb> resourceList;
};

static_assert(sizeof(idDeclAnimWebNode)==88,"AnimWeb node ABI drift");
static_assert(sizeof(idDeclAnimWebEdge)==56,"AnimWeb edge ABI drift");
static_assert(sizeof(idDeclAnimWeb)==504,"AnimWeb declaration ABI drift");
static_assert(sizeof(idDeclAnimSubWeb)==248,"AnimSubWeb declaration ABI drift");
