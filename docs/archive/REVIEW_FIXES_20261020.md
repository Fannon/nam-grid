# Review fixes — 2026-10-20

> Historical record. Branch names, measurements and completion claims below describe
> their original checkpoint. See the [archive index](README.md) for current guides.

The external review examined `500271c`, before the stereo/Spring/Wah increment.
All four findings were rechecked against `567f567` and still applied. The source
report and its original reproduction artifacts remain unchanged in
`C:/Users/Fannon/dev/tmp/nam-dual-review-20261020`.

| Finding | Change | Regression coverage |
| --- | --- | --- |
| Malformed WAV sizes can overwrite the heap | Validate signed sizes, frame alignment and remaining payload before allocating; require exact reads; validate incomplete headers; commit decoded audio/rate only on success | PCM16/24/32/float, extensible WAV, odd sizes, negative/oversized counts, every truncated prefix and direct decoder entry points; malformed IR module preparation; focused AddressSanitizer run |
| PCM32 IR polarity flips | Use the positive scale `1 / 2147483648` | Equivalent positive/negative samples and endpoints across PCM16/24/32/float |
| Pre 2 clamps copied input gain | Give Input the same −40…+40 dB range as every other slot | Actual CLAP conversion at ±30/40 dB; Copy/Move/Swap menu callbacks in both directions at ±30 dB, source clearing and live publication |
| Master automation steps abruptly | Smooth linear gain per sample with a 5 ms time constant; initialize from the current parameter on host reset | Sustained cosine automation at 44.1/48/96 kHz with 1/3/64/127/512-frame blocks; equal trajectories and recalled startup level |

The WAV fixes are carried in `patches/audio-dsp-001-harden-wav-decoding.patch`
against the pinned MIT AudioDSPTools revision `e19ef4b`. Setup applies that patch
idempotently, just as it applies the existing NAM Core patches. Neither submodule
gitlink changes, and no unpublished dependency commit is required. A modified
AudioDSPTools working tree after setup is intentional. Applying the patch to a
fresh copy of the pinned decoder reproduces the normalized fixed source exactly.

## Reproduce

Build the regular plugin and all four test targets, then run CTest as described
in [BUILD.md](../BUILD.md). `wav-decoder` is a separate fast test so parser failures
can be diagnosed without NAM inference or a DAW.

For the sanitizer check, configure a separate directory with
`-DNAM_WAV_ASAN=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo`, build `nam-wav-tests`, then
run `ctest --test-dir <directory> -R wav-decoder --output-on-failure`. MSVC uses
`/fsanitize=address`; GCC/Clang use AddressSanitizer and UndefinedBehaviorSanitizer.
On Windows run from the compiler's developer environment so its ASan runtime is
on PATH. The local test used MSVC 19.44 with
`ASAN_OPTIONS=windows_hook_rtl_allocators=true:detect_leaks=0`.

The native Windows edit test resolves a command from the plugin's generated menu
and delivers it through iPlug2's normal native selection callback. It tests the
real EditSlot/commit/publish path without a production test extension or global
input injection. It does not claim manual menu navigation or DAW interaction.

The review's broader suggestions—shorter bypass drain work, asynchronous model
preparation and full callback/race/real-host profiling—remain follow-ups. This
increment addresses the four actionable findings. Bitwig listening, macOS and
VST3-host validation are still unverified.

## Verified locally

After relocating the checkout to `C:/Users/Fannon/dev/nam-grid`, a fresh Windows
Release configuration built both CLAP and VST3. All four CTest suites passed
(capture-library, wav-decoder, stages, clap-smoke; 85.60 seconds total).
The separate MSVC AddressSanitizer WAV test passed in 2.52 seconds. Dependency
patch setup was rechecked for idempotency at the new location. The old checkout
folder was removed. The system-installed CLAP binary was not replaced.
