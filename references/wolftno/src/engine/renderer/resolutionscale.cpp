#include "engine/renderer/resolutionscale.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/str.h"

extern idCVar forge_logPerformanceMetrics;

idCVar rs_enable(
    "rs_enable", "0", 2,
    "Enable dynamic resolution scaling, 0 - off, 1 - vert only, 2 - horz only, 3 - both");
idCVar rs_forceFractionX(
    "rs_forceFractionX", "0", 4,
    "Force a specific 0.0 to 1.0 horizontal resolution scale");
idCVar rs_forceFractionY(
    "rs_forceFractionY", "0", 4,
    "Force a specific 0.0 to 1.0 vertical resolution scale");
idCVar rs_showResolutionChanges(
    "rs_showResolutionChanges", "0", 2,
    "1 = Print whenever the resolution scale changes, 2 = always");
idCVar rs_display("rs_display", "0", 2,
                  "0 - percentages, 1 - pixels per frame, 2 - resolution");
idCVar rs_minimumResolutionScale("rs_minimumResolutionScale", "0.5", 4,
                                 "Never go below this");
idCVar rs_dropMilliseconds(
    "rs_dropMilliseconds", "15.0", 4,
    "Drop the resolution when GPU time exceeds this");
idCVar rs_raiseMilliseconds(
    "rs_raiseMilliseconds", "13.0", 4,
    "Raise the resolution when GPU time is below this for several frames");
idCVar rs_dropFraction("rs_dropFraction", "0.11", 4,
                       "Drop the resolution in increments of this");
idCVar rs_raiseFraction("rs_raiseFraction", "0.06", 4,
                        "Raise the resolution in increments of this");
idCVar rs_raiseFrames(
    "rs_raiseFrames", "5", 2,
    "Require this many frames below rs_raiseMilliseconds");

idResolutionScale resolutionScale;

idResolutionScale::idResolutionScale()
    : resolutionScaleMetricX(idStr("resolutionScaleX"),
                             idStr("horizontal resolution scaling")),
      resolutionScaleMetricY(idStr("resolutionScaleY"),
                             idStr("vertical resolution scaling")) {
  dropMilliseconds = 15.0F;
  raiseMilliseconds = 13.0F;
  framesAboveRaise = 0;
  currentResolution = 1.0F;
}

void idResolutionScale::InitForMap(const char* mapName) {
  if (mapName != nullptr &&
      (idStr::Icmp(mapName, "maps/game/wasteland1/wellspring") == 0 ||
       idStr::Icmp(mapName,
                   "maps/game/wasteland1/bash_tv_entrance") == 0 ||
       idStr::Icmp(mapName, "maps/game/wasteland2/subway_town") == 0)) {
    dropMilliseconds = 16.0F;
    raiseMilliseconds = 15.0F;
    return;
  }

  dropMilliseconds = rs_dropMilliseconds.valueFloat;
  raiseMilliseconds = rs_raiseMilliseconds.valueFloat;
}

void idResolutionScale::GetCurrentResolutionScale(float& x, float& y) const {
  x = 1.0F;
  y = 1.0F;
  switch (rs_enable.valueInteger) {
    case 0:
      return;
    case 1:
      x = currentResolution;
      break;
    case 2:
      y = currentResolution;
      break;
    case 3:
      x = currentResolution;
      y = currentResolution;
      break;
    default:
      break;
  }

  if (rs_forceFractionX.valueFloat > 0.0F &&
      rs_forceFractionX.valueFloat <= 1.0F) {
    x = rs_forceFractionX.valueFloat;
  }
  if (rs_forceFractionY.valueFloat > 0.0F &&
      rs_forceFractionY.valueFloat <= 1.0F) {
    y = rs_forceFractionY.valueFloat;
  }
}

