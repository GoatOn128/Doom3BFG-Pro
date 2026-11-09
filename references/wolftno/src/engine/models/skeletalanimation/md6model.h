#pragma once

#include "engine/animation/md6.h"

class idCVar;

using idMD6ModelSkeletonLoader = idMD6Skel* (*)(const char* skeletonName);
using idMD6ModelGenerateBinary = bool (*)(const char* sourceFileName,
                                          const char* binaryFileName);
using idMD6ModelRewriteCallback = bool (*)(char* fileName);

void SetMD6ModelSkeletonLoader(idMD6ModelSkeletonLoader loader);
void SetMD6ModelGenerateBinary(idMD6ModelGenerateBinary generator);
void SetMD6ModelRewriteCallback(idMD6ModelRewriteCallback rewrite);

extern idCVar r_skipMergeMeshes;
extern idCVar r_skipGeneratedFileCheck;
