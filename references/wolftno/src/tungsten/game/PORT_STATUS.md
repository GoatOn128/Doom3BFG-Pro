# Tungsten game 1:1 parity status

Source recovery root: `retail/WolfNewOrder_x64_recovered/_external/w/build/tech5/tungsten/game`

Destination root: `src/tungsten/game`

The former function-count completion claims were invalidated on 2026-08-24.
They proved only that selected symbol names existed, not that implementations
matched retail behavior. Every file is being re-audited from the beginning
against both recovered Hex-Rays output and recovered assembly.

## Completion rules

- Traverse directories and `.cpp` files alphabetically.
- A `.cpp` is complete only after every recovered function has matching control
  flow, calls, state changes, constants, and observable side effects.
- Check ambiguous decompiler output against recovered assembly.
- Destination-only guards, generic handlers, placeholder bodies, and invented
  behavior are parity failures.
- Function-count, object-symbol, compile, and behavior checks are required, but
  function counts alone never prove parity.
- Per-file source counts include only `dia_line_record` entries emitted by that
  file's matching object. Header-emitted and compiler-generated records are
  retained as support inventory, but do not belong to the `.cpp` source count.

## Inventory

- Recovered `.cpp` files: 413
- Recovered DIA function records: 27,125
- Destination `.cpp` files at parity-audit start: 134
- Files initially accepted as 1:1: 0

## Alphabetical parity ledger

1. `ai/aasobstacle.cpp` — parity verified (7/7 functions)
2. `ai/aaspositionmgr.cpp` — parity verified (6/6 functions)
3. `ai/aasviscache.cpp` — parity verified (9/9 functions)
4. `ai/ai2_actions.cpp` — parity verified (129/129 records)
5. `ai/ai2.cpp` — active verification (the prior 344/485 progress claim was
   invalidated; record 345, `idAI2::UpdateWorldState`, is independently parity
   verified, but a retrospective body audit found 63 placeholder records among
   records 1-344; the sequential audit has restarted at record 1; file is not
   complete)
   - AI2's recovered export has 1,127 total DIA records: 517 compiler-primary,
   124 header-emitted, and 486 line records. One line record belongs to
   `AnimatedEntity.obj`, leaving 485 records sourced by `AI2.cpp`.
   - Sequential parity pass: record 1, `idAI2::ApplyDamage`, is verified.
     Record 345, `idAI2::UpdateWorldState`, remains independently verified.

All remaining `.cpp` files are unverified for 1:1 parity.
