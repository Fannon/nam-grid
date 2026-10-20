# Follow-up review fixes — 2026-10-20

Based on `a92e7ab` and the six findings in the separate static-review report.
Changes were developed in `nam-grid-review-fixes` on
`fix/review-followups-20261020` while another agent edited the main checkout.
The main checkout and its build were not modified by this work.

| Finding | Resolution | Regression |
| --- | --- | --- |
| F1: excessive tails/drain | Cap the entire host tail budget at 60 s. Stop delay feedback after the bypass fade; drain for 9 s (8.1 s ring plus settling), then suspend. | Actual CLAP tail extension for a mixed grid and 16 delays at several rates; mono/stereo ping-pong cleanup at maximum feedback/8 s sync, no allocation, no returning echoes. |
| F2: menu insert collisions | A shared production builder inserts each group at the next position, retaining tags and the library tree. A2 Lite follows the effect section separator. | Ordered labels, separator, selection tags/checkmarks, optional quality, empty/scanning library and active filters. |
| F3: stale verification docs | Schema 9 / 1262 parameters / 61 utility controls / four suites; mark older CPU measurements as historical. | Documentation checked against registered targets/parameters. |
| F4: Gate Decay label | Register Gate Release; accept old Gate Decay state keys as aliases without changing host parameter IDs. | Recall different old-name values in all 16 slots and compare canonical saved state. |
| F5: schemas 4/5 | Explicitly reject unused WIP schema numbers before any mutation. | Actual CLAP state-load failure preserves the prior state. Existing 2/3/6/7/8 migrations remain covered. |
| F6: stale references | Clarify historical prototype branch and benchmark modes. | Reference review. |

## Tail policy and limits

The 60-second host report is an automatic rendering/suspension budget, not a
mathematical upper bound on every audible echo. Extreme slow-synced, high-feedback
delays can outlast it. Continuous playback retains their natural decay; explicitly
select a longer render range when those echoes matter. Bypass already fades the
wet path to zero, so stopping feedback during cleanup changes no audible bypass
tail. This does not add compensation latency, allocation, locks or ring-wide
clears to the callback. Reverb/modulation drainage retains its existing bounds.

Bitwig suspend/bounce behavior, listening, macOS and VST3-host testing remain
unverified. No system plugin installation is performed by this branch.

## Integration

This branch is based on the reviewed 1262-parameter checkpoint. Concurrent new
modules may touch the same slot dispatch, enum, menu, state and parameter counts.
Merge this branch after that work is committed; preserve the new module entries
when resolving the menu/dispatch hunks and refresh current counts if they grow.
The new ModuleMenu.h is the production picker builder, so new module menu entries
belong there. Dependency revisions and tracked allocation/WAV patches are unchanged.

## Local verification

Fresh Windows x64 Release build: CLAP, VST3 and all test targets succeeded.
All four CTest suites passed: capture-library (0.23 s), wav-decoder (1.35 s),
stages (9.20 s) and clap-smoke (81.92 s). The new long-delay cleanup cases run
through mono and stereo slot dispatch with allocation/deletion tracking enabled;
the existing sample-rate, state, automation and native-editor regressions also passed.
