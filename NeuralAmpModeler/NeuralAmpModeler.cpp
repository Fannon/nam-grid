#include "NoisePrintState.h"
#include <algorithm> // std::clamp, std::min
#include <cmath> // pow
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

using namespace iplug;
using namespace igraphics;

const double kDCBlockerFrequency = 5.0;

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
  GetParam(kParallelBlend)->InitDouble("Parallel Blend", 50., 0., 100., .1, "% B");
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
  {
    const auto name = GridSlotName(slot);
    GetParam(SlotDynamicsParam(slot, 0))->InitDouble((name + " Gate Threshold").c_str(), -55., -90., -10., .1, "dB");
    GetParam(SlotDynamicsParam(slot, 1))->InitDouble((name + " Gate Release").c_str(), 120., 20., 500., 1., "ms");
    GetParam(SlotDynamicsParam(slot, 2))->InitDouble((name + " Gate Depth").c_str(), 60., 0., 80., .1, "dB");
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
      if (i == uTempoDivision)
      {
        param->InitEnum((name + " " + p.name).c_str(), 4, 9);
        for (int d = 0; d < 9; ++d)
          param->SetDisplayText(d, grid_tempo::names[d]);
      }
      else if (i == uTempoSync || i == uUtilityInvertL || i == uUtilityInvertR || i == uDelayPingPong
               || i == uReverbStereo || i == uNoiseLearn)
        param->InitBool((name + " " + p.name).c_str(), p.initial >= .5);
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
  const auto librarySettings = nam_library::ReadSettings(CaptureSettingsFile());
  mRescanOnStartup = librarySettings.rescanOnStartup;
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
    ui->AttachControl(new ITextControl(IRECT(24, 10, 220, 52), PLUG_NAME, grid_ui::Text(25.f)));
    ui->AttachControl(
      new ITextControl(IRECT(225, 15, 570, 49), "Pre / parallel A+B / Post", grid_ui::Text(12.f, grid_ui::muted)));
    ui->AttachControl(new grid_ui::Knob(IRECT(590, 2, 660, 67), kParallelBlend, "Blend A / B"));
    ui->AttachControl(new grid_ui::Knob(IRECT(680, 2, 750, 67), kOutputLevel, "Master"));
    ui->AttachControl(new grid_ui::Button(
      IRECT(862, 22, 976, 50),
      [](IControl* caller) {
        caller->GetUI()->GetControlWithTag(kCtrlTagTuner)->Hide(true);
        caller->GetUI()->GetControlWithTag(kCtrlTagSettingsBox)->Hide(false);
      },
      "Library & settings"));
    ui->AttachControl(new grid_ui::Button(
      IRECT(770, 22, 850, 50),
      [](IControl* caller) {
        caller->GetUI()->GetControlWithTag(kCtrlTagSettingsBox)->Hide(true);
        caller->GetUI()->GetControlWithTag(kCtrlTagTuner)->Hide(false);
      },
      "Tuner"));
    ui->AttachControl(new NAMCaptureSearchControl(IRECT(88, 76, 570, 104)), kCtrlTagCaptureSearch);
    ui->AttachControl(new grid_ui::Button(
                        IRECT(578, 76, 638, 104), [this](IControl*) { SetCaptureFilter({}); }, "Clear"),
                      kCtrlTagClearFilter);
    ui->AttachControl(new NAMGearFilterControl(IRECT(646, 76, 803, 104), compact), kCtrlTagGearFilter);
    ui->AttachControl(new grid_ui::Button(
                        IRECT(816, 76, 976, 104),
                        [this](IControl*) {
                          if (GetCaptureFolders().empty())
                            GetUI()->GetControlWithTag(kCtrlTagSettingsBox)->Hide(false);
                          else
                            RescanCaptureFolder();
                        },
                        GetLibraryMatchStatus().c_str()),
                      kCtrlTagCaptureStatus);
    ui->AttachControl(new grid_ui::Wiring(b));
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    {
      const float left = 88.f + 220.f * (slot % 4);
      const float top = 116.f + 104.f * (slot / 4);
      ui->AttachControl(new NAMGridSlotControl(IRECT(left, top, left + 208.f, top + 88.f), slot),
                        kCtrlTagGridSlotBase + static_cast<int>(slot));
    }
    for (int row = 0; row < 4; ++row)
      ui->AttachControl(new ITextControl(IRECT(12, 116.f + 104.f * row, 63, 139.f + 104.f * row),
                                         row == 0   ? "Pre"
                                         : row == 1 ? "A"
                                         : row == 2 ? "B"
                                                    : "Post",
                                         grid_ui::Text(11.f, grid_ui::muted)));
    ui->AttachControl(new ITextControl(IRECT(88, 523, 950, 546),
                                       "Select a module to edit below. Double-click to choose. The dot toggles bypass.",
                                       grid_ui::Text(12.f, grid_ui::muted)));
    // Flat input/output meters retain the upstream sender protocol, without bitmap skins.
    const auto meterStyle =
      compact.WithShowValue(false).WithColor(kX1, grid_ui::accent).WithColor(kFG, grid_ui::accent);
    ui->AttachControl(new IVPeakAvgMeterControl<2>(IRECT(45, 146, 50, 174), "", meterStyle), kCtrlTagInputMeter);
    ui->AttachControl(new IVPeakAvgMeterControl<2>(IRECT(963, 458, 967, 486), "", meterStyle), kCtrlTagOutputMeter);
    ui->AttachControl(new NAMGridSettingsControl(b), kCtrlTagSettingsBox)->Hide(true);
    ui->AttachControl(new NAMGridTunerControl(b), kCtrlTagTuner)->Hide(true);
    ui->SetKeyHandlerFunc([ui](const IKeyPress& key, bool keyUp) {
      auto* tuner = ui->GetControlWithTag(kCtrlTagTuner);
      if (!keyUp && key.VK == kVK_ESCAPE && !tuner->IsHidden())
      {
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
  };
}

NeuralAmpModeler::~NeuralAmpModeler()
{ _DeallocateIOPointers(); }

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
  // Host tail updates belong to audio (CLAP dispatches its notification there).
  const int effectTail = static_cast<int>(std::ceil(GetSampleRate() * mEffectTailSeconds.load()));
  if (GetTailSize() != effectTail)
    SetTailSize(effectTail);

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
  }
  // Preserve the former Amp's calibration/normalization controls on Pre 2 for migrated states.
  controls[1].calibrate = GetParam(kCalibrateInput)->Bool();
  controls[1].calibrationDB = GetParam(kInputCalibrationLevel)->Value();
  controls[1].outputMode = GetParam(kOutputMode)->Int();
  controls[1].slim = GetParam(kSlim)->Value();
  mGrid.Process(mOutputPointers[0], nFrames, controls, GetParam(kParallelBlend)->Value() * .01, mOutputPointers[1]);
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
  mMasterGain = GetParam(kOutputLevel)->DBToAmp();
  mMasterCoefficient = std::exp(-1. / (.005 * sampleRate));
  _PrepareBuffers(kNumChannelsInternal, maxBlockSize);
  for (auto& channel : mInputArray)
    std::fill(channel.begin(), channel.end(), 0.);
  for (auto& channel : mOutputArray)
    std::fill(channel.begin(), channel.end(), 0.);

  SetTailSize(static_cast<int>(std::ceil(sampleRate * mEffectTailSeconds.load())));
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
  mCaptureFolders = nam_library::NormalizeFolders(folders);
  mCaptureFolder = mCaptureFolders.empty() ? std::string{} : mCaptureFolders.front();
  if (fromUI || mRescanOnStartup)
    RescanCaptureFolder();
  else
  {
    mLibraryWorker.Request(std::vector<std::string>{}); // Cancel an older request without indexing the roots.
    mLibrary = std::make_shared<nam_library::Snapshot>();
    mLibraryScanning = mLibraryIndexed = false;
    _UpdateLibraryControls();
  }
  if (fromUI)
  {
    mLibraryDefaultSaveFailed = !nam_library::WriteSettings(CaptureSettingsFile(), {mCaptureFolders, mRescanOnStartup});
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
  if (slot >= FixedNAMGrid::kSlots || !IsValidSlotEffect(effect))
    return;
  _ClearModel(slot, effect);
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

void NeuralAmpModeler::SetRescanOnStartup(bool enabled)
{
  mRescanOnStartup = enabled;
  mLibraryDefaultSaveFailed = !nam_library::WriteSettings(CaptureSettingsFile(), {mCaptureFolders, mRescanOnStartup});
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
  for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    if (CanToggleSlotQuality(slot) && GetParam(SlotFullParam(slot))->Value() != mPreparedQuality[slot])
    {
      if (!_StageSlot(slot, SlotEffect::NAM, mSlotPaths[slot].Get()).empty())
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
  if (dataPos < 0 || length < 0 || length > chunk.Size() - dataPos)
    return -1;
  return chunk.GetStr(value, pos);
}

bool NeuralAmpModeler::SerializeState(IByteChunk& chunk) const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  nlohmann::json config;
  config["schema"] = noise_print::experimentSchema;
  config["DrivePath"] = mDrivePath.Get();
  config["NAMPath"] = mNAMPath.Get();
  config["IRPath"] = mIRPath.Get();
  config["CaptureFolder"] = mCaptureFolder;
  config["CaptureFolders"] = mCaptureFolders;
  config["SlotPaths"] = nlohmann::json::array();
  for (const auto& path : mSlotPaths)
    config["SlotPaths"].push_back(path.Get());
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
                  && schemaValue != 9 && schemaValue != 1001 && schemaValue != noise_print::experimentSchema)
               : schemaValue != 1)
        return -1;
      const int schema = schemaValue.get<int>();
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
        if (!config.contains(name) || !config[name].is_string() || config[name].get<std::string>().size() > 32768)
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
                || effect > (schema == 3   ? 4
                             : schema == 6 ? static_cast<int>(SlotEffect::Tape)
                             : schema == 7 ? static_cast<int>(SlotEffect::TubeBoost)
                             : schema == 8 ? static_cast<int>(SlotEffect::Utility)
                             : schema == 9 ? static_cast<int>(SlotEffect::Wah)
                                           : static_cast<int>(SlotEffect::NoisePrint))
                || !IsValidSlotEffect(static_cast<SlotEffect>(effect.get<int>()))
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
        if (grid && schema == 1001 && i >= kUtilityBase && i < kTunerReference
            && (i - kUtilityBase) % kUtilityControls == uNoiseGateThreshold)
          config[name] = GetParam(i)->GetDefault(); // First Noise Print experiment had no gate.
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
        if (grid && schema >= 6 && schema <= 9
            && ((i >= kTunerReference && schema < 8)
                || (i >= kUtilityBase && i < kTunerReference
                    && (i - kUtilityBase) % kUtilityControls >= (schema == 6   ? 30
                                                                 : schema == 7 ? 41
                                                                 : schema == 8 ? 48
                                                                               : 61))))
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
        if (!config.contains(name) || !config[name].is_number() || !std::isfinite(config[name].get<double>()))
          return -1;
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
      else if (schema == 1001 || schema == noise_print::experimentSchema)
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
  // IParam values are atomic. DSP reads them at block boundaries; UI never mutates live DSP.
}

