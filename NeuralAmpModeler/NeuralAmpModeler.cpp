#include "NoisePrintState.h"
#include "DefaultContent.h"
#include <fstream>
#include <algorithm> // std::clamp, std::min
#include <cmath> // pow
#include <cctype>
#include <filesystem>
#include <iostream>
#include <utility>

#include "../NeuralAmpModelerCore/NAM/activations.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
// clang-format off
// These includes need to happen in this order or else the latter won't know
// a bunch of stuff.
#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
// clang-format on
#include "architecture.hpp" // disable_denormals
#include "NAMGridControls.h"
#include "GridPerformanceControls.h"
#include "LibraryBrowserControls.h"
#include "RigPreset.h"
#include "RigPresetControls.h"
#include "UserPaths.h"

using namespace iplug;
using namespace igraphics;

const double kDCBlockerFrequency = 5.0;
// Project/rig state: one header plus a JSON object whose "schema" is kEffectStateSchema.
const char* const kStateHeader = "###nam-grid###";

EMsgBoxResult _ShowMessageBox(iplug::igraphics::IGraphics* pGraphics, const char* str, const char* caption,
                              EMsgBoxType type)
{
#ifdef OS_MAC
  // macOS is backwards?
  return pGraphics->ShowMessageBox(caption, str, type);
#else
  return pGraphics->ShowMessageBox(str, caption, type);
#endif
}

// Per-user settings.json in the platform's config folder; see UserPaths.h.
static std::filesystem::path CaptureSettingsFile() { return user_paths::SettingsFile(); }

NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  nam::activations::Activation::enable_fast_tanh();
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kParallelBlend)->InitDouble("A/B Blend", 50., 0., 100., .1, "%");
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
  {
    const auto name = GridSlotName(slot);
    GetParam(SlotInputParam(slot))->InitGain((name + " NAM Input").c_str(), 0., -40., 40., .1);
    GetParam(SlotTrimParam(slot))->InitGain((name + " NAM Output").c_str(), 0., -40., 40., .1);
    GetParam(SlotActiveParam(slot))->InitBool((name + " Enabled").c_str(), true);
    GetParam(SlotDynamicsParam(slot, 0))->InitDouble((name + " Gate Threshold").c_str(), -55., -90., -10., .1, "dB");
    GetParam(SlotDynamicsParam(slot, 1))->InitDouble((name + " Gate Release").c_str(), 120., 20., 500., 1., "ms");
    GetParam(SlotDynamicsParam(slot, 2))->InitDouble((name + " Gate Reduction").c_str(), 60., 0., 80., .1, "dB");
    GetParam(SlotDynamicsParam(slot, 3))->InitDouble((name + " Comp Threshold").c_str(), -18., -48., 0., .1, "dB");
    GetParam(SlotDynamicsParam(slot, 4))->InitDouble((name + " Comp Release").c_str(), 120., 40., 600., 1., "ms");
    GetParam(SlotDynamicsParam(slot, 5))->InitDouble((name + " Comp Makeup").c_str(), 0., 0., 18., .1, "dB");
    GetParam(SlotEQParam(slot, 0))->InitDouble((name + " EQ Bass").c_str(), 0., -12., 12., .1, "dB");
    GetParam(SlotEQParam(slot, 1))->InitDouble((name + " EQ Mid").c_str(), 0., -12., 12., .1, "dB");
    GetParam(SlotEQParam(slot, 2))->InitDouble((name + " EQ Treble").c_str(), 0., -12., 12., .1, "dB");
    GetParam(SlotEQParam(slot, 3))->InitDouble((name + " EQ Low Cut").c_str(), 0., 0., 300., 1., "Hz");
    GetParam(SlotEQParam(slot, 4))->InitDouble((name + " EQ High Cut").c_str(), 20000., 2000., 20000., 10., "Hz");
    GetParam(SlotEQParam(slot, 5))->InitDouble((name + " EQ Presence").c_str(), 0., -6., 6., .1, "dB");
    GetParam(SlotEQParam(slot, 6))->InitDouble((name + " IR Output").c_str(), -18., -36., 18., .1, "dB");
    for (int i = 0; i < kUtilityControls; ++i)
    {
      const auto& p = kEffectParameters[i];
      auto* param = GetParam(SlotUtilityParam(slot, i));
      const auto choices = EffectChoices(i);
      const int flags = IsAlgorithmMode(i) ? IParam::kFlagCannotAutomate : 0;
      if (!choices.empty())
      {
        param->InitEnum(
          (name + " " + p.name).c_str(), static_cast<int>(p.initial), static_cast<int>(choices.size()), "", flags);
        for (int choice = 0; choice < choices.size(); ++choice)
          param->SetDisplayText(choice, choices[choice]);
      }
      else if (p.minimum == 0. && p.maximum == 1. && p.step == 1.)
        param->InitBool((name + " " + p.name).c_str(), p.initial >= .5, "", flags);
      else if (p.power != 1.)
        param->InitDouble(
          (name + " " + p.name).c_str(), p.initial, p.minimum, p.maximum, p.step, p.unit, 0, "", IParam::ShapePowCurve(p.power));
      else if (p.exponential)
        param->InitDouble(
          (name + " " + p.name).c_str(), p.initial, p.minimum, p.maximum, p.step, p.unit, 0, "", IParam::ShapeExp());
      else
        param->InitDouble((name + " " + p.name).c_str(), p.initial, p.minimum, p.maximum, p.step, p.unit);
      if (i == uNoiseGateThreshold)
        param->SetDisplayText(p.minimum, "Off");
    }
    GetParam(SlotFullParam(slot))->InitBool((name + " Full Quality").c_str(), true);
  }
  GetParam(kTunerReference)->InitDouble("Tuner Reference", 440., 430., 450., .1, "Hz");
  GetParam(kTunerMute)->InitBool("Tuner Mute", true);
  GetParam(kTunerActive)->InitBool("Tuner", false);
  for (int i = 0; i < grid_performance::Count; ++i)
  {
    const auto& p = grid_performance::parameters[i]; auto* param = GetParam(PerformanceParam(i));
    if (i == grid_performance::LFODivision || i == grid_performance::StepDivision)
    {
      param->InitEnum(p.name, static_cast<int>(p.initial), static_cast<int>(grid_tempo::names.size()));
      for (int d = 0; d < grid_tempo::names.size(); ++d) param->SetDisplayText(d, grid_tempo::names[d]);
    }
    else if (i == grid_performance::LFOShape)
    {
      param->InitEnum(p.name, 0, static_cast<int>(grid_performance::lfoShapeNames.size()), "", IParam::kFlagCannotAutomate);
      for (int shape = 0; shape < grid_performance::lfoShapeNames.size(); ++shape)
        param->SetDisplayText(shape, grid_performance::lfoShapeNames[shape]);
    }
    else if (i == grid_performance::EnvInput)
    {
      param->InitEnum(p.name, 0, grid_performance::gridSlot + 1, "", IParam::kFlagCannotAutomate);
      param->SetDisplayText(0, "Grid input");
      for (int slot = 0; slot < grid_performance::gridSlot; ++slot)
        param->SetDisplayText(slot + 1, ("After " + GridSlotName(slot)).c_str());
    }
    else if (i == grid_performance::LFORetrigger || i == grid_performance::StepRetrigger)
    {
      param->InitEnum(p.name, 0, grid_performance::RetriggerModes, "", IParam::kFlagCannotAutomate);
      for (int mode = 0; mode < grid_performance::RetriggerModes; ++mode)
        param->SetDisplayText(mode, grid_performance::retriggerNames[mode]);
    }
    else if (grid_performance::StateOnly(i))
    {
      // Matrix routing is saved state, not automation (see grid_performance::StateOnly).
      const int field = (i - grid_performance::ModRow) % grid_performance::RowFields;
      if (field == grid_performance::RowSlot)
      {
        param->InitEnum(p.name, 0, grid_performance::gridSlot + 1, "", IParam::kFlagCannotAutomate);
        for (int slot = 0; slot < grid_performance::gridSlot; ++slot) param->SetDisplayText(slot, GridSlotName(slot).c_str());
        param->SetDisplayText(grid_performance::gridSlot, "Grid");
      }
      else if (field == grid_performance::RowDestination)
        param->InitInt(p.name, 0, static_cast<int>(p.minimum), static_cast<int>(p.maximum), "", IParam::kFlagCannotAutomate);
      else
      {
        param->InitEnum(p.name, 0, grid_performance::SourceCount, "", IParam::kFlagCannotAutomate);
        for (int source = 0; source < grid_performance::SourceCount; ++source)
          param->SetDisplayText(source, grid_performance::sourceNames[source]);
      }
    }
    else if (p.minimum == 0. && p.maximum == 1. && p.step == 1.) param->InitBool(p.name, p.initial >= .5);
    else if (p.exponential) param->InitDouble(p.name, p.initial, p.minimum, p.maximum, p.step, p.unit, 0, "", IParam::ShapeExp());
    else param->InitDouble(p.name, p.initial, p.minimum, p.maximum, p.step, p.unit);
    if (i == grid_performance::EnvThreshold) param->SetDisplayText(grid_performance::kThresholdOff, "Off");
  }
  {
    // Every slot shares its controls' curves; slot 0's parameters stand for all of them.
    std::array<const IParam*, parameter_layout::slotControls> slotCurves{};
    for (int control = 0; control < parameter_layout::slotControls; ++control)
      slotCurves[control] = GetParam(SlotParam(0, control));
    mModulationMatrix.Bind(slotCurves, {GetParam(kParallelBlend), GetParam(PerformanceParam(grid_performance::LaneAGain)),
                                        GetParam(PerformanceParam(grid_performance::LaneBGain)),
                                        GetParam(PerformanceParam(grid_performance::CrossoverFrequency))});
  }
  const auto prepared =
    nam_library::PrepareUserSettings(CaptureSettingsFile(), user_paths::LegacySettingsFile(), user_paths::ContentFolder());
  const auto& librarySettings = prepared.settings;
  mLibraryDefaultSaveFailed = !prepared.saved;
  mRescanOnStartup = librarySettings.rescanOnStartup;
  mShowVisualizations = librarySettings.showVisualizations;
  mPresetCrossfade = librarySettings.presetCrossfade;
  mIrFolders = librarySettings.irFolders;
  mPresetFolders = librarySettings.presetFolders;
  SetLibraryFolders(nam_library::FileKind::NAM, librarySettings.namFolders);
  mEmptyRig = _CurrentRig(); // A fresh instance is the empty grid New returns to.
  _InstallDefaultContent(prepared.newNamFolder, prepared.newPresetFolder);

  mMakeGraphicsFunc = [&]() {

#ifdef OS_IOS
    auto scaleFactor = GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT) * 0.85f;
#else
    auto scaleFactor = 1.0f;
