#include "engine/guis/swf/swf.h"

#include "engine/decls/declmanager.h"
#include "engine/guis/font.h"
#include "engine/guis/swf/swf_render.h"
#include "engine/guis/swf/swf_spriteinstance.h"
#include "engine/guis/swf/swf_textinstance.h"
#include "engine/framework/resourcemanager.h"
#include "engine/framework/keyinput.h"
#include "engine/framework/common.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/rendermodelgui.h"
#include "engine/sound/soundsystem.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr float kAlphaEpsilon = 0.001f;

idCVar swf_forceAlpha("swf_forceAlpha", "0", CVAR_FLOAT,
                      "force alpha on all SWF elements", 0.0f, 1.0f);
idCVar swf_skipRender("swf_skipRender", "0", CVAR_BOOL, "skip SWF rendering");
idCVar swf_skipRenderText("swf_skipRenderText", "0", CVAR_BOOL,
                          "skip SWF text rendering");
idCVar swf_skipRenderMask("swf_skipRenderMask", "0", CVAR_BOOL,
                          "skip SWF masks");
idCVar swf_skipTooltipIcons("swf_skipTooltipIcons", "0", CVAR_BOOL,
                            "skip SWF tooltip icons");
idCVar swf_timescale("swf_timescale", "1", CVAR_FLOAT, "SWF timescale");
idCVar swf_stopat("swf_stopat", "0", CVAR_FLOAT, "stop at a frame");
idCVar swf_enableSnapOnPixel("swf_enableSnapOnPixel", "0", CVAR_BOOL,
                             "snap SWF geometry to pixels");
idCVar swf_safeFrame("swf_safeFrame", "0.025", CVAR_FLOAT | CVAR_ARCHIVE,
                     "space between UI elements and screen edge");
idCVar swf_tweakSafeFrame("swf_tweakSafeFrame", "0.5", CVAR_FLOAT | CVAR_ARCHIVE,
                           "safe-frame inset");
idCVar swf_tweakSafeAreaWidth("swf_tweakSafeAreaWidth", "0.1", CVAR_FLOAT | CVAR_ARCHIVE,
                               "safe area width tweak");
idCVar swf_tweakSafeAreaHeight("swf_tweakSafeAreaHeight", "0.1", CVAR_FLOAT | CVAR_ARCHIVE,
                                "safe area height tweak");
idCVar swf_ignoreVisible("swf_ignoreVisible", "0", 0,
                         "ignore sprite visibility");
idCVar swf_radialtestinnerx("swf_radialtestinnerx", "10.0", CVAR_FLOAT,
                             "radial test inner x");
idCVar swf_radialtestinnery("swf_radialtestinnery", "10.0", CVAR_FLOAT,
                             "radial test inner y");
idCVar swf_skipFilesNotMatching("swf_skipFilesNotMatching", "", CVAR_GUI,
                                 "only render matching SWF files");
idCVar swf_debugMouse("swf_debugMouse", "0", CVAR_BOOL, "debug SWF mouse");

idVec4 ToVec4(const swfColorRGBA_t& color) {
  constexpr float scale = 1.0f / 255.0f;
  return {color.r * scale, color.g * scale, color.b * scale, color.a * scale};
}

idVec4 Lerp(const idVec4& from, const idVec4& to, float fraction) {
  return {from.x + (to.x - from.x) * fraction,
          from.y + (to.y - from.y) * fraction,
          from.z + (to.z - from.z) * fraction,
          from.w + (to.w - from.w) * fraction};
}

idVec2 Lerp(const idVec2& from, const idVec2& to, float fraction) {
  return {from.x + (to.x - from.x) * fraction,
          from.y + (to.y - from.y) * fraction};
}

void SetPacked(std::uint8_t (&bytes)[4], std::uint32_t value) {
  std::memcpy(bytes, &value, sizeof(value));
}

void InitializeVertex(idDrawVert& vertex, const idVec2& position, float z,
                      const idVec2& st, std::uint32_t multiply,
                      std::uint32_t add) {
  std::memset(&vertex, 0, sizeof(vertex));
  vertex.xyz = {position.x, position.y, z};
  vertex.st = st;
  SetPacked(vertex.normal, 0x7F000000u);
  SetPacked(vertex.tangent, add);
  SetPacked(vertex.color, multiply);
}

swfColorXform_t ApplyColor(const idVec4& base, const swfColorXform_t& transform) {
  swfColorXform_t local;
  local.mul = base;
  swfColorXform_t result = local.Multiply(transform);
  if (swf_forceAlpha.GetFloat() > 0.0f) {
    result.mul.w = swf_forceAlpha.GetFloat();
    result.add.w = 0.0f;
  }
  return result;
}

std::uint32_t PackAdd(const idVec4& add) {
  return idColor::PackColor({add.x * 0.5f + 0.5f, add.y * 0.5f + 0.5f,
                             add.z * 0.5f + 0.5f, add.w * 0.5f + 0.5f});
}

idVec4 QuadPoint(const swfMatrix_t& matrix, float x, float y, float s, float t) {
  const idVec2 point = matrix.Transform({x, y});
  return {point.x, point.y, s, t};
}

} // namespace

float tooltipIcon_t::GetRenderWidth(float maxGlyphHeight, float toolTipScale) const {
  return toolTipScale >= 0.0001f
             ? toolTipScale * 2.0f * maxGlyphHeight
             : static_cast<float>(imageWidth) / static_cast<float>(imageHeight) *
                   maxGlyphHeight;
}

std::uint64_t idSWF::GLStateForRenderState(const swfRenderState_t& renderState) const {
  std::uint64_t stencil = 0;
  if (renderState.activeMasks > 0)
    stencil = (static_cast<std::uint64_t>(
                   static_cast<std::uint8_t>(renderState.activeMasks + 1) | 0x5FF00u)
               << 20);
  switch (renderState.blendMode) {
    case 3:
    case 13:
    case 14: return stencil | 0x2A;
    case 4: return stencil | 0x4000A;
    case 5: return stencil | 0xC0008;
    case 6: return stencil | 0x80008;
    case 7:
    case 9: return stencil | 0x40008;
    case 8: return stencil | 8;
    default: return stencil | 0x28;
  }
}

swfMatrix_t swfMatrix_t::Inverse() const {
  const float determinant = xx * yy - xy * yx;
  if (std::fabs(determinant) < 1.17549435e-38f)
    return *this;
  const float inverse = 1.0f / determinant;
  swfMatrix_t result;
  result.xx = yy * inverse;
  result.yy = xx * inverse;
  result.xy = -xy * inverse;
  result.yx = -yx * inverse;
  result.tx = 0.0f;
  result.ty = 0.0f;
  return result;
}

swfColorXform_t swfColorXform_t::Multiply(const swfColorXform_t& other) const {
  swfColorXform_t result;
  for (int i = 0; i < 4; ++i) {
    result.mul[i] = mul[i] * other.mul[i];
    result.add[i] = add[i] * other.mul[i] + other.add[i];
  }
  return result;
}

TextIterator::stepResult_t TextIterator::StepForward() {
  if (m_charIndex >= m_endIndex)
    return STEP_END;
  if (idStr::IsColor(m_text->data + m_charIndex)) {
    m_charIndex += 2;
    return STEP_COLOR;
  }

  m_prevTC = m_currTC;
  m_currTC = m_text->UTF8Char(&m_charIndex);
  if (m_currTC == '\t') {
    m_lastBreakableCharIndex = ++m_charIndex;
    return STEP_TAB;
  }
  if (m_currTC == '\n') {
    ++m_numLines;
    m_lastBreakableCharIndex = m_charIndex;
    return STEP_NEWLINE;
  }
  if (m_tooltipIconList != nullptr && m_iconIndex < m_tooltipIconList->Num()) {
    const tooltipIcon_t& icon = (*m_tooltipIconList)[m_iconIndex];
    if (m_charIndex == icon.startIndex) {
      m_insertingImage = true;
      return STEP_ICON_START;
    }
    if (m_charIndex == icon.endIndex) {
      m_insertingImage = false;
      ++m_iconIndex;
    }
    if (m_insertingImage)
      return STEP_INSIDE_ICON;
  }
  int nextIndex = m_charIndex;
  const std::uint32_t next = m_text->UTF8Char(&nextIndex);
  if (idSWFTextInstance::CanBreakLine(m_currTC, next))
    m_lastBreakableCharIndex = m_charIndex;
  return STEP_GLYPH;
}

