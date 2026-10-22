#pragma once

#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../AudioDSPTools/dsp/NoiseGate.h"
#include "../AudioDSPTools/dsp/dsp.h"
#include "../AudioDSPTools/dsp/wav.h"
#include "../AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h"
#include "../NeuralAmpModelerCore/NAM/dsp.h"
#include "../NeuralAmpModelerCore/NAM/slimmable.h"

#include "Colors.h"
#include "ToneStack.h"
#include "NAMStage.h"
#include "FixedNAMGrid.h"
#include "SlotOperations.h"
#include "LibraryBrowser.h"
#include "LevelAnalysis.h"
#include "GridTuner.h"
#include <array>
#include <mutex>

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"


const int kNumPresets = 1;
// Stereo grid; mono NAM slots deliberately collapse to mono for one inference pass.
constexpr size_t kNumChannelsInternal = 2;

class NAMSender : public iplug::ISender<1, 64, std::pair<float, float>>
{
public:
  void Reset(double rate) { mAnalysis.Reset(rate); }
  void ProcessBlock(iplug::sample** inputs, int frames, int tag, int channels)
  {
    if (channels <= 0) return;
    for (int i = 0; i < frames; ++i)
    {
      const double mono = channels == 1 ? inputs[0][i] : .5 * (inputs[0][i] + inputs[1][i]);
      if (mAnalysis.Push(mono))
      {
        iplug::ISenderData<1, std::pair<float, float>> packet(tag, 1, 0);
        packet.vals[0] = {static_cast<float>(mAnalysis.peak), static_cast<float>(mAnalysis.rms)};
        PushData(packet);
      }
    }
  }
private:
  LevelAnalysis mAnalysis;
};

enum EParams
{
  // These need to be the first ones because I use their indices to place
  // their rects in the GUI.
  kInputLevel = 0,
  kNoiseGateThreshold,
  kToneBass,
  kToneMid,
  kToneTreble,
  kOutputLevel,
  // The rest is fine though.
  kNoiseGateActive,
  kEQActive,
  kIRToggle,
  // Input calibration
  kCalibrateInput,
  kInputCalibrationLevel,
  kOutputMode,
  kSlim,
  kDriveInput,
  kDriveTrim,
  kDriveActive,
  kAmpTrim,
  kAmpActive,
  kGridSlotBase,
  kParallelBlend = kGridSlotBase + 14 * 3,
  kDynamicsBase,
  kEQBase = kDynamicsBase + 16 * 6,
  kQualityBase = kEQBase + 16 * 7,
  kUtilityBase = kQualityBase + 15,
  kTunerReference = kUtilityBase + 16 * kUtilityControls,
  kTunerMute,
  kGridParamBase,
  kTunerActive = kGridParamBase + grid_performance::Count,
  kNumParams
};

constexpr int kDualNumParams = kGridSlotBase;
constexpr int kGridV2NumParams = kDynamicsBase;
constexpr int kGridV3NumParams = kEQBase;
constexpr int SlotUtilityParam(size_t slot, int control)
{
  return kUtilityBase + static_cast<int>(slot) * kUtilityControls + control;
}
constexpr int SlotEQParam(size_t slot, int control)
{ return kEQBase + static_cast<int>(slot) * 7 + control; }
constexpr int SlotFullParam(size_t slot)
{ return slot == 1 ? kSlim : kQualityBase + static_cast<int>(slot) - (slot > 1 ? 1 : 0); }
constexpr int SlotDynamicsParam(size_t slot, int control)
{ return kDynamicsBase + static_cast<int>(slot) * 6 + control; }
constexpr int SlotInputParam(size_t slot)
{ return slot == 0 ? kDriveInput : slot == 1 ? kInputLevel : kGridSlotBase + (slot - 2) * 3; }
constexpr int SlotTrimParam(size_t slot)
{ return slot == 0 ? kDriveTrim : slot == 1 ? kAmpTrim : SlotInputParam(slot) + 1; }
constexpr int SlotActiveParam(size_t slot)
{ return slot == 0 ? kDriveActive : slot == 1 ? kAmpActive : SlotInputParam(slot) + 2; }
inline std::string GridSlotName(size_t slot)
{ return std::string(slot < 4 ? "Pre " : slot < 8 ? "A " : slot < 12 ? "B " : "Post ") + std::to_string(slot % 4 + 1); }
inline std::array<int, 17 + kUtilityControls> SlotParamIndices(size_t slot)
{
  std::array<int, 17 + kUtilityControls> result{SlotInputParam(slot), SlotTrimParam(slot), SlotActiveParam(slot)};
  for (int i = 0; i < 6; ++i)
    result[3 + i] = SlotDynamicsParam(slot, i);
  for (int i = 0; i < 7; ++i)
    result[9 + i] = SlotEQParam(slot, i);
  result[16] = SlotFullParam(slot);
  for (int i = 0; i < kUtilityControls; ++i)
    result[17 + i] = SlotUtilityParam(slot, i);
  return result;
}

