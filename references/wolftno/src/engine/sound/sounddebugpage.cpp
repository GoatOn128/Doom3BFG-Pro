#include "engine/sound/sounddebugpage.h"

idCVar s_luStep("s_luStep", "6", CVAR_ARCHIVE | CVAR_FLOAT,
                "Difference between numbers on the lu scale");
idCVar s_luMin("s_luMin", "-36", CVAR_ARCHIVE | CVAR_FLOAT,
               "Minimum value on the lu meter");
idCVar s_luMax("s_luMax", "18", CVAR_ARCHIVE | CVAR_FLOAT,
               "Maximum value on the lu meter");
idCVar s_luGoodMin("s_luGoodMin", "-6", CVAR_ARCHIVE | CVAR_FLOAT,
                   "Minimum Good (Green) value on the lu meter");
idCVar s_luGoodMax("s_luGoodMax", "6", CVAR_ARCHIVE | CVAR_FLOAT,
                   "Maximum Good (Green) value on the lu meter");