#endif

    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, scaleFactor);
  };

  mLayoutFunc = [&](IGraphics* ui) {
    ui->AttachCornerResizer(EUIResizerMode::Scale, false);
    ui->AttachTextEntryControl();
    ui->EnableMouseOver(true);
    ui->EnableTooltips(true);
    ui->LoadFont("Roboto-Regular", ROBOTO_FN);
    const auto b = ui->GetBounds();
    const auto compact = grid_ui::Style();
    ui->AttachPanelBackground(COLOR_BLACK);
    ui->AttachControl(new ITextControl(IRECT(24, 8, 156, 48), PLUG_NAME, grid_ui::Text(28.f)));
    ui->AttachControl(new grid_ui::Button(IRECT(165, 14, 265, 42), [](IControl* caller) {
      caller->GetUI()->GetControlWithTag(kCtrlTagTuner)->Hide(true);
      caller->GetUI()->GetControlWithTag(kCtrlTagSettingsBox)->Hide(true);
      caller->GetUI()->GetControlWithTag(kCtrlTagPerformance)->Hide(false);
    }, "Modulation"))->SetTooltip("Modulation sources, macros and the modulation matrix.");
    for (int scene = 0; scene < 3; ++scene)
      ui->AttachControl(new grid_ui::SceneButton(IRECT(273.f + 32.f * scene, 14, 301.f + 32.f * scene, 42), scene));
    ui->AttachControl(new NAMRigPresetControl(IRECT(375, 14, 560, 42)), kCtrlTagPresetName);
    ui->AttachControl(new grid_ui::Button(IRECT(568, 14, 600, 42), [this](IControl*) { StepRigPreset(-1); }, "<"), kCtrlTagPresetPrevious);
    ui->AttachControl(new grid_ui::Button(IRECT(608, 14, 640, 42), [this](IControl*) { StepRigPreset(1); }, ">"), kCtrlTagPresetNext);
    ui->AttachControl(new grid_ui::Button(IRECT(648, 14, 702, 42), [this](IControl*) {
      if (!ConfirmNewRig()) return;
      const auto error = NewRig();
      if (!error.empty() && GetUI()) _ShowMessageBox(GetUI(), error.c_str(), "New rig", EMsgBoxType::kMB_OK);
    }, "New"), kCtrlTagPresetNew)->SetTooltip("Start a new empty rig. Library folders and tuner settings stay.");
    ui->AttachControl(new NAMRigSaveControl(IRECT(710, 14, 762, 42)), kCtrlTagPresetSave);
    ui->AttachControl(new grid_ui::Button(
      IRECT(862, 14, 976, 42),
      [](IControl* caller) {
        caller->GetUI()->GetControlWithTag(kCtrlTagTuner)->Hide(true);
        caller->GetUI()->GetControlWithTag(kCtrlTagSettingsBox)->Hide(false);
      },
      "Library & settings"));
    ui->AttachControl(new grid_ui::Toggle(IRECT(770, 14, 850, 42), kTunerActive, "Tuner", true), kCtrlTagTunerButton)
      ->SetTooltip("Show the tuner in place of the module editor; click again or select a slot to leave it. "
                   "Automatable; mutes the output while on unless Mute output is off.");
    ui->AttachControl(new grid_ui::Wiring(b));
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    {
      const float left = 88.f + 212.f * (slot % 4);
      const float top = grid_layout::cardTop + grid_layout::rowPitch * static_cast<float>(slot / 4);
      ui->AttachControl(new NAMGridSlotControl(IRECT(left, top, left + 188.f, top + grid_layout::cardHeight), slot),
                        kCtrlTagGridSlotBase + static_cast<int>(slot));
    }
    ui->AttachControl(new grid_ui::FlowKnob(IRECT(0, 67, 74, 135), PerformanceParam(grid_performance::InputGain), "IN"))
      ->SetTooltip("Input gain before every grid module. Double-click to reset to 0 dB.");
    ui->AttachControl(new grid_ui::FlowKnob(IRECT(928, 235, 1000, 303), kParallelBlend, "A / B"))
      ->SetTooltip("Lane balance: 0% is A, 50% is both lanes equally, 100% is B.");
    ui->AttachControl(new grid_ui::RoutingButton(IRECT(936, 316, 992, 340)), kCtrlTagRoutingButton);
    ui->AttachControl(new grid_ui::FlowKnob(IRECT(928, 379, 1000, 447), kOutputLevel, "OUT"))
      ->SetTooltip("Final output gain after the complete grid.");
    // Stereo L/R bars beside the first and last cards; the peak readout stays clear of the wiring.
    ui->AttachControl(new grid_ui::FlowMeter(IRECT(77, 68, 84, 140), IRECT(0, 140, 66, 158)), kCtrlTagInputMeter);
    ui->AttachControl(new grid_ui::FlowMeter(IRECT(921, 380, 928, 452), IRECT(932, 450, 1000, 468)), kCtrlTagOutputMeter);
    ui->AttachControl(new NAMGridSettingsControl(b), kCtrlTagSettingsBox)->Hide(true);
    ui->AttachControl(new NAMGridTunerControl(IRECT(0, grid_layout::editorTop, grid_layout::width, grid_layout::height)), kCtrlTagTuner)->Hide(true);
    ui->AttachControl(new NAMGridRoutingControl(IRECT(0, grid_layout::editorTop, grid_layout::width, grid_layout::height)), kCtrlTagRouting)->Hide(true);
    ui->AttachControl(new NAMGridPerformanceControl(b), kCtrlTagPerformance)->Hide(true);
    ui->AttachControl(new NAMLibraryBrowserControl(b), kCtrlTagLibraryBrowser)->Hide(true);
    ui->SetKeyHandlerFunc([this, ui](const IKeyPress& key, bool keyUp) {
      auto* browser = ui->GetControlWithTag(kCtrlTagLibraryBrowser);
      if (!browser->IsHidden())
        return keyUp || browser->As<NAMLibraryBrowserControl>()->HandleKey(key);
      auto* performance = ui->GetControlWithTag(kCtrlTagPerformance);
      if (!keyUp && key.VK == kVK_ESCAPE && !performance->IsHidden()) { performance->Hide(true); return true; }
      auto* tuner = ui->GetControlWithTag(kCtrlTagTuner);
      if (!keyUp && key.VK == kVK_ESCAPE && !tuner->IsHidden())
      {
        grid_ui::DisableTunerFromUI(ui);
        tuner->Hide(true);
        return true;
      }
      auto* settings = ui->GetControlWithTag(kCtrlTagSettingsBox);
      if (!keyUp && key.VK == kVK_ESCAPE && !settings->IsHidden())
      {
        settings->Hide(true);
        return true;
      }
      if (!keyUp && key.VK == kVK_ESCAPE && mRoutingOpen)
      {
        SetRoutingOpen(false);
        return true;
      }
      return false;
    });
    ui->ForAllControlsFunc([](IControl* control) { control->SetDirty(false); });
    _UpdatePresetControls();
  };
}

NeuralAmpModeler::~NeuralAmpModeler()
{ _DeallocateIOPointers(); }

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  // CLAP dispatches dynamic tail notifications on the audio thread.
  _UpdateTailSize();

  const size_t numChannelsExternalIn = (size_t)NInChansConnected();
  const size_t numChannelsExternalOut = (size_t)NOutChansConnected();
  const size_t numChannelsInternal = kNumChannelsInternal;
  const size_t numFrames = (size_t)nFrames;
  const double sampleRate = GetSampleRate();

  if (nFrames <= 0)
    return;
  if (numFrames > _GetBufferNumFrames() || mInputPointers == nullptr)
  {
    for (size_t c = 0; c < numChannelsExternalOut; ++c)
      std::fill_n(outputs[c], numFrames, 0.);
    return; // malformed host block; reactivation must prepare the new maximum
  }
  std::fenv_t fe_state;
  std::feholdexcept(&fe_state);
  disable_denormals();
  // A rig switch first holds the audible rig's controls while the next rig is prepared.
  const auto transition = mRigTransition.Begin();
  const bool holding = transition == RigTransition::kHolding;
  const bool switching = transition == RigTransition::kRunning;
  if (mRigTransition.Started())
  {
    // Freeze the outgoing rig exactly as it last sounded; the new rig reads fresh controls.
    mOutgoingControls = mLastModulated ? mAnimatedControls : mAudioControls.Values();
    mOutgoingRouting = grid_performance::Route(mHeldPerformance);
    mOutgoingBlend = mHeldBlend;
    mOutgoingGrid = mAudibleGrid;
    mAudibleGrid = mRigTransition.Target();
  }
  if (!holding)
  {
    mHeldInputGain = GetParam(PerformanceParam(grid_performance::InputGain))->DBToAmp();
    mHeldOutputGain = GetParam(kOutputLevel)->DBToAmp();
  }
  // Preserve host stereo until a mono NAM slot explicitly collapses it.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal, mHeldInputGain);
  mInputSender.ProcessBlock(mInputPointers, numFrames, kCtrlTagInputMeter, static_cast<int>(numChannelsInternal));
  mTuner.Enable(IsTunerActive());
  mTuner.Push(mInputPointers[0], mInputPointers[1], nFrames, sampleRate);
  for (size_t channel = 0; channel < numChannelsInternal; ++channel)
    std::copy_n(mInputPointers[channel], nFrames, mOutputPointers[channel]);
  const bool outgoing = switching && mRigTransition.OutgoingAudible();
  if (outgoing)
    for (size_t channel = 0; channel < mOutgoingBuffers.size(); ++channel)
      std::copy_n(mInputPointers[std::min(channel, numChannelsInternal - 1)], nFrames, mOutgoingBuffers[channel].data());
  auto& controls = mAudioControls.Values();
  const double tempo = GetTempo();
  // A held rig keeps the controls it last used; new values wait in the cache.
  for (size_t slot = 0; !holding && slot < controls.size(); ++slot)
  {
    mAudioControls.Refresh(slot, [&](auto& c) {
      grid_modulation::VisitSlotControls(c, [&](int control, auto& field) {
        field = grid_modulation::FieldValue<std::remove_reference_t<decltype(field)>>(GetParam(SlotParam(slot, control))->Value());
      });
    });
    // Host tempo can change without a parameter notification.
    controls[slot].utility.tempo = tempo;
  }
  if (!holding)
  {
    for (int i = 0; i < grid_performance::Count; ++i) mHeldPerformance[i] = GetParam(PerformanceParam(i))->Value();
    mHeldBlend = GetParam(kParallelBlend)->Value() * .01;
  }
  const auto& performance = mHeldPerformance;
  const auto routing = grid_performance::Route(performance);
  const double blend = mHeldBlend;
  auto& grid = mGrids[mAudibleGrid];
  // The one spectrum/scope capture follows the selected slot of the audible grid.
  if (const int slot = mSpectrumSlot.load(std::memory_order_relaxed); slot != mCaptureSlot || mAudibleGrid != mCaptureGrid)
  {
    for (auto& candidate : mGrids)
      for (auto& stage : candidate.stages) stage.AttachCapture(nullptr);
    if (slot >= 0 && slot < static_cast<int>(FixedNAMGrid::kSlots)) grid.stages[slot].AttachCapture(&mSignalCapture);
    mCaptureSlot = slot; mCaptureGrid = mAudibleGrid;
  }
  // A dip leaves the incoming rig silent at first; it starts once it becomes audible.
  const bool incoming = !switching || mRigTransition.IncomingAudible(nFrames);
  mLastModulated = incoming && mModulation.Active(performance);
  if (mLastModulated)
  {
    // A bounded control quantum avoids audio-rate model inference and keeps
    // modulation responsive without allocating or writing host parameters.
    // Copy every slot once per callback; quanta restore only the slots modulation edits.
    mAnimatedControls = controls;
    // The follower listens at the grid input or after one slot of this grid.
    const int tap = std::clamp(static_cast<int>(performance[grid_performance::EnvInput]), 0, 16) - 1;
    grid.SetTap(tap);
    // Every MIDI note-on of this block, in frame order.
    for (int i = 0; i < mNoteCount; ++i) mNoteFrames[i] = std::clamp(mNoteFrames[i], 0, nFrames - 1);
    std::sort(mNoteFrames.begin(), mNoteFrames.begin() + mNoteCount);
    int nextNote = 0;
    for (int offset = 0; offset < nFrames;)
    {
      const int count = std::min(mModulation.Quantum(), nFrames - offset);
      // Destinations are fixed for the block; only their amounts change per quantum.
      grid_modulation::Matrix::Targets targets;
      const int modulated = grid_modulation::Matrix::Collect(performance, mModulation, targets);
      grid_modulation::Matrix::Restore(mAnimatedControls, controls, targets, modulated);
      double animatedBlend = blend;
      auto animatedRouting = routing;
      mModulationMatrix.Apply(mAnimatedControls, animatedBlend, animatedRouting, targets, modulated);
      grid.Process(mOutputPointers[0] + offset, count, mAnimatedControls, animatedBlend, mOutputPointers[1] + offset,
                   animatedRouting);
      // This quantum's notes, as frames within it.
      std::array<int, kMaxNotesPerBlock> notes;
      int noteCount = 0;
      for (; nextNote < mNoteCount && mNoteFrames[nextNote] < offset + count; ++nextNote)
        notes[noteCount++] = mNoteFrames[nextNote] - offset;
      if (tap < 0)
        mModulation.Advance(mInputPointers[0] + offset, mInputPointers[1] + offset, count, performance, tempo, notes.data(), noteCount);
      else
        mModulation.Advance(grid.TapLeft(), grid.TapRight(), count, performance, tempo, notes.data(), noteCount);
      offset += count;
    }
  }
  else
  {
    mModulation.Idle();
    grid.SetTap(-1);
    if (incoming)
      grid.Process(mOutputPointers[0], nFrames, controls, blend, mOutputPointers[1], routing);
  }
  if (switching)
  {
    auto* left = mOutgoingBuffers[0].data();
    auto* right = mOutgoingBuffers[1].data();
    if (outgoing)
      mGrids[mOutgoingGrid].Process(left, nFrames, mOutgoingControls, mOutgoingBlend, right, mOutgoingRouting);
    for (int i = 0; i < nFrames; ++i)
    {
      const double in = mRigTransition.IncomingGain(), out = outgoing ? mRigTransition.OutgoingGain() : 0.;
      mOutputPointers[0][i] = in * mOutputPointers[0][i] + (outgoing ? out * left[i] : 0.);
      mOutputPointers[1][i] = in * mOutputPointers[1][i] + (outgoing ? out * right[i] : 0.);
      mRigTransition.Advance();
    }
    if (mRigTransition.Done())
      mRigTransition.Finish(); // The outgoing grid is not touched again.
  }
  mNoteCount = 0; // Notes act within their own block only.
  mMeterFrames += nFrames;
  const int meterInterval = std::max(1, static_cast<int>(sampleRate / 30.));
  if (mMeterFrames >= meterInterval)
  {
    mMeterFrames %= meterInterval;
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
      mSlotMeterSender.PushData({kCtrlTagGridSlotBase + static_cast<int>(slot), {grid.stages[slot].TakeMeter()}});
    if (const int slot = mSpectrumSlot.load(std::memory_order_relaxed); slot >= 0 && slot < static_cast<int>(FixedNAMGrid::kSlots))
    {
      // The packet is a member: a 20 KB window does not belong on a host's audio-thread stack.
      if (grid.stages[slot].TakeSignalWindow(mSignalPacket.vals[0]))
      {
        mSignalPacket.ctrlTag = kCtrlTagGridSlotBase + slot;
        mSlotSignalSender.PushData(mSignalPacket);
      }
    }
  }
  // Final DC blocker (upstream NAM issue 271).
  mHighPass.SetParams(recursive_linear_filter::HighPassParams(sampleRate, kDCBlockerFrequency));
  sample** hpfPointers = mHighPass.Process(mOutputPointers, numChannelsInternal, numFrames);
  std::feupdateenv(&fe_state);
  // Mono host outputs downmix; stereo host outputs retain independent channels.
  _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut, mHeldOutputGain);
  mOutputSender.ProcessBlock(
    outputs, nFrames, kCtrlTagOutputMeter, static_cast<int>(std::min(size_t{2}, numChannelsExternalOut)));
}

