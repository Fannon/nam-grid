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

// Styles
const IVColorSpec colorSpec{
  DEFAULT_BGCOLOR, // Background
  PluginColors::NAM_THEMECOLOR, // Foreground
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.3f), // Pressed
  PluginColors::NAM_THEMECOLOR.WithOpacity(0.4f), // Frame
  PluginColors::MOUSEOVER, // Highlight
  DEFAULT_SHCOLOR, // Shadow
  PluginColors::NAM_THEMECOLOR, // Extra 1
  COLOR_RED, // Extra 2 --> color for clipping in meters
  PluginColors::NAM_THEMECOLOR.WithContrast(0.1f), // Extra 3
};

const IVStyle style =
  IVStyle{true, // Show label
          true, // Show value
          colorSpec,
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Middle, PluginColors::NAM_THEMEFONTCOLOR}, // Knob label text5
          {DEFAULT_TEXT_SIZE + 3.f, EVAlign::Bottom, PluginColors::NAM_THEMEFONTCOLOR}, // Knob value text
          DEFAULT_HIDE_CURSOR,
          DEFAULT_DRAW_FRAME,
          false,
          DEFAULT_EMBOSS,
          0.2f,
          2.f,
          DEFAULT_SHADOW_OFFSET,
          DEFAULT_WIDGET_FRAC,
          DEFAULT_WIDGET_ANGLE};
const IVStyle titleStyle =
  DEFAULT_STYLE.WithValueText(IText(30, COLOR_WHITE, "Michroma-Regular")).WithDrawFrame(false).WithShadowOffset(2.f);
