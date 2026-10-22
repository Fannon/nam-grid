#include "NoisePrintState.h"
#include <algorithm> // std::clamp, std::min
#include <cmath> // pow
#include <cctype>
#include <filesystem>
#include <iostream>
#include <utility>

#include "Colors.h"
#include "../NeuralAmpModelerCore/NAM/activations.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
// clang-format off
// These includes need to happen in this order or else the latter won't know
// a bunch of stuff.
#include "NeuralAmpModeler.h"
#include "IPlug_include_in_plug_src.h"
// clang-format on
#include "architecture.hpp"

#include "NeuralAmpModelerControls.h"
#include "NAMGridControls.h"
#include "GridPerformanceControls.h"
#include "RigPreset.h"
#include "RigPresetControls.h"

using namespace iplug;
using namespace igraphics;

const double kDCBlockerFrequency = 5.0;
constexpr int kGateModeSchema = 15, kFilterModeSchema = 16, kLoFiSchema = 17, kIdentitySchema = 18, kFXNamesSchema = 19, kReverbDelaySchema = 20, kCosmicSchema = 21, kCutoffNamesSchema = 22, kTunerAutomationSchema = 23, kOutboardSchema = 24, kAirDynamicsSchema = 25, kGridStateSchema = kAirDynamicsSchema;

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

const std::string kCalibrateInputParamName = "CalibrateInput";
const bool kDefaultCalibrateInput = false;
const std::string kInputCalibrationLevelParamName = "InputCalibrationLevel";
const double kDefaultInputCalibrationLevel = 12.0;

static std::filesystem::path CaptureSettingsFile()
{
  WDL_String support;
  AppSupportPath(support);
  return support.GetLength() ? std::filesystem::u8path(support.Get()) / "NAMDualPrototype" / "library.json"
                             : std::filesystem::path{};
}

