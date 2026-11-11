# Build digest

commit: 20efe788433c

- windows-x86-Debug: success
- windows-x86-Release: success

## windows-x86-Debug

status: success

errors: 0
warnings: 504 in 8 kinds

### errors


### failure context


### warning kinds

- [271x] C4467
  - C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\MSVC\14.51.36231\include\CodeAnalysis\SourceAnnotations.h(68,1): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\game\gamesys\SaveGame.h(128,18): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\MSVC\14.51.36231\include\CodeAnalysis\SourceAnnotations.h(68,1): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\idlib\idlib.vcxproj]
  - C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\MSVC\14.51.36231\include\CodeAnalysis\SourceAnnotations.h(68,1): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\framework\Common_local.h(133,26): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\framework\DeclManager.cpp(210,30): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\framework\TokenParser.cpp(181,28): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\RenderLog.h(98,16): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\sys\sys_local.h(42,30): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [139x] C5208
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\sys\win32\win_local.h(117,16): warning C5208: unnamed class used in typedef name cannot declare members other than non-static data members, member enumerations, or member classes [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\tools\idstudio\qertypes.h(231,16): warning C5208: unnamed class used in typedef name cannot declare members other than non-static data members, member enumerations, or member classes [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [50x] C4005
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\extern\jxrlib\image\sys\ansi.h(45,9): warning C4005: 'FORCE_INLINE': macro redefinition [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\extern\jxrlib_encoder.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\extern\jxrlib\image\sys\ansi.h(51,9): warning C4005: 'UINTPTR_T': macro redefinition [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\extern\jxrlib_encoder.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\extern\jxrlib\image\sys\ansi.h(52,9): warning C4005: 'INTPTR_T': macro redefinition [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\extern\jxrlib_encoder.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\extern\jxrlib\image\decode\strdec.c(433,9): warning C4005: 'max': macro redefinition [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\extern\jxrlib_encoder.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\math\Vector.h(1501,9): warning C4005: 'VectorMA': macro redefinition [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [24x] C5051
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(7,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(8,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(9,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(10,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(11,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(12,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(13,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(14,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(7,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(8,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(9,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(10,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(11,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(12,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(13,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(14,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(7,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(8,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(9,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(10,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(11,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(12,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(13,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(14,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [12x] C4595
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(120,17): warning C4595: 'operator new': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(123,16): warning C4595: 'operator delete': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(126,17): warning C4595: 'operator new[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(129,16): warning C4595: 'operator delete[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(120,17): warning C4595: 'operator new': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\idlib\idlib.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(123,16): warning C4595: 'operator delete': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\idlib\idlib.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(126,17): warning C4595: 'operator new[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\idlib\idlib.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(129,16): warning C4595: 'operator delete[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\idlib\idlib.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(120,17): warning C4595: 'operator new': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(123,16): warning C4595: 'operator delete': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(126,17): warning C4595: 'operator new[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(129,16): warning C4595: 'operator delete[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [3x] C4018
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\virtualtextureencoder.cpp(272,16): warning C4018: '>': signed/unsigned mismatch [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\virtualtexturesystem.cpp(190,16): warning C4018: '>': signed/unsigned mismatch [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\virtualtexturesystem.cpp(1447,16): warning C4018: '>': signed/unsigned mismatch [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [3x] C4644
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\sys\win32\win_input.cpp(636,9): warning C4644: usage of the macro-based offsetof pattern in constant expressions is non-standard; use offsetof defined in the C++ standard library instead [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\sys\win32\win_input.cpp(641,9): warning C4644: usage of the macro-based offsetof pattern in constant expressions is non-standard; use offsetof defined in the C++ standard library instead [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\sys\win32\win_input.cpp(646,9): warning C4644: usage of the macro-based offsetof pattern in constant expressions is non-standard; use offsetof defined in the C++ standard library instead [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [2x] C4091
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\tools\idstudio\CapDialog.h(44,15): warning C4091: 'static ': ignored on left of 'CCapDialog::<unnamed-enum-BEVEL>' when no variable is declared [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]

## windows-x86-Release

status: success

errors: 0
warnings: 504 in 8 kinds

### errors


### failure context


### warning kinds

- [271x] C4467
  - C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\MSVC\14.51.36231\include\CodeAnalysis\SourceAnnotations.h(68,1): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\game\gamesys\SaveGame.h(128,18): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\MSVC\14.51.36231\include\CodeAnalysis\SourceAnnotations.h(68,1): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\idlib\idlib.vcxproj]
  - C:\Program Files\Microsoft Visual Studio\18\Enterprise\VC\Tools\MSVC\14.51.36231\include\CodeAnalysis\SourceAnnotations.h(68,1): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\framework\Common_local.h(133,26): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\framework\DeclManager.cpp(210,30): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\framework\TokenParser.cpp(181,28): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\RenderLog.h(98,16): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\sys\sys_local.h(42,30): warning C4467: usage of ATL attributes is deprecated [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [139x] C5208
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\sys\win32\win_local.h(117,16): warning C5208: unnamed class used in typedef name cannot declare members other than non-static data members, member enumerations, or member classes [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\tools\idstudio\qertypes.h(231,16): warning C5208: unnamed class used in typedef name cannot declare members other than non-static data members, member enumerations, or member classes [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [50x] C4005
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\extern\jxrlib\image\sys\ansi.h(45,9): warning C4005: 'FORCE_INLINE': macro redefinition [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\extern\jxrlib_encoder.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\extern\jxrlib\image\sys\ansi.h(51,9): warning C4005: 'UINTPTR_T': macro redefinition [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\extern\jxrlib_encoder.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\extern\jxrlib\image\sys\ansi.h(52,9): warning C4005: 'INTPTR_T': macro redefinition [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\extern\jxrlib_encoder.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\extern\jxrlib\image\decode\strdec.c(433,9): warning C4005: 'max': macro redefinition [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\extern\jxrlib_encoder.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\math\Vector.h(1501,9): warning C4005: 'VectorMA': macro redefinition [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [24x] C5051
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(7,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(8,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(9,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(10,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(11,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(12,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(13,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\fdct_ap922_int.cpp(14,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(7,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(8,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(9,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(10,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(11,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(12,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(7,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(13,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int.cpp(14,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(8,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(9,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(10,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(11,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(12,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(13,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\jobs\transcode\image\baredct\idct_ap922_int_w.cpp(14,3): warning C5051: attribute [[maybe_unused]] requires at least '/std:c++17'; ignored [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [12x] C4595
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(120,17): warning C4595: 'operator new': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(123,16): warning C4595: 'operator delete': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(126,17): warning C4595: 'operator new[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(129,16): warning C4595: 'operator delete[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\game\game.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(120,17): warning C4595: 'operator new': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\idlib\idlib.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(123,16): warning C4595: 'operator delete': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\idlib\idlib.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(126,17): warning C4595: 'operator new[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\idlib\idlib.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(129,16): warning C4595: 'operator delete[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\idlib\idlib.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(120,17): warning C4595: 'operator new': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(123,16): warning C4595: 'operator delete': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(126,17): warning C4595: 'operator new[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\idlib\Heap.h(129,16): warning C4595: 'operator delete[]': non-member operator new or delete functions may not be declared inline [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [3x] C4018
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\virtualtextureencoder.cpp(272,16): warning C4018: '>': signed/unsigned mismatch [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\virtualtexturesystem.cpp(190,16): warning C4018: '>': signed/unsigned mismatch [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\renderer\virtualtexturesystem.cpp(1447,16): warning C4018: '>': signed/unsigned mismatch [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [3x] C4644
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\sys\win32\win_input.cpp(636,9): warning C4644: usage of the macro-based offsetof pattern in constant expressions is non-standard; use offsetof defined in the C++ standard library instead [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\sys\win32\win_input.cpp(641,9): warning C4644: usage of the macro-based offsetof pattern in constant expressions is non-standard; use offsetof defined in the C++ standard library instead [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\sys\win32\win_input.cpp(646,9): warning C4644: usage of the macro-based offsetof pattern in constant expressions is non-standard; use offsetof defined in the C++ standard library instead [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]
- [2x] C4091
  - D:\a\Doom3BFG-Pro\Doom3BFG-Pro\neo\tools\idstudio\CapDialog.h(44,15): warning C4091: 'static ': ignored on left of 'CCapDialog::<unnamed-enum-BEVEL>' when no variable is declared [D:\a\Doom3BFG-Pro\Doom3BFG-Pro\out\Darklight.vcxproj]