void NeuralAmpModeler::OnReset()
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (mPreviewOriginal)
    if (auto* ui = GetUI())
      ui->GetControlWithTag(kCtrlTagLibraryBrowser)->Hide(true);
  EndSlotPreview(false);
  const auto sampleRate = GetSampleRate();
  const int maxBlockSize = std::max(1, GetBlockSize());
  mSampleRatePrepared = sampleRate;
  mMaxBlockPrepared = maxBlockSize;
  mMeterFrames = 0;
  mAudioControls.InvalidateAll();
  mModulation.Reset(sampleRate);
  mNoteCount = 0;
  mMasterGain = GetParam(kOutputLevel)->DBToAmp();
  mGlobalInputGain = GetParam(PerformanceParam(grid_performance::InputGain))->DBToAmp();
  mMasterCoefficient = std::exp(-1. / (.005 * sampleRate));
  _PrepareBuffers(kNumChannelsInternal, maxBlockSize);
  for (auto& channel : mInputArray)
    std::fill(channel.begin(), channel.end(), 0.);
  for (auto& channel : mOutputArray)
    std::fill(channel.begin(), channel.end(), 0.);

  _UpdateTailSize();
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  // Adopt pending slots and prepare them for the new rate and maximum block. Any rig
  // switch is complete: the edited grid is audible and the other one is released.
  mRigTransition.FinishStopped();
  mGrids[1 - mEditGrid].ClearStopped();
  mInactiveGridUsed = false;
  mGrids[mEditGrid].ResetStopped(sampleRate, maxBlockSize);
  mAudibleGrid = mOutgoingGrid = mEditGrid;
  for (auto& buffer : mOutgoingBuffers)
    buffer.assign(maxBlockSize, 0.);
  for (size_t i = 0; i < FixedNAMGrid::kSlots; ++i)
    mRequestedLatencies[i] = mGrids[mEditGrid].stages[i].Latency();
  _UpdateLatency();
  // Prime the reused output filter outside ProcessBlock.
  mHighPass.ClearHistory();
  mHighPass.SetParams(recursive_linear_filter::HighPassParams(sampleRate, kDCBlockerFrequency));
  mHighPass.Process(mInputPointers, kNumChannelsInternal, maxBlockSize);
}

std::vector<std::string>* NeuralAmpModeler::_LibraryFolders(nam_library::FileKind kind)
{
  switch (kind)
  {
    case nam_library::FileKind::NAM: return &mNamFolders;
    case nam_library::FileKind::IR: return &mIrFolders;
    case nam_library::FileKind::RigPreset: return &mPresetFolders;
    default: return nullptr;
  }
}

void NeuralAmpModeler::SetLibraryFolders(nam_library::FileKind kind, std::vector<std::string> folders, bool fromUI)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  auto* current = _LibraryFolders(kind);
  if (!current)
    return;
  auto normalized = nam_library::NormalizeFolders(folders);
  const bool needsIndex = normalized != *current || (!mLibraryIndexed && !mLibraryScanning);
  *current = std::move(normalized);
  if (needsIndex && (fromUI || mRescanOnStartup))
    RescanCaptureFolder();
  else if (needsIndex)
  {
    mLibraryWorker.Request(std::vector<std::string>{}); // Cancel an older request without indexing the roots.
    mLibrary = std::make_shared<nam_library::Snapshot>();
    mLibraryScanning = mLibraryIndexed = false;
  }
  if (fromUI)
  {
    mLibraryDefaultSaveFailed = !nam_library::WriteSettings(CaptureSettingsFile(),
      {mNamFolders, mRescanOnStartup, mShowVisualizations, mPresetFolders, mIrFolders});
    _NotifyHostStateChanged();
  }
  _UpdateLibraryControls();
}

// A fresh install's new default folders get a clean capture, the Empty grid and the Clean
// Default rig that uses that capture, so a first start is not blank. Existing files and
// folders the user chose are never touched. Optional: the plugin works without any of it.
void NeuralAmpModeler::_InstallDefaultContent(const std::filesystem::path& namFolder,
                                              const std::filesystem::path& presetFolder)
{
  std::error_code error;
  const auto capture = namFolder.empty() ? namFolder : namFolder / default_content::kCleanCaptureName;
  try
  {
    if (!capture.empty() && !std::filesystem::exists(capture, error))
    {
      const auto bytes = default_content::CleanCapture();
      std::ofstream(capture, std::ios::binary).write(reinterpret_cast<const char*>(bytes.data()), bytes.size());
    }
  }
  catch (const std::exception&) {}
  if (presetFolder.empty()) return;
  try
  {
    const auto empty = presetFolder / "Empty.ngrid";
    if (!std::filesystem::exists(empty, error))
    {
      auto rig = mEmptyRig;
      rig["PresetName"] = "Empty";
      rig_preset::Write(empty, rig);
    }
  }
  catch (const std::exception&) {}
  try
  {
    const auto preset = presetFolder / default_content::kCleanPresetName;
    if (capture.empty() || !std::filesystem::is_regular_file(capture, error) || std::filesystem::exists(preset, error)) return;
    const auto bytes = default_content::CleanPreset();
    auto document = nlohmann::json::parse(bytes.begin(), bytes.end());
    auto& rig = document.at("rig");
    // Bundled captures are named by filename; point them at the installed copies.
    for (auto& path : rig.at("SlotPaths"))
    {
      const auto name = path.get<std::string>();
      if (!name.empty() && !std::filesystem::u8path(name).has_parent_path())
        path = nam_library::PathText(namFolder / std::filesystem::u8path(name));
    }
    rig_preset::Write(preset, rig);
  }
  catch (const std::exception&) {}
}

void NeuralAmpModeler::_ReloadLibraryFolders()
{
  const auto file = CaptureSettingsFile();
  std::error_code error;
  if (file.empty() || !std::filesystem::exists(file, error)) return; // Keep the folders prepared at startup.
  // Only lists the file holds: an unreadable file never empties the library.
  const auto settings = nam_library::ReadSettings(file);
  if (settings.namChosen) SetLibraryFolders(nam_library::FileKind::NAM, settings.namFolders);
  if (settings.irChosen) SetLibraryFolders(nam_library::FileKind::IR, settings.irFolders);
  if (settings.presetChosen) SetLibraryFolders(nam_library::FileKind::RigPreset, settings.presetFolders);
}

std::vector<std::string> NeuralAmpModeler::GetLibraryFolders(nam_library::FileKind kind) const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return kind == nam_library::FileKind::NAM ? mNamFolders
         : kind == nam_library::FileKind::IR ? mIrFolders
         : kind == nam_library::FileKind::RigPreset ? mPresetFolders : std::vector<std::string>{};
}
void NeuralAmpModeler::AddLibraryFolder(nam_library::FileKind kind, const char* folder)
{
  auto folders = GetLibraryFolders(kind);
  if (folders.size() < 32)
    folders.emplace_back(folder);
  SetLibraryFolders(kind, std::move(folders), true);
}
void NeuralAmpModeler::RemoveLibraryFolder(nam_library::FileKind kind, size_t index)
{
  auto folders = GetLibraryFolders(kind);
  if (index >= folders.size())
    return;
  folders.erase(folders.begin() + index);
  SetLibraryFolders(kind, std::move(folders), true);
}
void NeuralAmpModeler::SelectSlot(size_t slot)
{
  if (slot >= FixedNAMGrid::kSlots)
    return;
  mSelectedSlot = slot;
  if (mRoutingOpen) SetRoutingOpen(false); // Choosing a slot shows its editor again.
  // The Tuner button works like selecting a module: choosing a slot leaves the tuner.
  if (IsTunerActive() && GetUI()) grid_ui::DisableTunerFromUI(GetUI());
  mSpectrumSlot.store(mShowVisualizations ? static_cast<int>(slot) : -1, std::memory_order_relaxed);
  if (auto* ui = GetUI())
    for (size_t i = 0; i < FixedNAMGrid::kSlots; ++i)
      if (auto* control = ui->GetControlWithTag(kCtrlTagGridSlotBase + static_cast<int>(i)))
        control->OnMsgFromDelegate(kMsgTagLoadedModel, 0, nullptr);
}

void NeuralAmpModeler::OpenLibraryBrowser(nam_library::BrowserContext context, nam_library::FileKind kind, size_t slot)
{
  if (!nam_library::Allows(context, kind) || (context == nam_library::BrowserContext::Cell && slot >= FixedNAMGrid::kSlots)) return;
  if (auto* ui = GetUI())
  {
    ui->GetControlWithTag(kCtrlTagSettingsBox)->Hide(true);
    ui->GetControlWithTag(kCtrlTagPerformance)->Hide(true);
    ui->GetControlWithTag(kCtrlTagTuner)->Hide(true);
    ui->GetControlWithTag(kCtrlTagLibraryBrowser)->As<NAMLibraryBrowserControl>()->Open(context, kind, slot);
  }
}

void NeuralAmpModeler::_NotifyHostStateChanged()
{
#ifdef CLAP_API
  const auto* host = _host.host();
  if (auto* state = static_cast<const clap_host_state*>(host->get_extension(host, CLAP_EXT_STATE)))
    state->mark_dirty(host);
#elif defined(VST3_API)
  Steinberg::FUnknownPtr<Steinberg::Vst::IComponentHandler2> handler(GetComponentHandler());
  if (handler)
    handler->setDirty(true);
#else
  DirtyParametersFromUI();
#endif
}

