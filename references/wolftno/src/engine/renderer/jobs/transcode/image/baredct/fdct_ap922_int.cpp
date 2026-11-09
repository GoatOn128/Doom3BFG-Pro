#include "engine/renderer/jobs/transcode/image/baredct/integer_dct_tables.h"

namespace {

using namespace baredct_integer_tables;

[[maybe_unused]] int tg_1_16 = Fixed16(std::tan(0.19634955f));
[[maybe_unused]] int tg_2_16 = Fixed16(std::tan(0.39269909f));
[[maybe_unused]] int tg_3_16 = Fixed16(std::tan(0.58904862f));
[[maybe_unused]] int cos_4_16 = Fixed16(std::cos(0.78539819f));
[[maybe_unused]] Table tab_f_04 = MakeForward(std::cos(0.78539819f));
[[maybe_unused]] Table tab_f_17 = MakeForward(std::cos(0.19634955f));
[[maybe_unused]] Table tab_f_26 = MakeForward(std::cos(0.39269909f));
[[maybe_unused]] Table tab_f_35 = MakeForward(std::cos(0.58904862f));

}  // namespace