bool idSWF::LoadAtlasImage(bool force) {
  if (atlasImage == nullptr)
    return false;
  imageState_t state = atlasImage->ImageState();
  if (state == IMAGE_STATE_LOADED) {
    if (atlasPurgable)
      MarkImageForPurging(atlasImage);
    if (atlasFile != nullptr && resourceManager != nullptr &&
        !resourceManager->IsResourceFile()) {
      if (idLib::production == PROD_DEVELOPMENT ||
          resourceManager->GetCacheFile(0) != nullptr)
        delete atlasFile;
      atlasFile = nullptr;
    }
    return true;
  }
  if (state != IMAGE_STATE_LOADING) {
    std::uint64_t offset = 0;
    if (idLib::production == PROD_PRODUCTION || idLib::production == PROD_LOADED) {
      std::uint64_t compressed = 0;
      std::uint64_t uncompressed = 0;
      std::uint32_t streamOffset = 0;
      std::uint8_t fileId = 0;
      if (resourceManager == nullptr ||
          !resourceManager->GetCacheFileInfo(atlasTextureName.str,
                                             reinterpret_cast<std::int64_t*>(&offset),
                                             &compressed, &uncompressed, &streamOffset,
                                             &fileId, false))
        return false;
      atlasFile = resourceManager->GetCacheFile(fileId);
      offset += 72;
    } else {
      atlasFile = fileSystem != nullptr ? fileSystem->OpenFileRead(atlasTextureName.str) : nullptr;
      offset = 72;
    }
    if (atlasFile == nullptr) {
      if (idLib::production != PROD_PRODUCTION && idLib::production != PROD_LOADED)
        idLib::Error("Required SWF atlas file '%s' could not be opened.",
                     atlasTextureName.str);
      return false;
    }
    atlasImage->BackgroundRead(atlasFile, offset);
  }
  if (!force)
    return false;
  bool startedTerminationThread = false;
  if (common != nullptr && common->IsTerminationThreadStopped()) {
    common->BeginTerminationThread();
    startedTerminationThread = true;
  }
  while (atlasImage->ImageState() == IMAGE_STATE_LOADING) {
    if (common != nullptr && common->IsDialogActive())
      break;
    Sys_Sleep(10);
  }
  if (startedTerminationThread)
    common->EndTerminationThread();
  return atlasImage->ImageState() == IMAGE_STATE_LOADED;
}

void idSWF::DrawEditCursor(idRenderModelGui* gui, float x, float y, float z,
                           float width, float height, const swfMatrix_t& matrix) {
  const idVec4 topLeft = QuadPoint(matrix, x, y, 0, 0);
  const idVec4 topRight = QuadPoint(matrix, x + width, y, 1, 0);
  const idVec4 bottomRight = QuadPoint(matrix, x + width, y + height, 1, 1);
  const idVec4 bottomLeft = QuadPoint(matrix, x, y + height, 0, 1);
  gui->DrawStretchPic(topLeft, topRight, bottomRight, bottomLeft, white, z);
}

void idSWF::DrawEditTextImages(const idList<swfGlyphIconInfo_t, 5>& images,
                               idRenderModelGui* gui) {
  for (int i = 0; i < images.Num(); ++i) {
    const auto& item = images[i];
    gui->currentVertexColor = idColor::PackColor(
        {item.iconColor.r, item.iconColor.g, item.iconColor.b, item.iconColor.a});
    const idVec4 topLeft{item.iconTop.x, item.iconTop.y, 0, 0};
    const idVec4 topRight{item.iconTop.z, item.iconTop.w, 1, 0};
    const idVec4 bottomRight{item.iconBottom.z, item.iconBottom.w, 1, 1};
    const idVec4 bottomLeft{item.iconBottom.x, item.iconBottom.y, 0, 1};
    gui->DrawStretchPic(topLeft, topRight, bottomRight, bottomLeft,
                        item.iconMat, item.zDepth);
  }
}

void idSWF::DrawEditTextSelections(
    const idList<swfGlyphSelectionInfo_t, 5>& selections, idRenderModelGui* gui) {
  if (selections.Num() == 0)
    return;
  const auto& left = selections[0];
  const auto& right = selections[selections.Num() - 1];
  gui->currentVertexColor = idColor::PackColor(
      {left.selColor.r, left.selColor.g, left.selColor.b, left.selColor.a});
  const idVec4 topLeft{left.selTop.x, left.selTop.y, 0, 0};
  const idVec4 topRight{right.selTop.z, right.selTop.w, 1, 0};
  const idVec4 bottomRight{right.selBottom.z, right.selBottom.w, 1, 1};
  const idVec4 bottomLeft{left.selBottom.x, left.selBottom.y, 0, 1};
  gui->DrawStretchPic(topLeft, topRight, bottomRight, bottomLeft,
                      left.selMat, left.zDepth);
}

void idSWF::DrawEditTextGlyphs(const idList<swfGlyphDrawInfo_t, 5>& glyphs,
                               idRenderModelGui* gui) {
  for (int i = 0; i < glyphs.Num(); ++i) {
    const auto& item = glyphs[i];
    gui->currentVertexColor = idColor::PackColor(
        {item.fontColor.r, item.fontColor.g, item.fontColor.b, item.fontColor.a});
    const idVec4 topLeft{item.glyphTop.x, item.glyphTop.y, item.s.x, item.t.x};
    const idVec4 topRight{item.glyphTop.z, item.glyphTop.w, item.s.y, item.t.x};
    const idVec4 bottomRight{item.glyphBottom.z, item.glyphBottom.w, item.s.y, item.t.y};
    const idVec4 bottomLeft{item.glyphBottom.x, item.glyphBottom.y, item.s.x, item.t.y};
    gui->DrawStretchPic(topLeft, topRight, bottomRight, bottomLeft,
                        item.glyphMat, item.zDepth);
  }
}