const IVStyle radioButtonStyle =
  style
    .WithColor(EVColor::kON, PluginColors::NAM_THEMECOLOR) // Pressed buttons and their labels
    .WithColor(EVColor::kOFF, PluginColors::NAM_THEMECOLOR.WithOpacity(0.1f)) // Unpressed buttons
    .WithColor(EVColor::kX1, PluginColors::NAM_THEMECOLOR.WithOpacity(0.6f)); // Unpressed buttons' labels

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
  GetParam(kInputLevel)->InitGain("Input", 0.0, -20.0, 20.0, 0.1);
  GetParam(kToneBass)->InitDouble("Bass", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneMid)->InitDouble("Middle", 5.0, 0.0, 10.0, 0.1);
  GetParam(kToneTreble)->InitDouble("Treble", 5.0, 0.0, 10.0, 0.1);
  GetParam(kOutputLevel)->InitGain("Output", 0.0, -40.0, 40.0, 0.1);
  GetParam(kNoiseGateThreshold)->InitGain("Threshold", -80.0, -100.0, 0.0, 0.1);
  GetParam(kNoiseGateActive)->InitBool("NoiseGateActive", true);
  GetParam(kEQActive)->InitBool("ToneStack", true);
  GetParam(kOutputMode)->InitEnum("OutputMode", 0, {"Raw", "Normalized", "Calibrated"}); // TODO DRY w/ control
  GetParam(kIRToggle)->InitBool("IRToggle", true);
  GetParam(kCalibrateInput)->InitBool(kCalibrateInputParamName.c_str(), kDefaultCalibrateInput);
  GetParam(kInputCalibrationLevel)
    ->InitDouble(kInputCalibrationLevelParamName.c_str(), kDefaultInputCalibrationLevel, -60.0, 60.0, 0.1, "dBu");
  GetParam(kDriveInput)->InitGain("Drive Input", 0., -40., 40., .1);
  GetParam(kDriveTrim)->InitGain("Drive Trim", 0., -40., 40., .1);
  GetParam(kDriveActive)->InitBool("Drive Enabled", true);
  GetParam(kAmpTrim)->InitGain("Amp Trim", 0., -40., 40., .1);
  GetParam(kAmpActive)->InitBool("Amp Enabled", true);
  GetParam(kSlim)->InitDouble("Slim", 0.0, 0.0, 1.0, 0.01);
  for (size_t slot = 2; slot < FixedNAMGrid::kSlots; ++slot)
  {
    const auto name = GridSlotName(slot);
    GetParam(SlotInputParam(slot))->InitGain((name + " In").c_str(), 0., -40., 40., .1);
    GetParam(SlotTrimParam(slot))->InitGain((name + " Trim").c_str(), 0., -40., 40., .1);
    GetParam(SlotActiveParam(slot))->InitBool((name + " Enabled").c_str(), true);
  }
  GetParam(kParallelBlend)->InitDouble("Parallel Blend", 50., 0., 100., .1, "% B");
  const auto defaultFolder = nam_library::ReadFolderSetting(CaptureSettingsFile());
  if (!defaultFolder.empty())
    SetCaptureFolder(defaultFolder.c_str());

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
    ui->LoadFont("Michroma-Regular", MICHROMA_FN);
    const auto b = ui->GetBounds();
    ui->AttachPanelBackground(IColor(255, 18, 19, 22));
    ui->AttachControl(new ITextControl(
      IRECT(35, 12, 900, 49), "NAM GRID EXPERIMENT", IText(24.f, COLOR_WHITE, "Michroma-Regular", EAlign::Near)));
    const auto compact = style.WithLabelText(IText(12.f, COLOR_WHITE, "Roboto-Regular"))
                           .WithValueText(IText(12.f, COLOR_WHITE, "Roboto-Regular"));
    ui->AttachControl(new IVKnobControl(IRECT(40, 55, 120, 133), kNoiseGateThreshold, "Gate", compact));
    ui->AttachControl(new IVToggleControl(IRECT(125, 72, 185, 108), kNoiseGateActive, "Gate", compact));
    ui->AttachControl(new IVKnobControl(IRECT(210, 55, 290, 133), kToneBass, "Bass", compact), -1, "EQ_KNOBS");
    ui->AttachControl(new IVKnobControl(IRECT(295, 55, 375, 133), kToneMid, "Mid", compact), -1, "EQ_KNOBS");
    ui->AttachControl(new IVKnobControl(IRECT(380, 55, 460, 133), kToneTreble, "Treble", compact), -1, "EQ_KNOBS");
    ui->AttachControl(new IVToggleControl(IRECT(465, 72, 525, 108), kEQActive, "EQ", compact));
    ui->AttachControl(new ITextControl(
      IRECT(550, 65, 825, 124), "4 serial > 2 x 4 parallel > 4 serial", IText(14.f, COLOR_WHITE, "Roboto-Regular")));
    ui->AttachControl(new IVKnobControl(IRECT(860, 55, 940, 133), kOutputLevel, "Master", compact));
    ui->AttachControl(new NAMCaptureSearchControl(IRECT(40, 145, 350, 170)));
    ui->AttachControl(new NAMGearFilterControl(IRECT(360, 145, 495, 170), compact));
    const auto fileSVG = ui->LoadSVG(FILE_FN), crossSVG = ui->LoadSVG(CLOSE_BUTTON_FN);
    const auto leftSVG = ui->LoadSVG(LEFT_ARROW_FN), rightSVG = ui->LoadSVG(RIGHT_ARROW_FN);
    const auto globeSVG = ui->LoadSVG(GLOBE_ICON_FN);
    auto loadIR = [this](const WDL_String& file, const WDL_String&) {
      if (file.GetLength())
      {
        const auto result = _StageIR(file);
        if (result != dsp::wav::LoadReturnCode::SUCCESS)
          _ShowMessageBox(GetUI(), dsp::wav::GetMsgForLoadReturnCode(result).c_str(), "IR load failed", kMB_OK);
      }
    };
    ui->AttachControl(
      new NAMFileBrowserControl(IRECT(515, 140, 815, 174), kMsgTagClearIR, "Select global IR...", "wav", loadIR,
                                compact, fileSVG, crossSVG, leftSVG, rightSVG, ui->LoadBitmap(FILEBACKGROUND_FN),
                                globeSVG, "IRs", "https://www.neuralampmodeler.com"),
      kCtrlTagIRFileBrowser);
    ui->AttachControl(new IVToggleControl(IRECT(830, 145, 935, 170), kIRToggle, "Global IR", compact));
    ui->AttachControl(new NAMGridWiringControl(b));
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
    {
      const float left = 90.f + 215.f * (slot % 4);
      const float top = 190.f + 135.f * (slot / 4);
      ui->AttachControl(new NAMGridSlotControl(IRECT(left, top, left + 205.f, top + 120.f), slot, compact),
                        kCtrlTagGridSlotBase + static_cast<int>(slot));
    }
    for (int row = 0; row < 4; ++row)
      ui->AttachControl(new ITextControl(IRECT(28, 225.f + 135.f * row, 85, 270.f + 135.f * row),
                                         row == 0   ? "INPUT"
                                         : row == 1 ? "LANE A"
                                         : row == 2 ? "LANE B"
                                                    : "OUTPUT",
                                         IText(12.f, COLOR_WHITE, "Roboto-Regular")));
    ui->AttachControl(new ITextControl(IRECT(70, 730, 400, 778), "Input row > split A/B > blend > output row",
                                       IText(13.f, COLOR_WHITE, "Roboto-Regular")));
    ui->AttachControl(new IVKnobControl(IRECT(425, 720, 525, 795), kParallelBlend, "Blend A / B", compact));
    ui->AttachControl(new ITextControl(IRECT(550, 730, 940, 778), "0% B = A only | 50% = equal | 100% B = B only",
                                       IText(12.f, COLOR_WHITE, "Roboto-Regular")));
    const auto meter = ui->LoadBitmap(METERBACKGROUND_FN);
    ui->AttachControl(new NAMMeterControl(IRECT(5, 195, 24, 715), meter, compact), kCtrlTagInputMeter);
    ui->AttachControl(new NAMMeterControl(IRECT(974, 195, 993, 715), meter, compact), kCtrlTagOutputMeter);
    ui->AttachControl(new NAMCircleButtonControl(
      CornerButtonArea(b),
      [](IControl* caller) {
        caller->GetUI()->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
      },
      ui->LoadSVG(GEAR_FN)));
    ui->AttachControl(
        new NAMSettingsPageControl(b, ui->LoadBitmap(BACKGROUND_FN), ui->LoadBitmap(INPUTLEVELBACKGROUND_FN),
                                   ui->LoadBitmap(SLIDESWITCHHANDLE_FN), crossSVG, style, radioButtonStyle),
        kCtrlTagSettingsBox)
      ->Hide(true);
    ui->ForAllControlsFunc([](IControl* control) { control->SetDirty(false); });
  };
}

