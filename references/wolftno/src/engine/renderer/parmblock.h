#pragma once

#include "engine/renderer/declrenderparm.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"

#include <cstring>

// The implementation is owned by renderer/parmblock.cpp.  Keep this callable
// surface in sync with the retail PDB: the second argument to the Get methods
// is a parent parm block, not a pointer to a default scalar/vector value.

class idFile;

enum expOpType_t : int {
  OP_TYPE_UNMASKED_MOVE_CONSTANT = 0,
  OP_TYPE_UNMASKED_MOVE = 1,
  OP_TYPE_MOVE_CONSTANT = 2,
  OP_TYPE_MOVE = 3,
  OP_TYPE_SWIZZLE = 4,
  OP_TYPE_ADD = 5,
  OP_TYPE_SUBTRACT = 6,
  OP_TYPE_MULTIPLY = 7,
  OP_TYPE_DOT3 = 8,
  OP_TYPE_DOT4 = 9,
  OP_TYPE_DIVIDE = 10,
  OP_TYPE_MOD = 11,
  OP_TYPE_GT = 12,
  OP_TYPE_GE = 13,
  OP_TYPE_LT = 14,
  OP_TYPE_LE = 15,
  OP_TYPE_EQ = 16,
  OP_TYPE_NE = 17,
  OP_TYPE_AND = 18,
  OP_TYPE_OR = 19,
  OP_TYPE_TABLE = 20
};

struct expOp_t {
  unsigned short type;
  short parmIndexDest;
  short parmIndexA;
  short parmIndexB;

  const idDeclRenderParm* GetDest() const;
  const idDeclRenderParm* GetA() const;
};

class idParmBlock {
public:
  idParmBlock();
  idParmBlock(const idParmBlock& other);
  ~idParmBlock() noexcept(false);
  idParmBlock& operator=(const idParmBlock& other);

  bool SetsRenderParm(const idDeclRenderParm* parm) const;
  void Print() const;
  bool operator==(const idParmBlock& other) const;
  bool operator!=(const idParmBlock& other) const { return !(*this == other); }
  void Clear();
  bool IsEqualSimple(const idParmBlock& other) const;
  float GetFloat(const idDeclRenderParm* parm,
                 const idParmBlock* parentBlock = nullptr) const;
  int GetInteger(const idDeclRenderParm* parm,
                 const idParmBlock* parentBlock = nullptr) const;
  idVec4 GetVector(const idDeclRenderParm* parm,
                   const idParmBlock* parentBlock = nullptr) const;
  const idImage* GetImage(const idDeclRenderParm* parm,
                          const idParmBlock* parentBlock = nullptr) const;
  const idDeclRenderProg* GetProgram(const idDeclRenderParm* parm,
                                     const idParmBlock* parentBlock = nullptr) const;
  const char* GetString(const idDeclRenderParm* parm,
                        const idParmBlock* parentBlock = nullptr) const;
  idStr* WriteString() const;
  void Save(idFile* file) const;
  bool Load(idFile* file);
  void RemoveRedundantOperations();
  void CopyFrom(const idParmBlock& other);
  void AddOp(const expOp_t& operation, const parmValue_t& constant);
  void ClearParm(const idDeclRenderParm* parm);
  void Append(const idParmBlock& other);
  void SetParm(const idDeclRenderParm* parm, const parmValue_t& value);
  void SetParm(const idDeclRenderParm* parm,
               const idDeclRenderParm* sourceParm);
  void SetParm(const idDeclRenderParm* parm, const idVec4& value);
  void SetParm(const idDeclRenderParm* parm, float value);
  void SetFloat(const idDeclRenderParm* parm, float value);
  void SetVec4(const idDeclRenderParm* parm, const idVec4& value);
  void SetImage(const idDeclRenderParm* parm, const idImage* image);

  idList<expOp_t, 56> ops;
  idList<parmValue_t, 56> constants;

private:
  static void WriteConstant(idStr& string, parmType_t parmType,
                            const parmValue_t& parmValue);
  static void WriteTerm(idStr& string, const idDeclRenderParm* term);
  void GetRenderParmFromOps_r(parmValue_t& parmValue, int numOps,
                              int parmIndex,
                              const idParmBlock* parentBlock) const;
};

template <int Capacity> class idStaticParmBlock : public idParmBlock {
public:
  idStaticParmBlock() {
    ops.list = staticOps;
    ops.num = 0;
    ops.size = Capacity;
    ops.granularity = 1;
    ops.memTag = 56;
    ops.listStatic = 1;
    constants.list = staticConstants;
    constants.num = 0;
    constants.size = Capacity;
    constants.granularity = 1;
    constants.memTag = 56;
    constants.listStatic = 1;
  }
  idStaticParmBlock(const idStaticParmBlock& other) : idStaticParmBlock() {
    *this = other;
  }
  ~idStaticParmBlock() = default;
  idStaticParmBlock& operator=(const idStaticParmBlock& other) {
    const int numOps = other.ops.num;
    const int numConstants = other.constants.num;
    ops.SetNum(numOps);
    constants.SetNum(numConstants);
    std::memmove(staticOps, other.staticOps,
                 sizeof(staticOps[0]) * numOps);
    std::memmove(staticConstants, other.staticConstants,
                 sizeof(staticConstants[0]) * numConstants);
    return *this;
  }

  expOp_t staticOps[Capacity];
  parmValue_t staticConstants[Capacity];
};

static_assert(sizeof(expOp_t) == 8, "expOp_t ABI drift");
static_assert(sizeof(idParmBlock) == 48, "idParmBlock ABI drift");
static_assert(sizeof(idStaticParmBlock<32>) == 816,
              "idStaticParmBlock<32> ABI drift");
