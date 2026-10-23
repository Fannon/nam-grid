# Release checklist and validation limits

NAM Grid is preview software. Automated DSP, state and native-editor tests do not
establish listening quality, DAW stability or complete CLAP/VST3 host coverage.
Windows x64 and macOS arm64 are the CI build targets; Linux has no supported
plugin build with the current framework. See [build.md](build.md) for packaging
and [GitHub Actions](https://github.com/Fannon/nam-grid/actions/workflows/build-test.yml)
for results for individual commits.

Record the commit, platform, host, plugin format and results for each check before
tagging. An unchecked item is a requirement to verify, not a claim that it failed.

## Automated checks and packages

- [ ] Build CLAP and VST3 from a clean checkout with the pinned dependencies and
  tracked patches, following [build.md](build.md).
- [ ] Run CTest (ir-convolution, capture-library, wav-decoder, resampling, stages,
  clap-smoke with the native editor, tuner, parameter-contract and, on Windows,
  vst3-parameter-contract) and the Python tool tests.
- [ ] Confirm the CI matrix passes for every platform included in the release.
- [ ] Download and inspect the actual archives: plugin bundles, resources,
  `ThirdPartyNotices.txt` and `notices/`; retain macOS executable permissions.
- [ ] Validate VST3 with the SDK validator and CLAP with
  [clap-validator](https://github.com/free-audio/clap-validator). Record host
  validation separately from a successful compile or link.
  Accepted deviation: the three `state-reproducibility-*` checks report only the
  `Noise Learn` parameters. Learning is transient by design and always saved as
  off, so reopening a project never restarts a noise capture. Any other difference
  is a failure.

## DAW and listening checks

- [ ] Load real pedal/amp captures and cabinet IRs in a DAW. Test at
  32/64/128/512 frames and 44.1/48/96 kHz where the interface supports them.
- [ ] Listen during bypass, module replacement, automation, lane blending,
  transport stop/start and offline export; confirm latency compensation.
- [ ] Save/reopen a project with the editor closed, reopen the UI, and compare
  sound, settings and external file references. Exercise moved/missing files.
- [ ] Test repeated instance creation, editor reopen, display scaling, mono/stereo
  layouts and at least a 30-minute low-buffer session.
- [ ] Repeat host loading and operation for both plugin formats and each claimed
  platform. A macOS CI build does not establish real-host operation on macOS.

## Publication

- [ ] Finalize the release version, [notes](release-notes.md) and supported-platform
  wording, including remaining limits and external capture/IR requirements.
- [ ] Create a preview tag, verify its draft release and archives, then review the
  draft before publication. See [release workflow instructions](build.md).
- [ ] Preserve plugin identities, parameter IDs/names/ranges, module and algorithm
  IDs and saved-state fields. `parameter-contract` must pass without re-recording.
- [ ] Check that `ThirdPartyNotices.txt` lists every shipped dependency, adapted
  algorithm and the font, and that `notices/` holds the SDK licenses.

Record review evidence and results in the release PR or ignored `tmp/` reports.
