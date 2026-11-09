#include "engine/models/foliage/declfoliage.h"

#include "engine/renderer/declmaterial.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/token.h"

namespace {

struct foliageShapeDesc_t {
  const char* name;
  foliageShape_t shape;
  const char* description;
};

const foliageShapeDesc_t foliageShapeDesc[] = {
    {"autosprites", FOLIAGE_SHAPE_AUTOSPRITES,
     "single quad foliage which auto-rotates to view"},
    {"doubles", FOLIAGE_SHAPE_DOUBLES,
     "2-quad foliage crossed at 90 degree angles"},
    {"triples", FOLIAGE_SHAPE_TRIPLES,
     "3-quad foliage crossed at 120 degree angles"},
    {"fixedsprites", FOLIAGE_SHAPE_FIXEDSPRITES,
     "single quad foliage which is fixed"},
    {nullptr, FOLIAGE_SHAPE_NUM, nullptr},
};

void BreakupSpaceSeparatedString(idList<idStr, 5>& strings,
                                 const char* text) {
  strings.ClearFree();
  strings.Append(idStr(text));
  int index = 0;
  int separator = strings[index].Find(" ");
  while (separator != -1) {
    strings.Append(idStr(strings[index].data + separator + 1));
    strings[index].CapLength(separator);
    ++index;
    separator = strings[index].Find(" ");
  }
}

} // namespace

idDeclInfoTemplate<idDeclFoliage> idDeclFoliage::resourceList(
    "foliage", "idDeclFoliage", "foliage", false);

idCVar r_skipFoliage("r_skipFoliage", "0", CVAR_BOOL,
                     "Completely disable foliage models");

const char* idDeclFoliage::DefaultDefinition() const {
  return "{\n\tmaterial\tfoliage/default\n\tquadWidth\t64\n\tquadHeight\t64\n\tshape\t\tautosprites\n}\n";
}

void idDeclFoliage::Print() const {
  const_cast<idDeclFoliage*>(this)->List();
}

void idDeclFoliage::Clear() {
  material = nullptr;
  quadWidth = 0;
  quadHeight = 0;
  widthVariance = 0.0f;
  heightVariance = 0.0f;
  colorVariance.x = 1.0f;
  colorVariance.y = 1.0f;
  colorVariance.z = 1.0f;
  rndFlipHoriz = false;
  swayMagnitude = 0.0f;
  shape = FOLIAGE_SHAPE_AUTOSPRITES;
  stampMaterials.ClearFree();
  stampScale = 1.0f;
  stampParms[3] = 1.0f;
  stampParms[2] = 1.0f;
  stampParms[1] = 1.0f;
  stampParms[0] = 1.0f;
  stampCovers[3] = 1.0f;
  stampCovers[2] = 1.0f;
  stampCovers[1] = 1.0f;
  stampCovers[0] = 1.0f;
  stampBlendMode.Set("none");
  isOpaque = false;
  isReferenceType = false;
  isSinglePlacement = false;
}

idDeclFoliage::idDeclFoliage() { Clear(); }

idDeclFoliage::~idDeclFoliage() = default;

idDeclInfo* idDeclFoliage::GetDeclInfo() const { return &resourceList; }

void idDeclFoliage::Parse(idParser& source) {
  Clear();

  idToken token;
  while (source.ReadToken(token)) {
    if (token.Icmp("}") == 0)
      break;

    if (token.Icmp("quadWidth") == 0) {
      quadWidth = static_cast<std::uint16_t>(source.ParseInt());
      continue;
    }
    if (token.Icmp("quadHeight") == 0) {
      quadHeight = static_cast<std::uint16_t>(source.ParseInt());
      continue;
    }
    if (token.Icmp("widthVariance") == 0) {
      widthVariance = source.ParseFloat();
      continue;
    }
    if (token.Icmp("heightVariance") == 0) {
      heightVariance = source.ParseFloat();
      continue;
    }
    if (token.Icmp("colorVariance") == 0) {
      colorVariance.x = source.ParseFloat();
      colorVariance.y = source.ParseFloat();
      colorVariance.z = source.ParseFloat();
      continue;
    }
    if (token.Icmp("rndFlipHoriz") == 0) {
      rndFlipHoriz = source.ParseBool();
      continue;
    }
    if (token.Icmp("swayMagnitude") == 0) {
      swayMagnitude = source.ParseFloat();
      continue;
    }
    if (token.Icmp("material") == 0) {
      if (!source.ExpectAnyToken(token))
        break;
      material = token.data != nullptr
                     ? static_cast<const idMaterial*>(
                           idMaterial::resourceList.FindWithInheritance(
                               token.data, true, true))
                     : nullptr;
      continue;
    }
    if (token.Icmp("shape") == 0) {
      source.ReadToken(token);
      int shapeIndex = 0;
      while (foliageShapeDesc[shapeIndex].name != nullptr &&
             token.Icmp(foliageShapeDesc[shapeIndex].name) != 0)
        ++shapeIndex;
      if (foliageShapeDesc[shapeIndex].name != nullptr) {
        shape = foliageShapeDesc[shapeIndex].shape;
        continue;
      }
      source.Error("bad shape type: %s\n", token.data);
    }

    if (token.Icmp("stampMaterial") == 0) {
      stampMaterials.ClearFree();
      source.ParseRestOfLine(token);
      idList<idStr, 5> materials;
      BreakupSpaceSeparatedString(materials, token.data);
      for (int index = 0; index < materials.Num(); ++index) {
        idStr materialName(materials[index]);
        materialName.Strip(" ");
        const idMaterial* stampMaterial =
            materialName.data != nullptr
                ? static_cast<const idMaterial*>(
                      idMaterial::resourceList.FindWithInheritance(
                          materialName.data, false, true))
                : nullptr;
        if (stampMaterial != nullptr)
          stampMaterials.Append(stampMaterial);
        else
          idLib::Warning("Foliage stamp material '%s' NOT found!",
                         materialName.data);
      }
    } else if (token.Icmp("stampScale") == 0) {
      stampScale = source.ParseFloat();
    } else if (token.Icmp("stampParms") == 0) {
      if (!source.Parse1DMatrix(4, stampParms))
        source.Error("Invalid stampParms parameter: %s\n", name.str);
    } else if (token.Icmp("stampCovers") == 0) {
      if (!source.Parse1DMatrix(4, stampCovers))
        source.Error("Invalid stampCovers parameter: %s\n", name.str);
    } else if (token.Icmp("stampBlendMode") == 0) {
      if (!source.ExpectAnyToken(token))
        break;
      stampBlendMode.Set(token.data);
    } else if (token.Icmp("isOpaque") == 0) {
      isOpaque = source.ParseBool();
    } else if (token.Icmp("referenceType") == 0) {
      isReferenceType = source.ParseBool();
    } else if (token.Icmp("singlePlacement") == 0) {
      isSinglePlacement = source.ParseBool();
    } else {
      source.Warning("idDeclFoliage::Parse - bad token %s", token.data);
    }
  }
}