const int numKnobs = 6;

enum ECtrlTags
{
  kCtrlTagModelFileBrowser = 0,
  kCtrlTagIRFileBrowser,
  kCtrlTagInputMeter,
  kCtrlTagOutputMeter,
  kCtrlTagSettingsBox,
  kCtrlTagOutputMode,
  kCtrlTagCalibrateInput,
  kCtrlTagInputCalibrationLevel,
  kCtrlTagSlimmableIcon,
  kCtrlTagSlimOverlayBackdrop,
  kCtrlTagSlimKnob,
  kCtrlTagDriveFileBrowser,
  kCtrlTagCaptureFolder,
  kCtrlTagCaptureStatus,
  kCtrlTagCaptureSearch,
  kCtrlTagGearFilter,
  kCtrlTagClearFilter,
  kCtrlTagTuner,
  kCtrlTagTunerButton,
  kCtrlTagPerformance,
  kCtrlTagPresetName,
  kCtrlTagPresetPrevious,
  kCtrlTagPresetNext,
  kCtrlTagLibraryBrowser,
  kCtrlTagGridSlotBase = 100,
  kNumCtrlTags
};

enum EMsgTags
{
  // These tags are used from UI -> DSP
  kMsgTagClearModel = 0,
  kMsgTagClearIR,
  kMsgTagHighlightColor,
  // The following tags are from DSP -> UI
  kMsgTagLoadFailed,
  kMsgTagLoadedModel,
  kMsgTagLoadedIR,
  kMsgTagClearDrive,
  kNumMsgTags
};

// The reused filter retains its histories across SetParams; host reset must
// explicitly clear both channels so a stopped project's tail cannot leak back.
class GridDCBlocker : public recursive_linear_filter::HighPass
{
public:
  void ClearHistory()
  {
    for (auto& channel : mInputHistory)
      std::fill(channel.begin(), channel.end(), 0.);
    for (auto& channel : mOutputHistory)
      std::fill(channel.begin(), channel.end(), 0.);
    mInputStart = static_cast<long>(mInputCoefficients.size());
    mOutputStart = static_cast<long>(mOutputCoefficients.size());
  }
};

class NeuralAmpModeler final : public iplug::Plugin
{
public:
  NeuralAmpModeler(const iplug::InstanceInfo& info);
  ~NeuralAmpModeler();

  void ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnIdle() override;

  bool SerializeState(iplug::IByteChunk& chunk) const override;
  int UnserializeState(const iplug::IByteChunk& chunk, int startPos) override;
  void OnUIOpen() override;
  bool IsTunerActive() const { return GetParam(kTunerActive)->Bool(); }
  auto TunerHistory() const { return mTuner.ReadHistory(); }
  double TunerFrequency() const { return mTuner.Frequency(); }
  void StorePerformanceScene(size_t scene);
  void RecallPerformanceScene(size_t scene);
  bool HasPerformanceScene(size_t scene) const;
  bool IsPerformanceSceneParam(int index) const;
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override { return true; }

