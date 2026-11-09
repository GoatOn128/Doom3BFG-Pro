#include "engine/framework/versionselector.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/parser.h"

#include <cstring>

idCVar feature_hq("feature_hq", "1", CVAR_BOOL | CVAR_INIT, "");
idCVar feature_lowgore("feature_lowgore", "0", CVAR_BOOL | CVAR_INIT, "");
idCVar feature_aq("feature_aq", "0", CVAR_BOOL | CVAR_INIT, "");
extern idCVar sys_region;

namespace {

constexpr const char* FEATURE_NAMES[] = {"hq", "lowgore"};
constexpr const char* tags[] = {"hg", "lg", "hq", "lq", "jp"};

struct versionPair_t {
  char endings[2][3];
};

constexpr versionPair_t pairs[] = {
    {{{'h', 'g', 0}, {'l', 'g', 0}}},
    {{{'h', 'q', 0}, {'l', 'q', 0}}}};

struct specialRemap_t {
  const char* from;
  const char* to;
};

#define JP_HG(stem) {stem "_hg.bik", stem "_jp.bik"}
#define JP_VIDEO(stem) {stem ".bik", stem "_jp.bik"}
constexpr specialRemap_t japaneseBinks[] = {
    JP_HG("bink/loading/c01/cs_c01_inthewater"),
    JP_HG("bink/loading/c01/cs_c01_introduction_intro"),
    JP_HG("bink/loading/c01/cs_c01_peptalk_intro"),
    JP_HG("bink/loading/c01/cs_c01_transition"),
    JP_HG("bink/loading/c02/cs_c02_fourteenyears_adrift"),
    JP_HG("bink/loading/c02/cs_c02_frontcourtyard_startcar"),
    JP_HG("bink/loading/c02/cs_c02_timelapse"),
    JP_HG("bink/loading/c03/cs_c03_coffeemachine"),
    JP_HG("bink/loading/c03/cs_c03_grandparents"),
    JP_HG("bink/loading/c03/cs_c03_safepassage_journey"),
    JP_HG("bink/loading/c04/cs_c04_leavingforprison"),
    JP_HG("bink/loading/c04/cs_c04_pastime_sexscene"),
    JP_HG("bink/loading/c04/cs_c04_thedoor_fergus"),
    JP_HG("bink/loading/c04/cs_c04_thedoor_wyatt"),
    JP_HG("bink/loading/c05/cs_c05_policecar_fergus"),
    JP_HG("bink/loading/c05/cs_c05_policecar_wyatt"),
    JP_HG("bink/loading/c06/cs_c06_bobbysac_fergus"),
    JP_HG("bink/loading/c06/cs_c06_bobbysac_wyatt"),
    JP_HG("bink/loading/c06/cs_c06_findparchments"),
    JP_HG("bink/loading/c07/cs_c07_helicopter_fergus"),
    JP_HG("bink/loading/c07/cs_c07_helicopter_wyatt"),
    JP_HG("bink/loading/c08/cs_c08_electrocuted"),
    JP_HG("bink/loading/c08/cs_c08_undercover_fergus"),
    JP_HG("bink/loading/c08/cs_c08_undercover_wyatt"),
    JP_HG("bink/loading/c09/cs_c09_escapingcamp_fergus"),
    JP_HG("bink/loading/c09/cs_c09_escapingcamp_wyatt"),
    JP_HG("bink/loading/c10/cs_c10_thingamajig"),
    JP_HG("bink/loading/c11/cs_c11_hiddencargo_fergus"),
    JP_HG("bink/loading/c11/cs_c11_hiddencargo_wyatt"),
    JP_HG("bink/loading/c11/cs_c11_theabyss_fergus"),
    JP_HG("bink/loading/c11/cs_c11_theabyss_wyatt"),
    JP_HG("bink/loading/c12/cs_c12_spindlytorque_fergus"),
    JP_HG("bink/loading/c12/cs_c12_spindlytorque_wyatt"),
    JP_HG("bink/loading/c12/cs_c12_traincrash_fergus"),
    JP_HG("bink/loading/c12/cs_c12_traincrash_wyatt"),
    JP_HG("bink/loading/c13/cs_c13_campmemory"),
    JP_HG("bink/loading/c13/cs_c13_lunarshuttle_launch"),
    JP_HG("bink/loading/c14/cs_c14_crashlanding_impact"),
    JP_HG("bink/loading/c15/cs_c15_thegetaway_fergus"),
    JP_HG("bink/loading/c15/cs_c15_thegetaway_wyatt"),
    JP_HG("bink/loading/c16/cs_c16_cannon_spindlytorque"),
    JP_HG("bink/loading/c16/cs_c16_escape_fergus"),
    JP_HG("bink/loading/c16/cs_c16_escape_wyatt"),
    JP_HG("bink/loading/c16/cs_c16_homecoming_fergus"),
    JP_HG("bink/loading/c16/cs_c16_homecoming_wyatt"),
    JP_HG("bink/ingame/c01/cs_c01_condornine_score"),
    JP_HG("bink/ingame/c01/cs_c01_crashedplane_radio"),
    JP_HG("bink/ingame/c01/cs_c01_thechoice_fergus"),
    JP_HG("bink/ingame/c01/cs_c01_thechoice_wyatt"),
    JP_HG("bink/ingame/c01/cs_c01_theplan"),
    JP_HG("bink/ingame/c02/cs_c02_bath_bigman"),
    JP_HG("bink/ingame/c02/cs_c02_decapitate"),
    JP_HG("bink/ingame/c04/cs_c04_berlinhotel_balcony"),
    JP_HG("bink/ingame/c05/cs_c05_inside_fergus"),
    JP_HG("bink/ingame/c05/cs_c05_inside_wyatt"),
    JP_HG("bink/ingame/c05/cs_c05_klausroom_thefolder"),
    JP_HG("bink/ingame/c05/cs_c05_maxroom2_hidage"),
    JP_HG("bink/ingame/c07/cs_c07_concrete_sawing"),
    JP_HG("bink/ingame/c07/cs_c07_daatyichud_kiss"),
    JP_HG("bink/ingame/c07/cs_c07_wrongchoice_fergus"),
    JP_HG("bink/ingame/c07/cs_c07_wrongchoice_wyatt"),
    JP_HG("bink/ingame/c08/cs_c08_backtoset_battery"),
    JP_HG("bink/ingame/c08/cs_c08_getinfo"),
    JP_HG("bink/ingame/c08/cs_c08_proposition"),
    JP_HG("bink/ingame/c08/cs_c08_swapuniform"),
    JP_HG("bink/ingame/c08/cs_c08_tattoo"),
    JP_HG("bink/ingame/c08/cs_c08_throwbattery"),
    JP_HG("bink/ingame/c09/cs_c09_movechair"),
    JP_HG("bink/ingame/c09/cs_c09_secretguitar_wyatt"),
    JP_HG("bink/ingame/c09/cs_c09_secretmilk_fergus"),
    JP_HG("bink/ingame/c09/cs_c09_welding_fergus"),
    JP_HG("bink/ingame/c09/cs_c09_welding_wyatt"),
    JP_HG("bink/ingame/c11/cs_c11_prism_fergus"),
    JP_HG("bink/ingame/c11/cs_c11_prism_wyatt"),
    JP_HG("bink/ingame/c13/cs_c13_gearup"),
    JP_HG("bink/ingame/c13/cs_c13_gettingkeys"),
    JP_HG("bink/ingame/c15/cs_c15_aircontrol_jdies"),
    JP_HG("bink/ingame/c15/cs_c15_aircontrol_tekladies"),
    JP_HG("bink/ingame/c16/cs_c16_mecha_defeated_fergus"),
    JP_HG("bink/ingame/c16/cs_c16_mecha_defeated_wyatt"),
    JP_HG("bink/ingame/c16/cs_c16_mecha_ending_fergus"),
    JP_HG("bink/ingame/c16/cs_c16_mecha_ending_wyatt"),
    JP_HG("bink/ingame/c16/cs_c16_releasefriend_fergus"),
    JP_HG("bink/ingame/c16/cs_c16_releasefriend_wyatt"),
    JP_VIDEO("bink/loadvideo_pc_xboxone"),
    JP_VIDEO("bink/loadvideo_ps3"),
    JP_VIDEO("bink/loadvideo_ps4"),
    JP_VIDEO("bink/loadvideo_xbox360"),
    {nullptr, nullptr}};
#undef JP_VIDEO
#undef JP_HG

} // namespace

