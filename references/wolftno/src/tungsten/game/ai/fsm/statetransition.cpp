#include "tungsten/game/ai/fsm/statetransition.h"

#include <cstdio>

// DIA: idStateTransition::idStateTransition
idStateTransition::idStateTransition(int transitionFlags)
    : name("transition"), flags(transitionFlags) {}

// DIA: idStateTransition::Save
void idStateTransition::Save(
    std::string* output, const char* indent,
    const idList<transDest_t, 5>* dests) const {
  if (output == nullptr) return;
  const char* prefix = indent != nullptr ? indent : "";
  char line[512];
  std::snprintf(line, sizeof(line), "%stransition \"%s\" {\n", prefix,
                GetName());
  output->append(line);
  InternalSave(output, prefix, dests);
  output->append(prefix).append("}\n");
}
