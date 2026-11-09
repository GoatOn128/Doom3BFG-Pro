#include "screenrect.h"

// RVAs 0x009D11A0 and 0x009D11C0; member offsets confirmed by disassembly.
void idScreenRect::Clear() {
  x1 = y1 = 32000;
  x2 = y2 = -32000;
}

void idScreenRect::Zero() {
  x1 = y1 = x2 = y2 = 0;
}
