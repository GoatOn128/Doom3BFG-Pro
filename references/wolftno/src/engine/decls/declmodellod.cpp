#include "declsimple.h"

idDeclInfoTemplate<idDeclModelLOD> idDeclModelLOD::resourceList("modelLOD", "idDeclModelLOD", "modellod", false);

idDeclModelLOD::idDeclModelLOD() : fade(0.25f) { resourceListPtr = &resourceList; }
void idDeclModelLOD::FreeData() { LODs.Clear(); }
idDeclInfo* idDeclModelLOD::GetDeclInfo() const { return &resourceList; }
