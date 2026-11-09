#include "tungsten/game/ai/cover/aicovermanager.h"

// DIA: idList<idAICover,5>::Resize
template bool idList<idAICover, 5>::Resize(int newSize);

// DIA: idAICoverManager::idAICoverManager
idAICoverManager::idAICoverManager() = default;

// DIA: `dynamic initializer for 'ai_coverRadiusOverride''
idCVar ai_coverRadiusOverride("ai_coverRadiusOverride", "-1", 4,
                              "override all AI cover radii when nonnegative");

// DIA: `dynamic initializer for 'ai_useFormationCover''
idCVar ai_useFormationCover("ai_useFormationCover", "1", 4,
                            "enable formation-provided cover");

// DIA: `dynamic initializer for 'ai_useVehicleCover''
idCVar ai_useVehicleCover("ai_useVehicleCover", "1", 4,
                          "enable vehicle-provided cover");

// DIA: `dynamic initializer for 'ai_forceVehicleCoverFlag''
idCVar ai_forceVehicleCoverFlag("ai_forceVehicleCoverFlag", "0", 1,
                                "force all vehicles to provide cover");

// DIA: `dynamic atexit destructor for 'ai_coverRadiusOverride''
// DIA: `dynamic atexit destructor for 'ai_useFormationCover''
// DIA: `dynamic atexit destructor for 'ai_useVehicleCover''
// DIA: `dynamic atexit destructor for 'ai_forceVehicleCoverFlag''
