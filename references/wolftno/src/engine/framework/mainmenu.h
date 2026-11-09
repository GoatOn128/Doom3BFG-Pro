#pragma once

#include <cstddef>

class idSWF;

// idMainMenu derives from the retail menu handler hierarchy.  SWFHelper only
// observes the inherited gui member at 0x18.
class idMainMenu {
public:
  std::byte inheritedMenuHandler[0x18];
  idSWF* gui;
};

static_assert(offsetof(idMainMenu, gui) == 0x18,
              "main-menu gui offset drift");
