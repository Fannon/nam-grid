#pragma once

// AudioDSPTools resampling and IGraphics menus expect these iPlug2/WDL headers first.
#include "IGraphicsStructs.h"
#include "../AudioDSPTools/dsp/ImpulseResponse.h"
#include "../AudioDSPTools/dsp/dsp.h"
#include "../AudioDSPTools/dsp/wav.h"
#include "../AudioDSPTools/dsp/ResamplingContainer/ResamplingContainer.h"
#include "../NeuralAmpModelerCore/NAM/dsp.h"
#include "../NeuralAmpModelerCore/NAM/slimmable.h"

#include "NAMStage.h"
#include "FixedNAMGrid.h"
#include "RigTransition.h"
#include "AudioControlCache.h"
#include "SlotOperations.h"
#include "ImpulseProbe.h"
#include "LibraryBrowser.h"
#include "LevelAnalysis.h"
#include "GridTuner.h"
#include "UserPaths.h"
#include <array>
#include <atomic>
#include <mutex>
#include <map>

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"


const int kNumPresets = 1;
// Stereo grid; mono NAM slots deliberately collapse to mono for one inference pass.
constexpr size_t kNumChannelsInternal = 2;

// Left/right sample peaks per UI frame for the grid IN/OUT meters; a mono source feeds both.
class NAMSender : public iplug::ISender<1, 64, std::pair<float, float>>
{
public:
  void Reset(double rate) { for (auto& analysis : mAnalysis) analysis.Reset(rate); }
  void ProcessBlock(iplug::sample** inputs, int frames, int tag, int channels)
  {
    if (channels <= 0) return;
    for (int i = 0; i < frames; ++i)
    {
      // Both channels share one window position, so they report together.
      const bool ready = mAnalysis[0].Push(inputs[0][i]);
      mAnalysis[1].Push(inputs[channels > 1 ? 1 : 0][i]);
      if (ready)
      {
        iplug::ISenderData<1, std::pair<float, float>> packet(tag, 1, 0);
        packet.vals[0] = {static_cast<float>(mAnalysis[0].peak), static_cast<float>(mAnalysis[1].peak)};
        PushData(packet);
      }
    }
  }
private:
  std::array<LevelAnalysis, 2> mAnalysis;
};

#include "ParameterLayout.h"
#include "ModulationMatrix.h"
#include "ModulePresets.h"

enum ECtrlTags
{
  kCtrlTagInputMeter = 0,
  kCtrlTagOutputMeter,
  kCtrlTagSettingsBox,
  kCtrlTagTuner,
  kCtrlTagTunerButton,
  kCtrlTagPerformance,
  kCtrlTagPresetName,
  kCtrlTagPresetPrevious,
  kCtrlTagPresetNext,
  kCtrlTagPresetSave,
  kCtrlTagPresetNew,
  kCtrlTagLibraryBrowser,
  kCtrlTagRouting,
  kCtrlTagRoutingButton,
  kCtrlTagMainMacros,
  kCtrlTagGridSlotBase = 100,
  kCtrlTagPlotHelpBase = 200,
  kNumCtrlTags = kCtrlTagPlotHelpBase + 16
};