const char* idVersionSelector::StripTags(char* buffer, int bufferSize,
                                         const char* name) {
  (void)bufferSize;
  for (std::size_t length = std::strlen(name); length > 3; length -= 3) {
    if (name[length - 3] != '_')
      break;
    const char* const* tag = tags;
    while (name[length - 2] != (*tag)[0] || name[length - 1] != (*tag)[1]) {
      if (++tag == tags + 5)
        return name;
    }
    if (name != buffer) {
      std::strcpy(buffer, name);
      name = buffer;
    }
    buffer[length - 3] = '\0';
  }
  return name;
}

unsigned int idVersionSelector::GetFeatureBits() {
  return (feature_hq.valueInteger != 0) |
         (2u * static_cast<unsigned int>(feature_lowgore.valueInteger != 0));
}

void idVersionSelector::GetOptionString(int versionBits,
                                        idStrStatic<256>& str) {
  str.Clear();
  if (versionBits == 0) {
    str = "base";
    return;
  }
  int bit = 1;
  for (const char* featureName : FEATURE_NAMES) {
    if ((versionBits & bit) != 0) {
      if (!str.IsEmpty())
        str.Append("+");
      str.Append(featureName);
    }
    bit = static_cast<int>((static_cast<unsigned int>(bit) << 1) |
                           (static_cast<unsigned int>(bit) >> 31));
  }
}

