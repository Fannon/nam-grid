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
#include "CaptureLibrary.h"
#include <array>
#include <mutex>

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"


const int kNumPresets = 1;
// The plugin is mono inside
constexpr size_t kNumChannelsInternal = 1;

class NAMSender : public iplug::IPeakAvgSender<>
{
public:
  NAMSender()
  : iplug::IPeakAvgSender<>(-90.0, true, 5.0f, 1.0f, 300.0f, 500.0f)
  {
  }
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
  kNumParams = kQualityBase + 15
};

constexpr int kDualNumParams = kGridSlotBase;
constexpr int kGridV2NumParams = kDynamicsBase;
constexpr int kGridV3NumParams = kEQBase;
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
{ return std::string(slot < 4 ? "IN " : slot < 8 ? "A " : slot < 12 ? "B " : "OUT ") + std::to_string(slot % 4 + 1); }
inline std::array<int, 17> SlotParamIndices(size_t slot)
{
  std::array<int, 17> result{SlotInputParam(slot), SlotTrimParam(slot), SlotActiveParam(slot)};
  for (int i = 0; i < 6; ++i)
    result[3 + i] = SlotDynamicsParam(slot, i);
  for (int i = 0; i < 7; ++i)
    result[9 + i] = SlotEQParam(slot, i);
  result[16] = SlotFullParam(slot);
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
  size_t GetSelectedSlot() const { return mSelectedSlot; }
  void SelectSlot(size_t slot);
  void RescanCaptureFolder();
  std::string GetCaptureFolder() const;
  std::string GetCaptureStatus() const;
  std::shared_ptr<const nam_library::Snapshot> GetCaptureLibrary() const;
  nam_library::Filter GetCaptureFilter() const;
  void SetCaptureFilter(nam_library::Filter filter);
  std::string GetSlotPath(size_t slot) const;
  bool IsSlotLoaded(size_t slot) const;
  SlotEffect GetSlotEffect(size_t slot) const;
  bool CanToggleSlotQuality(size_t slot) const;
  void ToggleSlotQuality(size_t slot);
  void SetSlotEffect(size_t slot, SlotEffect effect, bool fromUI = true);
  std::string LoadSlot(size_t slot, const char* path);
  std::string LoadSlotIR(size_t slot, const char* path);
  std::string EditSlot(size_t source, size_t destination, SlotEditMode mode);
  void ClearSlot(size_t slot);

private:
  // Allocates mInputPointers and mOutputPointers
  void _AllocateIOPointers(const size_t nChans);
  // Adopts a prepared IR at silence; retirement is collected by OnIdle.
  void _ApplyDSPStaging();
  // Deallocates mInputPointers and mOutputPointers
  void _DeallocateIOPointers();
  // Sizes based on mInputArray
  size_t _GetBufferNumChannels() const;
  size_t _GetBufferNumFrames() const;
  void _InitToneStack();
  // Loads/prepares a NAM model on the control thread, then publishes to its fixed slot.
  // Returns an empty string on success, or an error message on failure.
  std::string _StageModel(const WDL_String& dspFile, size_t slot = 1);
  std::string _StageSlot(size_t slot, SlotEffect effect, const std::string& path);
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

  // Make sure that the latency is reported correctly.
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
  std::array<WDL_String, FixedNAMGrid::kSlots> mSlotPaths;
  std::array<NAMModelInfo, FixedNAMGrid::kSlots> mSlotInfo;
  std::array<SlotEffect, FixedNAMGrid::kSlots> mSlotEffects{};
  std::array<double, FixedNAMGrid::kSlots> mPreparedQuality{};
  mutable std::recursive_mutex mControlMutex; // never taken in ProcessBlock
  nam_library::Worker mLibraryWorker;
  std::shared_ptr<const nam_library::Snapshot> mLibrary = std::make_shared<nam_library::Snapshot>();
  std::string mCaptureFolder; // First root alias for older project readers.
  std::vector<std::string> mCaptureFolders;
  size_t mSelectedSlot = 0; // Editor selection only; never read by the audio thread.
  nam_library::Filter mCaptureFilter;
  bool mLibraryScanning = false;
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
  recursive_linear_filter::HighPass mHighPass;
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