std::string NeuralAmpModeler::GetSlotPath(size_t slot) const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return slot < mSlotPaths.size() ? mSlotPaths[slot].Get() : "";
}

bool NeuralAmpModeler::IsSlotLoaded(size_t slot) const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return slot < mSlotInfo.size() && mSlotInfo[slot].loaded;
}

SlotEffect NeuralAmpModeler::GetSlotEffect(size_t slot) const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return slot < mSlotEffects.size() ? mSlotEffects[slot] : SlotEffect::NAM;
}

nam_library::Gear NeuralAmpModeler::GetSlotGear(size_t slot) const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return slot < mSlotGear.size() ? mSlotGear[slot] : nam_library::Gear::Unknown;
}

bool NeuralAmpModeler::CanToggleSlotQuality(size_t slot) const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return slot < FixedNAMGrid::kSlots && mSlotEffects[slot] == SlotEffect::NAM && mSlotInfo[slot].slimmable;
}

void NeuralAmpModeler::ToggleSlotQuality(size_t slot)
{
  if (!CanToggleSlotQuality(slot))
    return;
  const int index = SlotFullParam(slot);
  const double value = GetParam(index)->Value() >= .5 ? 0. : 1.;
  BeginInformHostOfParamChangeFromUI(index);
  SendParameterValueFromUI(index, value);
  SendParameterValueFromDelegate(index, value, true);
  EndInformHostOfParamChangeFromUI(index);
  if (!mPreviewOriginal) _NotifyHostStateChanged();
}

void NeuralAmpModeler::SetSlotEffect(size_t slot, SlotEffect effect, bool fromUI)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (slot >= FixedNAMGrid::kSlots || !IsValidSlotEffect(effect))
    return;
  try
  {
    SlotConfig config{effect};
    if (effect == SlotEffect::NoisePrint) config.noiseProfile = GetNoiseProfile(slot);
    _CommitSlot(slot, PrepareSlot(std::move(config), mSampleRatePrepared, mMaxBlockPrepared), true);
  }
  catch (const std::exception&)
  {
    SendControlMsgFromDelegate(kCtrlTagGridSlotBase + static_cast<int>(slot), kMsgTagLoadFailed);
    return;
  }
  if (fromUI)
    _NotifyHostStateChanged();
}

std::string NeuralAmpModeler::LoadSlot(size_t slot, const char* path)
{
  const auto error = _StageSlot(slot, SlotEffect::NAM, path);
  if (error.empty())
    _NotifyHostStateChanged();
  return error;
}

std::string NeuralAmpModeler::LoadSlotIR(size_t slot, const char* path)
{
  const auto error = _StageSlot(slot, SlotEffect::CabinetIR, path);
  if (error.empty())
    _NotifyHostStateChanged();
  return error;
}

std::string NeuralAmpModeler::BeginSlotPreview(size_t slot)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  EndSlotPreview(false);
  if (slot >= FixedNAMGrid::kSlots) return "Invalid grid slot";
  try
  {
    // Prepare a rollback before auditioning: cancellation never needs to reopen a file.
    const auto original = _GetSlotConfig(slot);
    auto rollback = original;
    // Unresolved files already have an empty DSP; retain their path for cancellation.
    if (!IsSlotLoaded(slot)) rollback.path.clear();
    mPreviewOriginal = std::make_unique<PreparedSlot>(PrepareSlot(std::move(rollback), mSampleRatePrepared, mMaxBlockPrepared));
    mPreviewOriginal->config = original;
    mPreviewSlot = slot;
    mPreviewChoices.clear();
    SelectSlot(slot);
    return {};
  }
  catch (const std::exception& error) { return error.what(); }
}

std::string NeuralAmpModeler::PreviewSlot(SlotEffect effect, const std::string& path)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (!mPreviewOriginal) return "Preview is unavailable";
  auto current = _GetSlotConfig(mPreviewSlot);
  const auto currentChoice = current.effect;
  if (effect == currentChoice && path == current.path) return {};
  mPreviewChoices[{currentChoice, current.path}] = current;
  try
  {
    SlotConfig config{effect};
    const auto found = mPreviewChoices.find({effect, path});
    if (found != mPreviewChoices.end()) config = found->second;
    else
    {
      // Keep NAM/IR gain edits while comparing files of the same kind.
      if (current.effect == effect) config.values = current.values;
      config.path = path;
      if (!path.empty()) config.hash = nam_library::ContentHash(std::filesystem::u8path(path));
    }
    _CommitSlot(mPreviewSlot, PrepareSlot(std::move(config), mSampleRatePrepared, mMaxBlockPrepared), true, false);
    return {};
  }
  catch (const std::exception& error) { return error.what(); }
}

void NeuralAmpModeler::EndSlotPreview(bool accept)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (!mPreviewOriginal) return;
  auto original = std::move(mPreviewOriginal);
  mPreviewChoices.clear();
  if (accept)
  {
    const auto indices = SlotParamIndices(mPreviewSlot);
    for (size_t i = 0; i < indices.size(); ++i)
      if (GetParam(indices[i])->Value() != original->config.values[i])
      {
        BeginInformHostOfParamChange(indices[i]);
        SetParameterValue(indices[i], GetParam(indices[i])->GetNormalized());
        EndInformHostOfParamChange(indices[i]);
      }
    _NotifyHostStateChanged();
  }
  else _CommitSlot(mPreviewSlot, std::move(*original), true, false);
}

noise_print::Profile NeuralAmpModeler::GetNoiseProfile(size_t slot) const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (slot >= FixedNAMGrid::kSlots || mSlotEffects[slot] != SlotEffect::NoisePrint)
    return {};
  return mGrids[mEditGrid].stages[slot].GetNoiseProfile(mNoiseEpochs[slot], mNoiseProfiles[slot]);
}

SlotConfig NeuralAmpModeler::_GetSlotConfig(size_t slot) const
{
  SlotConfig result;
  result.effect = mSlotEffects[slot];
  result.noiseProfile = GetNoiseProfile(slot);
  result.path = mSlotPaths[slot].Get();
  result.hash = mSlotHashes[slot];
  const auto indices = SlotParamIndices(slot);
  for (size_t i = 0; i < indices.size(); ++i)
    result.values[i] = GetParam(indices[i])->Value();
  result.values[17 + uNoiseLearn] = 0.; // Never copy an in-progress capture.
  return result;
}

std::string NeuralAmpModeler::EditSlot(size_t source, size_t destination, SlotEditMode mode)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (source >= FixedNAMGrid::kSlots || destination >= FixedNAMGrid::kSlots)
    return "Invalid grid slot";
  if (source == destination)
    return "";
  try
  {
    const auto verify = [&](size_t slot) {
      if (!mSlotHashes[slot].empty()
          && nam_library::ContentHash(std::filesystem::u8path(mSlotPaths[slot].Get())) != mSlotHashes[slot])
        throw std::runtime_error("Capture/IR content changed; load the file again before copying or moving it");
    };
    verify(source);
    if (mode == SlotEditMode::Swap) verify(destination);
    auto edit = PrepareSlotEdit(
      _GetSlotConfig(source), _GetSlotConfig(destination), mode, mSampleRatePrepared, mMaxBlockPrepared);
    _CommitSlot(destination, std::move(edit.destination), true);
    if (edit.source)
      _CommitSlot(source, std::move(*edit.source), true);
    _NotifyHostStateChanged();
    return "";
  }
  catch (const std::exception& error)
  {
    return error.what();
  }
}

void NeuralAmpModeler::ClearSlot(size_t slot)
{
  if (slot < FixedNAMGrid::kSlots)
  {
    _ClearModel(slot);
    _NotifyHostStateChanged();
  }
}

void NeuralAmpModeler::LoadSlapbackPreset(size_t slot)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (slot >= FixedNAMGrid::kSlots) return;
  SlotConfig config{SlotEffect::DigitalDelay};
  config.values[17 + uDelayTime] = 90.; config.values[17 + uDelayFeedback] = 0.;
  config.values[17 + uDelayTone] = 8000.; config.values[17 + uDelayCharacter] = 0.;
  config.values[17 + uDelayMix] = 25.;
  _CommitSlot(slot, PrepareSlot(std::move(config), mSampleRatePrepared, mMaxBlockPrepared), true);
  _NotifyHostStateChanged();
}

bool NeuralAmpModeler::IsPerformanceSceneParam(int index) const
{
  if (index == kTunerReference || index == kTunerMute || index == kTunerActive) return false;
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot) if (index == SlotFullParam(slot)) return false;
  // Hold captures are live audio; a scene must not silently latch an empty buffer.
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot) if (index == SlotUtilityParam(slot, uFreezeHold)) return false;
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    if (index == SlotUtilityParam(slot, uShimmerFreeze) || index == SlotUtilityParam(slot, uNoiseLearn))
      return false;
  return index >= 0 && index < kNumParams;
}
void NeuralAmpModeler::StorePerformanceScene(size_t scene)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (scene >= mPerformanceScenes.size()) return;
  auto values = nlohmann::json::object();
  for (int i = 0; i < kNumParams; ++i) if (IsPerformanceSceneParam(i)) values[GetParam(i)->GetName()] = GetParam(i)->Value();
  mPerformanceScenes[scene] = std::move(values); mActiveScene = static_cast<int>(scene); _NotifyHostStateChanged();
}
void NeuralAmpModeler::ClearPerformanceScene(size_t scene)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (scene >= mPerformanceScenes.size() || mPerformanceScenes[scene].empty()) return;
  mPerformanceScenes[scene] = nlohmann::json::object();
  if (mActiveScene == static_cast<int>(scene)) mActiveScene = -1;
  _NotifyHostStateChanged();
}
void NeuralAmpModeler::RecallPerformanceScene(size_t scene)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (scene >= mPerformanceScenes.size()) return;
  const auto& values = mPerformanceScenes[scene];
  if (values.empty()) return;
  mActiveScene = static_cast<int>(scene);
  for (int i = 0; i < kNumParams; ++i)
  {
    const auto it = values.find(GetParam(i)->GetName());
    if (!IsPerformanceSceneParam(i) || it == values.end() || GetParam(i)->Value() == it->get<double>()) continue;
    const double value = GetParam(i)->ToNormalized(it->get<double>());
    BeginInformHostOfParamChange(i); SetParameterValue(i, value);
    SendParameterValueFromDelegate(i, value, true); EndInformHostOfParamChange(i);
  }
  _UpdateControlsFromModel(); _NotifyHostStateChanged();
}
bool NeuralAmpModeler::HasPerformanceScene(size_t scene) const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return scene < mPerformanceScenes.size() && !mPerformanceScenes[scene].empty();
}

void NeuralAmpModeler::SetParameterFromUI(int index, double value)
{
  if (index < 0 || index >= kNumParams) return;
  const double normalized = GetParam(index)->ToNormalized(value);
  BeginInformHostOfParamChangeFromUI(index);
  SendParameterValueFromUI(index, normalized);
  SendParameterValueFromDelegate(index, normalized, true);
  EndInformHostOfParamChangeFromUI(index);
}

grid_performance::Values NeuralAmpModeler::PerformanceValues() const
{
  grid_performance::Values values{};
  for (int i = 0; i < grid_performance::Count; ++i) values[i] = GetParam(PerformanceParam(i))->Value();
  return values;
}

