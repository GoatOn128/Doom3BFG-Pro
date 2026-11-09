#include "shared/idlib/csystems/cvarsystemlocal.h"

// This translation unit remains IN PROGRESS. These definitions are owned by
// the retail sys_session_local.cpp and are needed by the recovered platform
// input and achievement layers while the session methods are reconstructed.
idCVar achievements_Verbose("achievements_Verbose", "0", CVAR_BOOL,
                            "debug spam for achievements");
idCVar in_unlockMouseInMenus("in_unlockMouseInMenus", "0", CVAR_BOOL,
                             "unlock the mouse while menus are active");
