#include "engine/models/detail/decldetail.h"

#include "engine/models/foliage/declfoliage.h"

idDeclInfoTemplate<idDeclDetail> idDeclDetail::resourceList(
    "detail", "idDeclDetail", "detail", false);

idCVar r_skipDetail("r_skipDetail", "0", CVAR_BOOL,
                    "Completely disable detail model");

void idDeclFoliage::List() const { idDecl::List(); }

const char* idDeclDetail::DefaultDefinition() const {
  return "{\n\tmodel\t_default\n}\n";
}

idDeclInfo* idDeclDetail::GetDeclInfo() const { return &resourceList; }

idDeclDetail::idDeclDetail()
    : modelName(""), colorVariance{1.0f, 1.0f, 1.0f} {}

void idDeclDetail::Parse(idParser& source) {
  modelName.Set("");
  colorVariance = {1.0f, 1.0f, 1.0f};
  idToken token;
  while (source.ReadToken(token) && token != "}") {
    if (token.Icmp("model") == 0) {
      if (!source.ExpectAnyToken(token))
        break;
      modelName.Set(token.data);
    } else if (token.Icmp("colorVariance") == 0) {
      colorVariance.x = source.ParseFloat();
      colorVariance.y = source.ParseFloat();
      colorVariance.z = source.ParseFloat();
    } else {
      source.Warning("idDeclDetail::Parse - bad token %s", token.data);
    }
  }
}