void idSWF::RenderShape(idRenderModelGui* gui, const idSWFShape* shape,
                        const swfRenderState_t& renderState) {
  if (shape == nullptr) {
    idLib::Warning("%s: RenderShape: shape == NULL", filename.str);
    return;
  }

  for (int fillIndex = 0; fillIndex < shape->fillDraws.Num(); ++fillIndex) {
    const idSWFShapeDrawFill& fill = shape->fillDraws[fillIndex];
    const idMaterial* material = renderState.material;
    idVec4 baseColor{1, 1, 1, 1};
    swfMatrix_t inverse;
    idVec2 size{1, 1};
    idVec2 atlasScale{0, 0};
    idVec2 atlasBias{0, 0};
    bool useAtlas = false;

    if (material != nullptr) {
      inverse.xx = inverse.yy = 0.05f;
    } else if (fill.style.type == 0) {
      material = guiSolid;
      baseColor = ToVec4(fill.style.startColor);
    } else if (fill.style.type == 4 && fill.style.bitmapID != 0xffff) {
      const idSWFDictionaryEntry* entry = FindDictionaryEntry(fill.style.bitmapID);
      material = atlasMaterial;
      if (entry == nullptr || material == nullptr)
        continue;
      size = {static_cast<float>(entry->imageSize.x),
              static_cast<float>(entry->imageSize.y)};
      const float atlasW = static_cast<float>(
          std::max(atlasImage != nullptr ? atlasImage->opts.width : atlasWidth, 1));
      const float atlasH = static_cast<float>(
          std::max(atlasImage != nullptr ? atlasImage->opts.height : atlasHeight, 1));
      atlasScale = {size.x / atlasW, size.y / atlasH};
      atlasBias = {entry->imageAtlasOffset.x / atlasW,
                   entry->imageAtlasOffset.y / atlasH};
      baseColor = entry->channelScale;
      inverse = fill.style.startMatrix.Inverse();
      useAtlas = true;
    } else {
      material = guiSolid;
    }

    const swfColorXform_t color = ApplyColor(baseColor, renderState.cxf);
    if (color.mul.w + color.add.w <= kAlphaEpsilon)
      continue;
    if (renderState.materialWidth > 0)
      size.x = static_cast<float>(renderState.materialWidth);
    if (renderState.materialHeight > 0)
      size.y = static_cast<float>(renderState.materialHeight);
    const idVec2 inverseSize{1.0f / size.x, 1.0f / size.y};
    gui->SetGLState(GLStateForRenderState(renderState));
    idDrawVert* vertices = gui->AllocTris(fill.startVerts.Num(), fill.indices.list,
                                          fill.indices.Num(), material);
    if (vertices == nullptr)
      continue;
    const std::uint32_t packedMul = idColor::PackColor(color.mul);
    const std::uint32_t packedAdd = PackAdd(color.add);
    for (int i = 0; i < fill.startVerts.Num(); ++i) {
      const idVec2 source = fill.startVerts[i];
      idVec2 st{0, 0};
      if (fill.style.type != 0) {
        const idVec2 normalized{(source.x - shape->startBounds.tl.x) *
                                    inverseSize.x * 20.0f,
                                (source.y - shape->startBounds.tl.y) *
                                    inverseSize.y * 20.0f};
        st = inverse.Transform(normalized);
        if (useAtlas)
          st = {st.x * atlasScale.x + atlasBias.x,
                st.y * atlasScale.y + atlasBias.y};
        st.x = std::clamp(st.x, 0.001f, 0.999f);
        st.y = std::clamp(st.y, 0.001f, 0.999f);
      }
      InitializeVertex(vertices[i], renderState.matrix.Transform(source), renderState.z,
                       st, packedMul, packedAdd);
    }
  }

  for (int lineIndex = 0; lineIndex < shape->lineDraws.Num(); ++lineIndex) {
    const idSWFShapeDrawLine& line = shape->lineDraws[lineIndex];
    const swfColorXform_t color = ApplyColor(ToVec4(line.style.startColor), renderState.cxf);
    if (color.mul.w + color.add.w <= kAlphaEpsilon)
      continue;
    gui->SetGLState(GLStateForRenderState(renderState) | 0x800u);
    idDrawVert* vertices = gui->AllocTris(line.startVerts.Num(), line.indices.list,
                                          line.indices.Num(), white);
    if (vertices == nullptr)
      continue;
    const std::uint32_t packedMul = idColor::PackColor(color.mul);
    const std::uint32_t packedAdd = PackAdd(color.add);
    for (int i = 0; i < line.startVerts.Num(); ++i)
      InitializeVertex(vertices[i], renderState.matrix.Transform(line.startVerts[i]),
                       renderState.z, {0, 0}, packedMul, packedAdd);
  }
}

void idSWF::RenderMorphShape(idRenderModelGui* gui, const idSWFShape* shape,
                             float ratio, const swfRenderState_t& renderState) {
  if (shape == nullptr) {
    idLib::Warning("%s: RenderMorphShape: shape == NULL", filename.str);
    return;
  }
  for (int fillIndex = 0; fillIndex < shape->fillDraws.Num(); ++fillIndex) {
    const idSWFShapeDrawFill& fill = shape->fillDraws[fillIndex];
    const idMaterial* material = renderState.material;
    idVec4 base{1, 1, 1, 1};
    if (material == nullptr && fill.style.type == 0) {
      material = guiSolid;
      // The retail renderer takes the color interpolation value from the state.
      // RenderSprite stores the display-list ratio there before dispatching.
      base = Lerp(ToVec4(fill.style.startColor), ToVec4(fill.style.endColor),
                  renderState.ratio);
    } else if (material == nullptr && fill.style.type == 4 &&
               fill.style.bitmapID != 0xffff) {
      const idSWFDictionaryEntry* entry = FindDictionaryEntry(fill.style.bitmapID);
      material = entry != nullptr ? entry->material : nullptr;
    } else if (material == nullptr) {
      material = guiSolid;
    }
    if (material == nullptr)
      continue;
    const swfColorXform_t color = ApplyColor(base, renderState.cxf);
    if (color.mul.w + color.add.w <= kAlphaEpsilon)
      continue;
    gui->SetGLState(GLStateForRenderState(renderState));
    idDrawVert* vertices = gui->AllocTris(fill.startVerts.Num(), fill.indices.list,
                                          fill.indices.Num(), material);
    if (vertices == nullptr)
      continue;
    swfMatrix_t style;
    style.xx = fill.style.startMatrix.xx +
               (fill.style.endMatrix.xx - fill.style.startMatrix.xx) * ratio;
    style.yy = fill.style.startMatrix.yy +
               (fill.style.endMatrix.yy - fill.style.startMatrix.yy) * ratio;
    style.xy = fill.style.startMatrix.xy +
               (fill.style.endMatrix.xy - fill.style.startMatrix.xy) * ratio;
    style.yx = fill.style.startMatrix.yx +
               (fill.style.endMatrix.yx - fill.style.startMatrix.yx) * ratio;
    style.tx = fill.style.startMatrix.tx +
               (fill.style.endMatrix.tx - fill.style.startMatrix.tx) * ratio;
    style.ty = fill.style.startMatrix.ty +
               (fill.style.endMatrix.ty - fill.style.startMatrix.ty) * ratio;
    const swfMatrix_t inverse = style.Inverse();
    const idVec2 boundsTop = Lerp(shape->startBounds.tl, shape->endBounds.tl, ratio);
    float width = static_cast<float>(material->GetWidth());
    float height = static_cast<float>(material->GetHeight());
    if (renderState.materialWidth > 0)
      width = static_cast<float>(renderState.materialWidth);
    if (renderState.materialHeight > 0)
      height = static_cast<float>(renderState.materialHeight);
    if (width == 0.0f || height == 0.0f)
      continue;
    const float inverseWidth = 1.0f / width;
    const float inverseHeight = 1.0f / height;
    const std::uint32_t packedMul = idColor::PackColor(color.mul);
    const std::uint32_t packedAdd = PackAdd(color.add);
    for (int i = 0; i < fill.startVerts.Num(); ++i) {
      const idVec2 end = i < fill.endVerts.Num() ? fill.endVerts[i] : fill.startVerts[i];
      const idVec2 source = Lerp(fill.startVerts[i], end, ratio);
      const idVec2 normalized{(source.x - boundsTop.x) * inverseWidth * 20.0f,
                              (source.y - boundsTop.y) * inverseHeight * 20.0f};
      idVec2 st = inverse.Transform(normalized);
      st.x += inverseWidth * 0.5f;
      st.y += inverseHeight * 0.5f;
      InitializeVertex(vertices[i], renderState.matrix.Transform(source), renderState.z, st,
                       packedMul, packedAdd);
    }
  }
}