NeuralAmpModeler::NeuralAmpModeler(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  _InitToneStack();
  nam::activations::Activation::enable_fast_tanh();
  GetParam(kInputLevel)->InitGain("Input", 0.0, -40.0, 40.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kNoiseGateThreshold)->InitGain("Threshold", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", false);
  GetParam(kEQActive)->InitBool("ToneStack", false);
  GetParam(kOutputMode)->InitEnum("OutputMode", 0, {"Raw", "Normalized", "Calibrated"}); // TODO DRY w/ control
  GetParam(kIRToggle)->InitBool("IRToggle", false);
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");
  GetParam(kDriveInput)->InitGain("Drive Input", 0., -40., 40., .1);
  GetParam(kDriveTrim)->InitGain("Drive Trim", 0., -40., 40., .1);
  GetParam(kDriveActive)->InitBool("Drive Enabled", true);
  GetParam(kAmpTrim)->InitGain("Amp Trim", 0., -40., 40., .1);
  GetParam(kAmpActive)->InitBool("Amp Enabled", true);
  GetParam(kSlim)->InitDouble("Slim", 1.0, 0.0, 1.0, 0.01);
  for (size_t slot = 2; slot < FixedNAMGrid::kSlots; ++slot)
  {
    const auto name = GridSlotName(slot);
    GetParam(SlotInputParam(slot))->InitGain((name + " In").c_str(), 0., -40., 40., .1);
    GetParam(SlotTrimParam(slot))->InitGain((name + " Trim").c_str(), 0., -40., 40., .1);
    GetParam(SlotActiveParam(slot))->InitBool((name + " Enabled").c_str(), true);
  }
  GetParam(kParallelBlend)->InitDouble("Parallel Blend", 50., 0., 100., .1, "%");
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
  {
    const auto name = GridSlotName(slot);
    GetParam(SlotDynamicsParam(slot, 0))->InitDouble((name + " Gate Threshold").c_str(), -55., -90., -10., .1, "dB");
    GetParam(SlotDynamicsParam(slot, 1))->InitDouble((name + " Gate Release").c_str(), 120., 20., 500., 1., "ms");
    GetParam(SlotDynamicsParam(slot, 2))->InitDouble((name + " Gate Reduction").c_str(), 60., 0., 80., .1, "dB");
    GetParam(SlotDynamicsParam(slot, 3))->InitDouble((name + " Comp Threshold").c_str(), -18., -48., 0., .1, "dB");
    GetParam(SlotDynamicsParam(slot, 4))->InitDouble((name + " Comp Release").c_str(), 120., 40., 600., 1., "ms");
    GetParam(SlotDynamicsParam(slot, 5))->InitDouble((name + " Comp Makeup").c_str(), 0., 0., 18., .1, "dB");
    GetParam(SlotEQParam(slot, 0))->InitDouble((name + " EQ Bass").c_str(), 0., -12., 12., .1, "dB");
    GetParam(SlotEQParam(slot, 1))->InitDouble((name + " EQ Mid").c_str(), 0., -12., 12., .1, "dB");
    GetParam(SlotEQParam(slot, 2))->InitDouble((name + " EQ Treble").c_str(), 0., -12., 12., .1, "dB");
    GetParam(SlotEQParam(slot, 3))->InitDouble((name + " EQ High Pass").c_str(), 0., 0., 300., 1., "Hz");
    GetParam(SlotEQParam(slot, 4))->InitDouble((name + " EQ Low Pass").c_str(), 20000., 2000., 20000., 10., "Hz");
    GetParam(SlotEQParam(slot, 5))->InitDouble((name + " EQ Presence").c_str(), 0., -6., 6., .1, "dB");
    GetParam(SlotEQParam(slot, 6))->InitDouble((name + " IR Level").c_str(), -18., -36., 18., .1, "dB");
    for (int i = 0; i < kUtilityControls; ++i)
    {
      const auto& p = kEffectParameters[i];
      auto* param = GetParam(SlotUtilityParam(slot, i));
      const auto choices = EffectChoices(i);
      if (!choices.empty())
      {
        param->InitEnum((name + " " + p.name).c_str(), static_cast<int>(p.initial), static_cast<int>(choices.size()));
        for (int choice = 0; choice < choices.size(); ++choice)
          param->SetDisplayText(choice, choices[choice]);
      }
      else if (p.minimum == 0. && p.maximum == 1. && p.step == 1.)
        param->InitBool((name + " " + p.name).c_str(), p.initial >= .5);
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
    if (slot != 1)
      GetParam(SlotFullParam(slot))->InitBool((name + " Full Quality").c_str(), true);
  }
  GetParam(kTunerReference)->InitDouble("Tuner Reference", 440., 430., 450., .1, "Hz");
  GetParam(kTunerMute)->InitBool("Tuner Mute", true);
  GetParam(kTunerActive)->InitBool("Tuner", false);
  for (int i = 0; i < grid_performance::Count; ++i)
  {
    const auto& p = grid_performance::parameters[i]; auto* param = GetParam(kGridParamBase + i);
    if (i == grid_performance::LFOSlot || i == grid_performance::EnvSlot || i == grid_performance::StepSlot
        || i == grid_performance::Macro1Slot || i == grid_performance::Macro2Slot)
    {
      param->InitEnum(p.name, 0, 16);
      for (int slot = 0; slot < 16; ++slot) param->SetDisplayText(slot, GridSlotName(slot).c_str());
    }
    else if (i == grid_performance::LFOTarget || i == grid_performance::EnvTarget || i == grid_performance::StepTarget
             || i == grid_performance::Macro1Target || i == grid_performance::Macro2Target)
    {
      param->InitEnum(p.name, 0, static_cast<int>(grid_performance::targetNames.size()));
      for (int target = 0; target < grid_performance::targetNames.size(); ++target)
        param->SetDisplayText(target, grid_performance::targetNames[target]);
    }
    else if (i == grid_performance::LFODivision || i == grid_performance::StepDivision)
    {
      param->InitEnum(p.name, static_cast<int>(p.initial), static_cast<int>(grid_tempo::names.size()));
      for (int d = 0; d < grid_tempo::names.size(); ++d) param->SetDisplayText(d, grid_tempo::names[d]);
    }
    else if (p.minimum == 0. && p.maximum == 1. && p.step == 1.) param->InitBool(p.name, p.initial >= .5);
    else if (p.exponential) param->InitDouble(p.name, p.initial, p.minimum, p.maximum, p.step, p.unit, 0, "", IParam::ShapeExp());
    else param->InitDouble(p.name, p.initial, p.minimum, p.maximum, p.step, p.unit);
  }
  const auto librarySettings = nam_library::ReadSettings(CaptureSettingsFile());
  mRescanOnStartup = librarySettings.rescanOnStartup;
  mShowVisualizations = librarySettings.showVisualizations;
  SetCaptureFolders(librarySettings.folders);

  mNoiseGateTrigger.AddListener(&mNoiseGateGain);

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
    ui->AttachControl(new grid_ui::Button(IRECT(165, 14, 285, 42), [](IControl* caller) {
      caller->GetUI()->GetControlWithTag(kCtrlTagTuner)->Hide(true);
      caller->GetUI()->GetControlWithTag(kCtrlTagSettingsBox)->Hide(true);
      caller->GetUI()->GetControlWithTag(kCtrlTagPerformance)->Hide(false);
    }, "Grid / Control"));
    ui->AttachControl(new NAMRigPresetControl(IRECT(300, 14, 640, 42)), kCtrlTagPresetName);
    ui->AttachControl(new grid_ui::Button(IRECT(648, 14, 680, 42), [this](IControl*) { StepRigPreset(-1); }, "<"), kCtrlTagPresetPrevious);
    ui->AttachControl(new grid_ui::Button(IRECT(688, 14, 720, 42), [this](IControl*) { StepRigPreset(1); }, ">"), kCtrlTagPresetNext);
    ui->AttachControl(new grid_ui::Button(
      IRECT(862, 14, 976, 42),
      [](IControl* caller) {
        caller->GetUI()->GetControlWithTag(kCtrlTagTuner)->Hide(true);
        caller->GetUI()->GetControlWithTag(kCtrlTagSettingsBox)->Hide(false);
      },
      "Library & settings"));
    ui->AttachControl(new grid_ui::Toggle(IRECT(770, 14, 850, 42), kTunerActive, "Tuner"), kCtrlTagTunerButton)
      ->SetTooltip("Automatable tuner and output mute. Turn off to restore sound; Mute output can be disabled below.");
    ui->AttachControl(new grid_ui::Wiring(b));
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    {
      const float left = 88.f + 212.f * (slot % 4);
      const float top = grid_layout::cardTop + grid_layout::rowPitch * static_cast<float>(slot / 4);
      ui->AttachControl(new NAMGridSlotControl(IRECT(left, top, left + 188.f, top + grid_layout::cardHeight), slot),
                        kCtrlTagGridSlotBase + static_cast<int>(slot));
    }
    ui->AttachControl(new grid_ui::FlowKnob(IRECT(0, 67, 74, 135), kGridParamBase + grid_performance::InputGain, "IN"))
      ->SetTooltip("Input gain before every grid module. Double-click to reset to 0 dB.");
    ui->AttachControl(new grid_ui::FlowKnob(IRECT(928, 235, 1000, 303), kParallelBlend, "A / B"))
      ->SetTooltip("Lane balance: 0% is A, 50% is both lanes equally, 100% is B.");
    ui->AttachControl(new grid_ui::FlowKnob(IRECT(928, 379, 1000, 447), kOutputLevel, "OUT"))
      ->SetTooltip("Final output gain after the complete grid.");
    ui->AttachControl(new grid_ui::FlowMeter(IRECT(81, 68, 84, 140), IRECT(0, 142, 76, 170)), kCtrlTagInputMeter);
    ui->AttachControl(new grid_ui::FlowMeter(IRECT(922, 380, 927, 452), IRECT(928, 450, 1000, 478)), kCtrlTagOutputMeter);
    ui->AttachControl(new NAMGridSettingsControl(b), kCtrlTagSettingsBox)->Hide(true);
    ui->AttachControl(new NAMGridTunerControl(IRECT(0, grid_layout::editorTop, grid_layout::width, grid_layout::height)), kCtrlTagTuner)->Hide(true);
    ui->AttachControl(new NAMGridPerformanceControl(b), kCtrlTagPerformance)->Hide(true);
    ui->SetKeyHandlerFunc([ui](const IKeyPress& key, bool keyUp) {
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
  // Preserve host stereo until a mono NAM slot explicitly collapses it.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
  mInputSender.ProcessBlock(mInputPointers, numFrames, kCtrlTagInputMeter, static_cast<int>(numChannelsInternal));
  mTuner.Enable(IsTunerActive());
  mTuner.Push(mInputPointers[0], mInputPointers[1], nFrames, sampleRate);
  _ApplyDSPStaging();
  const bool noiseGateActive = GetParam(kNoiseGateActive)->Value();
  const bool toneStackActive = GetParam(kEQActive)->Value();

  // Noise gate trigger
  sample** triggerOutput = mInputPointers;
  if (noiseGateActive)
  {
    const double time = 0.01;
    const double threshold = GetParam(kNoiseGateThreshold)->Value(); // GetParam...
    const double ratio = 0.1; // Quadratic...
    const double openTime = 0.005;
    const double holdTime = 0.01;
    const double closeTime = 0.05;
    const dsp::noise_gate::TriggerParams triggerParams(time, threshold, ratio, openTime, holdTime, closeTime);
    mNoiseGateTrigger.SetParams(triggerParams);
    mNoiseGateTrigger.SetSampleRate(sampleRate);
    triggerOutput = mNoiseGateTrigger.Process(mInputPointers, numChannelsInternal, numFrames);
  }

  for (size_t channel = 0; channel < numChannelsInternal; ++channel)
    std::copy_n(triggerOutput[channel], nFrames, mOutputPointers[channel]);
  std::array<FixedNAMGrid::Controls, FixedNAMGrid::kSlots> controls;
  for (size_t slot = 0; slot < controls.size(); ++slot)
  {
    auto& c = controls[slot];
    c.enabled = GetParam(SlotActiveParam(slot))->Bool();
    c.inputDB = GetParam(SlotInputParam(slot))->Value();
    c.trimDB = GetParam(SlotTrimParam(slot))->Value();
    c.dynamics.gateThreshold = GetParam(SlotDynamicsParam(slot, 0))->Value();
    c.dynamics.gateRelease = GetParam(SlotDynamicsParam(slot, 1))->Value();
    c.dynamics.gateRange = GetParam(SlotDynamicsParam(slot, 2))->Value();
    c.dynamics.gateMode = GetParam(SlotUtilityParam(slot, uGateMode))->Int();
    c.dynamics.compThreshold = GetParam(SlotDynamicsParam(slot, 3))->Value();
    c.dynamics.compRelease = GetParam(SlotDynamicsParam(slot, 4))->Value();
    c.dynamics.compMakeup = GetParam(SlotDynamicsParam(slot, 5))->Value();
    c.dynamics.compAttack = GetParam(SlotUtilityParam(slot, uCompAttack))->Value();
    c.dynamics.punchAttack = GetParam(SlotUtilityParam(slot, uPunchAttack))->Value();
    c.dynamics.compMix = GetParam(SlotUtilityParam(slot, uCompMix))->Value();
    c.eq.bass = GetParam(SlotEQParam(slot, 0))->Value();
    c.eq.mid = GetParam(SlotEQParam(slot, 1))->Value();
    c.eq.treble = GetParam(SlotEQParam(slot, 2))->Value();
    c.eq.highPass = GetParam(SlotEQParam(slot, 3))->Value();
    c.eq.lowPass = GetParam(SlotEQParam(slot, 4))->Value();
    c.eq.presence = GetParam(SlotEQParam(slot, 5))->Value();
    c.eq.irLevel = GetParam(SlotEQParam(slot, 6))->Value();
    c.utility.noise.learn = GetParam(SlotUtilityParam(slot, uNoiseLearn))->Bool();
    c.utility.noise.sensitivity = GetParam(SlotUtilityParam(slot, uNoiseSensitivity))->Value();
    c.utility.noise.reduction = GetParam(SlotUtilityParam(slot, uNoiseReduction))->Value();
    c.utility.noise.attack = GetParam(SlotUtilityParam(slot, uNoiseAttack))->Value();
    c.utility.noise.release = GetParam(SlotUtilityParam(slot, uNoiseRelease))->Value();
    c.utility.noise.mix = GetParam(SlotUtilityParam(slot, uNoiseMix))->Value();
    c.utility.noise.gateThreshold = GetParam(SlotUtilityParam(slot, uNoiseGateThreshold))->Value();
    c.utility.sync = GetParam(SlotUtilityParam(slot, uTempoSync))->Bool();
    c.utility.division = GetParam(SlotUtilityParam(slot, uTempoDivision))->Int();
    c.utility.tempo = GetTempo();
    c.utility.level = GetParam(SlotUtilityParam(slot, uUtilityLevel))->Value();
    c.utility.balance = GetParam(SlotUtilityParam(slot, uUtilityBalance))->Value();
    c.utility.stereo = GetParam(SlotUtilityParam(slot, uUtilityStereo))->Value();
    c.utility.invertL = GetParam(SlotUtilityParam(slot, uUtilityInvertL))->Bool();
    c.utility.invertR = GetParam(SlotUtilityParam(slot, uUtilityInvertR))->Bool();
    c.utility.pingPong = GetParam(SlotUtilityParam(slot, uDelayPingPong))->Bool();
    c.utility.chorusSpread = GetParam(SlotUtilityParam(slot, uChorusSpread))->Value();
    c.utility.stereoReverb = GetParam(SlotUtilityParam(slot, uReverbStereo))->Bool();
    for (int i = 0; i < 5; ++i)
    {
      c.utility.spring[i] = GetParam(SlotUtilityParam(slot, uSpringDecay + i))->Value();
      c.utility.wah[i] = GetParam(SlotUtilityParam(slot, uWahPosition + i))->Value();
    }
    c.utility.frequency = GetParam(SlotUtilityParam(slot, uFrequency))->Value();
    c.utility.gain = GetParam(SlotUtilityParam(slot, uGain))->Value();
    c.utility.q = GetParam(SlotUtilityParam(slot, uQ))->Value();
    c.utility.boost = GetParam(SlotUtilityParam(slot, uBoostGain))->Value();
    c.utility.tilt = GetParam(SlotUtilityParam(slot, uBoostTilt))->Value();
    c.utility.cutoff = GetParam(SlotUtilityParam(slot, uFilterCutoff))->Value();
    c.utility.resonance = GetParam(SlotUtilityParam(slot, uFilterResonance))->Value();
    c.utility.envelope = GetParam(SlotUtilityParam(slot, uFilterEnvelope))->Value();
    c.utility.filterMode = GetParam(SlotUtilityParam(slot, uFilterMode))->Int();
    c.utility.rise = GetParam(SlotUtilityParam(slot, uSwellRise))->Value();
    c.utility.threshold = GetParam(SlotUtilityParam(slot, uSwellThreshold))->Value();
    c.utility.release = GetParam(SlotUtilityParam(slot, uSwellRelease))->Value();
    for (int i = 0; i < 5; ++i)
    {
      c.utility.delay[i] = GetParam(SlotUtilityParam(slot, uDelayTime + i))->Value();
      c.utility.reverb[i] = GetParam(SlotUtilityParam(slot, uReverbDecay + i))->Value();
      c.utility.modulation[i] = GetParam(SlotUtilityParam(slot, uModRate + i))->Value();
    }
    for (int i = 0; i < 4; ++i)
      c.utility.tape[i] = GetParam(SlotUtilityParam(slot, uTapeDrive + i))->Value();
    c.utility.delayMotion = GetParam(SlotUtilityParam(slot, uDelayMotion))->Value();
    c.utility.delayOutput = GetParam(SlotUtilityParam(slot, uDelayOutput))->Value();
    c.utility.reverbMotion = GetParam(SlotUtilityParam(slot, uReverbMotion))->Value();
    c.utility.reverbHighPass = GetParam(SlotUtilityParam(slot, uReverbHighPass))->Value();
    for (int i = 0; i < 4; ++i)
      c.utility.tube[i] = GetParam(SlotUtilityParam(slot, uTubeDrive + i))->Value();
    c.utility.fineDelay = GetParam(SlotUtilityParam(slot, uUtilityDelay))->Value();
    c.utility.vibeMode = GetParam(SlotUtilityParam(slot, uVibeMode))->Value();
    c.utility.eqMode = GetParam(SlotUtilityParam(slot, uEQMode))->Value();
    const auto readFamily = [&](auto& values, int first) {
      for (size_t i = 0; i < values.size(); ++i) values[i] = GetParam(SlotUtilityParam(slot, first + static_cast<int>(i)))->Value();
    };
    readFamily(c.utility.rotary, uRotarySlow); readFamily(c.utility.limiter, uLimiterCeiling);
    readFamily(c.utility.octaver, uOctDry); readFamily(c.utility.doubler, uDoubleDetune);
    readFamily(c.utility.freeze, uFreezeHold); readFamily(c.utility.stutter, uStutterTime);
    readFamily(c.utility.crusher, uCrushBits); readFamily(c.utility.ring, uRingFrequency); readFamily(c.utility.dual, uDualRatio);
    readFamily(c.utility.juno, uJunoMode); readFamily(c.utility.shimmer, uShimmerMix); readFamily(c.utility.shift, uShiftFrequency);
    readFamily(c.utility.distortion, uDistStyle);
    readFamily(c.utility.lofi, uLoFiSpeed);
    c.utility.tapeHeads = GetParam(SlotUtilityParam(slot, uTapeHeads))->Int();
    c.utility.delayDiffusion = GetParam(SlotUtilityParam(slot, uDelayDiffusion))->Value();
    readFamily(c.utility.airPlate, uAirPlateInput); readFamily(c.utility.airHall, uAirHallSustain);
    readFamily(c.utility.space, uSpaceReplace);
    readFamily(c.utility.cosmic, uCosmicTime);
    c.utility.cosmicStyle = GetParam(SlotUtilityParam(slot, uCosmicStyle))->Int();
    readFamily(c.utility.strip, uStripLowCut); readFamily(c.utility.colour, uColourInput);
    readFamily(c.utility.consoleEQ, uConsoleTreble); readFamily(c.utility.butter, uButterCompress);
    c.utility.colourDesk = GetParam(SlotUtilityParam(slot, uColourDesk))->Int();
    readFamily(c.utility.black, uBlackThreshold); readFamily(c.utility.pressure, uPressureCompress);
    c.utility.blackMode = GetParam(SlotUtilityParam(slot, uBlackMode))->Int();
  }
  // Preserve the former Amp's calibration/normalization controls on Pre 2 for migrated states.
  controls[1].calibrate = GetParam(kCalibrateInput)->Bool();
  controls[1].calibrationDB = GetParam(kInputCalibrationLevel)->Value();
  controls[1].outputMode = GetParam(kOutputMode)->Int();
  controls[1].slim = GetParam(kSlim)->Value();
  grid_performance::Values performance{};
  for (int i = 0; i < grid_performance::Count; ++i) performance[i] = GetParam(kGridParamBase + i)->Value();
  const auto routing = grid_performance::Route(performance);
  const double blend = GetParam(kParallelBlend)->Value() * .01;
  if (mModulation.Active(performance))
  {
    // A bounded control quantum avoids audio-rate model inference and keeps
    // modulation responsive without allocating or writing host parameters.
    for (int offset = 0; offset < nFrames;)
    {
      const int count = std::min(mModulation.Quantum(), nFrames - offset);
      auto animated = controls; double animatedBlend = blend;
      mModulation.Apply(animated, animatedBlend, performance);
      mGrid.Process(mOutputPointers[0] + offset, count, animated, animatedBlend, mOutputPointers[1] + offset, routing);
      mModulation.Advance(mInputPointers[0] + offset, mInputPointers[1] + offset, count, performance, GetTempo());
      offset += count;
    }
  }
  else
  {
    mGrid.Process(mOutputPointers[0], nFrames, controls, blend, mOutputPointers[1], routing);
  }
  mMeterFrames += nFrames;
  const int meterInterval = std::max(1, static_cast<int>(sampleRate / 30.));
  if (mMeterFrames >= meterInterval)
  {
    mMeterFrames %= meterInterval;
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
      mSlotMeterSender.PushData({kCtrlTagGridSlotBase + static_cast<int>(slot), {mGrid.stages[slot].TakeMeter()}});
  }
  // Apply the noise gate after the NAM
  sample** gateGainOutput =
    noiseGateActive ? mNoiseGateGain.Process(mOutputPointers, numChannelsInternal, numFrames) : mOutputPointers;

  mToneStack->SetParam("bass", GetParam(kToneBass)->Value());
  mToneStack->SetParam("middle", GetParam(kToneMid)->Value());
  mToneStack->SetParam("treble", GetParam(kToneTreble)->Value());
  sample** toneStackOutPointers = (toneStackActive && mToneStack != nullptr)
                                    ? mToneStack->Process(gateGainOutput, numChannelsInternal, nFrames)
                                    : gateGainOutput;

  sample** irPointers = toneStackOutPointers;
  sample* legacyIRPointers[2]{};
  auto* cabinet = mCabinet.Active();
  if (cabinet && cabinet->ir && GetParam(kIRToggle)->Bool())
  {
    auto* l = toneStackOutPointers[0];
    auto* r = toneStackOutPointers[1];
    legacyIRPointers[0] = cabinet->ir->Process(&l, 1, numFrames)[0];
    legacyIRPointers[1] = cabinet->right->Process(&r, 1, numFrames)[0];
    irPointers = legacyIRPointers;
  }
  const bool switchingIR = mCabinet.HasPending();
  const double switchStep = 1. / (sampleRate * .005);
  for (size_t i = 0; i < numFrames; ++i)
  {
    mIRSwitchGain = switchingIR ? std::max(0., mIRSwitchGain - switchStep) : std::min(1., mIRSwitchGain + switchStep);
    for (size_t channel = 0; channel < numChannelsInternal; ++channel)
      irPointers[channel][i] *= mIRSwitchGain;
  }

  // And the HPF for DC offset (Issue 271)
  const double highPassCutoffFreq = kDCBlockerFrequency;
  // const double lowPassCutoffFreq = 20000.0;
  const recursive_linear_filter::HighPassParams highPassParams(sampleRate, highPassCutoffFreq);
  // const recursive_linear_filter::LowPassParams lowPassParams(sampleRate, lowPassCutoffFreq);
  mHighPass.SetParams(highPassParams);
  // mLowPass.SetParams(lowPassParams);
  sample** hpfPointers = mHighPass.Process(irPointers, numChannelsInternal, numFrames);
  // sample** lpfPointers = mLowPass.Process(hpfPointers, numChannelsInternal, numFrames);

  // restore previous floating point state
  std::feupdateenv(&fe_state);

  // Let's get outta here
  // Mono host outputs downmix; stereo host outputs retain independent channels.
  _ProcessOutput(hpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // _ProcessOutput(lpfPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
  // * Output of input leveling (inputs -> mInputPointers),
  // * Output of output leveling (mOutputPointers -> outputs)
  _UpdateMeters(mInputPointers, outputs, numFrames, numChannelsInternal, numChannelsExternalOut);
}

void NeuralAmpModeler::OnReset()
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  const auto sampleRate = GetSampleRate();
  const int maxBlockSize = std::max(1, GetBlockSize());
  mSampleRatePrepared = sampleRate;
  mMaxBlockPrepared = maxBlockSize;
  mMeterFrames = 0;
  mModulation.Reset(sampleRate);
  mMasterGain = GetParam(kOutputLevel)->DBToAmp();
  mGlobalInputGain = GetParam(kGridParamBase + grid_performance::InputGain)->DBToAmp();
  mMasterCoefficient = std::exp(-1. / (.005 * sampleRate));
  _PrepareBuffers(kNumChannelsInternal, maxBlockSize);
  for (auto& channel : mInputArray)
    std::fill(channel.begin(), channel.end(), 0.);
  for (auto& channel : mOutputArray)
    std::fill(channel.begin(), channel.end(), 0.);

  _UpdateTailSize();
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  // If there is a model or IR loaded, they need to be checked for resampling.
  _ResetModelAndIR(sampleRate, maxBlockSize);
  mToneStack->Reset(sampleRate, maxBlockSize);
  _UpdateLatency();
  // Prime all reused DSP paths, including currently bypassed ones, outside ProcessBlock.
  mNoiseGateTrigger.SetSampleRate(sampleRate);
  mNoiseGateTrigger.SetParams(dsp::noise_gate::TriggerParams(.01, -80., .1, .005, .01, .05));
  mNoiseGateTrigger.Process(mInputPointers, kNumChannelsInternal, maxBlockSize);
  mNoiseGateGain.Process(mInputPointers, kNumChannelsInternal, maxBlockSize);
  mToneStack->Process(mInputPointers, kNumChannelsInternal, maxBlockSize);
  mHighPass.ClearHistory();
  mHighPass.SetParams(recursive_linear_filter::HighPassParams(sampleRate, kDCBlockerFrequency));
  mHighPass.Process(mInputPointers, kNumChannelsInternal, maxBlockSize);
}

void NeuralAmpModeler::SetCaptureFolder(const char* folder, bool fromUI)
{ SetCaptureFolders(folder && folder[0] ? std::vector<std::string>{folder} : std::vector<std::string>{}, fromUI); }

void NeuralAmpModeler::SetCaptureFolders(std::vector<std::string> folders, bool fromUI)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  auto normalized = nam_library::NormalizeFolders(folders);
  const bool needsIndex = normalized != mCaptureFolders || (!mLibraryIndexed && !mLibraryScanning);
  mCaptureFolders = std::move(normalized);
  mCaptureFolder = mCaptureFolders.empty() ? std::string{} : mCaptureFolders.front();
  if (needsIndex && (fromUI || mRescanOnStartup))
    RescanCaptureFolder();
  else if (needsIndex)
  {
    mLibraryWorker.Request(std::vector<std::string>{}); // Cancel an older request without indexing the roots.
    mLibrary = std::make_shared<nam_library::Snapshot>();
    mLibraryScanning = mLibraryIndexed = false;
    _UpdateLibraryControls();
  }
  if (fromUI)
  {
    mLibraryDefaultSaveFailed = !nam_library::WriteSettings(CaptureSettingsFile(), {mCaptureFolders, mRescanOnStartup, mShowVisualizations});
    _UpdateLibraryControls();
    _NotifyHostStateChanged();
  }
}

std::vector<std::string> NeuralAmpModeler::GetCaptureFolders() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return mCaptureFolders;
}
void NeuralAmpModeler::AddCaptureFolder(const char* folder)
{
  auto folders = GetCaptureFolders();
  if (folders.size() < 32)
    folders.emplace_back(folder);
  SetCaptureFolders(std::move(folders), true);
}
void NeuralAmpModeler::RemoveCaptureFolder(size_t index)
{
  auto folders = GetCaptureFolders();
  if (index >= folders.size())
    return;
  folders.erase(folders.begin() + index);
  SetCaptureFolders(std::move(folders), true);
}
void NeuralAmpModeler::SelectSlot(size_t slot)
{
  if (slot >= FixedNAMGrid::kSlots)
    return;
  mSelectedSlot = slot;
  if (auto* ui = GetUI())
    for (size_t i = 0; i < FixedNAMGrid::kSlots; ++i)
      if (auto* control = ui->GetControlWithTag(kCtrlTagGridSlotBase + static_cast<int>(i)))
        control->OnMsgFromDelegate(kMsgTagLoadedModel, 0, nullptr);
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
  BeginInformHostOfParamChange(index);
  SetParameterValue(index, value);
  SendParameterValueFromDelegate(index, value, true);
  EndInformHostOfParamChange(index);
  _NotifyHostStateChanged();
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
  const auto error = _StageModel(WDL_String(path), slot);
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

noise_print::Profile NeuralAmpModeler::GetNoiseProfile(size_t slot) const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (slot >= FixedNAMGrid::kSlots || mSlotEffects[slot] != SlotEffect::NoisePrint)
    return {};
  return mGrid.stages[slot].GetNoiseProfile(mNoiseEpochs[slot], mNoiseProfiles[slot]);
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
  mPerformanceScenes[scene] = std::move(values); _NotifyHostStateChanged();
}
void NeuralAmpModeler::RecallPerformanceScene(size_t scene)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (scene >= mPerformanceScenes.size()) return;
  const auto& values = mPerformanceScenes[scene];
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

void NeuralAmpModeler::SetShowVisualizations(bool enabled)
{
  mShowVisualizations = enabled;
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
  mLibraryScanning = !mCaptureFolders.empty();
  mLibraryWorker.Request(mCaptureFolders);
  _UpdateLibraryControls();
}

std::string NeuralAmpModeler::GetCaptureFolder() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return mCaptureFolder;
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
  if (mCaptureFolders.empty())
    return "Add folders containing NAM captures or WAV impulse responses.";
  if (mLibraryScanning)
    return "Scanning NAM and IR files...";
  if (!mLibraryIndexed)
    return "Library not indexed. Choose Rescan all (startup rescan is off).";
  if (mLibraryDefaultSaveFailed)
    return "Folder saved in project; unable to save the default for new instances.";
  if (!mLibrary->error.empty())
    return mLibrary->error;
  size_t irs = 0;
  for (const auto& entry : mLibrary->captures)
    irs += entry.isIR;
  return std::to_string(mLibrary->captures.size() - irs) + " NAM / " + std::to_string(irs) + " IR / "
         + std::to_string(mCaptureFolders.size()) + " folders / " + std::to_string(mLibrary->unreadable)
         + " unreadable";
}

std::string NeuralAmpModeler::GetLibraryMatchStatus() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (mLibraryScanning)
    return "Scanning library...";
  if (mCaptureFolders.empty())
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
  {
    if (auto* control = ui->GetControlWithTag(kCtrlTagSettingsBox))
      control->SetDirty(false);
    if (auto* control = ui->GetControlWithTag(kCtrlTagCaptureSearch))
      control->As<ITextControl>()->SetStr(mCaptureFilter.search.c_str());
    if (auto* control = ui->GetControlWithTag(kCtrlTagGearFilter))
      control->As<IVButtonControl>()->SetLabelStr(
        mCaptureFilter.gear < 0 ? "All gear"
                                : nam_library::GearName(static_cast<nam_library::Gear>(mCaptureFilter.gear)));
    if (auto* control = ui->GetControlWithTag(kCtrlTagCaptureStatus))
      control->As<IVButtonControl>()->SetLabelStr(GetLibraryMatchStatus().c_str());
    if (auto* control = ui->GetControlWithTag(kCtrlTagClearFilter))
      control->SetDisabled(mCaptureFilter.search.empty() && mCaptureFilter.gear < 0);
  }
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
    const auto sequence = mGrid.stages[slot].NoiseSequence();
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
  for (auto& stage : mGrid.stages)
    stage.Collect();
  mCabinet.Collect();
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);
  mSlotMeterSender.TransmitData(*this);
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
  config["schema"] = kGridStateSchema;
  config["PresetName"] = mPresetName;
  config["PresetDirectory"] = mPresetDirectory;
  config["DrivePath"] = mDrivePath.Get();
  config["NAMPath"] = mNAMPath.Get();
  config["IRPath"] = mIRPath.Get();
  config["CaptureFolder"] = mCaptureFolder;
  config["CaptureFolders"] = mCaptureFolders;
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
  chunk.PutStr("###NAMGrid-v1###");
  return chunk.PutStr(config.dump().c_str()) > 0;
}