// Delegate -> slot control messages.
enum EMsgTags
{
  kMsgTagLoadFailed = 0,
  kMsgTagLoadedModel,
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
  void OnUIClose() override;
  void OnRestoreState() override { _SendAllParamValuesToUI(); }
  void OnParamReset(iplug::EParamSource source) override;
  void BeginInformHostOfParamChangeFromUI(int paramIdx) override;
  void SendParameterValueFromUI(int paramIdx, double value) override;
  void EndInformHostOfParamChangeFromUI(int paramIdx) override;
  bool IsTunerActive() const { return GetParam(kTunerActive)->Bool(); }
  // Editor only: measures built-in modules' impulse responses on a worker thread.
  ImpulseProbe& GetImpulseProbe() { return mImpulseProbe; }
  auto TunerHistory() const { return mTuner.ReadHistory(); }
  double TunerFrequency() const { return mTuner.Frequency(); }
  void StorePerformanceScene(size_t scene);
  void RecallPerformanceScene(size_t scene);
  void ClearPerformanceScene(size_t scene);
  bool HasPerformanceScene(size_t scene) const;
  // Last stored or recalled scene for the header buttons, or -1. Editor state only.
  int GetActiveScene() const { return mActiveScene; }
  // The routing panel replaces the module editor below the grid. Editor state only.
  bool IsRoutingOpen() const { return mRoutingOpen; }
  void SetRoutingOpen(bool open);
  bool IsPerformanceSceneParam(int index) const;
  // Modulation matrix editing on the control/UI thread. Values are plain (not normalized).
  void SetParameterFromUI(int index, double value);
  // Adds or finds a row from `source` to a destination; returns the row, or -1 when all are used.
  int AddModulation(int source, int slot, int control, double depth = 25.);
  void ClearModulationRow(int row);
  grid_performance::Values PerformanceValues() const;
  grid_performance::Modulation::Monitor ModulationMonitor() const { return mModulation.Read(); }
  bool OnHostRequestingSupportedViewConfiguration(int width, int height) override { return true; }

  void OnParamChange(int paramIdx) override;
  // Note-ons retrigger the LFO and sequencer when their Retrigger is MIDI note.
  void ProcessMidiMsg(const iplug::IMidiMsg& msg) override;
  void OnParamChangeUI(int paramIdx, iplug::EParamSource source) override;
  // Keep the existing parameter handles for editor/state compatibility, but do
  // not emit automation gestures or values for expandable selectors (IsStateOnlyParam).
  void BeginInformHostOfParamChange(int index) override
  {
    if (!IsStateOnlyParam(index))
      iplug::Plugin::BeginInformHostOfParamChange(index);
  }
  void InformHostOfParamChange(int index, double value) override
  {
    if (!IsStateOnlyParam(index))
      iplug::Plugin::InformHostOfParamChange(index, value);
  }
  void EndInformHostOfParamChange(int index) override
  {
    if (!IsStateOnlyParam(index))
      iplug::Plugin::EndInformHostOfParamChange(index);
  }