void idSWF::RenderRadial(idRenderModelGui* gui,
                         idSWFSpriteInstance* spriteInstance,
                         const swfRenderState_t& renderState) {
  if (spriteInstance == nullptr || spriteInstance->currentFrame <= 1)
    return;
  gui->SetGLState(GLStateForRenderState(renderState));
  for (int displayIndex = 0; displayIndex < spriteInstance->displayList.Num(); ++displayIndex) {
    idSWFDictionaryEntry* entry =
        FindDictionaryEntry(spriteInstance->displayList[displayIndex].characterID);
    if (entry == nullptr || entry->type != SWF_DICT_SHAPE || entry->shape == nullptr)
      continue;
    idVec2 minimum{100000.0f, 100000.0f};
    idVec2 maximum{-100000.0f, -100000.0f};
    idVec4 color{1, 1, 1, 1};
    for (int f = 0; f < entry->shape->fillDraws.Num(); ++f) {
      const idSWFShapeDrawFill& fill = entry->shape->fillDraws[f];
      color = ToVec4(fill.style.startColor);
      for (int v = 0; v < fill.startVerts.Num(); ++v) {
        const idVec2 point = renderState.matrix.Transform(fill.startVerts[v]);
        minimum.x = std::min(minimum.x, point.x);
        minimum.y = std::min(minimum.y, point.y);
        maximum.x = std::max(maximum.x, point.x);
        maximum.y = std::max(maximum.y, point.y);
      }
    }
    const int segments = std::min<int>(spriteInstance->currentFrame - 1, 199);
    if (segments <= 0)
      continue;
    std::uint16_t indices[600]{};
    for (int i = 0; i < segments; ++i) {
      indices[i * 3] = static_cast<std::uint16_t>(segments);
      indices[i * 3 + 1] = static_cast<std::uint16_t>(i);
      indices[i * 3 + 2] = static_cast<std::uint16_t>((i + 1) % 199);
    }
    idDrawVert* vertices = gui->AllocTris(segments + 1, indices, segments * 3, guiSolid);
    if (vertices == nullptr)
      continue;
    const idVec2 radius{(maximum.x - minimum.x) * 0.5f,
                        (maximum.y - minimum.y) * 0.5f};
    const idVec2 center{minimum.x + radius.x, minimum.y + radius.y};
    const std::uint32_t packedMul = idColor::PackColor(color);
    const std::uint32_t packedAdd = PackAdd({0, 0, 0, 0});
    for (int i = 0; i <= segments; ++i) {
      const float angle = (static_cast<float>(i) / 199.0f) * 6.2831855f;
      const idVec2 point{i == segments ? center.x : std::sin(angle) * radius.x + center.x,
                         i == segments ? center.y : center.y - std::cos(angle) * radius.y};
      InitializeVertex(vertices[i], point, 0.0f, {0, 0}, packedMul, packedAdd);
    }
  }
}

void idSWF::RenderMask(idRenderModelGui* gui, const swfDisplayEntry_t* mask,
                       const swfRenderState_t& renderState,
                       const idMaterial* material) {
  if (mask == nullptr)
    return;
  swfRenderState_t state;
  state.matrix = mask->matrix.Multiply(renderState.matrix);
  state.cxf = mask->cxf.Multiply(renderState.cxf);
  state.material = swf_skipRenderMask.GetBool() ? guiSolid : material;
  state.activeMasks = 0;
  state.ratio = mask->ratio;
  state.z = renderState.z;
  idSWFDictionaryEntry* entry = FindDictionaryEntry(mask->characterID);
  if (entry == nullptr)
    return;
  if (entry->type == SWF_DICT_SHAPE)
    RenderShape(gui, entry->shape, state);
  else if (entry->type == SWF_DICT_MORPH)
    RenderMorphShape(gui, entry->shape, mask->ratio, state);
}

void idSWF::RenderSprite(idRenderModelGui* gui,
                         idSWFSpriteInstance* spriteInstance,
                         const swfRenderState_t& renderState, int time,
                         bool isSplitscreen) {
  if (spriteInstance == nullptr) {
    idLib::Warning("%s: RenderSprite: spriteInstance == NULL", filename.str);
    return;
  }
  if ((!spriteInstance->isVisible && !swf_ignoreVisible.GetBool()) ||
      (renderState.cxf.mul.w + renderState.cxf.add.w <= kAlphaEpsilon &&
       swf_forceAlpha.GetFloat() <= 0.0f))
    return;
  if (parm != nullptr && swfResourceMenu != nullptr)
    gui->SetParm(parm, swfResourceMenu->snapToPixel ? 1.0f : 0.0f);

  idList<const swfDisplayEntry_t*, 5> activeMasks;
  for (int displayIndex = 0; displayIndex < spriteInstance->displayList.Num();
       ++displayIndex) {
    const swfDisplayEntry_t& display = spriteInstance->displayList[displayIndex];
    for (int maskIndex = 0; maskIndex < activeMasks.Num();) {
      if (display.depth > activeMasks[maskIndex]->clipDepth) {
        RenderMask(gui, activeMasks[maskIndex], renderState, guiStencilDecr);
        activeMasks[maskIndex] = activeMasks[activeMasks.Num() - 1];
        activeMasks.RemoveIndex(activeMasks.Num() - 1);
      } else {
        ++maskIndex;
      }
    }
    if (display.clipDepth != 0) {
      activeMasks.Append(&display);
      RenderMask(gui, &display, renderState, guiStencilIncr);
      continue;
    }

    idSWFDictionaryEntry* entry = FindDictionaryEntry(display.characterID);
    if (entry == nullptr)
      continue;
    swfRenderState_t state;
    state.matrix = display.matrix.Multiply(renderState.matrix);
    state.cxf = display.cxf.Multiply(renderState.cxf);
    state.z = renderState.z - spriteInstance->z;
    state.remainderScaleX = renderState.remainderScaleX;
    state.remainderScaleY = renderState.remainderScaleY;
    state.blendMode = display.blendMode != 0
                          ? static_cast<std::uint8_t>(display.blendMode)
                          : renderState.blendMode;
    state.ratio = display.ratio;
    state.activeMasks = renderState.activeMasks + activeMasks.Num();
    state.useBlackbars = renderState.useBlackbars;
    if (spriteInstance->materialOverride != nullptr) {
      state.material = spriteInstance->materialOverride;
      state.materialWidth = spriteInstance->materialWidth;
      state.materialHeight = spriteInstance->materialHeight;
    } else {
      state.material = renderState.material;
      state.materialWidth = renderState.materialWidth;
      state.materialHeight = renderState.materialHeight;
    }

    switch (entry->type) {
      case SWF_DICT_SPRITE: {
        if (display.spriteInstance == nullptr)
          break;
        display.spriteInstance->xOffset = spriteInstance->xOffset;
        display.spriteInstance->yOffset = spriteInstance->yOffset;
        if (parm != nullptr && display.spriteInstance->ignoreSnapToPixel)
          gui->SetParm(parm, 0.0f);

        const int border = display.spriteInstance->followBorder;
        if (border != -1) {
          const float virtualWidth = gui->GetVirtualWidth();
          const float virtualHeight = gui->GetVirtualHeight();
          const float widthAdjust = virtualWidth * swf_tweakSafeAreaWidth.GetFloat();
          const float heightAdjust = virtualHeight * swf_tweakSafeAreaHeight.GetFloat();
          const float blackbar = renderState.useBlackbars
                                     ? 0.0f
                                     : frameHeight * renderState.remainderScaleY * 0.5f;
          if (border >= 0 && border <= 2)
            state.matrix.ty = state.matrix.ty - blackbar +
                              heightAdjust * swf_tweakSafeFrame.GetFloat();
          else if (border >= 6 && border <= 8)
            state.matrix.ty = state.matrix.ty + blackbar -
                              heightAdjust * swf_tweakSafeFrame.GetFloat();
          if (border == 0 || border == 3 || border == 6)
            state.matrix.tx += widthAdjust * swf_tweakSafeFrame.GetFloat();
          else if (border == 2 || border == 5 || border == 8)
            state.matrix.tx -= widthAdjust * swf_tweakSafeFrame.GetFloat();
        }
        if (idStr::Cmp(display.spriteInstance->name.str, "__radial") == 0)
          RenderRadial(gui, display.spriteInstance, state);
        else
          RenderSprite(gui, display.spriteInstance, state, time, isSplitscreen);
        break;
      }
      case SWF_DICT_SHAPE:
        RenderShape(gui, entry->shape, state);
        break;
      case SWF_DICT_MORPH:
        RenderMorphShape(gui, entry->shape, display.ratio, state);
        break;
      case SWF_DICT_EDITTEXT:
        RenderEditText(gui, display.textInstance, state, time, isSplitscreen);
        break;
      default:
        break;
    }
  }
  for (int i = 0; i < activeMasks.Num(); ++i)
    RenderMask(gui, activeMasks[i], renderState, guiStencilDecr);
}

