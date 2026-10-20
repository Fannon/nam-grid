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
  return support.GetLength() ? std::filesystem::u8path(support.Get()) / BUNDLE_NAME / "library.json"
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

  mLayoutFunc = [&](IGraphics* pGraphics) {
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachTextEntryControl();
    pGraphics->EnableMouseOver(true);
    pGraphics->EnableTooltips(true);
    pGraphics->EnableMultiTouch(true);

    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);
    pGraphics->LoadFont("Michroma-Regular", MICHROMA_FN);

    const auto gearSVG = pGraphics->LoadSVG(GEAR_FN);
    const auto fileSVG = pGraphics->LoadSVG(FILE_FN);
    const auto globeSVG = pGraphics->LoadSVG(GLOBE_ICON_FN);
    const auto crossSVG = pGraphics->LoadSVG(CLOSE_BUTTON_FN);
    const auto rightArrowSVG = pGraphics->LoadSVG(RIGHT_ARROW_FN);
    const auto leftArrowSVG = pGraphics->LoadSVG(LEFT_ARROW_FN);
    const auto modelIconSVG = pGraphics->LoadSVG(MODEL_ICON_FN);
    const auto irIconOnSVG = pGraphics->LoadSVG(IR_ICON_ON_FN);
    const auto irIconOffSVG = pGraphics->LoadSVG(IR_ICON_OFF_FN);
    const auto slimIconSVG = pGraphics->LoadSVG(SLIMMABLE_ICON_FN);

    const auto backgroundBitmap = pGraphics->LoadBitmap(BACKGROUND_FN);
    const auto fileBackgroundBitmap = pGraphics->LoadBitmap(FILEBACKGROUND_FN);
    const auto inputLevelBackgroundBitmap = pGraphics->LoadBitmap(INPUTLEVELBACKGROUND_FN);
    const auto linesBitmap = pGraphics->LoadBitmap(LINES_FN);
    const auto knobBackgroundBitmap = pGraphics->LoadBitmap(KNOBBACKGROUND_FN);
    const auto switchHandleBitmap = pGraphics->LoadBitmap(SLIDESWITCHHANDLE_FN);
    const auto meterBackgroundBitmap = pGraphics->LoadBitmap(METERBACKGROUND_FN);

    const auto b = pGraphics->GetBounds();
    const auto mainArea = b.GetPadded(-20);
    const auto contentArea = mainArea.GetPadded(-10);
    const auto titleHeight = 50.0f;
    const auto titleArea = contentArea.GetFromTop(titleHeight);

    // Areas for knobs
    const auto knobsPad = 20.0f;
    const auto knobsExtraSpaceBelowTitle = 25.0f;
    const auto singleKnobPad = -2.0f;
    const auto knobsArea = contentArea.GetFromTop(NAM_KNOB_HEIGHT)
                             .GetReducedFromLeft(knobsPad)
                             .GetReducedFromRight(knobsPad)
                             .GetVShifted(titleHeight + knobsExtraSpaceBelowTitle);
    const auto inputKnobArea = knobsArea.GetGridCell(0, kInputLevel, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto noiseGateArea = knobsArea.GetGridCell(0, kNoiseGateThreshold, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto bassKnobArea = knobsArea.GetGridCell(0, kToneBass, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto midKnobArea = knobsArea.GetGridCell(0, kToneMid, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto trebleKnobArea = knobsArea.GetGridCell(0, kToneTreble, 1, numKnobs).GetPadded(-singleKnobPad);
    const auto outputKnobArea = knobsArea.GetGridCell(0, kOutputLevel, 1, numKnobs).GetPadded(-singleKnobPad);

    const auto ngToggleArea =
      noiseGateArea.GetVShifted(noiseGateArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);
    const auto eqToggleArea = midKnobArea.GetVShifted(midKnobArea.H()).SubRectVertical(2, 0).GetReducedFromTop(10.0f);

    // Areas for model and IR
    const auto fileWidth = 200.0f;
    const auto fileHeight = 30.0f;
    const auto irYOffset = 38.0f;
    const auto modelArea = IRECT(200, 290, 400, 320);
    const auto slimIconArea =
      IRECT(modelArea.R + 6.f, modelArea.MH() - 14.f, modelArea.R + 6.f + 2.f * 28.f, modelArea.MH() + 14.f);
    const auto modelIconArea = modelArea.GetFromLeft(30).GetTranslated(-40, 10);
    const auto irArea = modelArea.GetVShifted(irYOffset);
    const auto irSwitchArea = irArea.GetFromLeft(30.0f).GetHShifted(-40.0f).GetScaledAboutCentre(0.6f);

    // Areas for meters
    const auto inputMeterArea = contentArea.GetFromLeft(30).GetHShifted(-20).GetMidVPadded(100).GetVShifted(-25);
    const auto outputMeterArea = contentArea.GetFromRight(30).GetHShifted(20).GetMidVPadded(100).GetVShifted(-25);

    // Misc Areas
    const auto settingsButtonArea = CornerButtonArea(b);

    // Model loader button
    auto loadModelCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        // Sets mNAMPath and mStagedNAM
        const std::string msg = _StageModel(fileName);
        // TODO error messages like the IR loader.
        if (msg.size())
        {
          std::stringstream ss;
          ss << "Failed to load NAM model. Message:\n\n" << msg;
          _ShowMessageBox(GetUI(), ss.str().c_str(), "Failed to load model!", kMB_OK);
        }
        std::cout << "Loaded: " << fileName.Get() << std::endl;
      }
    };

    // IR loader button
    auto loadIRCompletionHandler = [&](const WDL_String& fileName, const WDL_String& path) {
      if (fileName.GetLength())
      {
        const dsp::wav::LoadReturnCode retCode = _StageIR(fileName);
        if (retCode != dsp::wav::LoadReturnCode::SUCCESS)
        {
          std::stringstream message;
          message << "Failed to load IR file " << fileName.Get() << ":\n";
          message << dsp::wav::GetMsgForLoadReturnCode(retCode);

          _ShowMessageBox(GetUI(), message.str().c_str(), "Failed to load IR!", kMB_OK);
        }
      }
    };

    pGraphics->AttachBackground(BACKGROUND_FN);
    pGraphics->AttachControl(new IBitmapControl(b, linesBitmap));
    pGraphics->AttachControl(new IVLabelControl(titleArea, "NAM DUAL PROTOTYPE", titleStyle));
    pGraphics->AttachControl(new ISVGControl(modelIconArea, modelIconSVG));

#ifdef NAM_PICK_DIRECTORY
    const std::string defaultNamFileString = "Select model directory...";
    const std::string defaultIRString = "Select IR directory...";
#else
    const std::string defaultNamFileString = "Select model...";
    const std::string defaultIRString = "Select IR...";
#endif
    // Getting started page listing additional resources
    const char* const getUrl = "https://www.neuralampmodeler.com/users#comp-marb84o5";
    pGraphics->AttachControl(
      new NAMFileBrowserControl(modelArea, kMsgTagClearModel, defaultNamFileString.c_str(), "nam",
                                loadModelCompletionHandler, style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG,
                                fileBackgroundBitmap, globeSVG, "Get NAM Models", getUrl),
      kCtrlTagModelFileBrowser);

    auto loadDrive = [&](const WDL_String& fileName, const WDL_String&) {
      if (!fileName.GetLength())
        return;
      const auto error = _StageModel(fileName, 0);
      if (!error.empty())
        _ShowMessageBox(GetUI(), error.c_str(), "Drive model failed", kMB_OK);
    };
    pGraphics->AttachControl(
      new NAMFileBrowserControl(IRECT(200, 240, 400, 270), kMsgTagClearDrive, "Select drive...", "nam", loadDrive,
                                style, fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "NAM Models", getUrl),
      kCtrlTagDriveFileBrowser);
    pGraphics->AttachControl(new IVKnobControl(IRECT(45, 385, 145, 495), kDriveInput, "Drive In", style));
    pGraphics->AttachControl(new IVKnobControl(IRECT(145, 385, 245, 495), kDriveTrim, "Drive Trim", style));
    pGraphics->AttachControl(new IVKnobControl(IRECT(345, 385, 445, 495), kAmpTrim, "Amp Trim", style));
    pGraphics->AttachControl(new IVToggleControl(IRECT(250, 405, 335, 465), kDriveActive, "Drive", style));
    pGraphics->AttachControl(new IVToggleControl(IRECT(450, 405, 535, 465), kAmpActive, "Amp", style));
    pGraphics->AttachControl(new IVLabelControl(IRECT(120, 240, 195, 270), "Drive", style));
    pGraphics->AttachControl(new IVLabelControl(IRECT(120, 290, 195, 320), "Amp", style));
    pGraphics->AttachControl(new NAMCaptureSearchControl(IRECT(150, 362, 400, 384)));
    pGraphics->AttachControl(new NAMGearFilterControl(
      IRECT(410, 362, 550, 384), style.WithLabelText(IText(13.f, COLOR_WHITE, "Roboto-Regular"))));

    auto hideSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(true);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(true);
      ui->SetAllControlsDirty();
    };
    auto showSlimOverlay = [](IControl* pCaller) {
      IGraphics* ui = pCaller->GetUI();
      if (auto* backdrop = ui->GetControlWithTag(kCtrlTagSlimOverlayBackdrop))
        backdrop->Hide(false);
      if (auto* knob = ui->GetControlWithTag(kCtrlTagSlimKnob))
        knob->Hide(false);
      ui->SetAllControlsDirty();
    };

    pGraphics
      ->AttachControl(
        new NAMSquareButtonControl(slimIconArea, DefaultClickActionFunc, slimIconSVG), kCtrlTagSlimmableIcon)
      ->SetAnimationEndActionFunction(showSlimOverlay)
      ->Hide(true);

    pGraphics->AttachControl(new ISVGSwitchControl(irSwitchArea, {irIconOffSVG, irIconOnSVG}, kIRToggle));
    pGraphics->AttachControl(
      new NAMFileBrowserControl(irArea, kMsgTagClearIR, defaultIRString.c_str(), "wav", loadIRCompletionHandler, style,
                                fileSVG, crossSVG, leftArrowSVG, rightArrowSVG, fileBackgroundBitmap, globeSVG,
                                "Get IRs", getUrl),
      kCtrlTagIRFileBrowser);
    pGraphics->AttachControl(
      new NAMSwitchControl(ngToggleArea, kNoiseGateActive, "Noise Gate", style, switchHandleBitmap));
    pGraphics->AttachControl(new NAMSwitchControl(eqToggleArea, kEQActive, "EQ", style, switchHandleBitmap));

    // The knobs
    pGraphics->AttachControl(new NAMKnobControl(inputKnobArea, kInputLevel, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(new NAMKnobControl(noiseGateArea, kNoiseGateThreshold, "", style, knobBackgroundBitmap));
    pGraphics->AttachControl(
      new NAMKnobControl(bassKnobArea, kToneBass, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(midKnobArea, kToneMid, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(
      new NAMKnobControl(trebleKnobArea, kToneTreble, "", style, knobBackgroundBitmap), -1, "EQ_KNOBS");
    pGraphics->AttachControl(new NAMKnobControl(outputKnobArea, kOutputLevel, "", style, knobBackgroundBitmap));

    // The meters
    pGraphics->AttachControl(new NAMMeterControl(inputMeterArea, meterBackgroundBitmap, style), kCtrlTagInputMeter);
    pGraphics->AttachControl(new NAMMeterControl(outputMeterArea, meterBackgroundBitmap, style), kCtrlTagOutputMeter);

    // Settings/help/about box
    pGraphics->AttachControl(new NAMCircleButtonControl(
      settingsButtonArea,
      [pGraphics](IControl* pCaller) {
        pGraphics->GetControlWithTag(kCtrlTagSettingsBox)->As<NAMSettingsPageControl>()->HideAnimated(false);
      },
      gearSVG));

    pGraphics
      ->AttachControl(new NAMSettingsPageControl(b, backgroundBitmap, inputLevelBackgroundBitmap, switchHandleBitmap,
                                                 crossSVG, style, radioButtonStyle),
                      kCtrlTagSettingsBox)
      ->Hide(true);

    const auto slimKnobArea = b.GetCentredInside(100.f, NAM_KNOB_HEIGHT + 24.f);
    pGraphics->AttachControl(new NAMSlimOverlayBackdropControl(b, hideSlimOverlay), kCtrlTagSlimOverlayBackdrop)
      ->Hide(true);
    pGraphics
      ->AttachControl(new NAMKnobControl(slimKnobArea, kSlim, "Slim", style, knobBackgroundBitmap), kCtrlTagSlimKnob)
      ->Hide(true);

    pGraphics->ForAllControlsFunc([](IControl* pControl) {
      pControl->SetMouseEventsWhenDisabled(true);
      pControl->SetMouseOverWhenDisabled(true);
    });

    // pGraphics->GetControlWithTag(kCtrlTagOutNorm)->SetMouseEventsWhenDisabled(false);
    // pGraphics->GetControlWithTag(kCtrlTagCalibrateInput)->SetMouseEventsWhenDisabled(false);
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
  mStages[0].Process(mOutputPointers[0], nFrames, GetParam(kDriveActive)->Bool(), GetParam(kDriveInput)->Value(),
                     GetParam(kDriveTrim)->Value(), false, 0., 0, 1.);
  mStages[1].Process(mOutputPointers[0], nFrames, GetParam(kAmpActive)->Bool(), GetParam(kInputLevel)->Value(),
                     GetParam(kAmpTrim)->Value(), GetParam(kCalibrateInput)->Bool(),
                     GetParam(kInputCalibrationLevel)->Value(), GetParam(kOutputMode)->Int(), GetParam(kSlim)->Value());
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
  _ResetModelAndIR(sampleRate, GetBlockSize());
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
    return "Choose a folder for nested Drive / Amp menus.";
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
  for (auto& stage : mStages)
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
  config["schema"] = 1;
  config["DrivePath"] = mDrivePath.Get();
  config["NAMPath"] = mNAMPath.Get();
  config["IRPath"] = mIRPath.Get();
  config["CaptureFolder"] = mCaptureFolder;
  for (int i = 0; i < kNumParams; ++i)
    config[GetParam(i)->GetName()] = GetParam(i)->Value();
  chunk.PutStr("###NAMDual-v1###");
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
    if (strcmp(header.Get(), "###NAMDual-v1###") == 0)
    {
      WDL_String serialized;
      pos = ReadStateString(chunk, serialized, pos);
      if (pos < 0)
        return -1;
      auto config = nlohmann::json::parse(serialized.Get());
      if (!config.is_object() || config.value("schema", 0) != 1)
        return -1;
      for (const char* name : {"DrivePath", "NAMPath", "IRPath"})
        if (!config.contains(name) || !config[name].is_string() || config[name].get<std::string>().size() > 32768)
          return -1;
      if (config.contains("CaptureFolder")
          && (!config["CaptureFolder"].is_string() || config["CaptureFolder"].get<std::string>().size() > 32768
              || config["CaptureFolder"].get<std::string>().find('\0') != std::string::npos))
        return -1;
      for (int i = 0; i < kNumParams; ++i)
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
  if (mDrivePath.GetLength())
    SendControlMsgFromDelegate(
      kCtrlTagDriveFileBrowser, kMsgTagLoadedModel, mDrivePath.GetLength() + 1, mDrivePath.Get());
  if (mNAMPath.GetLength())
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadedModel, mNAMPath.GetLength() + 1, mNAMPath.Get());
  if (mIRPath.GetLength())
    SendControlMsgFromDelegate(kCtrlTagIRFileBrowser, kMsgTagLoadedIR, mIRPath.GetLength() + 1, mIRPath.Get());
  if (mDrivePath.GetLength() && !mDriveLoaded)
    SendControlMsgFromDelegate(kCtrlTagDriveFileBrowser, kMsgTagLoadFailed);
  if (mNAMPath.GetLength() && !mAmpInfo.loaded)
    SendControlMsgFromDelegate(kCtrlTagModelFileBrowser, kMsgTagLoadFailed);
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
  for (auto& stage : mStages)
    stage.ResetStopped(sampleRate, maxBlockSize);
  mCabinet.Collect();
  mCabinet.Adopt();
  mCabinet.Collect();
  if (auto* cabinet = mCabinet.Active())
  {
    if (cabinet->ir && cabinet->ir->GetSampleRate() != sampleRate)
      cabinet->ir = std::make_unique<dsp::ImpulseResponse>(cabinet->ir->GetData(), sampleRate);
    cabinet->Prepare(maxBlockSize);
  }
  for (size_t i = 0; i < mStages.size(); ++i)
    mRequestedLatencies[i] = mStages[i].Latency();
}

std::string NeuralAmpModeler::_StageModel(const WDL_String& modelPath, size_t slot)
{
  std::lock_guard<std::recursive_mutex> guard(mControlMutex);
  const int tag = slot == 0 ? kCtrlTagDriveFileBrowser : kCtrlTagModelFileBrowser;
  try
  {
    auto model = nam::get_dsp(std::filesystem::u8path(modelPath.Get()));
    if (model->NumInputChannels() != 1 || model->NumOutputChannels() != 1)
      throw std::runtime_error("NAM slots require a mono input and output model");
    auto prepared = std::make_unique<NAMStage::Prepared>();
    prepared->model = std::make_unique<ResamplingNAM>(std::move(model), mSampleRatePrepared);
    prepared->Reset(mSampleRatePrepared, mMaxBlockPrepared);
    const auto info = prepared->Info();
    const bool latencyChanged = prepared->latency != mRequestedLatencies[slot];
    mRequestedLatencies[slot] = prepared->latency;
#ifdef CLAP_API
    mStages[slot].Publish(std::move(prepared), latencyChanged);
#else
    mStages[slot].Publish(std::move(prepared));
#endif
    if (slot == 0)
    {
      mDrivePath = modelPath;
      mDriveLoaded = true;
    }
    else
    {
      mNAMPath = modelPath;
      mAmpInfo = info;
    }
    _UpdateLatency();
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
    ui->GetControlWithTag(kCtrlTagSlimmableIcon)->Hide(!mAmpInfo.slimmable);
  }
}

void NeuralAmpModeler::_UpdateLatency()
{
  const int latency = mRequestedLatencies[0] + mRequestedLatencies[1];
  if (GetLatency() != latency)
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
  const bool latencyChanged = mRequestedLatencies[slot] != 0;
  mRequestedLatencies[slot] = 0;
#ifdef CLAP_API
  mStages[slot].Publish(std::make_unique<NAMStage::Prepared>(), latencyChanged);
#else
  mStages[slot].Publish(std::make_unique<NAMStage::Prepared>());
#endif
  if (slot == 0)
  {
    mDrivePath.Set("");
    mDriveLoaded = false;
  }
  else
  {
    mNAMPath.Set("");
    mAmpInfo = {};
  }
  _UpdateLatency();
}