void NeuralAmpModeler::_UpdatePresetControls()
{
  if (auto* ui = GetUI())
    if (auto* label = ui->GetControlWithTag(kCtrlTagPresetName))
    {
      label->As<IVButtonControl>()->SetLabelStr(mPresetName.c_str());
      label->SetTooltip(mPresetName.c_str());
      const bool empty = GetRigPresetFiles().empty();
      ui->GetControlWithTag(kCtrlTagPresetPrevious)->SetDisabled(empty);
      ui->GetControlWithTag(kCtrlTagPresetNext)->SetDisabled(empty);
    }
}

std::string NeuralAmpModeler::GetRigPresetName() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  return mPresetName;
}
std::vector<std::filesystem::path> NeuralAmpModeler::GetRigPresetFiles() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  std::vector<std::filesystem::path> files;
  if (mPresetDirectory.empty()) return files;
  std::error_code error;
  for (std::filesystem::directory_iterator it(std::filesystem::u8path(mPresetDirectory), error), end;
       !error && it != end; it.increment(error))
  {
    auto extension = nam_library::PathText(it->path().extension());
    std::transform(extension.begin(), extension.end(), extension.begin(), [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); });
    if (extension == ".ngrid" && it->is_regular_file(error)) files.push_back(it->path());
  }
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