int NeuralAmpModeler::AddModulation(int source, int slot, int control, double depth)
{
  using namespace grid_performance;
  if (source <= SourceNone || source >= SourceCount || !grid_modulation::ValidDestination(slot, control)) return -1;
  const auto row = [](int index, int field) { return PerformanceParam(RowParam(index, field)); };
  int free = -1;
  for (int index = 0; index < rows; ++index)
  {
    const auto value = [&](int field) { return static_cast<int>(GetParam(row(index, field))->Value()); };
    if (value(RowSource) == source && value(RowSlot) == slot && value(RowDestination) == control)
    {
      if (GetParam(row(index, RowDepth))->Value() == 0.) SetParameterFromUI(row(index, RowDepth), depth);
      return index;
    }
    if (free < 0 && value(RowSource) == SourceNone) free = index;
  }
  if (free < 0) return -1;
  SetParameterFromUI(row(free, RowSlot), slot);
  SetParameterFromUI(row(free, RowDestination), control);
  SetParameterFromUI(row(free, RowVia), SourceNone);
  SetParameterFromUI(row(free, RowDepth), depth);
  SetParameterFromUI(row(free, RowSource), source); // Last: the row becomes active complete.
  return free;
}

void NeuralAmpModeler::ClearModulationRow(int row)
{
  using namespace grid_performance;
  if (row < 0 || row >= rows) return;
  for (const int field : {RowSource, RowDepth, RowVia, RowSlot, RowDestination})
    SetParameterFromUI(PerformanceParam(RowParam(row, field)), grid_performance::parameters[RowParam(row, field)].initial);
}

void NeuralAmpModeler::SetPresetCrossfade(bool enabled)
{
  mPresetCrossfade = enabled;
  auto settings = nam_library::ReadSettings(CaptureSettingsFile());
  settings.presetCrossfade = enabled;
  mLibraryDefaultSaveFailed = !nam_library::WriteSettings(CaptureSettingsFile(), settings);
  _UpdateLibraryControls();
}

void NeuralAmpModeler::SetShowVisualizations(bool enabled)
{
  mShowVisualizations = enabled;
  mSpectrumSlot.store(enabled ? static_cast<int>(mSelectedSlot) : -1, std::memory_order_relaxed);
  auto settings = nam_library::ReadSettings(CaptureSettingsFile());
  settings.showVisualizations = enabled;
  mLibraryDefaultSaveFailed = !nam_library::WriteSettings(CaptureSettingsFile(), settings);
  if (auto* ui = GetUI()) ui->SetAllControlsDirty();
}
void NeuralAmpModeler::SetRescanOnStartup(bool enabled)
{
  mRescanOnStartup = enabled;
  auto settings = nam_library::ReadSettings(CaptureSettingsFile());
  settings.rescanOnStartup = enabled;
  mLibraryDefaultSaveFailed = !nam_library::WriteSettings(CaptureSettingsFile(), settings);
  if (enabled && !mLibraryIndexed && !mLibraryScanning)
    RescanCaptureFolder();
  _UpdateLibraryControls();
}

void NeuralAmpModeler::RescanCaptureFolder()
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  mLibrary = std::make_shared<nam_library::Snapshot>();
  mLibraryIndexed = false;
  mLibraryScanning = !mNamFolders.empty() || !mIrFolders.empty() || !mPresetFolders.empty();
  mLibraryWorker.Request(mNamFolders, mIrFolders, mPresetFolders);
  _UpdateLibraryControls();
}

std::shared_ptr<const nam_library::Snapshot> NeuralAmpModeler::GetCaptureLibrary() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return mLibrary;
}

nam_library::Filter NeuralAmpModeler::GetCaptureFilter() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return mCaptureFilter;
}

void NeuralAmpModeler::SetCaptureFilter(nam_library::Filter filter)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  mCaptureFilter = std::move(filter);
  _UpdateLibraryControls();
}

std::string NeuralAmpModeler::GetCaptureStatus() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (mNamFolders.empty() && mIrFolders.empty() && mPresetFolders.empty())
    return "No library folders";
  if (mLibraryScanning)
    return "Scanning...";
  if (!mLibraryIndexed)
    return "Library not indexed";
  if (mLibraryDefaultSaveFailed)
    return "Folder saved in project; unable to save the default for new instances.";
  if (!mLibrary->error.empty())
    return mLibrary->error;
  size_t nams = 0, irs = 0, presets = 0;
  for (const auto& entry : mLibrary->captures)
  {
    nams += entry.kind == nam_library::FileKind::NAM;
    irs += entry.kind == nam_library::FileKind::IR;
    presets += entry.kind == nam_library::FileKind::RigPreset;
  }
  const auto folders = [](size_t count) { return std::to_string(count) + (count == 1 ? " folder" : " folders"); };
  return std::to_string(nams) + " NAM in " + folders(mNamFolders.size()) + " / " + std::to_string(irs) + " IR in "
         + folders(mIrFolders.size()) + " / " + std::to_string(presets) + " presets in " + folders(mPresetFolders.size()) + " / "
         + std::to_string(mLibrary->unreadable) + " unreadable";
}

std::string NeuralAmpModeler::GetLibraryMatchStatus() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (mLibraryScanning)
    return "Scanning library...";
  if (mNamFolders.empty() && mIrFolders.empty())
    return "Add library folders";
  if (!mLibraryIndexed)
    return "Rescan library";
  const auto matches = std::count_if(mLibrary->captures.begin(), mLibrary->captures.end(),
                                     [&](const auto& capture) { return mCaptureFilter.Matches(capture); });
  return std::to_string(matches) + " / " + std::to_string(mLibrary->captures.size()) + " files";
}

void NeuralAmpModeler::_UpdateLibraryControls()
{
  if (auto* ui = GetUI())
    if (auto* control = ui->GetControlWithTag(kCtrlTagSettingsBox))
      control->SetDirty(false);
}

void NeuralAmpModeler::OnIdle()
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  _UpdateTunerControls();
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    if (CanToggleSlotQuality(slot) && GetParam(SlotFullParam(slot))->Value() != mPreparedQuality[slot])
    {
      if (!_StageSlot(slot, SlotEffect::NAM, mSlotPaths[slot].Get(), mSlotHashes[slot]).empty())
      {
        // Keep the old processor if a selected file has disappeared since loading.
        const int index = SlotFullParam(slot);
        SetParameterValue(index, mPreparedQuality[slot]);
        SendParameterValueFromDelegate(index, mPreparedQuality[slot], true);
        SendControlMsgFromDelegate(kCtrlTagGridSlotBase + static_cast<int>(slot), kMsgTagLoadedModel);
      }
    }
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
  {
    const auto sequence = mGrids[mEditGrid].stages[slot].NoiseSequence();
    if (sequence != mNoiseSeen[slot])
    {
      const auto profile = GetNoiseProfile(slot);
      mNoiseSeen[slot] = sequence;
      if (!(profile == mNoiseProfiles[slot]))
      {
        mNoiseProfiles[slot] = profile;
        _NotifyHostStateChanged();
        SendControlMsgFromDelegate(kCtrlTagGridSlotBase + static_cast<int>(slot), kMsgTagLoadedModel);
      }
    }
  }
  if (auto result = mLibraryWorker.TakeResult())
  {
    if (mLibraryScanning)
    {
      mLibrary = std::move(result);
      mLibraryScanning = false;
      mLibraryIndexed = true;
      _ResolveLibraryFiles();
      _UpdateLibraryControls();
    }
  }
  for (auto& grid : mGrids)
    for (auto& stage : grid.stages)
      stage.Collect();
  // After a rig switch, free the previous rig once audio has stopped using its grid.
  if (mInactiveGridUsed && mRigTransition.Idle())
  {
    mGrids[1 - mEditGrid].ClearStopped();
    mInactiveGridUsed = false;
  }
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);
  mSlotMeterSender.TransmitData(*this);
  mSlotSignalSender.TransmitData(*this);
}

// First state schema that saved a host parameter. Older states default it; a
// missing parameter that the saved schema already had makes the state invalid.
static int IntroducedIn(int index)
{
  if (const int utility = parameter_layout::UtilityControl(index); utility >= 0)
    return kEffectParameters[utility].introducedIn;
  if (const int performance = PerformanceIndex(index); performance >= 0)
    return grid_performance::parameters[performance].introducedIn;
  return kInitialEffectStateSchema;
}

static int ReadStateString(const IByteChunk& chunk, WDL_String& value, int pos)
{
  if (pos < 0 || pos > chunk.Size() - static_cast<int>(sizeof(int)))
    return -1;
  int length = 0;
  const int dataPos = chunk.Get(&length, pos);
  if (dataPos < 0 || length < 0 || length > chunk.Size() - dataPos
      || std::memchr(chunk.GetData() + dataPos, '\0', static_cast<size_t>(length)))
    return -1;
  return chunk.GetStr(value, pos);
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  nlohmann::json config;
  config["schema"] = kEffectStateSchema;
  config["PresetName"] = mPresetName;
  config["PresetDirectory"] = mPresetDirectory;
  // Library folders are per-user settings (settings.json), never part of a project or rig.
  config["PerformanceScenes"] = mPerformanceScenes;
  config["SlotPaths"] = nlohmann::json::array();
  for (const auto& path : mSlotPaths)
    config["SlotPaths"].push_back(path.Get());
  config["SlotHashes"] = mSlotHashes;
  config["NoisePrintProfiles"] = nlohmann::json::array();
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    config["NoisePrintProfiles"].push_back(noise_print::Save(GetNoiseProfile(slot)));
  config["SlotEffects"] = nlohmann::json::array();
  for (const auto effect : mSlotEffects)
    config["SlotEffects"].push_back(static_cast<int>(effect));
  for (int i = 0; i < kNumParams; ++i)
    config[GetParam(i)->GetName()] = GetParam(i)->Value();
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    config[GetParam(SlotUtilityParam(slot, uNoiseLearn))->GetName()] = 0.; // Learning is transient.
  if (mPreviewOriginal)
  {
    // Host autosaves and rig exports retain the committed slot until acceptance.
    const auto& original = mPreviewOriginal->config;
    config["SlotPaths"][mPreviewSlot] = original.path;
    config["SlotHashes"][mPreviewSlot] = original.hash;
    config["SlotEffects"][mPreviewSlot] = static_cast<int>(original.effect);
    config["NoisePrintProfiles"][mPreviewSlot] = noise_print::Save(original.noiseProfile);
    const auto indices = SlotParamIndices(mPreviewSlot);
    for (size_t i = 0; i < indices.size(); ++i)
      config[GetParam(indices[i])->GetName()] = original.values[i];
  }
  chunk.PutStr(kStateHeader);
  return chunk.PutStr(config.dump().c_str()) > 0;
}

void NeuralAmpModeler::_UpdatePresetControls()
{
  if (auto* ui = GetUI())
    if (auto* label = ui->GetControlWithTag(kCtrlTagPresetName))
    {
      label->As<IVButtonControl>()->SetLabelStr(mPresetName.c_str());
      label->SetTooltip((mPresetName + " | Browse rig presets. Right-click for load / save actions.").c_str());
      const bool empty = GetRigPresetFiles().empty();
      ui->GetControlWithTag(kCtrlTagPresetPrevious)->SetDisabled(empty);
      ui->GetControlWithTag(kCtrlTagPresetNext)->SetDisabled(empty);
      const auto file = GetRigPresetFile();
      ui->GetControlWithTag(kCtrlTagPresetSave)->SetTooltip(file.empty()
        ? "Save this rig as a new preset file. Right-click for Save / Save as."
        : ("Save as a new preset file. Shift- or Alt-click saves over " + nam_library::PathText(file.filename())
           + " directly. Right-click for Save / Save as.").c_str());
    }
}