  void OnParamChange(int paramIdx) override;
  void OnParamChangeUI(int paramIdx, iplug::EParamSource source) override;
  bool OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData) override;

  // Native library controls; all called on the control/UI thread.
  void SetCaptureFolder(const char* folder, bool fromUI = false);
  void SetCaptureFolders(std::vector<std::string> folders, bool fromUI = false);
  std::vector<std::string> GetCaptureFolders() const;
  void AddCaptureFolder(const char* folder);
  void RemoveCaptureFolder(size_t index);
  void SetPresetFolders(std::vector<std::string> folders, bool fromUI = false);
  std::vector<std::string> GetPresetFolders() const;
  void OpenLibraryBrowser(nam_library::BrowserContext context, nam_library::FileKind kind, size_t slot = 0);
  size_t GetSelectedSlot() const { return mSelectedSlot; }
  void SelectSlot(size_t slot);
  void RescanCaptureFolder();
  bool GetRescanOnStartup() const { return mRescanOnStartup; }
  bool GetShowVisualizations() const { return mShowVisualizations; }
  void SetShowVisualizations(bool enabled);
  void SetRescanOnStartup(bool enabled);
  std::string GetLibraryMatchStatus() const;
  bool IsLibraryScanning() const { return mLibraryScanning; }
  std::string GetCaptureFolder() const;
  std::string GetCaptureStatus() const;
  std::shared_ptr<const nam_library::Snapshot> GetCaptureLibrary() const;
  nam_library::Filter GetCaptureFilter() const;
  void SetCaptureFilter(nam_library::Filter filter);
  std::string GetSlotPath(size_t slot) const;
  bool IsSlotLoaded(size_t slot) const;
  SlotEffect GetSlotEffect(size_t slot) const;
  nam_library::Gear GetSlotGear(size_t slot) const;
  bool CanToggleSlotQuality(size_t slot) const;
  noise_print::Profile GetNoiseProfile(size_t slot) const;
  void ToggleSlotQuality(size_t slot);
  void SetSlotEffect(size_t slot, SlotEffect effect, bool fromUI = true);
  std::string LoadSlot(size_t slot, const char* path);
  std::string LoadSlotIR(size_t slot, const char* path);
  std::string EditSlot(size_t source, size_t destination, SlotEditMode mode);
  void ClearSlot(size_t slot);
  void LoadSlapbackPreset(size_t slot);
  std::string SaveRigPreset(const std::filesystem::path& path);
  std::string LoadRigPreset(const std::filesystem::path& path);
  void PromptRigPreset(bool save);
  std::vector<std::filesystem::path> GetRigPresetFiles() const;
  std::string GetRigPresetName() const;
  void StepRigPreset(int direction);