  // Native library controls; all called on the control/UI thread.
  // NAM, IR and RigPreset each have their own source list, scanned only for that kind.
  void SetLibraryFolders(nam_library::FileKind kind, std::vector<std::string> folders, bool fromUI = false);
  std::vector<std::string> GetLibraryFolders(nam_library::FileKind kind) const;
  void AddLibraryFolder(nam_library::FileKind kind, const char* folder);
  void RemoveLibraryFolder(nam_library::FileKind kind, size_t index);
  void OpenLibraryBrowser(nam_library::BrowserContext context, nam_library::FileKind kind, size_t slot = 0);
  size_t GetSelectedSlot() const { return mSelectedSlot; }
  void SelectSlot(size_t slot);
  void RescanCaptureFolder(bool rescan = true);
  bool GetRescanOnStartup() const { return mRescanOnStartup; }
  bool GetShowVisualizations() const { return mShowVisualizations; }
  bool GetShowMainMacros() const { return mShowMainMacros; }
  void SetShowMainMacros(bool enabled);
  std::string GetMacroName(int macro) const;
  void SetMacroName(int macro, std::string name);
  bool IsMacroAssigned(int macro) const;
  void SetShowVisualizations(bool enabled);
  // Rig preset switches crossfade both rigs, or dip (old out, then new in) to spare CPU.
  bool GetPresetCrossfade() const { return mPresetCrossfade; }
  void SetPresetCrossfade(bool enabled);
  void SetRescanOnStartup(bool enabled);
  std::string GetLibraryMatchStatus() const;
  bool IsLibraryScanning() const { return mLibraryScanning; }
  std::string GetCaptureStatus() const;
  std::shared_ptr<const nam_library::Snapshot> GetCaptureLibrary() const;
  nam_library::Filter GetCaptureFilter() const;
  void SetCaptureFilter(nam_library::Filter filter);
  std::string GetSlotPath(size_t slot) const;
  bool IsSlotLoaded(size_t slot) const;
  NAMModelInfo GetSlotModelInfo(size_t slot) const
  { return slot < mSlotInfo.size() ? mSlotInfo[slot] : NAMModelInfo{}; }
  SlotEffect GetSlotEffect(size_t slot) const;
  nam_library::Gear GetSlotGear(size_t slot) const;
  bool CanToggleSlotQuality(size_t slot) const;
  noise_print::Profile GetNoiseProfile(size_t slot) const;
  void ToggleSlotQuality(size_t slot);
  void SetSlotEffect(size_t slot, SlotEffect effect, bool fromUI = true);
  std::string LoadSlot(size_t slot, const char* path);
  std::string LoadSlotIR(size_t slot, const char* path);
  std::string BeginSlotPreview(size_t slot);
  std::string PreviewSlot(SlotEffect effect, const std::string& path);
  void EndSlotPreview(bool accept);
  std::string EditSlot(size_t source, size_t destination, SlotEditMode mode);
  void ClearSlot(size_t slot);
  void LoadSlapbackPreset(size_t slot);
  // Current values of one slot's controls, in SlotValues order.
  SlotValues GetSlotValues(size_t slot) const;
  // Sets the slot's module controls to one of its factory module presets (ModulePresets.h)
  // as ordinary host parameter changes; the module keeps running.
  void ApplyModulePreset(size_t slot, int preset);
  // User module presets: JSON files under each preset folder's "Module Presets". The
  // folder for saving this module's presets is empty when no preset folder is configured.
  std::filesystem::path GetModulePresetFolder(SlotEffect effect) const;
  std::vector<module_presets::UserPreset> GetUserModulePresets(SlotEffect effect, int* unreadable = nullptr) const;
  std::string SaveUserModulePreset(size_t slot, const std::filesystem::path& file);
  void ApplyUserModulePreset(size_t slot, const module_presets::UserPreset& preset);
  std::string SaveRigPreset(const std::filesystem::path& path);
  std::string LoadRigPreset(const std::filesystem::path& path);
  std::string InstallDrivePresets(const std::filesystem::path& folder);
  void PromptRigPreset(bool save);
  // Saves over the loaded preset file, or prompts Save as when there is none.
  void QuickSaveRigPreset();
  // The loaded preset's existing file, or empty for an unsaved rig.
  std::filesystem::path GetRigPresetFile() const;
  // Replaces the rig with a fresh instance's empty grid; folders and tuner stay.
  bool ConfirmNewRig();
  std::string NewRig();
  // Deletes the .ngrid file only; the loaded rig and its name are unchanged.
  bool ConfirmDeleteRigPreset(const std::filesystem::path& path);
  std::string DeleteRigPreset(const std::filesystem::path& path);
  std::vector<std::filesystem::path> GetRigPresetFiles() const;
  std::string GetRigPresetName() const;
  void StepRigPreset(int direction);

private:
  // Allocates mInputPointers and mOutputPointers
  void _AllocateIOPointers(const size_t nChans);
  void _UpdatePresetControls();
  void _SendAllParamValuesToUI();
  void _RefreshParamDependentUI(bool tunerChanged);
  nlohmann::json _CurrentRig() const;
  void _ApplyRig(nlohmann::json rig, const std::string& name, const std::string& directory);
  nlohmann::json mEmptyRig;
  std::string mPresetName = "Untitled", mPresetDirectory;
  // Deallocates mInputPointers and mOutputPointers
  void _DeallocateIOPointers();
  // Sizes based on mInputArray
  size_t _GetBufferNumChannels() const;
  size_t _GetBufferNumFrames() const;
  std::atomic<double> mEffectTailSeconds{2.};
  // Loads/prepares a slot on the control thread, then publishes it to the audio thread.
  // Returns an empty string on success, or an error message on failure.
  std::string _StageSlot(size_t slot, SlotEffect effect, const std::string& path, const std::string& expectedHash = {});
  void _ResolveLibraryFiles();
  SlotConfig _GetSlotConfig(size_t slot) const;
  void _CommitSlot(size_t slot, PreparedSlot prepared, bool updateParameters, bool notifyHost = true);
  void _ClearModel(size_t slot, SlotEffect effect = SlotEffect::NAM);
  // Prepare the input & output buffers
  void _PrepareBuffers(const size_t numChannels, const size_t numFrames);
  // Manage pointers
  void _PrepareIOPointers(const size_t nChans);
  // Copy the input buffer to the object, applying input level.
  // :param nChansIn: In from external
  // :param nChansOut: Out to the internal of the DSP routine
  void _ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn, const size_t nChansOut,
                     double gain);
  // Copy the output to the output buffer, applying output level.
  // :param nChansIn: In from internal
  // :param nChansOut: Out to external
  void _ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames, const size_t nChansIn,
                      const size_t nChansOut, double gain);
  void _UnserializeApplyConfig(nlohmann::json& config);
  // Library folders from this user's settings.json (control thread).
  void _ReloadLibraryFolders();
  void _InstallDefaultContent(const std::filesystem::path& namFolder, const std::filesystem::path& presetFolder);

  // Update all controls that depend on a model
  void _UpdateControlsFromModel();
  void _UpdateTunerControls();

  // Make sure that the latency is reported correctly.
  void _UpdateTailSize();
  void _UpdateLatency(bool forceRestart = false);
  void _UpdateLibraryControls();
  void _NotifyHostStateChanged();
  void _ApplyModuleValues(size_t slot, std::span<const module_presets::Value> values);

  // Member data

  // Input arrays to NAM
  std::vector<std::vector<iplug::sample>> mInputArray;
  // Output from NAM
  std::vector<std::vector<iplug::sample>> mOutputArray;
  // Pointer versions
  iplug::sample** mInputPointers = nullptr;
  iplug::sample** mOutputPointers = nullptr;

  // Two grids let a rig preset switch prepare the next rig while the current one plays.
  std::array<FixedNAMGrid, 2> mGrids;
  int mEditGrid = 0;              // control thread: receives slot edits and staged rigs
  bool mInactiveGridUsed = false; // control thread: the other grid still holds an old rig
  int mAudibleGrid = 0, mOutgoingGrid = 0; // audio thread, or host stopped
  RigTransition mRigTransition;
  // Audio only: the held rig's controls, and the frozen outgoing rig during a switch.
  grid_performance::Values mHeldPerformance{};
  double mHeldBlend = .5, mHeldInputGain = 1., mHeldOutputGain = 1.;
  std::array<FixedNAMGrid::Controls, FixedNAMGrid::kSlots> mOutgoingControls{};
  grid_performance::Routing mOutgoingRouting{};
  double mOutgoingBlend = .5;
  bool mLastModulated = false;
  std::array<std::vector<double>, 2> mOutgoingBuffers;
  AudioControlCache<FixedNAMGrid::Controls, FixedNAMGrid::kSlots> mAudioControls;
  std::array<FixedNAMGrid::Controls, FixedNAMGrid::kSlots> mAnimatedControls{}; // audio only
  grid_tuner::Tuner mTuner;
  grid_performance::Modulation mModulation;
  grid_modulation::Matrix mModulationMatrix;
  // MIDI note-on frames of the current block; later notes beyond the limit are ignored.
  static constexpr int kMaxNotesPerBlock = 128;
  std::array<int, kMaxNotesPerBlock> mNoteFrames{};
  int mNoteCount = 0;
  int mActiveScene = -1;
  bool mRoutingOpen = false;
  // Low 16 bits: Freeze slots; high 16 bits: Shimmer slots. One control/audio snapshot.
  std::atomic<uint32_t> mFreezeMask{0}, mStutterMask{0};
  std::array<nlohmann::json, 3> mPerformanceScenes{nlohmann::json::object(), nlohmann::json::object(), nlohmann::json::object()};
  std::array<std::string, 4> mMacroNames{"Macro 1", "Macro 2", "Macro 3", "Macro 4"};
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
  std::array<double, FixedNAMGrid::kSlots> mPreparedIRLength{};
  std::array<double, FixedNAMGrid::kSlots> mPreparedIRTail{};
  mutable std::recursive_mutex mControlMutex; // never taken in ProcessBlock
  // Every control-state section locks through this. On Linux, iPlug2 draws and runs OnIdle on
  // threads of its own and holds its graphics mutex while calling the plugin, so take both
  // together: one lock order on every thread, and no deadlock with plugin-to-UI messages.
