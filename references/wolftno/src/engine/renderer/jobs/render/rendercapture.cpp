#include "engine/renderer/jobs/render/rendercapture.h"

void RenderCapture(captureParms_t* parameters) {
  switch (parameters->capture) {
    case CAPTURE_VIEW_COLOR:
      CaptureViewColor(parameters);
      break;
    case CAPTURE_VIEW_DEPTH:
      CaptureViewDepth(parameters);
      break;
    case CAPTURE_VIEW_NORMAL:
      // The retail target is the linker-folded no-op at 0x140001000.
      break;
    case CAPTURE_FEEDBACK:
      CaptureFeedback(parameters);
      break;
    case CAPTURE_GLARE_MAP:
      CaptureGlareMap(parameters);
      break;
    case CAPTURE_GUI_IMAGE:
      CaptureGuiImage(parameters);
      break;
    default:
      break;
  }
}