std::string NeuralAmpModeler::GetRigPresetName() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return mPresetName;
}
static bool HasRigPresetExtension(const std::filesystem::path& path)
{
  auto extension = nam_library::PathText(path.extension());
  std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
  return extension == ".ngrid";
}
std::vector<std::filesystem::path> NeuralAmpModeler::GetRigPresetFiles() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  std::vector<std::filesystem::path> files;
  if (mPresetDirectory.empty()) return files;
  // The arrows skip presets that a containing preset folder's .ignore rules exclude.
  std::vector<nam_library::IgnoreRules> rules;
  for (const auto& folder : mPresetFolders)
    rules.emplace_back(std::filesystem::u8path(folder));
  std::error_code error;
  for (std::filesystem::directory_iterator it(std::filesystem::u8path(mPresetDirectory), error), end;
       !error && it != end; it.increment(error))
    if (HasRigPresetExtension(it->path()) && it->is_regular_file(error)
        && std::none_of(rules.begin(), rules.end(), [&](auto& rule) { return rule.Ignored(it->path(), false); }))
      files.push_back(it->path());
  std::sort(files.begin(), files.end(), [](const auto& a, const auto& b) { return a.filename() < b.filename(); });
  return files;
}
void NeuralAmpModeler::StepRigPreset(int direction)
{
  const auto files = GetRigPresetFiles();
  if (files.empty()) return;
  const auto name = GetRigPresetName();
  int current = -1;
  for (int i = 0; i < files.size(); ++i) if (nam_library::PathText(files[i].stem()) == name) current = i;
  const int count = static_cast<int>(files.size());
  const int next = current < 0 ? (direction > 0 ? 0 : count - 1) : (current + direction + count) % count;
  const auto error = LoadRigPreset(files[next]);
  if (!error.empty() && GetUI()) _ShowMessageBox(GetUI(), error.c_str(), "Rig preset", EMsgBoxType::kMB_OK);
}

// The rig part of the state: no instance folders, tuner choices or held audio.
nlohmann::json NeuralAmpModeler::_CurrentRig() const
{
  IByteChunk chunk;
  if (!SerializeState(chunk)) throw std::runtime_error("Cannot serialize this rig.");
  WDL_String header, text;
  const int pos = chunk.GetStr(header, 0);
  if (pos < 0 || chunk.GetStr(text, pos) < 0) throw std::runtime_error("Cannot read this rig's state.");
  auto rig = nlohmann::json::parse(text.Get());
  rig.erase("PresetDirectory");
  rig.erase(GetParam(kTunerReference)->GetName()); rig.erase(GetParam(kTunerMute)->GetName());
  rig.erase(GetParam(kTunerActive)->GetName());
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
  {
    rig[GetParam(SlotUtilityParam(slot, uFreezeHold))->GetName()] = 0.;
    rig[GetParam(SlotUtilityParam(slot, uShimmerFreeze))->GetName()] = 0.;
    rig[GetParam(SlotUtilityParam(slot, uNoiseLearn))->GetName()] = 0.;
  }
  return rig;
}

std::string NeuralAmpModeler::SaveRigPreset(const std::filesystem::path& path)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  try
  {
    const auto name = nam_library::PathText(path.stem());
    if (name.empty() || name.size() > 512) throw std::runtime_error("Choose a shorter preset filename.");
    auto rig = _CurrentRig();
    rig["PresetName"] = name;
    rig_preset::Write(path, rig);
    mPresetName = name; mPresetDirectory = nam_library::PathText(path.parent_path());
    for (const auto& folder : mPresetFolders)
      if (nam_library::Within(path, std::filesystem::u8path(folder))) { RescanCaptureFolder(); break; }
    _UpdatePresetControls(); _NotifyHostStateChanged();
    return {};
  }
  catch (const std::exception& error) { return error.what(); }
}

void NeuralAmpModeler::_ApplyRig(nlohmann::json rig, const std::string& name, const std::string& directory)
{
  rig["PresetName"] = name;
  rig["PresetDirectory"] = directory;
  // Tuner choices belong to this instance, not the rig.
  rig[GetParam(kTunerReference)->GetName()] = GetParam(kTunerReference)->Value();
  rig[GetParam(kTunerMute)->GetName()] = GetParam(kTunerMute)->Value();
  rig[GetParam(kTunerActive)->GetName()] = GetParam(kTunerActive)->Value();
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
  {
    rig[GetParam(SlotUtilityParam(slot, uFreezeHold))->GetName()] = 0.;
    rig[GetParam(SlotUtilityParam(slot, uShimmerFreeze))->GetName()] = 0.;
    rig[GetParam(SlotUtilityParam(slot, uNoiseLearn))->GetName()] = 0.;
  }
  IByteChunk chunk;
  chunk.PutStr(kStateHeader); chunk.PutStr(rig.dump().c_str());
  // While audio holds the current rig, stage the new one in the other grid, then switch.
  // Without a running host there is nothing to fade: edit the audible grid directly.
  const bool switching = mRigTransition.Hold(std::chrono::milliseconds(250));
  const int previousGrid = mEditGrid;
  if (switching)
    mEditGrid = 1 - mEditGrid;
  if (UnserializeState(chunk, 0) < 0)
  {
    mEditGrid = previousGrid;
    if (switching) mRigTransition.Release();
    throw std::runtime_error("Preset contains invalid or unsupported rig state.");
  }
  if (switching)
  {
    mGrids[mEditGrid].ResetStopped(mSampleRatePrepared, mMaxBlockPrepared); // Audio does not use this grid yet.
    mInactiveGridUsed = true;
    mRigTransition.Start(mEditGrid, mPresetCrossfade ? RigTransition::Mode::Crossfade : RigTransition::Mode::Dip,
                         mSampleRatePrepared);
  }
  mPresetDirectory = directory;
  OnRestoreState();
#ifdef CLAP_API
  // A rig has more parameters than iPlug2's 512-event UI queue. Ask the host
  // to read all current values instead of enqueueing thousands of gestures.
  const auto* host = _host.host();
  if (auto* params = static_cast<const clap_host_params*>(host->get_extension(host, CLAP_EXT_PARAMS)))
    params->rescan(host, CLAP_PARAM_RESCAN_VALUES);
#else
  DirtyParametersFromUI();
#endif
  _UpdateControlsFromModel(); _NotifyHostStateChanged();
}

std::string NeuralAmpModeler::LoadRigPreset(const std::filesystem::path& path)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  try
  {
    auto rig = rig_preset::Read(path);
    const auto name = nam_library::PathText(path.stem());
    if (name.empty() || name.size() > 512) throw std::runtime_error("Preset filename is too long.");
    _ApplyRig(std::move(rig), name, nam_library::PathText(path.parent_path()));
    return {};
  }
  catch (const std::exception& error) { return error.what(); }
}

bool NeuralAmpModeler::ConfirmNewRig()
{
  auto* ui = GetUI();
  return ui && _ShowMessageBox(ui, "Start a new empty rig?\n\nUnsaved changes to the current rig are lost.", "New rig",
                               EMsgBoxType::kMB_YESNO) == EMsgBoxResult::kYES;
}

std::string NeuralAmpModeler::NewRig()
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  try
  {
    // Keep the preset folder so Save as and the arrows stay where the user works.
    _ApplyRig(mEmptyRig, "Untitled", mPresetDirectory);
    return {};
  }
  catch (const std::exception& error) { return error.what(); }
}

std::filesystem::path NeuralAmpModeler::GetRigPresetFile() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (mPresetDirectory.empty()) return {};
  const auto path = std::filesystem::u8path(mPresetDirectory) / std::filesystem::u8path(mPresetName + ".ngrid");
  std::error_code error;
  return std::filesystem::is_regular_file(path, error) ? path : std::filesystem::path{};
}

void NeuralAmpModeler::QuickSaveRigPreset()
{
  const auto path = GetRigPresetFile();
  if (path.empty()) { PromptRigPreset(true); return; }
  const auto error = SaveRigPreset(path);
  if (!error.empty() && GetUI()) _ShowMessageBox(GetUI(), error.c_str(), "Rig preset", EMsgBoxType::kMB_OK);
}

void NeuralAmpModeler::PromptRigPreset(bool save)
{
  if (auto* ui = GetUI())
  {
    // Without a loaded preset, Save as / Open start in the first preset folder.
    WDL_String file, directory((mPresetDirectory.empty() && !mPresetFolders.empty() ? mPresetFolders.front() : mPresetDirectory).c_str());
    if (save) file.Set((mPresetName + ".ngrid").c_str());
    ui->PromptForFile(file, directory, save ? EFileAction::Save : EFileAction::Open, "ngrid",
      [this, save](const WDL_String& chosen, const WDL_String&) {
        if (!chosen.GetLength()) return;
        auto path = std::filesystem::u8path(chosen.Get());
        if (save && !path.has_extension()) path += ".ngrid";
        const auto error = save ? SaveRigPreset(path) : LoadRigPreset(path);
        if (!error.empty() && GetUI()) _ShowMessageBox(GetUI(), error.c_str(), "Rig preset", EMsgBoxType::kMB_OK);
        else if (auto* ui = GetUI())
          if (auto* control = ui->GetControlWithTag(kCtrlTagLibraryBrowser); control && !control->IsHidden())
          {
            if (save) control->As<NAMLibraryBrowserControl>()->FocusCurrent();
            else control->Hide(true);
          }
      });
  }
}

bool NeuralAmpModeler::ConfirmDeleteRigPreset(const std::filesystem::path& path)
{
  auto* ui = GetUI();
  if (!ui) return false;
  const auto message = "Delete \"" + nam_library::PathText(path.stem()) + "\" from disk?\n\n" + nam_library::PathText(path)
                       + "\n\nThis cannot be undone. The current rig stays loaded.";
  return _ShowMessageBox(ui, message.c_str(), "Delete preset", EMsgBoxType::kMB_YESNO) == EMsgBoxResult::kYES;
}

std::string NeuralAmpModeler::DeleteRigPreset(const std::filesystem::path& path)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  std::error_code error;
  if (!HasRigPresetExtension(path) || !std::filesystem::is_regular_file(path, error))
    return "Only existing .ngrid preset files can be deleted.";
  if (!std::filesystem::remove(path, error))
    return "Cannot delete the preset file.";
  // Drop the entry without re-hashing every capture; a running scan may already have listed it.
  if (mLibraryScanning) RescanCaptureFolder();
  else
  {
    auto library = std::make_shared<nam_library::Snapshot>(*mLibrary);
    library->captures.erase(std::remove_if(library->captures.begin(), library->captures.end(),
      [&](const auto& entry) { return nam_library::SamePath(entry.path, path); }), library->captures.end());
    mLibrary = library;
    _UpdateLibraryControls();
  }
  _UpdatePresetControls();
  return {};
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (mPreviewOriginal)
    if (auto* ui = GetUI())
      ui->GetControlWithTag(kCtrlTagLibraryBrowser)->Hide(true);
  EndSlotPreview(false);
  try
  {
    if (chunk.Size() > 1024 * 1024 || startPos < 0)
      return -1;
    WDL_String header;
    int pos = ReadStateString(chunk, header, startPos);
    if (pos < 0)
      return -1;
    if (strcmp(header.Get(), kStateHeader) != 0)
      return -1;
    WDL_String serialized;
    pos = ReadStateString(chunk, serialized, pos);
    if (pos < 0)
      return -1;
    auto config = nlohmann::json::parse(serialized.Get());
    // Check integer domains on the JSON value itself: get<int>() would wrap out-of-range values.
    const auto integerIn = [](const nlohmann::json& value, int64_t minimum, int64_t maximum) {
      if (value.is_number_unsigned())
        return value.get<uint64_t>() <= static_cast<uint64_t>(maximum)
               && static_cast<int64_t>(value.get<uint64_t>()) >= minimum;
      return value.is_number_integer() && value.get<int64_t>() >= minimum && value.get<int64_t>() <= maximum;
    };
    if (!config.is_object() || !config.contains("schema")
        || !integerIn(config["schema"], kInitialEffectStateSchema, kEffectStateSchema))
      return -1;
    // Newer states are rejected. Older schemas default only controls introduced after them.
    const int schema = config["schema"].get<int>();
    const auto validText = [](const nlohmann::json& value, size_t limit) {
      return value.is_string() && value.get<std::string>().size() <= limit
             && value.get<std::string>().find('\0') == std::string::npos;
    };
    const auto slotArray = [&](const char* field) {
      return config.contains(field) && config[field].is_array() && config[field].size() == FixedNAMGrid::kSlots;
    };
    if (config.contains("PresetName") && !validText(config["PresetName"], 512)) return -1;
    if (config.contains("PresetDirectory") && !validText(config["PresetDirectory"], 32768)) return -1;
    if (!config.contains("PerformanceScenes") || !config["PerformanceScenes"].is_array()
        || config["PerformanceScenes"].size() != mPerformanceScenes.size())
      return -1;
    for (const auto& scene : config["PerformanceScenes"])
    {
      if (!scene.is_object() || scene.size() > kNumParams) return -1;
      size_t recognized = 0;
      for (int i = 0; i < kNumParams; ++i)
      {
        const auto it = scene.find(GetParam(i)->GetName());
        if (it == scene.end()) continue;
        if (!IsPerformanceSceneParam(i) || !it->is_number() || !std::isfinite(it->get<double>())) return -1;
        if (IsStateOnlyParam(i))
        {
          const double value = it->get<double>();
          if (std::floor(value) != value || value < GetParam(i)->GetMin() || value > GetParam(i)->GetMax())
            return -1;
        }
        ++recognized;
      }
      if (recognized != scene.size()) return -1;
    }
    if (!slotArray("SlotPaths") || !slotArray("SlotEffects") || !slotArray("SlotHashes") || !slotArray("NoisePrintProfiles"))
      return -1;
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    {
      const auto& path = config["SlotPaths"][slot];
      const auto& effect = config["SlotEffects"][slot];
      if (!validText(path, 32768) || !integerIn(effect, 0, static_cast<int>(SlotEffect::Last))
          || !IsValidSlotEffect(static_cast<SlotEffect>(effect.get<int>())))
        return -1;
      const auto type = static_cast<SlotEffect>(effect.get<int>());
      if (type != SlotEffect::NAM && type != SlotEffect::CabinetIR && path != "")
        return -1;
      const auto& hash = config["SlotHashes"][slot];
      if (!hash.is_string()) return -1;
      const auto value = hash.get<std::string>();
      if (!value.empty() && (value.size() != 40 || !std::all_of(value.begin(), value.end(), [](char ch) {
            return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); }))) return -1;
      if (noise_print::Load(config["NoisePrintProfiles"][slot]).valid && type != SlotEffect::NoisePrint)
        return -1;
    }
    for (int i = 0; i < kNumParams; ++i)
    {
      const char* name = GetParam(i)->GetName();
      if (schema < IntroducedIn(i) && !config.contains(name))
        config[name] = GetParam(i)->GetDefault();
      if (!config.contains(name) || !config[name].is_number() || !std::isfinite(config[name].get<double>()))
        return -1;
      // An unknown algorithm must fail before applying any state, rather than
      // silently clamp to a different sound. Native mode IDs never normalize.
      if (IsStateOnlyParam(i))
      {
        const double value = config[name].get<double>();
        if (std::floor(value) != value || value < GetParam(i)->GetMin() || value > GetParam(i)->GetMax())
          return -1;
      }
    }
    _UnserializeApplyConfig(config);
    return pos;
  }
  catch (const std::exception&)
  {
    return -1;
  }
}

