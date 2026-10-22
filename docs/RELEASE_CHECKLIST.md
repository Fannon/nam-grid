# Release checklist and validation limits

nam-grid is preview software. Automated DSP, state and native-editor tests do not
establish listening quality, DAW stability or complete CLAP/VST3 host coverage.
Windows x64 and macOS arm64 are the CI build targets; Linux has no supported
plugin build with the current framework. See [BUILD.md](BUILD.md) for packaging
and [GitHub Actions](https://github.com/Fannon/nam-grid/actions/workflows/build-native.yml)
for results for individual commits.

Record the commit, platform, host, plugin format and results for each check before
tagging. An unchecked item is a requirement to verify, not a claim that it failed.

## Automated checks and packages

- [ ] Build CLAP and VST3 from a clean checkout with the pinned dependencies and
  tracked patches, following [BUILD.md](BUILD.md).
- [ ] Run CTest: capture-library, wav-decoder, resampling, stages, clap-smoke and tuner.
- [ ] Confirm the CI matrix passes for every platform included in the release.
- [ ] Download and inspect the actual archives: plugin bundles, resources,
  `ThirdPartyNotices.txt` and `notices/`; retain macOS executable permissions.
- [ ] Validate VST3 with the SDK validator and CLAP with a dedicated validator.
  Record host validation separately from a successful compile or link.

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

- [ ] Finalize the release version, [notes](RELEASE_NOTES.md) and supported-platform
  wording, including remaining limits and external capture/IR requirements.
- [ ] Create a preview tag, verify its draft release and archives, then review the
  draft before publication. See [release workflow instructions](BUILD.md).
- [ ] Preserve existing plugin identities. Document compatibility changes to
  prototype projects, parameter IDs, names or enum ranges.

The [2026-10-21 first-release review](archive/FIRST_RELEASE_REVIEW.md) preserves
the original findings and checkpoint evidence behind this checklist.
