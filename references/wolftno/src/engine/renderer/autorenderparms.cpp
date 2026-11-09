#include "engine/renderer/autorenderparms.h"

#define WOLF_DEFINE_AUTO_RENDER_PARM(variable, resourceName) \
  idCodeResource<idDeclRenderParm> variable(resourceName);
WOLF_AUTO_RENDER_PARM_LIST(WOLF_DEFINE_AUTO_RENDER_PARM)
#undef WOLF_DEFINE_AUTO_RENDER_PARM