std::string NeuralAmpModeler::SaveRigPreset(const std::filesystem::path& path)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  try
  {
    const auto name = nam_library::PathText(path.stem());
    if (name.empty() || name.size() > 512) throw std::runtime_error("Choose a shorter preset filename.");
    IByteChunk chunk;
    if (!SerializeState(chunk)) throw std::runtime_error("Cannot serialize this rig.");
    WDL_String header, text;
    const int pos = chunk.GetStr(header, 0);
    if (pos < 0 || chunk.GetStr(text, pos) < 0) throw std::runtime_error("Cannot read this rig's state.");
    auto rig = nlohmann::json::parse(text.Get());
    rig["PresetName"] = name;
    rig.erase("PresetDirectory");
    rig.erase("CaptureFolder"); rig.erase("CaptureFolders");
    rig.erase(GetParam(kTunerReference)->GetName()); rig.erase(GetParam(kTunerMute)->GetName());
    rig.erase(GetParam(kTunerActive)->GetName());
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    {
      rig[GetParam(SlotUtilityParam(slot, uFreezeHold))->GetName()] = 0.;
      rig[GetParam(SlotUtilityParam(slot, uShimmerFreeze))->GetName()] = 0.;
      rig[GetParam(SlotUtilityParam(slot, uNoiseLearn))->GetName()] = 0.;
    }
    rig_preset::Write(path, rig);
    mPresetName = name; mPresetDirectory = nam_library::PathText(path.parent_path());
    _UpdatePresetControls(); _NotifyHostStateChanged();
    return {};
  }
  catch (const std::exception& error) { return error.what(); }
}