idStr idSWFTextInstance::GetSpeaker() { return subSpeaker; }

void idSWF::ReplaceTooltipIcons(idList<tooltipIcon_t, 72>& iconList,
                                idStr& text, const idList<int, 5>& bindsets,
                                int deviceNum) {
  if (swf_skipTooltipIcons.GetBool())
    return;

  for (int commandIndex = idStr::Find(text.data, '_', 0); commandIndex != -1;
       commandIndex = idStr::Find(text.data, '_', commandIndex + 1)) {
    const userCmdString_t* command = userCmdStrings;
    while (command != nullptr && command->string != nullptr &&
           idStr::IcmpPrefix(text.data + commandIndex, command->string) != 0)
      ++command;
    if (command == nullptr || command->string == nullptr)
      continue;

    idStr replacement;
    for (int setIndex = 0; setIndex < bindsets.Num() && replacement.IsEmpty(); ++setIndex) {
      const bindContext_t bindset = static_cast<bindContext_t>(bindsets[setIndex]);
      if (GetPlatform() == 2) {
        keyBindings_t bindings;
        idKeyInput::KeyBindingsFromBinding(bindings, deviceNum, bindset, command->string);
        const idStr* visible = nullptr;
        const idStr* key = nullptr;
        if (!bindings.mouse.IsEmpty()) {
          visible = &bindings.mouse;
          key = &bindings.mouseKey;
        } else if (!bindings.keyboard.IsEmpty()) {
          visible = &bindings.keyboard;
          key = &bindings.keyboardKey;
        } else if (!bindings.gamepad.IsEmpty() && bindset != BIND_MENUS) {
          visible = &bindings.gamepad;
          key = &bindings.gamepadKey;
        }
        if (visible != nullptr) {
          bool iconKnown = false;
          for (int i = 0; i < tooltipButtonImage.Num(); ++i) {
            if (idStr::Icmp(tooltipButtonImage[i].key, visible->data) == 0) {
              replacement = visible->data;
              iconKnown = true;
              break;
            }
          }
          if (!iconKnown && key != nullptr)
            replacement.Format("^7[^3%s^7]", key->data);
        }
      } else {
        replacement = idKeyInput::KeysFromBinding(deviceNum, bindset,
                                                   command->string, true);
      }

      if (GetPlatform() != 2 && bindset == BIND_DEFAULT && replacement.IsEmpty()) {
        const char* fallback = nullptr;
        if (std::strcmp(command->string, "_quickgrenade") == 0)
          fallback = "_quickgrenadeandweaponsel";
        else if (std::strcmp(command->string, "_quickgrenadeandweaponsel") == 0 ||
                 std::strcmp(command->string, "_toggledualwield") == 0)
          fallback = "_toggledwandweaponsel";
        if (fallback != nullptr)
          replacement = idKeyInput::KeysFromBinding(deviceNum, BIND_DEFAULT,
                                                     fallback, true);
      }
      if (replacement.IsEmpty() && bindset == BIND_MENUS)
        text.Replace(command->string, "");
    }
    if (!replacement.IsEmpty()) {
      replacement.ToUpper();
      text.Replace(command->string, replacement.data);
    }
  }

  for (int index = idStr::Find(text.data, '<', 0); index != -1;
       index = idStr::Find(text.data, '<', index + 1)) {
    for (int imageIndex = 0; imageIndex < tooltipButtonImage.Num(); ++imageIndex) {
      const keyButtonImages_t& image = tooltipButtonImage[imageIndex];
      if (idStr::CmpPrefix(text.data + index, image.key) != 0)
        continue;
      tooltipIcon_t icon{};
      icon.startIndex = index;
      icon.endIndex = index + idStr::Length(image.key) - 1;
      switch (GetPlatform()) {
        case 1: icon.material = image.ps3Material; break;
        case 4: icon.material = image.ps4Material; break;
        case 3: icon.material = image.xbOneMaterial; break;
        default: icon.material = image.xb360Material; break;
      }
      if (icon.material != nullptr) {
        icon.imageWidth = static_cast<std::int16_t>(image.width);
        icon.imageHeight = static_cast<std::int16_t>(image.height);
        icon.baseline = image.baseline;
      }
      iconList.Append(icon);
      break;
    }
  }
}