private:
  // Allocates mInputPointers and mOutputPointers
  void _AllocateIOPointers(const size_t nChans);
  // Adopts a prepared IR at silence; retirement is collected by OnIdle.
  void _ApplyDSPStaging();
  void _UpdatePresetControls();
  std::string mPresetName = "Untitled", mPresetDirectory;
  // Deallocates mInputPointers and mOutputPointers
  void _DeallocateIOPointers();
  // Sizes based on mInputArray
  size_t _GetBufferNumChannels() const;
  size_t _GetBufferNumFrames() const;
  std::atomic<double> mEffectTailSeconds{2.};
  void _InitToneStack();
  // Loads/prepares a NAM model on the control thread, then publishes to its fixed slot.
  // Returns an empty string on success, or an error message on failure.
  std::string _StageModel(const WDL_String& dspFile, size_t slot = 1);
  std::string _StageSlot(size_t slot, SlotEffect effect, const std::string& path, const std::string& expectedHash = {});
  void _ResolveLibraryFiles();
  SlotConfig _GetSlotConfig(size_t slot) const;
  void _CommitSlot(size_t slot, PreparedSlot prepared, bool updateParameters);
  // Loads/prepares an IR on the control thread.
  // Return status code so that error messages can be relayed if
  // it wasn't successful.
  dsp::wav::LoadReturnCode _StageIR(const WDL_String& irPath);

  void _ClearModel(size_t slot, SlotEffect effect = SlotEffect::NAM);
  // Prepare the input & output buffers
  void _PrepareBuffers(const size_t numChannels, const size_t numFrames);
  // Manage pointers
  void _PrepareIOPointers(const size_t nChans);
  // Copy the input buffer to the object, applying input level.
  // :param nChansIn: In from external
  // :param nChansOut: Out to the internal of the DSP routine
  void _ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn, const size_t nChansOut);
  // Copy the output to the output buffer, applying output level.
  // :param nChansIn: In from internal
  // :param nChansOut: Out to external
  void _ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames, const size_t nChansIn,
                      const size_t nChansOut);
  // Resetting for models and IRs, called by OnReset
  void _ResetModelAndIR(const double sampleRate, const int maxBlockSize);


  // See: Unserialization.cpp
  void _UnserializeApplyConfig(nlohmann::json& config);
  // 0.7.9 and later
  int _UnserializeStateWithKnownVersion(const iplug::IByteChunk& chunk, int startPos);
  // Hopefully 0.7.3-0.7.8, but no gurantees
  int _UnserializeStateWithUnknownVersion(const iplug::IByteChunk& chunk, int startPos);

  // Update all controls that depend on a model
  void _UpdateControlsFromModel();
  void _UpdateTunerControls();

  // Make sure that the latency is reported correctly.
  void _UpdateTailSize();
  void _UpdateLatency(bool forceRestart = false);
  void _UpdateLibraryControls();
  void _NotifyHostStateChanged();

  // Update level meters
  // Called within ProcessBlock().
  // Assume _ProcessInput() and _ProcessOutput() were run immediately before.
  void _UpdateMeters(iplug::sample** inputPointer, iplug::sample** outputPointer, const size_t nFrames,
                     const size_t nChansIn, const size_t nChansOut);

  // Member data

  // Input arrays to NAM
  std::vector<std::vector<iplug::sample>> mInputArray;
  // Output from NAM
  std::vector<std::vector<iplug::sample>> mOutputArray;
  // Pointer versions
  iplug::sample** mInputPointers = nullptr;
  iplug::sample** mOutputPointers = nullptr;

  // Noise gates
  dsp::noise_gate::Trigger mNoiseGateTrigger;
  dsp::noise_gate::Gain mNoiseGateGain;
  FixedNAMGrid mGrid;
  grid_tuner::Tuner mTuner;
  grid_performance::Modulation mModulation;
  // Low 16 bits: Freeze slots; high 16 bits: Shimmer slots. One control/audio snapshot.
  std::atomic<uint32_t> mFreezeMask{0};
  std::array<nlohmann::json, 3> mPerformanceScenes{nlohmann::json::object(), nlohmann::json::object(), nlohmann::json::object()};
  double mTunerGain = 1., mMasterGain = 1., mMasterCoefficient = 0.;
  double mGlobalInputGain = 1.;
  std::array<WDL_String, FixedNAMGrid::kSlots> mSlotPaths;
  std::array<std::string, FixedNAMGrid::kSlots> mSlotHashes;
  std::array<NAMModelInfo, FixedNAMGrid::kSlots> mSlotInfo;
  std::array<SlotEffect, FixedNAMGrid::kSlots> mSlotEffects{};
  std::array<noise_print::Profile, FixedNAMGrid::kSlots> mNoiseProfiles{};
  std::array<uint64_t, FixedNAMGrid::kSlots> mNoiseEpochs{}, mNoiseSeen{};
  uint64_t mNextNoiseEpoch = 0;
  std::array<nam_library::Gear, FixedNAMGrid::kSlots> mSlotGear{};
  std::array<double, FixedNAMGrid::kSlots> mPreparedQuality{};
  mutable std::recursive_mutex mControlMutex; // never taken in ProcessBlock
  nam_library::Worker mLibraryWorker;
  std::shared_ptr<const nam_library::Snapshot> mLibrary = std::make_shared<nam_library::Snapshot>();
  std::string mCaptureFolder; // First root alias for older project readers.
  std::vector<std::string> mCaptureFolders;
  std::vector<std::string> mPresetFolders;
  size_t mSelectedSlot = 0; // Editor selection only; never read by the audio thread.
  nam_library::Filter mCaptureFilter;
  bool mLibraryScanning = false;
  bool mLibraryIndexed = false;
  bool mRescanOnStartup = true;
  bool mShowVisualizations = true;
  bool mLibraryDefaultSaveFailed = false;
  RealtimeExchange<PreparedIR> mCabinet;
  NAMModelInfo mAmpInfo;
  bool mDriveLoaded = false, mIRLoaded = false;
  double mSampleRatePrepared = 48000.0;
  int mMaxBlockPrepared = 2048;
  std::array<int, FixedNAMGrid::kSlots> mRequestedLatencies{};
  double mIRSwitchGain = 1.;
  // Tone stack modules
  std::unique_ptr<dsp::tone_stack::AbstractToneStack> mToneStack;

  // Post-IR filters
  GridDCBlocker mHighPass;
  //  recursive_linear_filter::LowPass mLowPass;

  // Path to model's config.json or model.nam
  WDL_String mNAMPath;
  WDL_String mDrivePath;
  // Path to IR (.wav file)
  WDL_String mIRPath;

  WDL_String mHighLightColor{PluginColors::NAM_THEMECOLOR.ToColorCode()};

  NAMSender mInputSender, mOutputSender;
  iplug::ISender<1, 128, SlotMeterData> mSlotMeterSender;
  int mMeterFrames = 0;
};
