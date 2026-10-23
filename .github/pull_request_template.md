**Thanks for making a Pull Request!**
Please fill out this template so that you can be sure that your PR does everything it needs to be accepted.

## Description
What does your PR do?
Include [Closing words](https://docs.github.com/en/issues/tracking-your-work-with-issues/using-issues/linking-a-pull-request-to-an-issue) to link this PR to the Issue(s) that it relates to.

## PR Checklist
- [ ] Did you follow the repository's formatting and [contribution guidance](../CONTRIBUTING.md)?
- [ ] Did all CTest suites pass? Note the OS/compiler and any separate DAW tests.
- [ ] Does the VST3 plugin pass all of the unit tests in the [VST3PluginTestHost](https://steinbergmedia.github.io/vst3_dev_portal/pages/What+is+the+VST+3+SDK/Plug-in+Test+Host.html)? (Download it as part of the VST3 SDK [here](https://www.steinberg.net/developers/).)
  - [ ] Windows
  - [ ] macOS
- [ ] Does your PR add, remove, or rename any plugin parameters? If yes...
  - [ ] Have you ensured that the plug-in unserializes correctly?
  - [ ] Did you follow the [compatibility contract](../docs/development.md#parameter-and-project-compatibility) (append-only IDs, `introducedIn`, schema bump)?
- [ ] Does your PR add or remove any graphical assets? If yes, check the CMake resources, [main.rc](../NeuralAmpModeler/resources/main.rc) and [ThirdPartyNotices.txt](../ThirdPartyNotices.txt).
  