void idSWF::RenderEditText(idRenderModelGui* gui,
                           idSWFTextInstance* textInstance,
                           const swfRenderState_t& renderState, int time,
                           bool isSplitscreen) {
  if (swf_skipRenderText.GetBool())
    return;
  if (textInstance == nullptr) {
    idLib::Warning("%s: RenderEditText: textInstance == NULL", filename.str);
    return;
  }
  if (textInstance->editText == nullptr || !textInstance->visible)
    return;
  const idSWFEditText& edit = *textInstance->editText;
  idStr text;
  if (textInstance->variable.IsEmpty()) {
    if (textInstance->renderMode == SWF_TEXT_RENDER_PARAGRAPH) {
      if (textInstance->triggerGenerate)
        textInstance->StartParagraphText(time);
      text = textInstance->GetParagraphText(time);
    } else if (textInstance->renderMode == SWF_TEXT_RENDER_RANDOM_APPEAR ||
               textInstance->renderMode == SWF_TEXT_RENDER_RANDOM_APPEAR_CAPS) {
      if (textInstance->triggerGenerate)
        textInstance->StartRandomText(time);
      text = textInstance->GetRandomText(time);
    } else {
      text = idLocalization::languageDict.GetString(textInstance->text.data);
    }
  } else {
    idSWFScriptVar value = globals != nullptr
                               ? globals->Get(textInstance->variable.data)
                               : idSWFScriptVar();
    text = value.IsUndefined() ? textInstance->text : value.ToString();
    text = idLocalization::languageDict.GetString(text.data);
  }
  if (text.IsEmpty()) {
    textInstance->selectionStart = -1;
    textInstance->selectionEnd = -1;
  }

  if (textInstance->NeedsSoundPlayed()) {
    PlaySound(textInstance->soundClip, SND_CHANNEL_ANY, false);
    textInstance->needsSoundUpdate = false;
  }
  if (textInstance->isSubtitle) {
    if (text.IsEmpty() && textInstance->subtitleText.IsEmpty())
      return;
  } else if (text.IsEmpty()) {
    return;
  }

  idSWFDictionaryEntry* fontEntry = FindDictionaryEntry(edit.fontID, SWF_DICT_FONT);
  idFont* font = fontEntry != nullptr && fontEntry->font != nullptr
                     ? fontEntry->font->fontID
                     : nullptr;
  if (font == nullptr || font->GetPointSize() == 0)
    return;

  idList<tooltipIcon_t, 72> icons;
  if (textInstance->tooltip) {
    idList<int, 5> bindsets;
    bindsets.Append(textInstance->bindset);
    bindsets.Append(1);
    ReplaceTooltipIcons(icons, text, bindsets,
                        common != nullptr ? common->GetFocusDeviceNum() : 0);
  }

  const idVec2 xAxis = renderState.matrix.Scale({1, 0});
  const idVec2 yAxis = renderState.matrix.Scale({0, 1});
  const float xScale = std::max(std::sqrt(xAxis.x * xAxis.x + xAxis.y * xAxis.y),
                                1.17549435e-38f);
  float yScale = std::sqrt(yAxis.x * yAxis.x + yAxis.y * yAxis.y);
  if (isSplitscreen)
    yScale *= 0.5f;
  yScale = std::max(yScale, 1.17549435e-38f);
  swfMatrix_t textMatrix = renderState.matrix;
  textMatrix.xx /= xScale;
  textMatrix.xy /= xScale;
  textMatrix.yy /= yScale;
  textMatrix.yx /= yScale;
  const float glyphScaleX = edit.fontHeight * 0.05f * xScale /
                            static_cast<float>(font->GetPointSize());
  const float glyphScale = edit.fontHeight * 0.05f * yScale /
                           static_cast<float>(font->GetPointSize());
  const float lineSpacing =
      (font->GetAscender() - font->GetDescender()) * glyphScale + edit.leading * 0.05f;
  const float leftBound = (edit.bounds.tl.x + edit.leftMargin * 0.05f) * xScale;
  float rightBound = (edit.bounds.br.x - edit.rightMargin * 0.05f) * xScale;
  const float topBound = edit.bounds.tl.y * yScale;
  float bottomBound = edit.bounds.br.y * yScale;
  if (textInstance->forceAutoSize || (edit.flags & SWF_ET_AUTOSIZE)) {
    rightBound = frameWidth;
    bottomBound = frameHeight;
  }
  const float availableWidth = std::max(rightBound - leftBound, 1.0f);
  const int maxLines = std::max(
      static_cast<int>((bottomBound - topBound) / std::max(lineSpacing, 0.0001f)), 1);

  idList<idStr, 5> lines;
  idStr current;
  float currentWidth = 0.0f;
  int lastBreak = 0;
  float widthAtLastBreak = 0.0f;
  int sourceIndex = textInstance->isSubtitle ? textInstance->subCharStartIndex : 0;
  int iconIndex = 0;
  bool inIcon = false;
  while (sourceIndex < text.Length()) {
    const int charStart = sourceIndex;
    if (idStr::IsColor(text.data + sourceIndex)) {
      current += idStr(text.data, sourceIndex, sourceIndex + 2);
      sourceIndex += 2;
      continue;
    }
    const std::uint32_t code = text.UTF8Char(&sourceIndex);
    if (code == '\n' && (edit.flags & SWF_ET_MULTILINE)) {
      lines.Append(current);
      current.Clear();
      currentWidth = 0;
      lastBreak = 0;
      widthAtLastBreak = 0.0f;
      continue;
    }
    float advance = font->GetGlyph(code).xSkip * glyphScaleX;
    if (code == '\t') {
      const float tabWidth = 5.0f * font->GetGlyph(' ').xSkip * glyphScaleX;
      advance = tabWidth > 0.0f
                    ? tabWidth * std::floor(currentWidth / tabWidth + 1.0f) - currentWidth
                    : 0.0f;
    }
    if (iconIndex < icons.Num()) {
      if (charStart == icons[iconIndex].startIndex) {
        inIcon = true;
        advance = icons[iconIndex].GetRenderWidth(
                                                  (font->GetAscender() - font->GetDescender()) * glyphScale,
                                                  textInstance->tooltipScale);
      } else if (charStart == icons[iconIndex].endIndex) {
        inIcon = false;
        ++iconIndex;
        advance = 0;
      } else if (inIcon) {
        advance = 0;
      }
    }
    if (currentWidth + advance > availableWidth && !current.IsEmpty()) {
      if ((edit.flags & (SWF_ET_MULTILINE | SWF_ET_WORDWRAP)) == 0)
        break;
      if (lastBreak > 0) {
        if (maxLines == 1) {
          current.CapLength(std::max(current.Length() - 3, 0));
          current.Append("...");
          break;
        }
        const idStr carry = current.Mid(lastBreak, current.Length() - lastBreak);
        current.CapLength(lastBreak);
        lines.Append(current);
        current = carry;
        currentWidth = std::max(currentWidth - widthAtLastBreak, 0.0f);
      } else {
        lines.Append(current);
        current.Clear();
        currentWidth = 0.0f;
      }
      lastBreak = 0;
      widthAtLastBreak = 0.0f;
    }
    current += idStr(text.data, charStart, sourceIndex);
    currentWidth += advance;
    int nextIndex = sourceIndex;
    const std::uint32_t next = nextIndex < text.Length() ? text.UTF8Char(&nextIndex) : 0;
    if (idSWFTextInstance::CanBreakLine(code, next)) {
      lastBreak = current.Length();
      widthAtLastBreak = currentWidth;
    }
  }
  if (!current.IsEmpty() || lines.Num() == 0)
    lines.Append(current);

  int inputDisplayStart = 0;
  if (focusWindow == textInstance && lines.Num() > 0) {
    const idStr fullInput = lines[0];
    inputDisplayStart = textInstance->inputTextStartChar;
    if (inputDisplayStart < 0 || inputDisplayStart >= fullInput.Length())
      inputDisplayStart = 0;
    if (textInstance->selectionEnd >= 0 &&
        textInstance->selectionEnd < inputDisplayStart)
      inputDisplayStart = textInstance->selectionEnd;

    auto rangeWidth = [&](int begin, int finish) {
      float width = 0.0f;
      std::uint32_t previous = 0;
      int at = begin;
      while (at < finish && at < fullInput.Length()) {
        const std::uint32_t code = fullInput.UTF8Char(&at);
        if (previous != 0)
          width += font->GetKerning(previous, code) * glyphScaleX;
        width += font->GetGlyph(code).xSkip * glyphScaleX;
        previous = code;
      }
      return width;
    };

    const int cursor = std::clamp(textInstance->selectionEnd, 0, fullInput.Length());
    while (inputDisplayStart < cursor &&
           rangeWidth(inputDisplayStart, cursor) > availableWidth) {
      fullInput.UTF8Char(&inputDisplayStart);
    }
    int inputDisplayEnd = inputDisplayStart;
    int next = inputDisplayEnd;
    while (next < fullInput.Length()) {
      fullInput.UTF8Char(&next);
      if (rangeWidth(inputDisplayStart, next) > availableWidth)
        break;
      inputDisplayEnd = next;
    }
    textInstance->inputTextStartChar = inputDisplayStart;
    lines[0] = fullInput.Mid(inputDisplayStart,
                             inputDisplayEnd - inputDisplayStart);
  }

  if (textInstance->isSubtitle && textInstance->subUpdating) {
    if (lines.Num() > 0 && textInstance->subNeedsSwitch) {
      const int requestedWord = textInstance->GetApproximateSubtitleBreak(time);
      int newEndCharacter = textInstance->subCharStartIndex + lines[0].Length();
      int wordCount = 0;
      bool foundRequestedWord = false;
      for (int index = 0; index < lines[0].Length(); ++index) {
        if (lines[0][index] != ' ' && lines[0][index] != '-')
          continue;
        if (index != 0 && wordCount == requestedWord) {
          newEndCharacter = textInstance->subCharStartIndex + index;
          foundRequestedWord = true;
          break;
        }
        if (index > 0 && lines[0][index - 1] != ' ')
          ++wordCount;
      }
      if (wordCount == 0 && !lines[0].IsEmpty())
        wordCount = 1;
      if (!foundRequestedWord)
        textInstance->LastWordChanged(wordCount, time);
      textInstance->SetSubEndIndex(newEndCharacter, time);

      idSWFParmList subtitleParameters;
      const idStr subtitleLine = lines[0].Left(
          newEndCharacter - textInstance->subCharStartIndex);
      subtitleParameters.Append(idSWFScriptVar(subtitleLine));
      subtitleParameters.Append(idSWFScriptVar(textInstance->GetSpeaker()));
      subtitleParameters.Append(idSWFScriptVar(textInstance->subAlign));
      Invoke("subtitleChanged", subtitleParameters);

      textInstance->SetSubNextStartIndex(textInstance->subCharEndIndex);
      textInstance->SwitchSubtitleText(time);
    }
    if (!textInstance->UpdateSubtitle(time)) {
      textInstance->SubtitleComplete();
      idSWFParmList completeParameters;
      completeParameters.Append(idSWFScriptVar(textInstance->subAlign));
      Invoke("subtitleComplete", completeParameters);
      textInstance->SubtitleCleanup();
    }
  }

  textInstance->numLines = lines.Num();
  textInstance->maxscroll = std::max(lines.Num() - maxLines, 0);
  if (textInstance->renderMode == SWF_TEXT_RENDER_AUTOSCROLL)
    textInstance->scroll = textInstance->maxscroll;
  else
    textInstance->scroll = std::clamp(textInstance->scroll, 0, textInstance->maxscroll);

  idList<swfGlyphDrawInfo_t, 5> glyphs;
  idList<swfGlyphSelectionInfo_t, 5> selections;
  idList<swfGlyphIconInfo_t, 5> imageDraws;
  idColor defaultColor = textInstance->GetColor();
  swfColorXform_t textColor = ApplyColor(
      {defaultColor.r, defaultColor.g, defaultColor.b, defaultColor.a},
      renderState.cxf);
  idColor activeColor{textColor.mul.x + textColor.add.x,
                      textColor.mul.y + textColor.add.y,
                      textColor.mul.z + textColor.add.z,
                      textColor.mul.w + textColor.add.w};
  const float inverseMaterialWidth = 1.0f / font->GetMaterialWidth();
  const float inverseMaterialHeight = 1.0f / font->GetMaterialHeight();
  int overallIndex = textInstance->isSubtitle ? textInstance->subCharStartIndex
                                               : inputDisplayStart;
  iconIndex = 0;
  const bool drawCursor = focusWindow == textInstance &&
                          (((idLib::frameNumber - textInstance->lastInputTime) / 30) & 1) == 0;
  const int cursorPosition = textInstance->selectionEnd;
  float cursorX = leftBound;
  float cursorY = topBound;
  bool cursorLocated = cursorPosition <= 0;

  const int firstLine = textInstance->scroll;
  const int lastLine = std::min(lines.Num(), firstLine + maxLines);
  for (int skippedLine = 0; skippedLine < firstLine; ++skippedLine)
    overallIndex += lines[skippedLine].Length();
  for (int lineIndex = firstLine; lineIndex < lastLine; ++lineIndex) {
    idStr line = lines[lineIndex];
    if (textInstance->isSubtitle && lineIndex == firstLine &&
        textInstance->subCharEndIndex > textInstance->subCharStartIndex) {
      line.CapLength(std::min(line.Length(),
                              textInstance->subCharEndIndex -
                                  textInstance->subCharStartIndex));
    }
    float lineWidth = 0;
    int measure = 0;
    std::uint32_t previousMeasured = 0;
    while (measure < line.Length()) {
      if (idStr::IsColor(line.data + measure)) {
        measure += 2;
        continue;
      }
      const std::uint32_t measured = line.UTF8Char(&measure);
      if (previousMeasured != 0)
        lineWidth += font->GetKerning(previousMeasured, measured) * glyphScaleX;
      lineWidth += font->GetGlyph(measured).xSkip * glyphScaleX;
      previousMeasured = measured;
    }
    float x = leftBound;
    if (textInstance->alignment == SWF_ET_ALIGN_RIGHT)
      x = rightBound - lineWidth;
    else if (textInstance->alignment == SWF_ET_ALIGN_CENTER)
      x = leftBound + (availableWidth - lineWidth) * 0.5f;
    float y = topBound + (lineIndex - firstLine) * lineSpacing;
    int index = 0;
    std::uint32_t previousCode = 0;
    while (index < line.Length()) {
      if (idStr::IsColor(line.data + index)) {
        const char code = line[index + 1];
        if (code == '0')
          activeColor = {textColor.mul.x + textColor.add.x,
                         textColor.mul.y + textColor.add.y,
                         textColor.mul.z + textColor.add.z,
                         textColor.mul.w + textColor.add.w};
        else {
          const idVec4& c = idStr::ColorForIndex(code);
          activeColor = {c.x, c.y, c.z, defaultColor.a};
        }
        index += 2;
        overallIndex += 2;
        continue;
      }
      const int characterStart = index;
      const std::uint32_t code = line.UTF8Char(&index);
      const glyphInfo_t& glyph = font->GetGlyph(code);
      if (previousCode != 0)
        x += font->GetKerning(previousCode, code) * glyphScaleX;
      const float advance = glyph.xSkip * glyphScaleX;
      const float left = x + glyph.left * glyphScaleX;
      const float top = y - glyph.top * glyphScale + font->GetAscender() * glyphScale;
      const float width = (glyph.width + 1.0f) * glyphScaleX;
      const float height = (glyph.height + 1.0f) * glyphScale;

      const int absoluteCharacter = overallIndex + characterStart;
      bool iconCharacter = false;
      if (iconIndex < icons.Num() && absoluteCharacter == icons[iconIndex].startIndex) {
        const tooltipIcon_t& icon = icons[iconIndex];
        const float maxGlyphHeight =
            (font->GetAscender() - font->GetDescender()) * glyphScale;
        const float renderHeight = textInstance->tooltipScale >= 0.0001f
                                       ? textInstance->tooltipScale * 2.0f * maxGlyphHeight
                                       : maxGlyphHeight;
        const float renderWidth = icon.GetRenderWidth(maxGlyphHeight,
                                                      textInstance->tooltipScale);
        float imageX = x + glyph.left * glyphScaleX;
        if (textInstance->centerTooltip)
          imageX += renderWidth * 0.5f;
        if (textInstance->alignment == SWF_ET_ALIGN_RIGHT &&
            imageX + renderWidth > rightBound)
          imageX = rightBound - renderWidth;
        const float imageY = y + (maxGlyphHeight - renderHeight) * 0.5f;
        swfGlyphIconInfo_t draw{};
        const idVec2 tl = textMatrix.Transform({imageX, imageY});
        const idVec2 tr = textMatrix.Transform({imageX + renderWidth, imageY});
        const idVec2 br = textMatrix.Transform({imageX + renderWidth, imageY + renderHeight});
        const idVec2 bl = textMatrix.Transform({imageX, imageY + renderHeight});
        draw.iconTop = {tl.x, tl.y, tr.x, tr.y};
        draw.iconBottom = {bl.x, bl.y, br.x, br.y};
        draw.iconColor = {1.0f, 1.0f, 1.0f, activeColor.a};
        draw.zDepth = renderState.z;
        draw.iconMat = icon.material;
        imageDraws.Append(draw);
        x += renderWidth;
        iconCharacter = true;
      }
      if (iconIndex < icons.Num() && absoluteCharacter <= icons[iconIndex].endIndex) {
        iconCharacter = true;
        if (absoluteCharacter == icons[iconIndex].endIndex)
          ++iconIndex;
      }
      if (!iconCharacter) {
        swfGlyphDrawInfo_t draw{};
        const idVec2 tl = textMatrix.Transform({left, top});
        const idVec2 tr = textMatrix.Transform({left + width, top});
        const idVec2 br = textMatrix.Transform({left + width, top + height});
        const idVec2 bl = textMatrix.Transform({left, top + height});
        draw.glyphTop = {tl.x, tl.y, tr.x, tr.y};
        draw.glyphBottom = {bl.x, bl.y, br.x, br.y};
        draw.s = {(glyph.s - 0.5f) * inverseMaterialWidth,
                  (glyph.s + glyph.width + 0.5f) * inverseMaterialWidth};
        draw.t = {(glyph.t - 0.5f) * inverseMaterialHeight,
                  (glyph.t + glyph.height + 0.5f) * inverseMaterialHeight};
        draw.glyphMat = textInstance->fontFXMaterialIndex >= 0 &&
                                textInstance->fontFXMaterialIndex < fontFXMaterials.Num()
                            ? fontFXMaterials[textInstance->fontFXMaterialIndex]
                            : font->GetMaterial();
        draw.fontColor = activeColor;
        draw.zDepth = renderState.z;
        glyphs.Append(draw);
        x += advance;
      }

      if (!cursorLocated && absoluteCharacter >= cursorPosition) {
        cursorX = x;
        cursorY = y;
        cursorLocated = true;
      }

      const int selectionStart = std::min(textInstance->selectionStart,
                                          textInstance->selectionEnd);
      const int selectionEnd = std::max(textInstance->selectionStart,
                                        textInstance->selectionEnd);
      if (absoluteCharacter >= selectionStart && absoluteCharacter < selectionEnd) {
        swfGlyphSelectionInfo_t selection{};
        const idVec2 tl = textMatrix.Transform({x - advance, y});
        const idVec2 tr = textMatrix.Transform({x, y});
        const idVec2 br = textMatrix.Transform({x, y + lineSpacing});
        const idVec2 bl = textMatrix.Transform({x - advance, y + lineSpacing});
        selection.selTop = {tl.x, tl.y, tr.x, tr.y};
        selection.selBottom = {bl.x, bl.y, br.x, br.y};
        selection.selColor = {0.2f, 0.4f, 1.0f, 0.5f};
        selection.selMat = white;
        selection.zDepth = renderState.z;
        selections.Append(selection);
      }
      previousCode = code;
    }
    overallIndex += line.Length();
  }
  DrawEditTextSelections(selections, gui);
  DrawEditTextGlyphs(glyphs, gui);
  DrawEditTextImages(imageDraws, gui);
  if (drawCursor && cursorLocated)
    DrawEditCursor(gui, cursorX, cursorY, renderState.z, 2.0f,
                   (font->GetAscender() - font->GetDescender()) * glyphScale,
                   textMatrix);
}