std::string NeuralAmpModeler::LoadRigPreset(const std::filesystem::path& path)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  try
  {
    auto rig = rig_preset::Read(path);
    const auto name = nam_library::PathText(path.stem());
    if (name.empty() || name.size() > 512) throw std::runtime_error("Preset filename is too long.");
    rig["PresetName"] = name;
    rig["PresetDirectory"] = nam_library::PathText(path.parent_path());
    // Library folders and tuner choices belong to this instance, not the rig.
    rig["CaptureFolder"] = mCaptureFolder; rig["CaptureFolders"] = mCaptureFolders;
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
    chunk.PutStr("###NAMGrid-v1###"); chunk.PutStr(rig.dump().c_str());
    if (UnserializeState(chunk, 0) < 0) throw std::runtime_error("Preset contains invalid or unsupported rig state.");
    mPresetDirectory = nam_library::PathText(path.parent_path());
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
    return {};
  }
  catch (const std::exception& error) { return error.what(); }
}

void NeuralAmpModeler::PromptRigPreset(bool save)
{
  if (auto* ui = GetUI())
  {
    WDL_String file, directory(mPresetDirectory.c_str());
    if (save) file.Set((mPresetName + ".ngrid").c_str());
    ui->PromptForFile(file, directory, save ? EFileAction::Save : EFileAction::Open, "ngrid",
      [this, save](const WDL_String& chosen, const WDL_String&) {
        if (!chosen.GetLength()) return;
        auto path = std::filesystem::u8path(chosen.Get());
        if (save && !path.has_extension()) path += ".ngrid";
        const auto error = save ? SaveRigPreset(path) : LoadRigPreset(path);
        if (!error.empty() && GetUI()) _ShowMessageBox(GetUI(), error.c_str(), "Rig preset", EMsgBoxType::kMB_OK);
      });
  }
}