void NeuralAmpModeler::OnUIOpen()
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  _SendAllParamValuesToUI();
  _UpdateLibraryControls();
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    SendControlMsgFromDelegate(kCtrlTagGridSlotBase + static_cast<int>(slot), kMsgTagLoadedModel);
  _UpdateControlsFromModel();
}

// iPlug2's default full sync sends each parameter separately: a scan of every control plus
// OnParamChangeUI per parameter, which took seconds with this many parameters and controls.
// One pass over the controls sets the same values; the dependent UI refresh then runs once.
void NeuralAmpModeler::_SendAllParamValuesToUI()
{
  auto* ui = GetUI();
  if (!ui)
    return;
  for (int c = 0; c < ui->NControls(); ++c)
  {
    auto* control = ui->GetControl(c);
    for (int v = 0; v < control->NVals(); ++v)
      if (const int index = control->GetParamIdx(v); index > kNoParameter && index < NParams())
        control->SetValueFromDelegate(GetParam(index)->GetNormalized(), v);
  }
  _RefreshParamDependentUI(true);
}

// iPlug2's default also runs OnParamChangeUI for every parameter after a state load.
void NeuralAmpModeler::OnParamReset(iplug::EParamSource)
{
  for (int i = 0; i < NParams(); ++i)
    OnParamChange(i);
  _RefreshParamDependentUI(true);
}

void NeuralAmpModeler::OnUIClose()
{
  EndSlotPreview(false);
  iplug::Plugin::OnUIClose();
}

void NeuralAmpModeler::BeginInformHostOfParamChangeFromUI(int paramIdx)
{
  if (!mPreviewOriginal || SlotControlOwner(paramIdx) != static_cast<int>(mPreviewSlot))
    iplug::Plugin::BeginInformHostOfParamChangeFromUI(paramIdx);
}

void NeuralAmpModeler::SendParameterValueFromUI(int paramIdx, double value)
{
  if (!mPreviewOriginal || SlotControlOwner(paramIdx) != static_cast<int>(mPreviewSlot))
    iplug::Plugin::SendParameterValueFromUI(paramIdx, value);
  else
  {
    GetParam(paramIdx)->SetNormalized(value);
    OnParamChange(paramIdx);
    OnParamChangeUI(paramIdx, EParamSource::kUI);
  }
}

void NeuralAmpModeler::EndInformHostOfParamChangeFromUI(int paramIdx)
{
  if (!mPreviewOriginal || SlotControlOwner(paramIdx) != static_cast<int>(mPreviewSlot))
    iplug::Plugin::EndInformHostOfParamChangeFromUI(paramIdx);
}

void NeuralAmpModeler::ProcessMidiMsg(const IMidiMsg& msg)
{
  // Hosts deliver notes before processing their block (or, while not processing, from
  // a parameter flush), so this list belongs to the processing context, like ProcessBlock.
  if (msg.StatusMsg() == IMidiMsg::kNoteOn && msg.Velocity() > 0 && mNoteCount < kMaxNotesPerBlock)
    mNoteFrames[mNoteCount++] = std::max(0, msg.mOffset);
}

void NeuralAmpModeler::SetRoutingOpen(bool open)
{
  auto* ui = GetUI();
  if (open && IsTunerActive() && ui) grid_ui::DisableTunerFromUI(ui);
  mRoutingOpen = open;
  if (!ui) return;
  ui->GetControlWithTag(kCtrlTagRouting)->Hide(!open);
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    ui->GetControlWithTag(kCtrlTagGridSlotBase + static_cast<int>(slot))->OnMsgFromDelegate(kMsgTagLoadedModel, 0, nullptr);
  ui->SetAllControlsDirty();
}

void NeuralAmpModeler::OnParamChange(int paramIdx)
{
  if (const int slot = SlotControlOwner(paramIdx); slot >= 0)
    mAudioControls.Invalidate(static_cast<size_t>(slot));
  // Enabling detection only publishes atomics; worker/DSP state stays on its owning thread.
  if (paramIdx == kTunerActive) mTuner.Enable(IsTunerActive());
  // Changed slot banks are read at audio block boundaries.
}

void NeuralAmpModeler::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (source == EParamSource::kUI && IsStateOnlyParam(paramIdx)
      && (!mPreviewOriginal || SlotControlOwner(paramIdx) != static_cast<int>(mPreviewSlot)))
    _NotifyHostStateChanged(); // Algorithm and matrix selections are saved state, not automation gestures.
  _RefreshParamDependentUI(paramIdx == kTunerActive);
}

void NeuralAmpModeler::_RefreshParamDependentUI(bool tunerChanged)
{
  if (tunerChanged && IsTunerActive())
    if (auto* ui = GetUI())
    {
      ui->GetControlWithTag(kCtrlTagSettingsBox)->Hide(true);
      ui->GetControlWithTag(kCtrlTagPerformance)->Hide(true);
      if (mRoutingOpen) SetRoutingOpen(false); // The tuner takes the editor area.
    }
  _UpdateTunerControls();
  // Refresh labels/quality and selected-module controls after automation or preset recall.
  if (auto* ui = GetUI())
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
      if (auto* control = ui->GetControlWithTag(kCtrlTagGridSlotBase + static_cast<int>(slot)))
        control->OnMsgFromDelegate(kMsgTagLoadedModel, 0, nullptr);
}

// Private methods ============================================================