void idSWF::CreateAltasImageAndMaterial() {
  idImageOpts options{};
  options.textureType = TT_2D;
  options.format = FMT_DXT5;
  options.colorFormat = CFM_DEFAULT;
  options.width = atlasWidth;
  options.height = atlasHeight;
  options.numLevels = 1;
  options.packedTail = true;
  options.linear = true;
  options.overlayMemory = atlasPurgable;
  options.startPurged = true;
  options.textureBindType = TB_SHADER;
  options.textureUsage = TU_DEFAULT;
  options.textureOptions = TO_NONE;
  options.sampler.filter = TF_LINEAR;
  options.sampler.repeat = TR_REPEAT;
  options.sampler.lodMaxClamp = 13.0f;
  atlasImage = globalImages != nullptr
                   ? globalImages->ScratchImage(atlasTextureName.str, options)
                   : nullptr;
  idStr materialText;
  materialText.Format("{\ntransmap %s\nstageprogram guiBlend_CoACgY_premulalpha\n}",
                      atlasTextureName.str);
  idStr materialName(filename.str);
  materialName.StripFileExtension();
  materialName.Append("_");
  if (common != nullptr && common->IsClient())
    materialName.Append("atlas_client_");
  atlasMaterial = declManager != nullptr
                      ? static_cast<const idMaterial*>(declManager->CreateNewDecl(
                            &idMaterial::resourceList, materialName.data, "",
                            materialText.data))
                      : nullptr;
  const unsigned previousPlatform = resourceManager != nullptr
                                        ? resourceManager->GetResourceFilePlatform()
                                        : 0;
  if (resourceManager != nullptr)
    resourceManager->SetResourceFilePlatform(63);
  if (globalImages != nullptr)
    atlasImage = globalImages->ImageFromFile(atlasTextureName.str, nullptr, true);
  if (resourceManager != nullptr)
    resourceManager->SetResourceFilePlatform(previousPlatform);
}