void idResolutionScale::SetCurrentGPUFrameTime(int microseconds) {
  const float previousResolution = currentResolution;
  const float gpuMilliseconds = static_cast<float>(microseconds) * 0.001F;
  if (gpuMilliseconds > dropMilliseconds) {
    currentResolution = previousResolution - rs_dropFraction.valueFloat;
    if (currentResolution < rs_minimumResolutionScale.valueFloat) {
      currentResolution = rs_minimumResolutionScale.valueFloat;
    }
  } else if (gpuMilliseconds < raiseMilliseconds) {
    ++framesAboveRaise;
    if (framesAboveRaise >= rs_raiseFrames.valueInteger) {
      framesAboveRaise = 0;
      currentResolution = previousResolution + rs_raiseFraction.valueFloat;
      if (currentResolution > 1.0F) {
        currentResolution = 1.0F;
      }
    }
  } else {
    framesAboveRaise = 0;
  }

  if (rs_showResolutionChanges.valueInteger > 1 ||
      (rs_showResolutionChanges.valueInteger == 1 &&
       currentResolution != previousResolution)) {
    idLib::Printf("GPU msec: %4.1f resolutionScale: %4.2f\n",
                  gpuMilliseconds, currentResolution);
  }
}

void idResolutionScale::GetConsoleText(idStr& text) {
  if (rs_enable.valueInteger == 0) {
    text = "rs-off";
    return;
  }

  float fractionX = 1.0F;
  float fractionY = 1.0F;
  switch (rs_enable.valueInteger) {
    case 1:
      fractionX = currentResolution;
      break;
    case 2:
      fractionY = currentResolution;
      break;
    case 3:
      fractionX = currentResolution;
      fractionY = currentResolution;
      break;
    default:
      break;
  }
  if (rs_forceFractionX.valueFloat > 0.0F &&
      rs_forceFractionX.valueFloat <= 1.0F) {
    fractionX = rs_forceFractionX.valueFloat;
  }
  if (rs_forceFractionY.valueFloat > 0.0F &&
      rs_forceFractionY.valueFloat <= 1.0F) {
    fractionY = rs_forceFractionY.valueFloat;
  }

  if (rs_display.valueInteger > 1) {
    text = va("rs-res: %i x %i", static_cast<int>(fractionX * 1280.0F),
              static_cast<int>(fractionY * 720.0F));
  } else if (rs_display.valueInteger == 1) {
    float scaledWidth = fractionX * 1280.0F;
    float scaledHeight = fractionY * 720.0F;
    if (rs_enable.valueInteger == 1) {
      scaledHeight = 1.0F;
    } else if (rs_enable.valueInteger == 2) {
      scaledWidth = 1.0F;
    }
    text = va("rs-pixels %i", static_cast<int>(scaledWidth * scaledHeight));
  } else if (rs_enable.valueInteger == 3) {
    text = va("%2i%%h,%2i%%v", static_cast<int>(fractionX * 100.0F),
              static_cast<int>(fractionY * 100.0F));
  } else {
    const float fraction =
        rs_enable.valueInteger == 1 ? fractionX : fractionY;
    const char* axis = rs_enable.valueInteger == 1 ? "h" : "v";
    text = va("%2i%%%s", static_cast<int>(fraction * 100.0F), axis);
  }
}

void idResolutionScale::LogMetrics() {
  if (forge_logPerformanceMetrics.valueInteger == 0) {
    return;
  }

  float fractionX = 1.0F;
  float fractionY = 1.0F;
  if (rs_enable.valueInteger != 0) {
    switch (rs_enable.valueInteger) {
      case 1:
        fractionX = currentResolution;
        break;
      case 2:
        fractionY = currentResolution;
        break;
      case 3:
        fractionX = currentResolution;
        fractionY = currentResolution;
        break;
      default:
        break;
    }
    if (rs_forceFractionX.valueFloat > 0.0F &&
        rs_forceFractionX.valueFloat <= 1.0F) {
      fractionX = rs_forceFractionX.valueFloat;
    }
    if (rs_forceFractionY.valueFloat > 0.0F &&
        rs_forceFractionY.valueFloat <= 1.0F) {
      fractionY = rs_forceFractionY.valueFloat;
    }
  }

  resolutionScaleMetricX.Log(fractionX, false);
  resolutionScaleMetricY.Log(fractionY, false);
}