int NeuralAmpModeler::UnserializeState(const IByteChunk& chunk, int startPos)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  try
  {
    if (chunk.Size() > 1024 * 1024 || startPos < 0)
      return -1;
    WDL_String header;
    int pos = ReadStateString(chunk, header, startPos);
    if (pos < 0)
      return -1;
    const bool grid = strcmp(header.Get(), "###NAMGrid-v1###") == 0;
    if (grid || strcmp(header.Get(), "###NAMDual-v1###") == 0)
    {
      WDL_String serialized;
      pos = ReadStateString(chunk, serialized, pos);
      if (pos < 0)
        return -1;
      auto config = nlohmann::json::parse(serialized.Get());
      if (!config.is_object())
        return -1;
      if (!config.contains("schema") || !config["schema"].is_number_integer())
        return -1;
      const auto& schemaValue = config["schema"];
      if (grid ? (schemaValue != 2 && schemaValue != 3 && schemaValue != 6 && schemaValue != 7 && schemaValue != 8
                  && schemaValue != 9 && schemaValue != 10 && schemaValue != 11 && schemaValue != 12
                  && schemaValue != 13 && schemaValue != 1001 && schemaValue != noise_print::experimentSchema
                  && schemaValue != noise_print::integratedSchema && schemaValue != kGateModeSchema
                  && schemaValue != kFilterModeSchema && schemaValue != kLoFiSchema && schemaValue != kIdentitySchema
                  && schemaValue != kFXNamesSchema && schemaValue != kReverbDelaySchema && schemaValue != kCosmicSchema && schemaValue != kCutoffNamesSchema && schemaValue != kTunerAutomationSchema && schemaValue != kOutboardSchema && schemaValue != kGridStateSchema)
               : schemaValue != 1)
        return -1;
      const int schema = schemaValue.get<int>();
      if (grid && schema < kCutoffNamesSchema)
      {
        const auto renameCutoffs = [&](nlohmann::json& values) {
          for (size_t slot=0; slot<FixedNAMGrid::kSlots; ++slot)
            for (const auto& [control, previous] : kPreviousCutoffNames)
            {
              const auto prefix=GridSlotName(slot)+" ";
              const auto oldName=prefix+previous, name=prefix+kEffectParameters[control].name;
              const auto it=values.find(oldName);
              if (it == values.end()) continue;
              if (!values.contains(name)) values[name]=*it;
              values.erase(oldName);
            }
        };
        renameCutoffs(config);
        if (config.contains("PerformanceScenes") && config["PerformanceScenes"].is_array())
          for (auto& scene : config["PerformanceScenes"]) if (scene.is_object()) renameCutoffs(scene);
      }
      if (config.contains("PresetName") && (!config["PresetName"].is_string()
          || config["PresetName"].get<std::string>().size() > 512
          || config["PresetName"].get<std::string>().find('\0') != std::string::npos)) return -1;
      if (config.contains("PresetDirectory") && (!config["PresetDirectory"].is_string()
          || config["PresetDirectory"].get<std::string>().size() > 32768
          || config["PresetDirectory"].get<std::string>().find('\0') != std::string::npos)) return -1;
      if (config.contains("PerformanceScenes"))
      {
        const auto& scenes = config["PerformanceScenes"];
        if (!scenes.is_array() || scenes.size() != 3) return -1;
        for (const auto& scene : scenes)
        {
          if (!scene.is_object() || scene.size() > kNumParams) return -1;
          size_t recognized = 0;
          for (int i = 0; i < kNumParams; ++i)
          {
            const auto it = scene.find(GetParam(i)->GetName());
            if (it == scene.end()) continue;
            if (!IsPerformanceSceneParam(i) || !it->is_number() || !std::isfinite(it->get<double>())) return -1;
            ++recognized;
          }
          if (recognized != scene.size()) return -1;
        }
      }
      if (config.contains("CaptureFolders"))
      {
        const auto& folders = config["CaptureFolders"];
        if (!folders.is_array() || folders.size() > 32)
          return -1;
        for (const auto& folder : folders)
          if (!folder.is_string() || folder.get<std::string>().size() > 32768
              || folder.get<std::string>().find('\0') != std::string::npos)
            return -1;
      }
      for (const char* name : {"DrivePath", "NAMPath", "IRPath"})
        if (!config.contains(name) || !config[name].is_string() || config[name].get<std::string>().size() > 32768
            || config[name].get<std::string>().find('\0') != std::string::npos)
          return -1;
      if (config.contains("CaptureFolder")
          && (!config["CaptureFolder"].is_string() || config["CaptureFolder"].get<std::string>().size() > 32768
              || config["CaptureFolder"].get<std::string>().find('\0') != std::string::npos))
        return -1;
      if (grid)
      {
        if (!config.contains("SlotPaths") || !config["SlotPaths"].is_array()
            || config["SlotPaths"].size() != FixedNAMGrid::kSlots)
          return -1;
        for (const auto& path : config["SlotPaths"])
          if (!path.is_string() || path.get<std::string>().size() > 32768
              || path.get<std::string>().find('\0') != std::string::npos)
            return -1;
        if (schema >= 3)
        {
          if (!config.contains("SlotEffects") || !config["SlotEffects"].is_array()
              || config["SlotEffects"].size() != FixedNAMGrid::kSlots)
            return -1;
          for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
          {
            const auto& effect = config["SlotEffects"][slot];
            if (!effect.is_number_integer() || effect < 0
                || effect > (schema == 3    ? 4
                             : schema == 6  ? static_cast<int>(SlotEffect::Tape)
                             : schema == 7  ? static_cast<int>(SlotEffect::TubeBoost)
                             : schema == 8  ? static_cast<int>(SlotEffect::Utility)
                             : schema == 9  ? static_cast<int>(SlotEffect::Wah)
                             : schema == 10 ? static_cast<int>(SlotEffect::RingMod)
                             : schema <= 12 ? static_cast<int>(SlotEffect::FrequencyShifter)
                             : schema == 13 ? static_cast<int>(SlotEffect::Distortion)
                             : schema < 20 ? static_cast<int>(SlotEffect::HallReverb)
                             : schema == kReverbDelaySchema ? static_cast<int>(SlotEffect::GalacticSpace)
                             : schema < kOutboardSchema ? static_cast<int>(SlotEffect::CosmicEcho)
                             : schema < kAirDynamicsSchema ? static_cast<int>(SlotEffect::ButterComp)
                                            : static_cast<int>(SlotEffect::Last))
                || !IsValidSlotEffect(static_cast<SlotEffect>(effect.get<int>()))
                || (effect == static_cast<int>(SlotEffect::LoFiTape) && (schema < kLoFiSchema || schema >= 1001))
                || (effect != 0 && effect != 7 && config["SlotPaths"][slot] != ""))
              return -1;
          }
        }
        for (int slot = 0; slot < 2; ++slot)
        {
          const auto expected =
            schema >= 3 && config["SlotEffects"][slot] != 0 ? nlohmann::json("") : config["SlotPaths"][slot];
          if (config[slot == 0 ? "DrivePath" : "NAMPath"] != expected)
            return -1;
        }
      }
      else
      {
        config.erase("SlotPaths");
        for (int i = kDualNumParams; i < kNumParams; ++i)
          config.erase(GetParam(i)->GetName());
      }
      const int count = !grid         ? kDualNumParams
                        : schema == 2 ? kGridV2NumParams
                        : schema == 3 ? kGridV3NumParams
                                      : kNumParams;
      if (schema < 3)
        config.erase("SlotEffects");
      for (int i = count; i < kNumParams; ++i)
        config.erase(GetParam(i)->GetName());
      for (int i = 0; i < count; ++i)
      {
        const char* name = GetParam(i)->GetName();
        if (grid && (schema < kAirDynamicsSchema || schema >= 1001) && i >= kUtilityBase && i < kTunerReference
            && (i - kUtilityBase) % kUtilityControls >= uBlackThreshold)
          config[name] = GetParam(i)->GetDefault();
        if (grid && (schema < kOutboardSchema || schema >= 1001) && i >= kUtilityBase && i < kTunerReference
            && (i - kUtilityBase) % kUtilityControls >= uStripLowCut)
          config[name] = GetParam(i)->GetDefault();
        if (grid && i == kTunerActive && (schema < kTunerAutomationSchema || schema >= 1001)) config[name] = 0.;
        if (grid && schema < kReverbDelaySchema && i >= kUtilityBase && i < kTunerReference
            && (i - kUtilityBase) % kUtilityControls >= uTapeHeads)
          config[name] = GetParam(i)->GetDefault();
        if (grid && (schema < kCosmicSchema || schema >= 1001) && i >= kUtilityBase && i < kTunerReference
            && (i - kUtilityBase) % kUtilityControls >= uCosmicTime)
          config[name] = GetParam(i)->GetDefault();
        if (grid && i >= kUtilityBase && i < kTunerReference
            && (i - kUtilityBase) % kUtilityControls >= uLoFiSpeed && (schema < kLoFiSchema || schema >= 1001))
          config[name] = GetParam(i)->GetDefault();
        if (grid && (schema < 10 || schema >= 1001) && i >= kGridParamBase && i < kNumParams)
          config[name] = GetParam(i)->GetDefault();
        if (grid && (schema < 12 || schema >= 1001) && i == kGridParamBase + grid_performance::InputGain)
          config[name] = GetParam(i)->GetDefault();
        if (grid && schema == 1001 && i >= kUtilityBase && i < kTunerReference
            && (i - kUtilityBase) % kUtilityControls == uNoiseGateThreshold)
          config[name] = GetParam(i)->GetDefault(); // First Noise Print experiment had no gate.
        if (grid && i >= kUtilityBase && i < kTunerReference)
        {
          const int control = (i - kUtilityBase) % kUtilityControls;
          if ((schema <= 13 && control >= uNoiseLearn)
              || (schema >= 1001 && control >= uUtilityDelay && control < uNoiseLearn))
            config[name] = GetParam(i)->GetDefault();
        }
        // Prior WIP states called the gate close time Decay. Keep their value
        // while exposing the accurate Release name to hosts and new states.
        if (i >= kDynamicsBase && i < kEQBase && (i - kDynamicsBase) % 6 == 1)
        {
          const auto oldName = GridSlotName((i - kDynamicsBase) / 6) + " Gate Decay";
          if (!config.contains(name) && config.contains(oldName))
            config[name] = config[oldName];
          config.erase(oldName);
        }
        // Schemas 6/7 had 30/41 named utility controls per slot. Fill only newly added
        // controls; the expanded per-slot stride does not change saved names.
        if (grid && schema >= 6 && schema < 10
            && ((i >= kTunerReference && schema < 8)
                || (i >= kUtilityBase && i < kTunerReference
                    && (i - kUtilityBase) % kUtilityControls >= (schema == 6   ? 30
                                                                 : schema == 7 ? 41
                                                                 : schema == 8 ? 48 : 61))))
          config[name] = GetParam(i)->GetDefault();
        // Schema 10 had 103 utility controls per slot; the three HQ modules append 17.
        if (grid && schema >= 11 && schema <= 12 && i >= kUtilityBase && i < kTunerReference
            && (i - kUtilityBase) % kUtilityControls >= uDistStyle)
          config[name] = GetParam(i)->GetDefault();
        if (grid && schema == 10 && i >= kUtilityBase && i < kTunerReference
            && (i - kUtilityBase) % kUtilityControls >= 103)
          config[name] = GetParam(i)->GetDefault();
        if (grid && schema == 8 && i >= kUtilityBase && i < kTunerReference
            && (i - kUtilityBase) % kUtilityControls == uUtilityStereo)
        {
          const auto oldName = GridSlotName((i - kUtilityBase) / kUtilityControls) + " Utility Width";
          if (!config.contains(oldName) || !config[oldName].is_number()
              || !std::isfinite(config[oldName].get<double>()))
            return -1;
          config[name] = config[oldName].get<double>() - 100.;
        }
        if (grid && (schema < kGateModeSchema || schema >= 1001)
            && i >= kUtilityBase && i < kTunerReference && (i - kUtilityBase) % kUtilityControls == uGateMode)
        {
          const size_t slot = (i - kUtilityBase) / kUtilityControls;
          config[name] = config.contains("SlotEffects")
                           && config["SlotEffects"][slot] == static_cast<int>(SlotEffect::TightGate) ? 1. : 0.;
        }
        if (grid && (schema < kFilterModeSchema || schema >= 1001)
            && i >= kUtilityBase && i < kTunerReference && (i - kUtilityBase) % kUtilityControls == uFilterMode)
          config[name] = 0.;
        if (!config.contains(name) || !config[name].is_number() || !std::isfinite(config[name].get<double>()))
          return -1;
      }
      if (config.contains("SlotHashes"))
      {
        const auto& hashes = config["SlotHashes"];
        if (!hashes.is_array() || hashes.size() != FixedNAMGrid::kSlots) return -1;
        for (const auto& hash : hashes)
        {
          if (!hash.is_string()) return -1;
          const auto value = hash.get<std::string>();
          if (!value.empty() && (value.size() != 40 || !std::all_of(value.begin(), value.end(), [](char ch) {
                return (ch >= '0' && ch <= '9') || (ch >= 'a' && ch <= 'f'); }))) return -1;
        }
      }
      if (config.contains("NoisePrintProfiles"))
      {
        const auto& profiles = config["NoisePrintProfiles"];
        if (!profiles.is_array() || profiles.size() != FixedNAMGrid::kSlots)
          return -1;
        for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
        {
          const auto profile = noise_print::Load(profiles[slot]);
          if (profile.valid
              && (!grid || !config.contains("SlotEffects")
                  || config["SlotEffects"][slot] != static_cast<int>(SlotEffect::NoisePrint)))
            return -1;
        }
      }
      else if (schema == 1001 || schema == noise_print::experimentSchema || schema == noise_print::integratedSchema
               || schema == kGateModeSchema || schema == kFilterModeSchema || schema == kLoFiSchema
               || schema == kIdentitySchema || schema == kFXNamesSchema || schema == kReverbDelaySchema || schema == kCosmicSchema || schema == kCutoffNamesSchema || schema == kTunerAutomationSchema || schema == kOutboardSchema || schema == kGridStateSchema)
        return -1;
      _UnserializeApplyConfig(config);
      return pos;
    }
    if (strcmp(header.Get(), "###NeuralAmpModeler###") == 0)
      return _UnserializeStateWithKnownVersion(chunk, pos);
    return _UnserializeStateWithUnknownVersion(chunk, startPos);
  }
  catch (const std::exception&)
  {
    return -1;
  }
}