void NeuralAmpModeler::_AllocateIOPointers(const size_t nChans)
{
  if (mInputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mInputPointers without freeing");
  mInputPointers = new sample*[nChans];
  if (mInputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Tried to re-allocate mOutputPointers without freeing");
  mOutputPointers = new sample*[nChans];
  if (mOutputPointers == nullptr)
    throw std::runtime_error("Failed to allocate pointer to output buffer!\n");
}

void NeuralAmpModeler::_DeallocateIOPointers()
{
  if (mInputPointers != nullptr)
  {
    delete[] mInputPointers;
    mInputPointers = nullptr;
  }
  if (mInputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to input buffer!\n");
  if (mOutputPointers != nullptr)
  {
    delete[] mOutputPointers;
    mOutputPointers = nullptr;
  }
  if (mOutputPointers != nullptr)
    throw std::runtime_error("Failed to deallocate pointer to output buffer!\n");
}

void NeuralAmpModeler::_ResolveLibraryFiles()
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (!mLibrary) return;
  bool changed = false;
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
  {
    const auto effect = mSlotEffects[slot];
    if (effect != SlotEffect::NAM && effect != SlotEffect::CabinetIR) continue;
    const auto* capture = nam_library::FindHash(*mLibrary, mSlotHashes[slot], effect == SlotEffect::CabinetIR);
    if (!capture) continue;
    const auto path = nam_library::PathText(capture->path);
    if (IsSlotLoaded(slot) && path == mSlotPaths[slot].Get()) continue;
    // Keep the file that is loaded or was chosen while it still holds the remembered content;
    // a rescan only relinks slots whose file disappeared or was replaced.
    if (mSlotPaths[slot].GetLength())
      try
      {
        if (nam_library::ContentHash(std::filesystem::u8path(mSlotPaths[slot].Get())) == mSlotHashes[slot]) continue;
      }
      catch (const std::exception&) {}
    try { if (nam_library::ContentHash(capture->path) != mSlotHashes[slot]) continue; }
    catch (const std::exception&) { continue; } // File changed/disappeared after scanning.
    if (IsSlotLoaded(slot))
    {
      // Identical content is already processing: only update its canonical file reference.
      mSlotPaths[slot].Set(path.c_str()); mSlotGear[slot] = capture->gear;
      SendControlMsgFromDelegate(kCtrlTagGridSlotBase + static_cast<int>(slot), kMsgTagLoadedModel);
      changed = true;
    }
    else if (_StageSlot(slot, effect, path, mSlotHashes[slot]).empty()) changed = true;
  }
  if (changed) _NotifyHostStateChanged();
}
std::string NeuralAmpModeler::_StageSlot(size_t slot, SlotEffect effect, const std::string& path, const std::string& expectedHash)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (slot >= FixedNAMGrid::kSlots)
    return "Invalid grid slot";
  const int tag = kCtrlTagGridSlotBase + static_cast<int>(slot);
  try
  {
    auto config = _GetSlotConfig(slot);
    config.effect = effect;
    config.noiseProfile = effect == SlotEffect::NoisePrint ? mNoiseProfiles[slot] : noise_print::Profile{};
    config.path = path;
    config.hash.clear();
    if (!path.empty() && (effect == SlotEffect::NAM || effect == SlotEffect::CabinetIR))
    {
      const auto isIR = effect == SlotEffect::CabinetIR;
      const auto file = std::filesystem::u8path(config.path);
      const auto hashOf = [](const std::filesystem::path& candidate) {
        try { return nam_library::ContentHash(candidate); }
        catch (const std::exception&) { return std::string{}; }
      };
      // A saved or explicitly chosen path keeps its own file while that file still holds the
      // remembered content. The content hash only takes over when the file was renamed, moved
      // or replaced, so choosing one of several identical captures is never redirected.
      auto chosenHash = hashOf(file);
      if (!expectedHash.empty() && chosenHash != expectedHash && mLibrary)
        if (const auto* known = nam_library::FindHash(*mLibrary, expectedHash, isIR))
          if (const auto knownHash = hashOf(known->path); knownHash == expectedHash)
          {
            config.path = nam_library::PathText(known->path);
            chosenHash = knownHash;
          }
      if (chosenHash.empty()) chosenHash = nam_library::ContentHash(file); // Surface the read error.
      if (!expectedHash.empty() && chosenHash != expectedHash)
        throw std::runtime_error("The capture/IR at this path has different content");
      config.hash = std::move(chosenHash);
    }
    _CommitSlot(slot, PrepareSlot(std::move(config), mSampleRatePrepared, mMaxBlockPrepared), false);
  }
  catch (const std::exception& error)
  {
    SendControlMsgFromDelegate(tag, kMsgTagLoadFailed);
    return error.what();
  }
  return "";
}

void NeuralAmpModeler::_CommitSlot(size_t slot, PreparedSlot prepared, bool updateParameters, bool notifyHost)
{
  const auto& config = prepared.config;
  const auto info = prepared.dsp->Info();
  const bool latencyChanged = prepared.dsp->latency != mRequestedLatencies[slot];
  mRequestedLatencies[slot] = prepared.dsp->latency;
  mNoiseProfiles[slot] = config.effect == SlotEffect::NoisePrint ? config.noiseProfile : noise_print::Profile{};
  mNoiseEpochs[slot] = ++mNextNoiseEpoch;
  prepared.dsp->noiseEpoch = mNoiseEpochs[slot];
#ifdef CLAP_API
  mGrids[mEditGrid].stages[slot].Publish(std::move(prepared.dsp), latencyChanged);
#else
  mGrids[mEditGrid].stages[slot].Publish(std::move(prepared.dsp));
#endif
  if (updateParameters)
  {
    const auto indices = SlotParamIndices(slot);
    for (size_t i = 0; i < indices.size(); ++i)
    {
      const int index = indices[i];
      if (GetParam(index)->Value() == config.values[i])
        continue;
      const double normalized = GetParam(index)->ToNormalized(config.values[i]);
      if (notifyHost)
      {
        BeginInformHostOfParamChange(index);
        SetParameterValue(index, normalized);
      }
      else
      {
        GetParam(index)->SetNormalized(normalized);
        OnParamChange(index);
      }
      SendParameterValueFromDelegate(index, normalized, true);
      if (notifyHost) EndInformHostOfParamChange(index);
    }
  }
  mSlotPaths[slot].Set(config.path.c_str());
  mSlotHashes[slot] = config.hash;
  mSlotEffects[slot] = config.effect;
  uint32_t freezeMask = 0;
  for (size_t i = 0; i < mSlotEffects.size(); ++i)
  {
    if (mSlotEffects[i] == SlotEffect::Freeze) freezeMask |= 1u << i;
    if (mSlotEffects[i] == SlotEffect::ShimmerReverb) freezeMask |= 1u << (i + FixedNAMGrid::kSlots);
  }
  mFreezeMask.store(freezeMask);
  // Preserve main's finite 60 s render policy; active Freeze/Shimmer holds are
  // reported separately as infinite by the processing callback.
  std::array<double, 4> rowTails{};
  for (size_t i = 0; i < mSlotEffects.size(); ++i)
    rowTails[i / 4] += mSlotEffects[i] == SlotEffect::DualDelay ? 1200.
                      : IsDelay(mSlotEffects[i]) ? 560. : IsAirReverb(mSlotEffects[i]) ? 60. : IsReverb(mSlotEffects[i]) ? 45.
                      : mSlotEffects[i] == SlotEffect::CosmicEcho ? 60. : mSlotEffects[i] == SlotEffect::ShimmerReverb ? 30.
                      : mSlotEffects[i] == SlotEffect::FrequencyShifter ? 560.
                      : mSlotEffects[i] == SlotEffect::Stutter ? 33. : 0.;
  mEffectTailSeconds.store(std::min(60., 2. + rowTails[0] + std::max(rowTails[1], rowTails[2]) + rowTails[3]));

  mSlotInfo[slot] = info;
  mSlotGear[slot] = config.effect == SlotEffect::CabinetIR ? nam_library::Gear::Cabinet : nam_library::Gear::Unknown;
  if (config.effect == SlotEffect::NAM && !config.path.empty())
  {
    try
    {
      const auto path = std::filesystem::u8path(config.path);
      mSlotGear[slot] = nam_library::Inspect(path, path.root_path()).gear; // Metadata/path hints, control thread only.
    }
    catch (const std::exception&)
    {
    } // Missing files retain a neutral color, never guess cabinet inclusion.
  }
  mPreparedQuality[slot] = config.values[16];
  _UpdateLatency(latencyChanged);
  SendControlMsgFromDelegate(kCtrlTagGridSlotBase + static_cast<int>(slot), kMsgTagLoadedModel);
}

size_t NeuralAmpModeler::_GetBufferNumChannels() const
{
  // Assumes input=output (no mono->stereo effects)
  return mInputArray.size();
}

size_t NeuralAmpModeler::_GetBufferNumFrames() const
{
  if (_GetBufferNumChannels() == 0)
    return 0;
  return mInputArray[0].size();
}

void NeuralAmpModeler::_PrepareBuffers(const size_t numChannels, const size_t numFrames)
{
  const bool updateChannels = numChannels != _GetBufferNumChannels();
  const bool updateFrames = updateChannels || (_GetBufferNumFrames() != numFrames);
  //  if (!updateChannels && !updateFrames)  // Could we do this?
  //    return;

  if (updateChannels)
  {
    _PrepareIOPointers(numChannels);
    mInputArray.resize(numChannels);
    mOutputArray.resize(numChannels);
  }
  if (updateFrames)
  {
    for (auto c = 0; c < mInputArray.size(); c++)
    {
      mInputArray[c].resize(numFrames);
      std::fill(mInputArray[c].begin(), mInputArray[c].end(), 0.0);
    }
    for (auto c = 0; c < mOutputArray.size(); c++)
    {
      mOutputArray[c].resize(numFrames);
      std::fill(mOutputArray[c].begin(), mOutputArray[c].end(), 0.0);
    }
  }
  // Would these ever get changed by something?
  for (auto c = 0; c < mInputArray.size(); c++)
    mInputPointers[c] = mInputArray[c].data();
  for (auto c = 0; c < mOutputArray.size(); c++)
    mOutputPointers[c] = mOutputArray[c].data();
}

void NeuralAmpModeler::_PrepareIOPointers(const size_t numChannels)
{
  _DeallocateIOPointers();
  _AllocateIOPointers(numChannels);
}

void NeuralAmpModeler::_ProcessInput(iplug::sample** inputs, const size_t nFrames, const size_t nChansIn,
                                     const size_t nChansOut, double target)
{
  for (size_t s = 0; s < nFrames; ++s)
  {
    mGlobalInputGain = target + mMasterCoefficient * (mGlobalInputGain - target);
    for (size_t c = 0; c < nChansOut; ++c)
      mInputArray[c][s] = nChansIn ? inputs[std::min(c, nChansIn - 1)][s] * mGlobalInputGain : 0.;
  }
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut, double gain)
{
  const double target = mTuner.Enabled() && GetParam(kTunerMute)->Bool() ? 0. : 1.;
  const double step = 1. / std::max(1., GetSampleRate() * .005);
  for (size_t s = 0; s < nFrames; ++s)
  {
    mTunerGain += std::clamp(target - mTunerGain, -step, step);
    mMasterGain = gain + mMasterCoefficient * (mMasterGain - gain);
    for (size_t c = 0; c < nChansOut; ++c)
    {
      const double value = nChansOut == 1 ? .5 * (inputs[0][s] + inputs[1][s]) : inputs[std::min(c, nChansIn - 1)][s];
      outputs[c][s] = mMasterGain * mTunerGain * value;
#ifdef APP_API
      outputs[c][s] = std::clamp(outputs[c][s], -1., 1.);
#endif
    }
  }
}

void NeuralAmpModeler::_UpdateTunerControls()
{
  if (auto* ui = GetUI())
  {
    auto* tuner = ui->GetControlWithTag(kCtrlTagTuner);
    if (!tuner) return;
    ui->GetControlWithTag(kCtrlTagTunerButton)->SetValueFromDelegate(GetParam(kTunerActive)->GetNormalized());
    static_cast<NAMGridTunerControl*>(tuner)->Refresh();
    const bool visible = IsTunerActive() && ui->GetControlWithTag(kCtrlTagSettingsBox)->IsHidden()
                         && ui->GetControlWithTag(kCtrlTagPerformance)->IsHidden();
    if (tuner->IsHidden() == visible)
    {
      tuner->Hide(!visible);
      for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
        ui->GetControlWithTag(kCtrlTagGridSlotBase + static_cast<int>(slot))->OnMsgFromDelegate(kMsgTagLoadedModel, 0, nullptr);
      ui->SetAllControlsDirty();
    }
  }
}

void NeuralAmpModeler::_UpdateControlsFromModel()
{
  _UpdateTunerControls();
  if (auto* ui = GetUI())
  {
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
      if (auto* control = ui->GetControlWithTag(kCtrlTagGridSlotBase + static_cast<int>(slot)))
        control->OnMsgFromDelegate(kMsgTagLoadedModel, 0, nullptr);
  }
}

void NeuralAmpModeler::_UpdateTailSize()
{
  bool holding = false;
  const auto mask = mFreezeMask.load();
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots && mask; ++slot)
    holding |=
      (((mask & (1u << slot)) && GetParam(SlotUtilityParam(slot, uFreezeHold))->Bool())
       || ((mask & (1u << (slot + FixedNAMGrid::kSlots))) && GetParam(SlotUtilityParam(slot, uShimmerFreeze))->Bool()))
      && GetParam(SlotActiveParam(slot))->Bool();
  const int tail = holding
                     ? kTailInfinite
                     : static_cast<int>(std::min(2147483646., std::ceil(GetSampleRate() * mEffectTailSeconds.load())));
  if (GetTailSize() != tail)
    SetTailSize(tail);
}

void NeuralAmpModeler::_UpdateLatency(bool forceRestart)
{
  const int latency = FixedNAMGrid::TotalLatency(mRequestedLatencies);
  // A lane's delay can change while the longer lane and total host latency remain the same.
  // CLAP must still reactivate to adopt/reset the pending model and alignment buffers.
  if (GetLatency() != latency || forceRestart)
    SetLatency(latency);
}

// Applies a state that UnserializeState has completely validated.
void NeuralAmpModeler::_UnserializeApplyConfig(nlohmann::json& config)
{
  mPresetName = config.value("PresetName", std::string("Untitled"));
  mPresetDirectory = config.value("PresetDirectory", std::string{});
  _UpdatePresetControls();
  for (size_t i = 0; i < mPerformanceScenes.size(); ++i)
    mPerformanceScenes[i] = config["PerformanceScenes"][i];
  mActiveScene = -1;
  ENTER_PARAMS_MUTEX
  // JSON object lookup avoids a full parameter-name scan for every saved value.
  for (int i = 0; i < kNumParams; ++i)
    GetParam(i)->Set(config[GetParam(i)->GetName()].get<double>());
  OnParamReset(iplug::EParamSource::kPresetRecall);
  LEAVE_PARAMS_MUTEX

  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    _ClearModel(slot);
  // Library folders come from this user's settings, not from the state: a project never
  // replaces them, and every instance picks up folders another instance changed.
  _ReloadLibraryFolders();
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
  {
    const auto path = config["SlotPaths"][slot].get<std::string>();
    const auto effect = static_cast<SlotEffect>(config["SlotEffects"][slot].get<int>());
    const auto hash = config["SlotHashes"][slot].get<std::string>();
    mNoiseProfiles[slot] = noise_print::Load(config["NoisePrintProfiles"][slot]);
    if (!_StageSlot(slot, effect, path, hash).empty())
    {
      // A missing file remains selected for relinking, with a safely empty processor.
      _ClearModel(slot, effect);
      mSlotPaths[slot].Set(path.c_str());
      mSlotHashes[slot] = hash;
      SendControlMsgFromDelegate(kCtrlTagGridSlotBase + static_cast<int>(slot), kMsgTagLoadedModel);
    }
  }
}

void NeuralAmpModeler::_ClearModel(size_t slot, SlotEffect effect)
{ _StageSlot(slot, effect, ""); }
