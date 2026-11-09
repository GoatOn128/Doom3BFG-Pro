#include "declsimple.h"

idDeclInfoTemplate<idDeclStaticImage> idDeclStaticImage::resourceList(
    "staticImage", "idDeclStaticImage", "tdef", false);

idDeclStaticImage::idDeclStaticImage() { resourceListPtr = &resourceList; }
idDeclInfo* idDeclStaticImage::GetDeclInfo() const { return &resourceList; }