const char* idVersionSelector::SpecialRemap(const char* name) {
  if (std::strcmp(sys_region.GetString(), "jp") != 0)
    return name;
  for (const specialRemap_t* remap = japaneseBinks; remap->from != nullptr;
       ++remap) {
    if (std::strcmp(name, remap->from) == 0)
      return remap->to;
  }
  return name;
}

const char* idVersionSelector::RemapResourceName(char* buffer, int bufferSize,
                                                 const char* name) {
  (void)bufferSize;
  if (name == nullptr)
    return nullptr;

  const int wantedVersion[2] = {feature_lowgore.valueInteger != 0,
                                feature_hq.valueInteger == 0};
  std::size_t location = std::strlen(name);
  for (std::size_t index = location; index >= 3; --index) {
    if (name[index] == '.') {
      location = index;
      break;
    }
    if (name[index] == '/')
      break;
  }

  while (location > 3 && name[location - 3] == '_') {
    bool recognized = false;
    for (int pairIndex = 0; pairIndex < 2; ++pairIndex) {
      const char* wanted = pairs[pairIndex].endings[wantedVersion[pairIndex]];
      const char* unwanted = pairs[pairIndex].endings[wantedVersion[pairIndex] ^ 1];
      if (name[location - 2] == unwanted[0] &&
          name[location - 1] == unwanted[1]) {
        if (name != buffer) {
          std::strcpy(buffer, name);
          name = buffer;
        }
        buffer[location - 2] = wanted[0];
        buffer[location - 1] = wanted[1];
        recognized = true;
        break;
      }
      if (name[location - 2] == wanted[0] && name[location - 1] == wanted[1]) {
        recognized = true;
        break;
      }
    }
    if (!recognized)
      return name;
    location -= 3;
  }
  return name;
}

const char* idVersionSelector::RemapVideoName(char* buffer, int bufferSize,
                                              const char* name) {
  return RemapResourceName(buffer, bufferSize, SpecialRemap(name));
}

void idVersionSelector::Assign(int versionBits, idStr str) {
  idOption& option = options[versionBits];
  if (option.exists)
    idLib::Warning("'%s' is overridden with '%s'", option.str.data, str.data);
  else
    ++numOptions;
  option.str = str;
  option.exists = true;
  combinedMask |= versionBits;
}

void idVersionSelector::Finalize() {
  const int selected = static_cast<int>(GetFeatureBits());
  const unsigned int compareMask =
      ~static_cast<unsigned int>(feature_aq.valueInteger != 0);
  for (unsigned int versionBits = 0; versionBits < 4; ++versionBits) {
    if (((~combinedMask & versionBits) == 0) || versionBits == 0)
      options[versionBits].used = true;
    options[versionBits].build =
        (versionBits & compareMask) ==
        (static_cast<unsigned int>(selected & combinedMask) & compareMask);
    if (options[versionBits].build)
      buildOptions[numBuildOptions++] = static_cast<int>(versionBits);
  }

  for (int versionBits = 0; versionBits < 4; ++versionBits) {
    if (options[versionBits].build != options[versionBits].exists) {
      idStrStatic<256> str;
      GetOptionString(versionBits, str);
      idLib::Warning("%s is missing", str.data);
    }
  }
}

int idVersionSelector::Parse(idParser& src, const char* context) {
  (void)context;
  idToken token;
  src.ExpectTokenType(TT_PUNCTUATION, 46, token);
  while (src.ReadToken(token)) {
    if (token.type == TT_PUNCTUATION) {
      if (token.subtype == 47)
        break;
    } else if (token.type == TT_NAME) {
      int versionBits = 0;
      for (;;) {
        unsigned int featureIndex = 0;
        while (featureIndex < 2 && token.Icmp(FEATURE_NAMES[featureIndex]) != 0)
          ++featureIndex;
        if (featureIndex < 2) {
          const int bit = 1 << featureIndex;
          if ((versionBits & bit) != 0)
            src.Error("");
          versionBits |= bit;
        } else if (token.Icmp("base") != 0) {
          src.Error("");
        }

        src.ReadToken(token);
        if (token.type != TT_PUNCTUATION)
          break;
        if (token.subtype == 30)
          src.ExpectTokenType(TT_NAME, 0, token);
        else
          src.Error("");
      }
      if (token.type != TT_STRING && token.type != TT_NAME)
        src.Error("");
      Assign(versionBits, idStr(token));
      continue;
    }
    src.Error("");
  }
  return 0;
}

idVersionSelector::idVersionSelector(idParser& src, const char* context)
    : options{}, buildOptions{}, numOptions(0), numBuildOptions(0),
      combinedMask(0) {
  for (idOption& option : options) {
    option.exists = false;
    option.used = false;
    option.build = false;
  }
  Parse(src, context);
  Finalize();
}