void NeuralAmpModeler::OnUIOpen()
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  iplug::Plugin::OnUIOpen();
  _UpdateLibraryControls();
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    SendControlMsgFromDelegate(kCtrlTagGridSlotBase + static_cast<int>(slot), kMsgTagLoadedModel);
  if (mIRPath.GetLength())
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength() + 1, mIRPath.Get());
  if (mIRPath.GetLength() && !mIRLoaded)
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
  _UpdateControlsFromModel();
}

void NeuralAmpModeler::OnParamChange(int paramIdx)
{
  // Enabling detection only publishes atomics; worker/DSP state stays on its owning thread.
  if (paramIdx == kTunerActive) mTuner.Enable(IsTunerActive());
  // Other IParam values are read at audio block boundaries.
}

void NeuralAmpModeler::OnParamChangeUI(int paramIdx, EParamSource source)
{
  if (paramIdx == kTunerActive && IsTunerActive())
    if (auto* ui = GetUI())
    {
      ui->GetControlWithTag(kCtrlTagSettingsBox)->Hide(true);
      ui->GetControlWithTag(kCtrlTagPerformance)->Hide(true);
    }
  _UpdateTunerControls();
  // Refresh labels/quality and selected-module controls after automation or preset recall.
  if (auto* ui = GetUI())
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
      if (auto* control = ui->GetControlWithTag(kCtrlTagGridSlotBase + static_cast<int>(slot)))
        control->OnMsgFromDelegate(kMsgTagLoadedModel, 0, nullptr);
}

