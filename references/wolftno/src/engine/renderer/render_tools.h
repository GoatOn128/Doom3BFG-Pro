#pragma once

#include "engine/framework/coderesource.h"
#include "engine/renderer/jobs/render/render_types.h"
#include "shared/idlib/color.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

class idDeclRenderParm;
class idDeclRenderProg;

idColor BrightColorForNumber(int number);

extern idCVar r_showPVS;
extern idCVar r_showRuler;
extern idCVar r_showModelGroupMasters;
extern idCVar r_showVmtrChannel;
extern idCVar r_showGlobalShadows;
extern idCVar r_testTextureReconstruct;
extern idCVar r_drawLoadedImages;
extern idCVar r_drawLoadedImagesScale;
extern idCVar testImageScale;

extern idList<drawSurf_t, 3> toolsViewList;

extern idCodeResource<idDeclRenderProg> progshowIntensity;
extern idCodeResource<idDeclRenderProg> progshowDepth;
extern idCodeResource<idDeclRenderProg> proglandUnique;
extern idCodeResource<idDeclRenderProg> progflatShadeReal;
extern idCodeResource<idDeclRenderProg> progshowTangentSpace;
extern idCodeResource<idDeclRenderProg> progShowTextureSpace;
extern idCodeResource<idDeclRenderProg> prog_landUniqueShowGlobalShadows;
extern idCodeResource<idDeclRenderProg> rp_progAutosprite1;
extern idCodeResource<idDeclRenderProg> rp_progAutosprite2;