NeuralAmpModeler::~NeuralAmpModeler()
{ _DeallocateIOPointers(); }

void NeuralAmpModeler::ProcessBlock(iplug::sample** inputs, iplug::sample** outputs, int nFrames)
{
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
  // Input is collapsed to mono in preparation for the NAM.
  _ProcessInput(inputs, numFrames, numChannelsExternalIn, numChannelsInternal);
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

  std::copy_n(triggerOutput[0], nFrames, mOutputPointers[0]);
  std::array<FixedNAMGrid::Controls, FixedNAMGrid::kSlots> controls;
  for (size_t slot = 0; slot < controls.size(); ++slot)
  {
    auto& c = controls[slot];
    c.enabled = GetParam(SlotActiveParam(slot))->Bool();
    c.inputDB = GetParam(SlotInputParam(slot))->Value();
    c.trimDB = GetParam(SlotTrimParam(slot))->Value();
  }
  // Preserve the former Amp's calibration/normalization controls on IN 2 for migrated states.
  controls[1].calibrate = GetParam(kCalibrateInput)->Bool();
  controls[1].calibrationDB = GetParam(kInputCalibrationLevel)->Value();
  controls[1].outputMode = GetParam(kOutputMode)->Int();
  controls[1].slim = GetParam(kSlim)->Value();
  mGrid.Process(mOutputPointers[0], nFrames, controls, GetParam(kParallelBlend)->Value() * .01);
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
  auto* cabinet = mCabinet.Active();
  if (cabinet && cabinet->ir && GetParam(kIRToggle)->Bool())
    irPointers = cabinet->ir->Process(toneStackOutPointers, numChannelsInternal, numFrames);
  const bool switchingIR = mCabinet.HasPending();
  const double switchStep = 1. / (sampleRate * .005);
  for (size_t i = 0; i < numFrames; ++i)
  {
    mIRSwitchGain = switchingIR ? std::max(0., mIRSwitchGain - switchStep) : std::min(1., mIRSwitchGain + switchStep);
    irPointers[0][i] *= mIRSwitchGain;
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
  // This is where we exit mono for whatever the output requires.
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
  _PrepareBuffers(1, maxBlockSize);

  // Tail is because the HPF DC blocker has a decay.
  // 10 cycles should be enough to pass the VST3 tests checking tail behavior.
  // I'm ignoring the model & IR, but it's not the end of the world.
  const int tailCycles = 10;
  SetTailSize(tailCycles * (int)(sampleRate / kDCBlockerFrequency));
  mInputSender.Reset(sampleRate);
  mOutputSender.Reset(sampleRate);
  // If there is a model or IR loaded, they need to be checked for resampling.
  _ResetModelAndIR(sampleRate, maxBlockSize);
  mToneStack->Reset(sampleRate, maxBlockSize);
  _UpdateLatency();
  // Prime all reused DSP paths, including currently bypassed ones, outside ProcessBlock.
  mNoiseGateTrigger.SetSampleRate(sampleRate);
  mNoiseGateTrigger.SetParams(dsp::noise_gate::TriggerParams(.01, -80., .1, .005, .01, .05));
  mNoiseGateTrigger.Process(mInputPointers, 1, maxBlockSize);
  mNoiseGateGain.Process(mInputPointers, 1, maxBlockSize);
  mToneStack->Process(mInputPointers, 1, maxBlockSize);
  mHighPass.SetParams(recursive_linear_filter::HighPassParams(sampleRate, kDCBlockerFrequency));
  mHighPass.Process(mInputPointers, 1, maxBlockSize);
}

void NeuralAmpModeler::SetCaptureFolder(const char* folder, bool fromUI)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  mCaptureFolder = folder;
  RescanCaptureFolder();
  if (fromUI)
  {
    mLibraryDefaultSaveFailed = !nam_library::WriteFolderSetting(CaptureSettingsFile(), mCaptureFolder);
    _UpdateLibraryControls();
    _NotifyHostStateChanged();
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

std::string NeuralAmpModeler::LoadSlot(size_t slot, const char* path)
{
  const auto error = _StageModel(WDL_String(path), slot);
  if (error.empty())
    _NotifyHostStateChanged();
  return error;
}

void NeuralAmpModeler::ClearSlot(size_t slot)
{
  if (slot < FixedNAMGrid::kSlots)
  {
    _ClearModel(slot);
    _NotifyHostStateChanged();
  }
}

void NeuralAmpModeler::RescanCaptureFolder()
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  mLibrary = std::make_shared<nam_library::Snapshot>();
  mLibraryScanning = !mCaptureFolder.empty();
  mLibraryWorker.Request(mCaptureFolder);
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
}

std::string NeuralAmpModeler::GetCaptureStatus() const
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (mCaptureFolder.empty())
    return "Choose a folder for nested NAM slot menus.";
  if (mLibraryScanning)
    return "Scanning NAM captures...";
  if (mLibraryDefaultSaveFailed)
    return "Folder saved in project; unable to save the default for new instances.";
  if (!mLibrary->error.empty())
    return mLibrary->error;
  return std::to_string(mLibrary->captures.size()) + " NAM captures; " + std::to_string(mLibrary->unreadable)
         + " unreadable. Gear inferred from metadata or explicit path hints.";
}

