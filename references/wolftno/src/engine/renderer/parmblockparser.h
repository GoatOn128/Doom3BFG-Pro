#pragma once

#include "parmblock.h"
#include "shared/idlib/containers/BitArray.h"

#include <cstddef>

class idParser;
class idToken;
class idDeclTable;

class idParmBlockParser {
public:
  idParmBlockParser(idParser& source, idParmBlock& block);
  ~idParmBlockParser() = default;

  idBitArray usedTempParms;
  idList<expOp_t, 56>* ops;
  idList<parmValue_t, 56>* constants;
  parmValue_t defaultConstant;

private:
#if defined(WOLF_RENDERER_PARMBLOCKPARSER_TEST_ACCESS)
  friend struct idParmBlockParserValidationAccess;
#endif

  bool CleanupTemporaries();
  const idDeclRenderParm* GetExpressionTemporaryParm();
  expOp_t* GetExpressionOp(idParser& source,
                           const parmValue_t& constant);
  const idDeclRenderParm* GetConstantVector(idParser& source,
                                            const float* const value);
  const idDeclRenderParm* EmitOp(idParser& source,
                                 const idDeclRenderParm* parmA,
                                 const idDeclRenderParm* parmB,
                                 const idDeclTable* table,
                                 expOpType_t opType);
  const idDeclRenderParm* ParseEmitOp(idParser& source,
                                      const idDeclRenderParm* parmA,
                                      expOpType_t opType, int priority);
  const idDeclRenderParm* ParseIntrinsic(idParser& source,
                                         expOpType_t opType);
  const idDeclRenderParm* ParseTerm(idParser& source, idToken* fakeToken);
  const idDeclRenderParm* ParseExpressionPriority(idParser& source,
                                                  int priority);

  static const idDeclRenderParm* tempParms[32];
};

static_assert(sizeof(idParmBlockParser) == 56, "idParmBlockParser ABI drift");
