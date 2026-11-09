#pragma once

#if defined(WOLF_RENDERER_RENDERSYSTEM_INIT_TEST_ACCESS)

enum renderSystemInitDeclType_t : int {
  RSID_RENDER_PARM,
  RSID_MATERIAL,
  RSID_RENDER_PROG,
  RSID_TABLE,
};

struct RenderSystemInitTestHooks {
  void* (*findDecl)(renderSystemInitDeclType_t type, const char* name,
                    bool makeDefault, bool remap){};
};

class RenderSystemInitTestAccess {
public:
  static void SetHooks(const RenderSystemInitTestHooks& hooks);
};

#endif