void NeuralAmpModeler::_UpdateLibraryControls()
{
  if (auto* ui = GetUI())
  {
    if (auto* control = ui->GetControlWithTag(kCtrlTagCaptureFolder))
    {
      auto label = mCaptureFolder.empty() ? std::string("Choose capture folder...") : mCaptureFolder;
      if (label.size() > 64)
        label = "..." + label.substr(label.size() - 61);
      control->As<IVButtonControl>()->SetLabelStr(label.c_str());
      control->SetTooltip(mCaptureFolder.c_str());
      control->SetDirty(false);
    }
    if (auto* control = ui->GetControlWithTag(kCtrlTagCaptureStatus))
      control->As<ITextControl>()->SetStr(GetCaptureStatus().c_str());
  }
}

void NeuralAmpModeler::OnIdle()
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (auto result = mLibraryWorker.TakeResult())
  {
    mLibrary = std::move(result);
    mLibraryScanning = false;
    _UpdateLibraryControls();
  }
  for (auto& stage : mGrid.stages)
    stage.Collect();
  mCabinet.Collect();
  mInputSender.TransmitData(*this);
  mOutputSender.TransmitData(*this);
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
  config["schema"] = 2;
  config["DrivePath"] = mDrivePath.Get();
  config["NAMPath"] = mNAMPath.Get();
  config["IRPath"] = mIRPath.Get();
  config["CaptureFolder"] = mCaptureFolder;
  config["SlotPaths"] = nlohmann::json::array();
  for (const auto& path : mSlotPaths)
    config["SlotPaths"].push_back(path.Get());
  for (int i = 0; i < kNumParams; ++i)
    config[GetParam(i)->GetName()] = GetParam(i)->Value();
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
      if (!config.is_object() || config.value("schema", 0) != (grid ? 2 : 1))
        return -1;
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
        if (config["SlotPaths"][0] != config["DrivePath"] || config["SlotPaths"][1] != config["NAMPath"])
          return -1;
      }
      else
      {
        config.erase("SlotPaths");
        for (int i = kDualNumParams; i < kNumParams; ++i)
          config.erase(GetParam(i)->GetName());
      }
      for (int i = 0; i < (grid ? kNumParams : kDualNumParams); ++i)
      {
        const char* name = GetParam(i)->GetName();
        if (!config.contains(name) || !config[name].is_number() || !std::isfinite(config[name].get<double>()))
          return -1;
      }
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
  if (auto pGraphics = GetUI())
  {
    bool active = GetParam(paramIdx)->Bool();

    switch (paramIdx)
    {
      case kNoiseGateActive: pGraphics->GetControlWithParamIdx(kNoiseGateThreshold)->SetDisabled(!active); break;
      case kEQActive:
        pGraphics->ForControlInGroup("EQ_KNOBS", [active](IControl* pControl) { pControl->SetDisabled(!active); });
        break;
      case kIRToggle: pGraphics->GetControlWithTag(kCtrlTagIRFileBrowser)->SetDisabled(!active); break;
      default: break;
    }
  }
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
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (slot >= FixedNAMGrid::kSlots)
    return "Invalid grid slot";
  const int tag = kCtrlTagGridSlotBase + static_cast<int>(slot);
  try
  {
    auto model = nam::get_dsp(std::filesystem::u8path(modelPath.Get()));
    if (model->NumInputChannels() != 1 || model->NumOutputChannels() != 1)
      throw std::runtime_error("NAM slots require a mono input and output model");
    auto prepared = std::make_unique<NAMStage::Prepared>();
    prepared->model = std::make_unique<ResamplingNAM>(std::move(model), mSampleRatePrepared);
    prepared->Reset(mSampleRatePrepared, mMaxBlockPrepared);
    if (prepared->latency > FixedNAMGrid::kMaxLaneDelay / 4)
      throw std::runtime_error("Model conversion delay exceeds the grid's supported range");
    const auto info = prepared->Info();
    const bool latencyChanged = prepared->latency != mRequestedLatencies[slot];
    mRequestedLatencies[slot] = prepared->latency;
#ifdef CLAP_API
    mGrid.stages[slot].Publish(std::move(prepared), latencyChanged);
#else
    mGrid.stages[slot].Publish(std::move(prepared));
#endif
    if (slot == 0)
    {
      mDrivePath = modelPath;
      mDriveLoaded = true;
    }
    else if (slot == 1)
    {
      mNAMPath = modelPath;
      mAmpInfo = info;
    }
    mSlotPaths[slot] = modelPath;
    mSlotInfo[slot] = info;
    _UpdateLatency(latencyChanged);
    SendControlMsgFromDelegate(tag, kMsgTagLoadedModel, modelPath.GetLength() + 1, modelPath.Get());
    if (slot == 1)
      _UpdateControlsFromModel();
  }
  catch (const std::exception& error)
  {
    SendControlMsgFromDelegate(tag, kMsgTagLoadFailed);
    return error.what();
  }
  return "";
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
  // We'll assume that the main processing is mono for now. We'll handle dual amps later.
  if (nChansOut != 1)
  {
    std::stringstream ss;
    ss << "Expected mono output, but " << nChansOut << " output channels are requested!";
    throw std::runtime_error(ss.str());
  }

  // On the standalone, we can probably assume that the user has plugged into only one input and they expect it to be
  // carried straight through. Don't apply any division over nChansIn because we're just "catching anything out there."
  // However, in a DAW, it's probably something providing stereo, and we want to take the average in order to avoid
  // doubling the loudness. (This would change w/ double mono processing)
  double gain = 1.;