void NeuralAmpModeler::OnParamChangeUI(int paramIdx, EParamSource source)
{
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

std::string NeuralAmpModeler::_StageSlot(size_t slot, SlotEffect effect, const std::string& path)
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
  mSlotEffects[slot] = config.effect;
  // Serial bounds add; parallel lanes use their maximum. Limit the host's
  // automatic render/suspend budget to 60 s, including automation to long tails.
  // This is a render policy, not a promise that extreme echoes decay by then;
  // continuous playback retains their natural decay (see docs/MODULES.md).
  std::array<double, 4> rowTails{};
  for (size_t i = 0; i < mSlotEffects.size(); ++i)
    rowTails[i / 4] += IsDelay(mSlotEffects[i]) ? 560. : IsReverb(mSlotEffects[i]) ? 30. : 0.;
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
  for (size_t c = 0; c < nChansOut; ++c)
  {
    if (!nChansIn)
      std::fill_n(mInputArray[c].data(), nFrames, 0.);
    else
      std::copy_n(inputs[std::min(c, nChansIn - 1)], nFrames, mInputArray[c].data());
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

void NeuralAmpModeler::_UpdateControlsFromModel()
{
  if (auto* ui = GetUI())
  {
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
      if (auto* control = ui->GetControlWithTag(kCtrlTagGridSlotBase + static_cast<int>(slot)))
        control->OnMsgFromDelegate(kMsgTagLoadedModel, 0, nullptr);
  }
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
  mInputSender.ProcessBlock(
    inputPointer, static_cast<int>(nFrames), kCtrlTagInputMeter, static_cast<int>(std::min(size_t{2}, nChansIn)));
  mOutputSender.ProcessBlock(
    outputPointer, static_cast<int>(nFrames), kCtrlTagOutputMeter, static_cast<int>(std::min(size_t{2}, nChansOut)));
}

// HACK
#include "Unserialization.cpp"

void NeuralAmpModeler::_ClearModel(size_t slot, SlotEffect effect)
{ _StageSlot(slot, effect, ""); }