bool NeuralAmpModeler::OnMessage(int msgTag, int ctrlTag, int dataSize, const void* pData)
{
  switch (msgTag)
  {
    case kMsgTagClearModel: _ClearModel(1); return true;
    case kMsgTagClearDrive: _ClearModel(0); return true;
    case kMsgTagClearIR:
    {
      std::lock_guard<std::recursive_mutex> guard(mControlMutex);
      mCabinet.Publish(std::make_unique<PreparedIR>());
      mIRPath.Set("");
      mIRLoaded = false;
      return true;
    }
    case kMsgTagHighlightColor:
    {
      mHighLightColor.Set((const char*)pData);

      if (GetUI())
      {
        GetUI()->ForStandardControlsFunc([&](IControl* pControl) {
          if (auto* pVectorBase = pControl->As<IVectorBase>())
          {
            IColor color = IColor::FromColorCodeStr(mHighLightColor.Get());

            pVectorBase->SetColor(kX1, color);
            pVectorBase->SetColor(kPR, color.WithOpacity(0.3f));
            pVectorBase->SetColor(kFR, color.WithOpacity(0.4f));
            pVectorBase->SetColor(kX3, color.WithContrast(0.1f));
          }
          pControl->GetUI()->SetAllControlsDirty();
        });
      }

      return true;
    }
    default: return false;
  }
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

void NeuralAmpModeler::_ApplyDSPStaging()
{
  if (mCabinet.HasPending() && mIRSwitchGain <= 0.)
    mCabinet.Adopt();
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

void NeuralAmpModeler::_ResetModelAndIR(const double sampleRate, const int maxBlockSize)
{
  mGrid.ResetStopped(sampleRate, maxBlockSize);
  mCabinet.Collect();
  mCabinet.Adopt();
  mCabinet.Collect();
  if (auto* cabinet = mCabinet.Active())
  {
    if (cabinet->ir && cabinet->ir->GetSampleRate() != sampleRate)
      cabinet->ir = std::make_unique<dsp::ImpulseResponse>(cabinet->ir->GetData(), sampleRate);
    cabinet->Prepare(maxBlockSize);
  }
  for (size_t i = 0; i < mGrid.stages.size(); ++i)
    mRequestedLatencies[i] = mGrid.stages[i].Latency();
}

std::string NeuralAmpModeler::_StageModel(const WDL_String& modelPath, size_t slot)
{ return _StageSlot(slot, SlotEffect::NAM, modelPath.Get()); }

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
      if (slot == 0) mDrivePath.Set(effect == SlotEffect::NAM ? path.c_str() : "");
      if (slot == 1) mNAMPath.Set(effect == SlotEffect::NAM ? path.c_str() : "");
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

void NeuralAmpModeler::_CommitSlot(size_t slot, PreparedSlot prepared, bool updateParameters)
{
  const auto& config = prepared.config;
  const auto info = prepared.dsp->Info();
  const bool latencyChanged = prepared.dsp->latency != mRequestedLatencies[slot];
  mRequestedLatencies[slot] = prepared.dsp->latency;
  mNoiseProfiles[slot] = config.effect == SlotEffect::NoisePrint ? config.noiseProfile : noise_print::Profile{};
  mNoiseEpochs[slot] = ++mNextNoiseEpoch;
  prepared.dsp->noiseEpoch = mNoiseEpochs[slot];
#ifdef CLAP_API
  mGrid.stages[slot].Publish(std::move(prepared.dsp), latencyChanged);
#else
  mGrid.stages[slot].Publish(std::move(prepared.dsp));
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
      BeginInformHostOfParamChange(index);
      SetParameterValue(index, normalized);
      SendParameterValueFromDelegate(index, normalized, true);
      EndInformHostOfParamChange(index);
    }
  }
  if (slot == 0)
  {
    mDrivePath.Set(config.effect == SlotEffect::NAM ? config.path.c_str() : "");
    mDriveLoaded = config.effect == SlotEffect::NAM && info.loaded;
  }
  else if (slot == 1)
  {
    mNAMPath.Set(config.effect == SlotEffect::NAM ? config.path.c_str() : "");
    mAmpInfo = config.effect == SlotEffect::NAM ? info : NAMModelInfo{};
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
  if (slot == 1)
    _UpdateControlsFromModel();
}

dsp::wav::LoadReturnCode NeuralAmpModeler::_StageIR(const WDL_String& irPath)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  try
  {
    // Check data before constructing IR (an empty IR underflows its history length).
    std::vector<float> raw;
    double rate;
    const auto status = dsp::wav::Load(irPath.Get(), raw, rate);
    if (status != dsp::wav::LoadReturnCode::SUCCESS || raw.empty() || rate <= 0.)
      throw std::runtime_error("Invalid or empty impulse response");
    auto prepared = std::make_unique<PreparedIR>();
    dsp::ImpulseResponse::IRData data;
    data.mRawAudio = std::move(raw);
    data.mRawAudioSampleRate = rate;
    prepared->ir = std::make_unique<dsp::ImpulseResponse>(data, mSampleRatePrepared);
    prepared->Prepare(mMaxBlockPrepared);
    mCabinet.Publish(std::move(prepared));
    mIRPath = irPath;
    mIRLoaded = true;
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, irPath.GetLength() + 1, irPath.Get());
    return dsp::wav::LoadReturnCode::SUCCESS;
  }
  catch (const std::exception&)
  {
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadFailed);
    return dsp::wav::LoadReturnCode::ERROR_OTHER;
  }
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

void NeuralAmpModeler::_InitToneStack()
{
  // If you want to customize the tone stack, then put it here!
  mToneStack = std::make_unique<dsp::tone_stack::BasicNamToneStack>();
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
                                     const size_t nChansOut)
{
  const double target = GetParam(kGridParamBase + grid_performance::InputGain)->DBToAmp();
  for (size_t s = 0; s < nFrames; ++s)
  {
    mGlobalInputGain = target + mMasterCoefficient * (mGlobalInputGain - target);
    for (size_t c = 0; c < nChansOut; ++c)
      mInputArray[c][s] = nChansIn ? inputs[std::min(c, nChansIn - 1)][s] * mGlobalInputGain : 0.;
  }
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = GetParam(kOutputLevel)->DBToAmp();
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
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    holding |= GetParam(SlotActiveParam(slot))->Bool()
               && (((mask & (1u << slot)) && GetParam(SlotUtilityParam(slot, uFreezeHold))->Bool())
                   || ((mask & (1u << (slot + FixedNAMGrid::kSlots)))
                       && GetParam(SlotUtilityParam(slot, uShimmerFreeze))->Bool()));
  const int tail = holding ? kTailInfinite
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

void NeuralAmpModeler::_UpdateMeters(sample** inputPointer, sample** outputPointer, const size_t nFrames,
                                     const size_t nChansIn, const size_t nChansOut)
{
  mOutputSender.ProcessBlock(
    outputPointer, static_cast<int>(nFrames), kCtrlTagOutputMeter, static_cast<int>(std::min(size_t{2}, nChansOut)));
}

// HACK
#include "Unserialization.cpp"

void NeuralAmpModeler::_ClearModel(size_t slot, SlotEffect effect)
{ _StageSlot(slot, effect, ""); }