void idSWF::Render(idRenderModelGui* gui, int currentTime, bool isSplitscreen) {
  if (globals == nullptr || idLib::headless || swf_skipRender.GetBool() ||
      frameRate == 0 || !isActive || gui == nullptr)
    return;
  if (!swf_skipFilesNotMatching.valueString.IsEmpty()) {
    bool matched = false;
    const char* cursor = swf_skipFilesNotMatching.GetString();
    while (*cursor != '\0') {
      while (*cursor == ' ' || *cursor == '\t')
        ++cursor;
      const char* start = cursor;
      while (*cursor != '\0' && *cursor != ' ' && *cursor != '\t')
        ++cursor;
      if (cursor != start) {
        idStr candidate(start, 0, static_cast<int>(cursor - start));
        if (idStr::Icmp(candidate.data, filename.str) == 0) {
          matched = true;
          break;
        }
      }
    }
    if (!matched)
      return;
  }
  if (swf_stopat.GetInteger() > 0 && mainspriteInstance != nullptr &&
      mainspriteInstance->currentFrame == swf_stopat.GetInteger())
    swf_timescale.SetFloat(0.0f);
  if (!LoadAtlasImage(false))
    return;

  SetGlobal("currentTime", idSWFScriptVar(currentTime));
  if (paused)
    lastRenderTime = currentTime;
  idSWFScriptVar onRender = GetGlobal("onRender");
  if (onRender.IsFunction()) {
    idSWFParmList parameters;
    onRender.GetFunction()->Call(nullptr, parameters);
  }
  if (swf_timescale.GetFloat() > 0.0f && mainspriteInstance != nullptr) {
    int framesToRun = 1;
    if (lastRenderTime != 0) {
      const float framesPerSecond = frameRate * (1.0f / 256.0f) *
                                    swf_timescale.GetFloat();
      framesToRun = static_cast<int>(framesPerSecond *
                                     (currentTime - lastRenderTime) / 1000.0f);
      lastRenderTime += static_cast<int>(framesToRun * (1000.0f / framesPerSecond));
      framesToRun = std::min(framesToRun, 10);
    } else {
      lastRenderTime = currentTime;
    }
    while (framesToRun-- > 0) {
      mainspriteInstance->Run();
      mainspriteInstance->RunActions();
    }
  }

  const float virtualWidth = gui->GetVirtualWidth();
  const float virtualHeight = gui->GetVirtualHeight();
  float horizontalScale = virtualWidth / frameWidth * swfScale;
  float verticalScale = virtualHeight / frameHeight * swfScale;
  float unusedHorizontal = 0.0f;
  float unusedVertical = 0.0f;
  if (horizontalScale <= verticalScale) {
    unusedVertical = verticalScale - horizontalScale;
    verticalScale = horizontalScale;
  } else {
    unusedHorizontal = horizontalScale - verticalScale;
    horizontalScale = verticalScale;
  }
  swfRenderState_t state;
  state.matrix.xx = isSplitscreen ? horizontalScale * 0.5f : horizontalScale;
  state.matrix.yy = verticalScale;
  const float remainingWidth =
      virtualWidth - frameWidth * (isSplitscreen ? horizontalScale * 0.5f
                                                 : horizontalScale);
  state.matrix.tx = remainingWidth * 0.5f;
  state.matrix.ty = (virtualHeight - frameHeight * verticalScale) * 0.5f;
  state.remainderScaleX = unusedHorizontal;
  state.remainderScaleY = unusedVertical;
  state.useBlackbars = false;
  renderBorder = state.matrix.tx / horizontalScale;
  RenderSprite(gui, mainspriteInstance, state, currentTime, isSplitscreen);

  if (blackbars && unusedHorizontal + unusedVertical > 0.0f) {
    const float barWidth = state.matrix.tx + 0.5f;
    const float barHeight = state.matrix.ty + 0.5f;
    gui->currentVertexColor = 0xFF000000u;
    if (barWidth > 0.0f) {
      gui->DrawStretchPic(0, 0, 0, barWidth, virtualHeight, 0, 0, 1, 1, white);
      gui->DrawStretchPic(virtualWidth - barWidth, 0, 0, barWidth,
                          virtualHeight, 0, 0, 1, 1, white);
    }
    if (barHeight > 0.0f) {
      gui->DrawStretchPic(0, 0, 0, virtualWidth, barHeight, 0, 0, 1, 1, white);
      gui->DrawStretchPic(0, virtualHeight - barHeight, 0, virtualWidth,
                          barHeight, 0, 0, 1, 1, white);
    }
  }
  if (showMouseCursor && mouseEnabled && useMouse &&
      (IsPlayerControlInhibited() || !useInhibitControl)) {
    gui->SetGLState(0x2C);
    gui->currentVertexColor = 0xffffffffu;
    const idVec2 mouse = state.matrix.Transform(
        {static_cast<float>(mouseX - 1), static_cast<float>(mouseY - 2)});
    gui->DrawStretchPic(static_cast<float>(static_cast<int>(mouse.x)),
                        static_cast<float>(static_cast<int>(mouse.y)), 0, 32, 32,
                        0, 0, 1, 1,
                        hasHitObject ? guiCursor_hand : guiCursor_arrow);
  }
  gui->SetGLState(0x28);
  PurgeOrphanage();
}
