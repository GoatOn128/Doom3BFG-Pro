#pragma once

#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <cstdint>

void DefaultUncompressedPageData(std::uint8_t* pageData);

extern idCVar vt_maxMonoSpecularError;
extern idCVar vt_useSpecularScale;
extern idCVar vt_specularThreshold;
extern idCVar vt_specularScale;
extern idCVar vt_qualityDCTLuma;
extern idCVar vt_qualityDCTChroma;
extern idCVar vt_qualityDCTNormal;
extern idCVar vt_qualityDCTSpecular;
extern idCVar vt_qualityDCTPower;
extern idCVar vt_qualityHDPDiffuse;
extern idCVar vt_qualityHDPNormal;
extern idCVar vt_qualityHDPSpecular;
extern idCVar vt_qualityHDPPower;
extern idCVar vt_qualityHDPLossless;
extern idCVar vt_qualityJXRDiffuse;
extern idCVar vt_qualityJXRNormal;
extern idCVar vt_qualityJXRSpecular;
extern idCVar vt_qualityJXRPower;
extern idCVar vt_qualityJXRLossless;