#ifndef APP_API
  if (nChansIn)
    gain /= (float)nChansIn;
#endif
  std::fill_n(mInputArray[0].data(), nFrames, 0.);
  // Mono input preparation
  for (size_t c = 0; c < nChansIn; c++)
    for (size_t s = 0; s < nFrames; s++)
      if (c == 0)
        mInputArray[0][s] = gain * inputs[c][s];
      else
        mInputArray[0][s] += gain * inputs[c][s];
}

void NeuralAmpModeler::_ProcessOutput(iplug::sample** inputs, iplug::sample** outputs, const size_t nFrames,
                                      const size_t nChansIn, const size_t nChansOut)
{
  const double gain = GetParam(kOutputLevel)->DBToAmp();
  // Assume _PrepareBuffers() was already called
  if (nChansIn != 1)
    throw std::runtime_error("Plugin is supposed to process in mono.");
  // Broadcast the internal mono stream to all output channels.
  const size_t cin = 0;
  for (auto cout = 0; cout < nChansOut; cout++)
    for (auto s = 0; s < nFrames; s++)
#ifdef APP_API // Ensure valid output to interface
      outputs[cout][s] = std::clamp(gain * inputs[cin][s], -1.0, 1.0);
#else // In a DAW, other things may come next and should be able to handle large
      // values.
      outputs[cout][s] = gain * inputs[cin][s];
#endif
}

