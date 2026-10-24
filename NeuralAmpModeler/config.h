// Display name in hosts and the editor. Files, IDs and folders use "nam-grid".
#define PLUG_NAME "NAM Grid"
#define PLUG_MFR "Simon Heimler"
#define PLUG_VERSION_HEX 0x00000101
#define PLUG_VERSION_STR "0.1.1"
#define PLUG_VERSION_RC 0,1,1,0 // Windows VERSIONINFO; keep in step with PLUG_VERSION_STR
// Persistent host identity. Never change these after a release: hosts use them
// to find the plugin in saved projects. VST3 class IDs derive from both IDs.
#define PLUG_UNIQUE_ID 'NGrd'
#define PLUG_MFR_ID 'Fann'
#define PLUG_URL_STR "https://github.com/Fannon/nam-grid"
#define PLUG_EMAIL_STR ""
#define PLUG_COPYRIGHT_STR "Copyright 2026 Simon Heimler"
#define PLUG_CLASS_NAME NeuralAmpModeler
// CLAP ID: BUNDLE_DOMAIN "." BUNDLE_MFR "." BUNDLE_NAME = io.github.fannon.nam-grid
#define BUNDLE_NAME "nam-grid"
#define BUNDLE_MFR "fannon"
#define BUNDLE_DOMAIN "io.github"
#define CLAP_SUPPORT_URL "https://github.com/Fannon/nam-grid/issues"
#define CLAP_MANUAL_URL "https://fannon.github.io/nam-grid/"
#define CLAP_DESCRIPTION "Guitar rig grid for Neural Amp Modeler captures, cabinet IRs and built-in effects"

#define SHARED_RESOURCES_SUBPATH "nam-grid"

#define PLUG_CHANNEL_IO "1-1 1-2 2-2"

#define PLUG_LATENCY 0
#define PLUG_TYPE 0
// MIDI note-ons can retrigger the LFO and sequencer; audio is unaffected. CMake sets
// VST3_NUM_CC_CHANS=0 so VST3 adds no MIDI-CC proxy parameters to the host list.
#define PLUG_DOES_MIDI_IN 1
#define PLUG_DOES_MIDI_OUT 0
#define PLUG_DOES_MPE 0
#define PLUG_DOES_STATE_CHUNKS 1
#define PLUG_HAS_UI 1
#define PLUG_WIDTH 1000
#define PLUG_HEIGHT 720
#define PLUG_FPS 60
#define PLUG_SHARED_RESOURCES 0
#define PLUG_HOST_RESIZE 0
#define PLUG_MAX_WIDTH PLUG_WIDTH * 4
#define PLUG_MAX_HEIGHT PLUG_HEIGHT * 4

#define VST3_SUBCATEGORY "Fx"

#define ROBOTO_FN "Roboto-Regular.ttf"