#ifdef OS_LINUX
  [[nodiscard]] std::scoped_lock<std::recursive_mutex, std::recursive_mutex> _LockControls() const
  {
    return std::scoped_lock(const_cast<NeuralAmpModeler*>(this)->GfxMutex(), mControlMutex);
  }
#else
  [[nodiscard]] std::lock_guard<std::recursive_mutex> _LockControls() const { return std::lock_guard(mControlMutex); }
#endif
  nam_library::Worker mLibraryWorker{user_paths::IndexFile()};
  std::vector<std::string> mFactoryLibraryFiles;
  std::shared_ptr<const nam_library::Snapshot> mLibrary = std::make_shared<nam_library::Snapshot>();
  std::vector<std::string> mNamFolders, mIrFolders, mPresetFolders;
  std::vector<std::string>* _LibraryFolders(nam_library::FileKind kind);
  size_t mSelectedSlot = 0; // Editor selection only; never read by the audio thread.
  std::unique_ptr<PreparedSlot> mPreviewOriginal;
  size_t mPreviewSlot = 0;
  std::map<std::pair<SlotEffect, std::string>, SlotConfig> mPreviewChoices;
  nam_library::Filter mCaptureFilter;
  bool mLibraryScanning = false;
  bool mLibraryIndexed = false;
  bool mRescanOnStartup = true;
  bool mShowVisualizations = true;
  bool mShowMainMacros = true;
  bool mPresetCrossfade = false;
  bool mLibraryDefaultSaveFailed = false;
  double mSampleRatePrepared = 48000.0;
  int mMaxBlockPrepared = 2048;
  std::array<int, FixedNAMGrid::kSlots> mRequestedLatencies{};
  GridDCBlocker mHighPass; // Final DC blocker
  NAMSender mInputSender, mOutputSender;
  iplug::ISender<1, 128, SlotMeterData> mSlotMeterSender;
  // One output window for the selected spectrum view only. Its packet size
  // differs from the meter packet, which is how slot controls tell them apart.
  iplug::ISender<1, 4, grid_eq_signal::Window> mSlotSignalSender;
  iplug::ISenderData<1, grid_eq_signal::Window> mSignalPacket{iplug::kNoTag, 1, 0}; // Audio thread only.
  SignalCapture mSignalCapture; // Audio thread only; attached to the selected slot's stage.
  ImpulseProbe mImpulseProbe;
  int mCaptureSlot = -2, mCaptureGrid = -1;
  static_assert(sizeof(iplug::ISenderData<1, grid_eq_signal::Window>) != sizeof(iplug::ISenderData<1, SlotMeterData>));
  std::atomic<int> mSpectrumSlot{0}; // Selected slot while visualizations are shown, else -1.
  std::atomic<bool> mEditorOpen{false};
  bool mMetering = true; // Audio-owned; first callback disables telemetry if the editor is closed.
  int mMeterFrames = 0;
};
