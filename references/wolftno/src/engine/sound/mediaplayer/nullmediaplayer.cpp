#include "engine/sound/mediaplayer/nullmediaplayer.h"

// The retail Windows build installs a process-lifetime null implementation.
// Its compiler-generated initializer and destructor provide the two remaining
// functions recorded for this compiland in the PDB.
idNullMediaPlayer nullMediaPlayer;
idMediaPlayer* mediaPlayer = &nullMediaPlayer;