void NeuralAmpModeler::_UpdateControlsFromModel()
{
  if (auto* ui = GetUI())
  {
    ModelInfo info;
    info.sampleRate.known = mAmpInfo.loaded;
    info.sampleRate.value = mAmpInfo.rate;
    info.inputCalibrationLevel.known = mAmpInfo.hasInput;
    info.inputCalibrationLevel.value = mAmpInfo.input;
    info.outputCalibrationLevel.known = mAmpInfo.hasOutput;
    info.outputCalibrationLevel.value = mAmpInfo.output;
    auto* settings = static_cast<NAMSettingsPageControl*>(ui->GetControlWithTag(kCtrlTagSettingsBox));
    if (mAmpInfo.loaded)
      settings->SetModelInfo(info);
    else
      settings->ClearModelInfo();
    ui->GetControlWithTag(kCtrlTagCalibrateInput)->SetDisabled(!mAmpInfo.hasInput);
    ui->GetControlWithTag(kCtrlTagInputCalibrationLevel)->SetDisabled(!mAmpInfo.hasInput);
    auto* mode = static_cast<OutputModeControl*>(ui->GetControlWithTag(kCtrlTagOutputMode));
    mode->SetNormalizedDisable(!mAmpInfo.hasLoudness);
    mode->SetCalibratedDisable(!mAmpInfo.hasOutput);
    if (auto* control = ui->GetControlWithTag(kCtrlTagSlimmableIcon))
      control->Hide(!mAmpInfo.slimmable);
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
  // Right now, we didn't specify MAXNC when we initialized these, so it's 1.
  const int nChansHack = 1;
  mInputSender.ProcessBlock(inputPointer, (int)nFrames, kCtrlTagInputMeter, nChansHack);
  mOutputSender.ProcessBlock(outputPointer, (int)nFrames, kCtrlTagOutputMeter, nChansHack);
}

// HACK
#include "Unserialization.cpp"

void NeuralAmpModeler::_ClearModel(size_t slot)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  if (slot >= FixedNAMGrid::kSlots)
    return;
  const bool latencyChanged = mRequestedLatencies[slot] != 0;
  mRequestedLatencies[slot] = 0;
#ifdef CLAP_API
  mGrid.stages[slot].Publish(std::make_unique<NAMStage::Prepared>(), latencyChanged);
#else
  mGrid.stages[slot].Publish(std::make_unique<NAMStage::Prepared>());
#endif
  if (slot == 0)
  {
    mDrivePath.Set("");
    mDriveLoaded = false;
  }
  else if (slot == 1)
  {
    mNAMPath.Set("");
    mAmpInfo = {};
  }
  mSlotPaths[slot].Set("");
  mSlotInfo[slot] = {};
  if (slot == 1)
    _UpdateControlsFromModel();
  _UpdateLatency(latencyChanged);
  SendControlMsgFromDelegate(kCtrlTagGridSlotBase + static_cast<int>(slot), kMsgTagLoadedModel);
}
