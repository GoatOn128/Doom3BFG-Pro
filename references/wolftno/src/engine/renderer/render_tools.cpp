#include "engine/renderer/render_tools.h"

#include <cmath>
#include <cstdint>

idCVar r_showPVS("r_showPVS", "0", CVAR_INTEGER, "");
idCVar r_showRuler("r_showRuler", "0", CVAR_BOOL, "");
idCVar r_showModelGroupMasters(
    "r_showModelGroupMasters", "0", CVAR_INTEGER,
    "draw lines from from models that use another model's origin for lighting "
    "and shadowing (1 = visible models, 2 = all models)");

namespace {

// These four render parms are private to Render_Tools.obj in the retail image.
// Their spelling and capitalization come directly from the initialized data in
// WolfNewOrder_x64.exe.
idCodeResource<idDeclRenderParm> rpLightProjectionS("LightProjectionS");
idCodeResource<idDeclRenderParm> rpLightProjectionT("lightProjectionT");
idCodeResource<idDeclRenderParm> rpLightProjectionR("lightProjectionR");
idCodeResource<idDeclRenderParm> rpLightProjectionQ("lightProjectionQ");

}  // namespace

idCVar r_showVmtrChannel(
    "r_showVmtrChannel", "0", CVAR_INTEGER,
    "Show VMTR components, 1 = Dffuse, 2 = Specular, 3 = Bump, 4 = Power, "
    "5 = Cover");

idList<drawSurf_t, 3> toolsViewList;

idCodeResource<idDeclRenderProg> progshowIntensity("showIntensity");
idCodeResource<idDeclRenderProg> progshowDepth("showDepth");
idCodeResource<idDeclRenderProg> proglandUnique("landUnique");
idCodeResource<idDeclRenderProg> progflatShadeReal("flatShadeReal");
idCodeResource<idDeclRenderProg> progshowTangentSpace("showTangentSpace");
idCodeResource<idDeclRenderProg> progShowTextureSpace("showTextureSpace");

idCVar r_showGlobalShadows(
    "r_showGlobalShadows", "0", CVAR_INTEGER,
    "1 = display the map as an image, 2 = draw the dynamics with the global "
    "shadow contributino");

idCodeResource<idDeclRenderProg> prog_landUniqueShowGlobalShadows(
    "landUniqueShowGlobalShadows");

idCVar r_testTextureReconstruct("r_testTextureReconstruct", "0", CVAR_BOOL,
                                "");
idCVar r_drawLoadedImages("r_drawLoadedImages", "0", CVAR_INTEGER,
                          "Draw loaded images scaled on the screen");
idCVar r_drawLoadedImagesScale("r_drawLoadedImagesScale", "64", CVAR_FLOAT,
                               "Shrink factor for r_drawLoadedImages");
idCVar testImageScale("testImageScale", "0.5", CVAR_FLOAT,
                      "Test image scale.");

idCodeResource<idDeclRenderProg> rp_progAutosprite1("autosprite1ColorOnly");
idCodeResource<idDeclRenderProg> rp_progAutosprite2("autosprite2ColorOnly");

bool idMat3::Compare(const idMat3& other, float epsilon) const {
  if (std::fabs(mat[0].x - other.mat[0].x) > epsilon) {
    return false;
  }
  if (std::fabs(mat[0].y - other.mat[0].y) > epsilon) {
    return false;
  }
  if (std::fabs(mat[0].z - other.mat[0].z) > epsilon) {
    return false;
  }
  if (std::fabs(mat[1].x - other.mat[1].x) > epsilon) {
    return false;
  }
  if (std::fabs(mat[1].y - other.mat[1].y) > epsilon) {
    return false;
  }
  if (std::fabs(mat[1].z - other.mat[1].z) > epsilon) {
    return false;
  }
  if (std::fabs(mat[2].x - other.mat[2].x) > epsilon) {
    return false;
  }
  if (std::fabs(mat[2].y - other.mat[2].y) > epsilon) {
    return false;
  }
  if (std::fabs(mat[2].z - other.mat[2].z) > epsilon) {
    return false;
  }
  return true;
}

idColor BrightColorForNumber(int number) {
  std::uint32_t state = static_cast<std::uint32_t>(number);
  std::uint8_t red;
  std::uint8_t green;
  std::uint8_t blue;

  do {
    state = state * 1103515245u + 12345u;
    red = static_cast<std::uint8_t>(state >> 23);
    state = state * 1103515245u + 12345u;
    green = static_cast<std::uint8_t>(state >> 23);
    state = state * 1103515245u + 12345u;
    blue = static_cast<std::uint8_t>(state >> 23);
  } while (static_cast<unsigned int>(red) +
                       static_cast<unsigned int>(green) +
                       static_cast<unsigned int>(blue) <
                   255u ||
               (red < 128u && green < 128u && blue < 128u));

  constexpr float byteToFloat = 1.0f / 255.0f;
  idColor color;
  color.a = 1.0f;
  color.r = static_cast<float>(red) * byteToFloat;
  color.g = static_cast<float>(green) * byteToFloat;
  color.b = static_cast<float>(blue) * byteToFloat;
  return color;
}
