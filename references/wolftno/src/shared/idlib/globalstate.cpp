#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/cassystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"

// The retail compiland owned construction and destruction of these global
// subsystems. In the reconstruction each singleton is defined next to its
// implementation; including their complete types here preserves the original
// compiland boundary without inventing an artificial exported function.
