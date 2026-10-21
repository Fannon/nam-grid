#pragma once
#include "ModuleMenu.h"
#include "GridLayout.h"
#include <chrono>

namespace grid_ui
{
const IColor background = COLOR_BLACK;
const IColor surface(255, 23, 26, 30), border(255, 78, 85, 95), muted(255, 183, 190, 200);
const IColor accent(255, 113, 199, 178), text(255, 232, 235, 239);
// Stable semantic palette. Future effects already have reserved colors.
const IColor ampColor(255, 238, 164, 90), cabColor(255, 109, 174, 255), pedalColor(255, 135, 216, 107);
const IColor compColor(255, 232, 206, 105), gateColor(255, 89, 200, 190), eqColor(255, 229, 140, 182);
const IColor boostColor(255, 238, 126, 95), modulationColor(255, 168, 153, 245);
const IColor delayColor(255, 104, 206, 232), reverbColor(255, 199, 143, 237), neutralColor(255, 170, 179, 192);
struct NodeTone
{
  IColor first, second;
  const char* label;
};
inline NodeTone Tone(SlotEffect effect, nam_library::Gear gear)
{
  if (effect == SlotEffect::NaturalGate || effect == SlotEffect::TightGate || effect == SlotEffect::NoisePrint)
    return {gateColor, gateColor, "Gate"};
  if (effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp || effect == SlotEffect::Limiter)
    return {compColor, compColor, "Compressor"};
  if (effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ || effect == SlotEffect::ParametricEQ)
    return {eqColor, eqColor, "EQ"};
  if (effect == SlotEffect::Boost || effect == SlotEffect::TubeBoost)
    return {boostColor, boostColor, "Boost"};
  if (effect == SlotEffect::Distortion)
    return {boostColor, boostColor, "Distortion"};
  if (IsDelay(effect))
    return {delayColor, delayColor, "Delay"};
  if (IsReverb(effect) || effect == SlotEffect::ShimmerReverb)
    return {reverbColor, reverbColor, "Reverb"};
  if (IsModulation(effect) || effect == SlotEffect::SynthFilter || effect == SlotEffect::Wah
      || effect == SlotEffect::Rotary || effect == SlotEffect::UniVibe || effect == SlotEffect::RingMod
      || effect == SlotEffect::JunoChorus)
    return {modulationColor, modulationColor, "Modulation"};
  if (effect == SlotEffect::FrequencyShifter)
    return {modulationColor, modulationColor, "Pitch"};
  if (effect == SlotEffect::Octaver || effect == SlotEffect::Doubler)
    return {modulationColor, modulationColor, "Pitch"};
  if (effect == SlotEffect::Freeze || effect == SlotEffect::Stutter)
    return {delayColor, delayColor, "Capture"};
  if (effect == SlotEffect::Bitcrusher)
    return {boostColor, boostColor, "Texture"};
  if (effect == SlotEffect::AutoSwell)
    return {compColor, compColor, "Volume"};
  if (effect == SlotEffect::Tape)
    return {boostColor, boostColor, "Tape"};
  if (effect == SlotEffect::Utility)
    return {neutralColor, neutralColor, "Utility"};
  if (effect == SlotEffect::CabinetIR)
    return {cabColor, cabColor, "WAV / IR"};
  switch (gear)
  {
    case nam_library::Gear::AmpHead: return {ampColor, ampColor, "Amp Head"};
    case nam_library::Gear::AmpCab: return {ampColor, cabColor, "Amp + Cab"};
    case nam_library::Gear::Cabinet: return {cabColor, cabColor, "Cabinet"};
    case nam_library::Gear::Pedal: return {pedalColor, pedalColor, "Pedal"};
    case nam_library::Gear::Spaces: return {reverbColor, reverbColor, "Spaces"};
    case nam_library::Gear::Outboard: return {neutralColor, neutralColor, "Outboard"};
    default: return {neutralColor, neutralColor, "NAM"};
  }
}
inline void Gradient(IGraphics& g, const IRECT& r, IColor first, IColor second, float opacity, float radius = 3.f)
{
  const auto pattern = IPattern::CreateLinearGradient(
    r.L, r.T, r.R, r.T, {{first.WithOpacity(opacity), 0.f}, {second.WithOpacity(opacity), 1.f}});
  g.PathRoundRect(r, radius);
  g.PathFill(pattern);
}

inline IText Text(float size = 13.f, IColor color = text, EAlign align = EAlign::Near)
{ return IText(size, color, "Roboto-Regular", align); }
// Measure in the active font and remove whole UTF-8 code points when shortening.
inline void FittedText(IGraphics& g, const IText& style, std::string label, const IRECT& bounds)
{
  IRECT measured;
  g.MeasureText(style, label.c_str(), measured);
  if (measured.W() > bounds.W())
  {
    do
    {
      if (label.empty()) break;
      size_t end = label.size() - 1;
      while (end > 0 && (static_cast<unsigned char>(label[end]) & 0xc0) == 0x80) --end;
      label.resize(end);
      g.MeasureText(style, (label + "...").c_str(), measured);
    } while (measured.W() > bounds.W());
    label += "...";
  }
  g.DrawText(style, label.c_str(), bounds);
}
inline IVStyle Style()
{
  return DEFAULT_STYLE.WithLabelText(Text(13.f, muted, EAlign::Center))
    .WithValueText(Text(14.f, text, EAlign::Center))
    .WithDrawShadows(false)
    .WithEmboss(false)
    .WithRoundness(.15f)
    .WithShadowOffset(0)
    .WithFrameThickness(1.f)
    .WithColor(kBG, COLOR_TRANSPARENT)
    .WithColor(kFG, surface)
    .WithColor(kPR, border)
    .WithColor(kFR, border)
    .WithColor(kHL, border)
    .WithColor(kX1, accent);
}
class Knob : public IVKnobControl
{
public:
  Knob(const IRECT& r, int parameter, const char* label, bool compact = false)
  : IVKnobControl(r, parameter, label,
                  (compact ? Style().WithWidgetFrac(.82f).WithLabelText(Text(15.f, muted, EAlign::Center))
                           : Style()).WithValueText(Text(14.f, text, EAlign::Center).WithVAlign(EVAlign::Bottom)), true, false)
  {
  }
  void DrawHandle(IGraphics& g, const IRECT& r) override { g.FillCircle(surface, r.MW(), r.MH(), r.W() * .5f); }
  void DrawIndicatorTrack(IGraphics& g, float angle, float cx, float cy, float radius) override
  {
    g.DrawArc(border, cx, cy, radius, -135.f, 135.f, &mBlend, 2.f);
    g.DrawArc(accent, cx, cy, radius, -135.f, angle, &mBlend, 2.f);
  }
  void DrawPointer(IGraphics& g, float angle, float cx, float cy, float radius) override
  { g.DrawRadialLine(text, cx, cy, angle, radius * .55f, radius * .9f, &mBlend, 2.f); }
};
class Button : public IVButtonControl
{
public:
  Button(const IRECT& r, IActionFunction action, const char* label)
  : IVButtonControl(r, action, label, Style().WithLabelText(Text(13.f, text, EAlign::Center)))
  {
  }
};
// Eight equal-width positions span the editor. Enabled is always last.
constexpr int kControlPositions = 8;
// Flow controls share numeric entry below the dial and double-click reset.
class FlowKnob : public Knob
{
public:
  using Knob::Knob;
};

inline IRECT ControlBounds(int position)
{
  const float left = grid_layout::parameterLeft + grid_layout::parameterWidth * position;
  return IRECT(left + 4.f, grid_layout::knobTop, left + grid_layout::parameterWidth - 4.f, grid_layout::knobBottom);
}
class Toggle : public IVToggleControl
{
public:
  Toggle(const IRECT& r, int param, const char* label = "")
  : IVToggleControl(r, param, label, Style(), "OFF", "ON")
  , mLabel(label)
  {
  }
  void Draw(IGraphics& g) override
  {
    const bool tall = mRECT.H() > 60.f;
    if (tall)
      g.DrawText(Text(15.f, muted, EAlign::Center), mLabel.c_str(), mRECT.GetFromTop(22));
    const auto button = tall ? IRECT(mRECT.L + 12, mRECT.T + 40, mRECT.R - 12, mRECT.T + 74) : mRECT;
    const bool on = GetValue() >= .5;
    g.FillRoundRect(on ? IColor(255, 37, 68, 62) : surface, button, 3.f);
    g.DrawRoundRect(on ? accent : border, button, 3.f);
    const std::string caption = !tall && !mLabel.empty() ? mLabel + (on ? ": ON" : ": OFF") : on ? "ON" : "OFF";
    g.DrawText(Text(13.f, on ? accent : text, EAlign::Center), caption.c_str(), button);
  }
  bool IsHit(float x, float y) const override { return mRECT.Contains(x, y); }

private:
  std::string mLabel;
};

class Editor : public IContainerBase
{
public:
  Editor(const IRECT& r, std::function<void(IGraphics&)> draw)
  : IContainerBase(r)
  , mDraw(std::move(draw))
  {
  }
  void Draw(IGraphics& g) override { mDraw(g); }

private:
  std::function<void(IGraphics&)> mDraw;
};

// Decorative wiring stays behind the cards; no bitmaps, shadows or animated routing.
class Wiring : public IControl
{
public:
  Wiring(const IRECT& r)
  : IControl(r)
  { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override
  {
    for (int row = 0; row < 4; ++row)
      for (int col = 0; col < 3; ++col)
      {
        const float x = 276.f + 212.f * col, y = 102.f + 104.f * row;
        g.DrawLine(border, x, y, x + 24, y);
      }
    g.DrawLine(accent, 67, 102, 88, 102);
    g.DrawLine(accent, 912, 414, 938, 414);
    g.DrawLine(border, 912, 102, 928, 102);
    g.DrawLine(border, 928, 102, 928, 154);
    g.DrawLine(border, 928, 154, 69, 154);
    g.DrawLine(border, 69, 154, 69, 310);
    g.DrawLine(border, 69, 206, 88, 206);
    g.DrawLine(border, 69, 310, 88, 310);
    g.DrawLine(border, 912, 206, 930, 206);
    g.DrawLine(border, 930, 206, 930, 362);
    g.DrawLine(border, 912, 310, 930, 310);
    g.DrawLine(border, 930, 362, 69, 362);
    g.DrawLine(border, 69, 362, 69, 414);
    g.DrawLine(border, 69, 414, 88, 414);
  }
};
} // namespace grid_ui

// A selectable card with an independently clipped module editor below the grid.
class NAMGridSlotControl : public IControl
{
public:
  NAMGridSlotControl(const IRECT& bounds, size_t slot)
  : IControl(bounds.GetHPadded(8.f))
  , mCardBounds(bounds)
  , mSlot(slot)
  {
  }

  void Draw(IGraphics& g) override
  {
    using namespace grid_ui;
    const bool selected = PLUG()->GetSelectedSlot() == mSlot;
    const bool enabled = GetDelegate()->GetParam(SlotActiveParam(mSlot))->Bool();
    const auto tone = Tone(PLUG()->GetSlotEffect(mSlot), PLUG()->GetSlotGear(mSlot));
    const bool occupied = PLUG()->IsSlotLoaded(mSlot) || !PLUG()->GetSlotPath(mSlot).empty();
    g.FillRoundRect(surface, mCardBounds, 4.f);
    if (occupied)
      Gradient(g, mCardBounds.GetPadded(-1.f), tone.first, tone.second, enabled ? .1f : .035f, 4.f);
    const auto outline = occupied ? (enabled ? tone.first : IColor::LinearInterpolateBetween(border, tone.first, .35f))
                                 : selected ? accent : border;
    g.DrawRoundRect(outline, mCardBounds, 4.f, nullptr, selected ? 2.f : occupied ? 1.5f : 1.f);
    auto slotName = GridSlotName(mSlot);
    slotName.erase(std::remove(slotName.begin(), slotName.end(), ' '), slotName.end());
    g.DrawText(Text(14.f, selected ? tone.first : muted), slotName.c_str(),
               IRECT(mCardBounds.L + 12, mCardBounds.T + 8, mCardBounds.R - 36, mCardBounds.T + 25));
    g.FillCircle(enabled ? tone.first : muted, mCardBounds.R - 17, mCardBounds.T + 16, 3.f);
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    const auto path = PLUG()->GetSlotPath(mSlot);
    const bool empty = path.empty() && effect == SlotEffect::NAM;
    const auto label = empty          ? std::string("+ Add module")
                       : path.empty() ? std::string(SlotEffectName(effect))
                                      : nam_library::PathText(std::filesystem::u8path(path).stem());
    FittedText(g, Text(14.f, empty ? muted : text), label,
               IRECT(mCardBounds.L + 12, mCardBounds.T + 29, mCardBounds.R - 12, mCardBounds.T + 52));
    const auto type = empty ? "Empty" : !path.empty() && !PLUG()->IsSlotLoaded(mSlot) ? "Missing file" : tone.label;
    g.DrawText(
      Text(12.f, muted), type, IRECT(mCardBounds.L + 12, mCardBounds.T + 50, mCardBounds.R - 12, mCardBounds.B - 4));
    const auto bar = [&](const IRECT& r, double value, bool clipped) {
      g.FillRect(surface, r); // Opaque track covers wiring drawn behind this card.
      g.DrawRect(border, r);
      const double fill = std::clamp((20. * std::log10(std::max(1.e-6, value)) + 60.) / 60., 0., 1.);
      g.FillRect(clipped ? COLOR_RED : tone.first, IRECT(r.L, r.B - static_cast<float>(fill) * r.H(), r.R, r.B));
    };
    if (enabled && PLUG()->IsSlotLoaded(mSlot))
    {
      if (mSlot != 0) // The global IN meter already covers the first slot's input.
        bar(IRECT(mCardBounds.L - 7, mCardBounds.T + 5, mCardBounds.L - 4, mCardBounds.B - 5), mMeter.input,
            Now() < mInputClip);
      bar(IRECT(mCardBounds.R + 4, mCardBounds.T + 5, mCardBounds.R + 7, mCardBounds.B - 5), mMeter.output,
          Now() < mOutputClip);
    }
  }
  void DrawEditor(IGraphics& g)
  {
    using namespace grid_ui;
    auto slotName = GridSlotName(mSlot);
    slotName.erase(std::remove(slotName.begin(), slotName.end(), ' '), slotName.end());
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    const auto path = PLUG()->GetSlotPath(mSlot);
    const bool empty = path.empty() && effect == SlotEffect::NAM;
    const auto label = empty          ? std::string("Empty module")
                       : path.empty() ? std::string(SlotEffectName(effect))
                                      : nam_library::PathText(std::filesystem::u8path(path).stem());
    const auto panel = mEditor->GetRECT();
    g.FillRoundRect(surface, panel, 5.f);
    g.DrawRoundRect(border, panel, 5.f);
    const auto tone = Tone(effect, PLUG()->GetSlotGear(mSlot));
    Gradient(g, IRECT(42, 539, 768, 541), tone.first, tone.second, .8f, 1.f);
    g.DrawText(Text(18.f, tone.first), slotName.c_str(), IRECT(42, 487, 110, 536));
    FittedText(g, Text(17.f), label, IRECT(118, 487, HasVisualization() ? 700 : 956, 536));
    if (HasVisualization() && mShowVisualization) DrawVisualization(g);
    std::array<bool, kControlPositions> used{};
    used.back() = true;
    if (effect == SlotEffect::NAM && !path.empty())
      used.fill(true);
    else if (effect == SlotEffect::CabinetIR)
      used[0] = true;
    else if (effect == SlotEffect::ParametricEQ)
      used[0] = used[1] = used[2] = used[3] = used[4] = true;
    else if (effect == SlotEffect::Boost || effect == SlotEffect::TubeBoost)
      used[2] = true;
    else if (effect == SlotEffect::NaturalGate || effect == SlotEffect::TightGate || effect == SlotEffect::SmoothComp
             || effect == SlotEffect::PunchComp || effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ)
      used[0] = used[1] = used[2] = true;
    for (int p = 0; p < kUtilityControls; ++p)
      if (UsesUtility(effect, p) && !(effect == SlotEffect::ParametricEQ && p == uGain
          && GetDelegate()->GetParam(SlotUtilityParam(mSlot, uEQMode))->Bool()))
        used[kEffectParameters[p].position] = true;
    for (int i = 0; i < kControlPositions; ++i)
    {
      const auto cell = ControlBounds(i).GetPadded(3.f);
      g.FillRoundRect(IColor(255, 17, 20, 24), cell, 4.f);
      if (!used[i])
        g.DrawText(Text(16.f, border, EAlign::Center), "-", cell);
    }
  }
  bool IsHit(float x, float y) const override { return mCardBounds.Contains(x, y); }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    PLUG()->SelectSlot(mSlot);
    if (mod.R)
      ShowEditMenu();
    else if (x > mCardBounds.R - 32 && y < mCardBounds.T + 28)
    {
      auto* param = GetDelegate()->GetParam(SlotActiveParam(mSlot));
      const double value = param->Bool() ? 0. : 1.;
      GetDelegate()->BeginInformHostOfParamChangeFromUI(SlotActiveParam(mSlot));
      GetDelegate()->SendParameterValueFromUI(SlotActiveParam(mSlot), value);
      GetDelegate()->EndInformHostOfParamChangeFromUI(SlotActiveParam(mSlot));
      mEnabled->SetValueFromDelegate(value);
      SetDirty(false);
    }
    else if (PLUG()->GetSlotEffect(mSlot) == SlotEffect::NAM && PLUG()->GetSlotPath(mSlot).empty()
             && y >= mCardBounds.T + 29 && y < mCardBounds.T + 52)
      ShowMenu();
  }

  void OnMouseDblClick(float, float, const IMouseMod&) override
  {
    PLUG()->SelectSlot(mSlot);
    ShowMenu();
  }

  bool IsDirty() override
  {
    const bool enabled = GetDelegate()->GetParam(SlotActiveParam(mSlot))->Bool(), loaded = PLUG()->IsSlotLoaded(mSlot);
    if (enabled != mLastEnabled || loaded != mLastLoaded)
    {
      mLastEnabled = enabled; mLastLoaded = loaded;
      SetDirty(false);
      if (mEditor && PLUG()->GetSelectedSlot() == mSlot) mEditor->SetDirty(false);
    }
    if (!enabled || !loaded)
    { mMeter = {}; mInputClip = mOutputClip = 0.; }
    const bool sync = GetDelegate()->GetParam(SlotUtilityParam(mSlot, uTempoSync))->Bool();
    if (sync != mLastSync)
    {
      mLastSync = sync;
      Refresh();
    }
    const double now = Now(), dt = std::max(0., now - mTick);
    mTick = now;
    if (PLUG()->GetSlotEffect(mSlot) == SlotEffect::NoisePrint && mEditor)
      mEditor->SetDirty(false);
    const bool moving = mMeter.input > 1.e-4f || mMeter.output > 1.e-4f || mMeter.reduction > .05f
                        || now < mInputClip + .1 || now < mOutputClip + .1;
    const float decay = static_cast<float>(std::exp(-dt / .3));
    mMeter.input *= decay;
    mMeter.output *= decay;
    mMeter.reduction *= decay;
    if (moving)
    {
      SetDirty(false);
      if (mEditor && PLUG()->GetSelectedSlot() == mSlot)
        mEditor->SetDirty(false);
    }
    return IControl::IsDirty();
  }

  void OnAttached() override
  {
    mEditor = new grid_ui::Editor(IRECT(24, grid_layout::editorTop, 976, grid_layout::editorBottom), [this](IGraphics& g) { DrawEditor(g); });
    GetUI()->AttachControl(mEditor);
    // Controls live in a shared bottom editor; each slot keeps its original automation bindings.
    // Inactive editors are hidden, so switching selection never changes parameter ownership.
    mModelButton = new grid_ui::Button(IRECT(42, 546, 416, 576), [this](IControl*) { ShowMenu(); }, "Choose module...");
    mEditor->AddChildControl(mModelButton);
    mEditor->AddChildControl(new grid_ui::Button(IRECT(428, 546, 464, 576), [this](IControl*) { Step(-1); }, "<"));
    mEditor->AddChildControl(new grid_ui::Button(IRECT(472, 546, 508, 576), [this](IControl*) { Step(1); }, ">"));
    mPlotButton = new grid_ui::Button(IRECT(710, 496, 768, 524), [this](IControl*) {
      mShowVisualization = !mShowVisualization;
      mPlotButton->SetLabelStr(mShowVisualization ? "Hide" : "Graph");
      mEditor->SetDirty(false);
    }, "Hide");
    mPlotButton->SetTooltip("Show or hide this module's visualization.");
    mEditor->AddChildControl(mPlotButton);
    mEnabled = new grid_ui::Toggle(grid_ui::ControlBounds(7), SlotActiveParam(mSlot), "Enabled");
    mEditor->AddChildControl(mEnabled);
    mNAMKnobs[0] = new grid_ui::Knob(grid_ui::ControlBounds(0), SlotInputParam(mSlot), "Input", true);
    mNAMKnobs[1] = new grid_ui::Knob(grid_ui::ControlBounds(6), SlotTrimParam(mSlot), "Output trim", true);
    for (auto* knob : mNAMKnobs)
      mEditor->AddChildControl(knob);
    const char* labels[] = {"Threshold", "Release", "Depth", "Threshold", "Release", "Makeup"};
    for (int i = 0; i < 6; ++i)
    {
      mFXKnobs[i] = new grid_ui::Knob(grid_ui::ControlBounds(i % 3), SlotDynamicsParam(mSlot, i), labels[i], true);
      mEditor->AddChildControl(mFXKnobs[i]);
    }
    const char* eqLabels[] = {"Bass", "Mid", "Treble", "High pass", "Low pass", "Presence", "Level"};
    for (int i = 0; i < 7; ++i)
    {
      mEQKnobs[i] = new grid_ui::Knob(grid_ui::ControlBounds(i == 6 ? 0 : i % 3), SlotEQParam(mSlot, i), eqLabels[i], true);
      mEditor->AddChildControl(mEQKnobs[i]);
    }
    for (int i = 0; i < kUtilityControls; ++i)
    {
      const auto& p = kEffectParameters[i];
      if (i == uTempoSync)
        mUtilityKnobs[i] = new grid_ui::Toggle(IRECT(520, 546, 632, 576), SlotUtilityParam(mSlot, i), "Tempo sync");
      else if (i == uDelayPingPong || i == uReverbStereo)
        mUtilityKnobs[i] = new grid_ui::Toggle(IRECT(640, 546, 768, 576), SlotUtilityParam(mSlot, i), p.label);
      else if (i != uGateMode && p.minimum == 0. && p.maximum == 1. && p.step == 1.)
        mUtilityKnobs[i] = new grid_ui::Toggle(grid_ui::ControlBounds(p.position), SlotUtilityParam(mSlot, i), p.label);
      else
        mUtilityKnobs[i] = new grid_ui::Knob(grid_ui::ControlBounds(p.position), SlotUtilityParam(mSlot, i), p.label, true);
      mEditor->AddChildControl(mUtilityKnobs[i]);
    }
    SetTooltip(
      "Select to edit. Double-click to choose a module. Right-click for Copy / Move / Swap. Dot toggles bypass.");
    Refresh();
  }

  void OnMsgFromDelegate(int message, int size, const void* data) override
  {
    if (message == 0 && data && size == sizeof(iplug::ISenderData<1, SlotMeterData>))
    {
      const auto& meter = static_cast<const iplug::ISenderData<1, SlotMeterData>*>(data)->vals[0];
      mMeter.input = std::max(mMeter.input, meter.input);
      mMeter.output = std::max(mMeter.output, meter.output);
      mMeter.reduction = std::max(mMeter.reduction, meter.reduction);
      mReductionNow = meter.reduction;
      mInputHistory[mReductionWrite] = meter.input;
      mOutputHistory[mReductionWrite] = meter.output;
      mReductionWrite = (mReductionWrite + 1) % mInputHistory.size();
      if (mEditor && PLUG()->GetSelectedSlot() == mSlot && HasVisualization() && mShowVisualization)
        mEditor->SetDirty(false);
      if (meter.input >= 1.f)
        mInputClip = Now() + 1.;
      if (meter.output >= 1.f)
        mOutputClip = Now() + 1.;
      SetDirty(false);
    }
    else
      Refresh();
  }

  void OnPopupMenuSelection(IPopupMenu* menu, int) override
  {
    if (!menu)
      return;
    auto* item = menu->GetChosenItem();
    if (!item)
      return;
    if (item->GetTag() == -10)
    {
      WDL_String file, directory;
      directory.Set(PLUG()->GetSlotPath(mSlot).c_str());
      directory.remove_filepart();
      GetUI()->PromptForFile(
        file, directory, EFileAction::Open, "nam", [this](const WDL_String& chosen, const WDL_String&) {
          if (chosen.GetLength())
            Load(chosen.Get());
        });
    }
    else if (item->GetTag() == -11)
      PLUG()->ClearSlot(mSlot);
    else if (item->GetTag() == -13)
      PLUG()->SetCaptureFilter({});
    else if (item->GetTag() == -14)
      PLUG()->RescanCaptureFolder();
    else if (item->GetTag() == -15)
      PLUG()->ToggleSlotQuality(mSlot);
    else if (item->GetTag() == -16)
      PLUG()->LoadSlapbackPreset(mSlot);
    else if (item->GetTag() == -12)
    {
      WDL_String file, directory;
      directory.Set(PLUG()->GetSlotPath(mSlot).c_str());
      directory.remove_filepart();
      GetUI()->PromptForFile(
        file, directory, EFileAction::Open, "wav", [this](const WDL_String& chosen, const WDL_String&) {
          if (chosen.GetLength())
            ShowError(PLUG()->LoadSlotIR(mSlot, chosen.Get()));
        });
    }
    else if (item->GetTag() <= -21 && item->GetTag() >= -20 - static_cast<int>(SlotEffect::NoisePrint)
             && IsValidSlotEffect(static_cast<SlotEffect>(-item->GetTag() - 20)))
      PLUG()->SetSlotEffect(mSlot, static_cast<SlotEffect>(-item->GetTag() - 20));
    else if (item->GetTag() <= -1000 && item->GetTag() > -1048)
    {
      const int action = -item->GetTag() - 1000;
      ShowError(PLUG()->EditSlot(mSlot, action % 16, static_cast<SlotEditMode>(action / 16)));
    }
    else if (mMenuLibrary && item->GetTag() >= 0 && static_cast<size_t>(item->GetTag()) < mMenuLibrary->captures.size())
      Load(nam_library::PathText(mMenuLibrary->captures[item->GetTag()].path));
  }

private:
  bool HasVisualization()
  {
    const auto e = PLUG()->GetSlotEffect(mSlot);
    return (e != SlotEffect::NAM && e != SlotEffect::CabinetIR) || !PLUG()->GetSlotPath(mSlot).empty();
  }
  void DrawVisualization(IGraphics& g)
  {
    using namespace grid_ui;
    const IRECT box(780, 488, 956, 576), plot(788, 513, 948, 568);
    g.FillRoundRect(IColor(255, 17, 20, 24), box, 3.f);
    g.DrawRoundRect(border, box, 3.f);
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    const auto color = Tone(effect, PLUG()->GetSlotGear(mSlot)).first;
    const auto levelY = [&](float db, float floorDB) {
      return plot.T + plot.H() * std::clamp(db / floorDB, 0.f, 1.f);
    };
    const auto levels = [&](float floorDB) {
      for (int i = 0; i <= 3; ++i)
      {
        const float y = plot.T + plot.H() * i / 3.f;
        g.DrawLine(border.WithOpacity(.4f), plot.L, y, plot.R, y);
      }
      const auto trace = [&](const auto& history, IColor traceColor, float width) {
        const auto y = [&](size_t index) {
          const float level = history[(mReductionWrite + index) % history.size()];
          return levelY(20.f * std::log10(std::max(level, 1.e-6f)), floorDB);
        };
        float previous = y(0);
        for (size_t i = 1; i < history.size(); ++i)
        {
          const float current = y(i);
          g.DrawLine(traceColor, plot.L + plot.W() * (i - 1) / (history.size() - 1), previous,
                                 plot.L + plot.W() * i / (history.size() - 1), current, nullptr, width);
          previous = current;
        }
      };
      trace(mInputHistory, muted.WithOpacity(.85f), 1.f);
      trace(mOutputHistory, color, 1.5f);
    };
    if (effect == SlotEffect::NAM)
    {
      using Gear = nam_library::Gear;
      const auto gear = PLUG()->GetSlotGear(mSlot);
      const float cy = box.MH();
      g.DrawText(Text(13.f, color), gear == Gear::Unknown ? "Capture" : Tone(effect, gear).label,
                 IRECT(788, 490, 948, 510));
      const auto head = [&](float top) {
        g.DrawRoundRect(ampColor, IRECT(830, top, 906, top + 20), 3.f);
        g.DrawLine(ampColor, 854, top, 854, top - 4);
        g.DrawLine(ampColor, 854, top - 4, 882, top - 4);
        g.DrawLine(ampColor, 882, top - 4, 882, top);
        for (int i = 0; i < 4; ++i) g.DrawCircle(ampColor, 846.f + 14 * i, top + 11, 2.f);
      };
      const auto cabinet = [&](float top, float bottom) {
        g.DrawRoundRect(cabColor, IRECT(830, top, 906, bottom), 2.f);
        for (float x : {850.f, 886.f}) g.DrawCircle(cabColor, x, (top + bottom) * .5f, (bottom - top) * .35f);
      };
      if (gear == Gear::AmpHead) head(cy - 8);
      else if (gear == Gear::AmpCab) { head(cy - 17); cabinet(cy + 5, cy + 20); }
      else if (gear == Gear::Cabinet) cabinet(cy - 17.5f, cy + 17.5f);
      else if (gear == Gear::Outboard)
      {
        g.DrawRoundRect(color, IRECT(810, cy - 13.5f, 926, cy + 13.5f), 2.f);
        for (float x : {815.f, 921.f})
          for (float y : {cy - 8.5f, cy + 8.5f}) g.FillCircle(color, x, y, 1.5f);
        g.DrawRect(color, IRECT(830, cy - 7.5f, 858, cy + 7.5f));
        g.DrawLine(color, 834, cy + 4.5f, 852, cy - 3.5f);
        for (float x : {880.f, 899.f}) g.DrawCircle(color, x, cy, 5.f);
      }
      else if (gear == Gear::Pedal)
      {
        g.DrawRoundRect(color, IRECT(849, cy - 18.5f, 887, cy + 18.5f), 4.f);
        for (float x : {860.f, 876.f}) g.DrawCircle(color, x, cy - 8.5f, 3.f);
        g.DrawCircle(color, 868, cy + 8.5f, 4.f);
        g.DrawLine(color, 843, cy, 849, cy); g.DrawLine(color, 887, cy, 893, cy);
      }
      else if (gear == Gear::Spaces)
      {
        g.DrawRect(color, IRECT(829, cy - 17, 907, cy + 17));
        g.DrawRect(color, IRECT(851, cy - 8, 885, cy + 8));
        g.DrawLine(color, 829, cy - 17, 851, cy - 8); g.DrawLine(color, 907, cy - 17, 885, cy - 8);
        g.DrawLine(color, 829, cy + 17, 851, cy + 8); g.DrawLine(color, 907, cy + 17, 885, cy + 8);
      }
      else
      {
        g.DrawRoundRect(color, IRECT(849, cy - 18.5f, 887, cy + 18.5f), 2.f);
        for (float y : {cy - 8.f, cy, cy + 8.f}) g.DrawLine(color, 857, y, 879, y);
      }
    }
    else if (effect == SlotEffect::NaturalGate || effect == SlotEffect::TightGate || effect == SlotEffect::NoisePrint
        || effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp || effect == SlotEffect::Limiter)
    {
      const bool gate = effect == SlotEffect::NaturalGate || effect == SlotEffect::TightGate;
      const bool smart = effect == SlotEffect::NoisePrint;
      const double threshold = smart ? GetDelegate()->GetParam(SlotUtilityParam(mSlot, uNoiseGateThreshold))->Value()
                             : effect == SlotEffect::Limiter ? GetDelegate()->GetParam(SlotUtilityParam(mSlot, uLimiterCeiling))->Value()
                             : GetDelegate()->GetParam(SlotDynamicsParam(mSlot, gate ? 0 : 3))->Value();
      const bool off = smart && threshold <= -100.;
      WDL_String caption;
      if (off) caption.SetFormatted(64, "Gate Off | GR %.1f dB", mReductionNow);
      else caption.SetFormatted(64, "Thr %.0f | GR %.1f dB", threshold, mReductionNow);
      g.DrawText(Text(12.f, color), caption.Get(), IRECT(788, 490, 948, 510));
      levels(gate || smart ? -100.f : -60.f);
      if (!off)
      {
        const float y = levelY(static_cast<float>(threshold), gate || smart ? -100.f : -60.f);
        for (float x = plot.L; x < plot.R; x += 8.f)
          g.DrawLine(color, x, y, std::min(x + 4.f, plot.R), y, nullptr, 1.5f);
      }
    }
    else if (effect == SlotEffect::DigitalDelay || effect == SlotEffect::AnalogDelay || effect == SlotEffect::TapeDelay)
    {
      const auto value = [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); };
      double time = value(uDelayTime);
      if (value(uTempoSync) >= .5)
        time = 1000. * grid_tempo::Seconds(PLUG()->GetTempo() > 0. ? PLUG()->GetTempo() : 120., static_cast<int>(value(uTempoDivision)));
      WDL_String caption; caption.SetFormatted(64, "Echo timing  %.0f ms", time);
      g.DrawText(Text(13.f, delayColor), caption.Get(), IRECT(788, 490, 948, 510));
      g.DrawLine(border, plot.L, plot.B, plot.R, plot.B);
      const double horizon = std::max(1000., 4. * time), mix = value(uDelayMix) * .01;
      const double feedback = value(uDelayFeedback) * .01;
      g.DrawLine(muted, plot.L, plot.B, plot.L, plot.B - plot.H() * (1. - mix), nullptr, 2.f);
      double level = mix;
      for (int i = 1; i <= 12 && i * time <= horizon && level > .005; ++i)
      {
        const float x = plot.L + static_cast<float>(plot.W() * i * time / horizon);
        g.DrawLine(delayColor, x, plot.B, x, plot.B - static_cast<float>(plot.H() * level), nullptr, 2.f);
        level *= feedback;
      }
    }
    else
    {
      // Reuse the existing mono meter packets: no audio capture or extra DSP work.
      g.DrawText(Text(13.f, muted), "IN", IRECT(788, 490, 818, 510));
      g.DrawText(Text(13.f, color), "OUT", IRECT(823, 490, 858, 510));
      g.DrawText(Text(11.f, muted, EAlign::Far), "2.4 s", IRECT(870, 490, 948, 510));
      levels(-60.f);
    }
  }
  void Refresh()
  {
    if (!mModelButton)
      return;
    const auto path = PLUG()->GetSlotPath(mSlot);
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    if (effect != mPlotEffect || path != mPlotPath)
    {
      mPlotEffect = effect; mPlotPath = path; mInputHistory = {}; mOutputHistory = {};
      mReductionWrite = 0; mReductionNow = 0.f;
    }
    const bool nam = effect == SlotEffect::NAM;
    const bool gate = effect == SlotEffect::NaturalGate || effect == SlotEffect::TightGate;
    const bool compressor = effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp;
    const bool ir = effect == SlotEffect::CabinetIR;
    const bool selected = PLUG()->GetSelectedSlot() == mSlot;
    mEditor->Hide(!selected);
    if (selected)
    {
      mPlotButton->Hide(!HasVisualization());
      mPlotButton->SetTooltip(nam ? "Capture category from library metadata or filename inference."
                                 : "Gray: input, color: output. Dynamics: dashed threshold and numeric gain reduction.");
      for (auto* knob : mNAMKnobs)
        knob->Hide(!nam || path.empty());
      for (int i = 0; i < 6; ++i)
        mFXKnobs[i]->Hide(i < 3 ? !gate : !compressor);
      for (int i = 0; i < 7; ++i)
        mEQKnobs[i]->Hide(i < 3   ? (effect != SlotEffect::ToneEQ && !(nam && !path.empty()))
                          : i < 6 ? effect != SlotEffect::CutEQ
                                  : !ir);
      const bool tempo = IsDelay(effect) || IsModulation(effect) || effect == SlotEffect::UniVibe || effect == SlotEffect::Stutter;
      const bool sync = GetDelegate()->GetParam(SlotUtilityParam(mSlot, uTempoSync))->Bool();
      for (int i = 0; i < kUtilityControls; ++i)
      {
        bool visible = UsesUtility(effect, i);
        if (effect == SlotEffect::ParametricEQ && i == uGain
            && GetDelegate()->GetParam(SlotUtilityParam(mSlot, uEQMode))->Bool()) visible = false;
        if (i == uTempoSync)
          visible = tempo;
        if (i == uDelayPingPong)
          visible = IsDelay(effect);
        if (i == uReverbStereo)
          visible = IsReverb(effect);
        if (i == uTempoDivision)
          visible = tempo && sync;
        if ((i == uDelayTime || i == uModRate || i == uStutterTime) && sync)
          visible = false;
        mUtilityKnobs[i]->Hide(!visible);
        if (i == uDelayCharacter)
          mUtilityKnobs[i]->SetTooltip(effect == SlotEffect::DigitalDelay
                                         ? "Ducks repeats while you play; echoes recover in the gaps."
                                       : effect == SlotEffect::AnalogDelay
                                         ? "More darkening and soft saturation on the repeats."
                                         : "More soft saturation on the repeats. Use Modulation for tape wobble.");
        else if (i == uModCharacter)
          mUtilityKnobs[i]->SetTooltip(
            effect == SlotEffect::Phaser    ? "Raises the center of the phase sweep."
            : effect == SlotEffect::Flanger ? "Moves feedback from positive through zero to negative."
            : effect == SlotEffect::Tremolo ? "Sharper pulses; stronger playing speeds up the pulse."
                                            : "Warmer tone and more irregular pitch movement.");
        else if (i == uUtilityStereo)
          mUtilityKnobs[i]->SetTooltip(
            "-100: fold to mono. 0: unchanged. +100: double the side signal. Does not create width from mono.");
        else if (i == uChorusSpread)
          mUtilityKnobs[i]->SetTooltip(
            "Phase offset between left and right chorus voices; 0 is centered, 100 is opposite motion.");
        else if (i == uWahPosition)
          mUtilityKnobs[i]->SetTooltip(
            "Heel-to-toe sweep. Automate this parameter or map it to an expression controller in the host.");
        else if (i == uShimmerPitch)
          mUtilityKnobs[i]->SetTooltip(
            "Semitone interval added on each reverb pass. Positive values climb; negative values fall.");
        else if (i == uShimmerAmount)
          mUtilityKnobs[i]->SetTooltip(
            "Pitched reverb feedback. Higher amounts create a longer, more obvious shimmer.");
        else if (i == uShiftFrequency)
          mUtilityKnobs[i]->SetTooltip(
            "Fixed-hertz offset per repeat. Unlike pitch shift, every partial moves by the same amount.");
        else if (i == uShiftSide)
          mUtilityKnobs[i]->SetTooltip(
            "Lower sideband at -100, ring modulation at 0, upper sideband at +100.");
        else if (i == uTapeBump)
          mUtilityKnobs[i]->SetTooltip("Low-frequency tape head emphasis.");
      }
      if (effect == SlotEffect::ParametricEQ)
      {
        mEQKnobs[3]->Hide(false);
        mEQKnobs[4]->Hide(false);
        mEQKnobs[3]->SetRECT(grid_ui::ControlBounds(3));
        mEQKnobs[4]->SetRECT(grid_ui::ControlBounds(4));
      }
      else
      {
        mEQKnobs[3]->SetRECT(grid_ui::ControlBounds(0));
        mEQKnobs[4]->SetRECT(grid_ui::ControlBounds(1));
      }
      if (nam && !path.empty())
      {
        mEQKnobs[3]->Hide(false);
        mEQKnobs[4]->Hide(false);
        mEQKnobs[3]->SetRECT(grid_ui::ControlBounds(4));
        mEQKnobs[4]->SetRECT(grid_ui::ControlBounds(5));
      }
      if (effect == SlotEffect::Boost || effect == SlotEffect::TubeBoost)
      {
        mEQKnobs[3]->Hide(false);
        mEQKnobs[3]->SetRECT(grid_ui::ControlBounds(2));
      }
      for (int i = 0; i < 3; ++i)
        mEQKnobs[i]->SetRECT(grid_ui::ControlBounds(nam ? i + 1 : i));
    }
    mModelButton->SetLabelStr(nam ? "Choose NAM / effect..." : ir ? "Choose IR / effect..." : "Change module...");
    SetTooltip(path.empty() ? "Select to edit. Double-click to choose. Right-click for Copy / Move / Swap."
                            : path.c_str());
    SetDirty(false);
  }
  void Load(const std::string& path)
  {
    auto extension = std::filesystem::u8path(path).extension().string();
    for (auto& ch : extension)
      ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    const bool ir = extension == ".wav";
    ShowError(ir ? PLUG()->LoadSlotIR(mSlot, path.c_str()) : PLUG()->LoadSlot(mSlot, path.c_str()));
    Refresh();
  }
  void ShowError(const std::string& error)
  {
    if (!error.empty())
    {
#ifdef OS_MAC
      GetUI()->ShowMessageBox("Slot operation failed", error.c_str(), kMB_OK);
#else
      GetUI()->ShowMessageBox(error.c_str(), "Slot operation failed", kMB_OK);
#endif
    }
  }
  void ShowEditMenu()
  {
    mMenu.Clear();
    mMenu.AddItem(new IPopupMenu::Item("Clear slot", 0, -11));
    mMenu.AddSeparator();
    for (int mode = 0; mode < 3; ++mode)
    {
      auto* destinations = new IPopupMenu;
      for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
        if (slot != mSlot)
        {
          auto label = GridSlotName(slot) + ": ";
          const auto path = PLUG()->GetSlotPath(slot);
          label += path.empty() ? PLUG()->IsSlotLoaded(slot) ? SlotEffectName(PLUG()->GetSlotEffect(slot)) : "Empty"
                                : nam_library::PathText(std::filesystem::u8path(path).stem());
          destinations->AddItem(new IPopupMenu::Item(label.c_str(), 0, -1000 - mode * 16 - static_cast<int>(slot)));
        }
      mMenu.AddItem(mode == 0 ? "Copy to (replace)" : mode == 1 ? "Move to (replace)" : "Swap with", destinations);
    }
    GetUI()->CreatePopupMenu(*this, mMenu, mCardBounds);
  }
  void ShowMenu()
  {
    mMenuLibrary = PLUG()->GetCaptureLibrary();
    nam_library::FillModuleMenu(mMenu, *mMenuLibrary, PLUG()->GetCaptureFilter(), PLUG()->GetSlotPath(mSlot),
                                PLUG()->GetSlotEffect(mSlot), PLUG()->CanToggleSlotQuality(mSlot),
                                GetDelegate()->GetParam(SlotFullParam(mSlot))->Value() < .5,
                                PLUG()->IsLibraryScanning());
    GetUI()->CreatePopupMenu(*this, mMenu, mCardBounds);
  }
  void Step(int direction)
  {
    const auto library = PLUG()->GetCaptureLibrary();
    const auto filter = PLUG()->GetCaptureFilter();
    const auto path = PLUG()->GetSlotPath(mSlot);
    std::vector<const nam_library::Capture*> matches;
    int current = -1;
    for (const auto& capture : library->captures)
      if (filter.Matches(capture))
      {
        if (nam_library::PathText(capture.path) == path)
          current = static_cast<int>(matches.size());
        matches.push_back(&capture);
      }
    if (!matches.empty())
    {
      const int count = static_cast<int>(matches.size());
      const int next = current < 0 ? (direction > 0 ? 0 : count - 1) : (current + direction + count) % count;
      Load(nam_library::PathText(matches[next]->path));
    }
  }
  IRECT mCardBounds;
  size_t mSlot;
  grid_ui::Editor* mEditor = nullptr;
  IVButtonControl* mModelButton = nullptr;
  grid_ui::Button* mPlotButton = nullptr;
  bool mShowVisualization = true;
  SlotEffect mPlotEffect = SlotEffect::NAM;
  std::string mPlotPath;
  std::array<float, 120> mInputHistory{}, mOutputHistory{};
  size_t mReductionWrite = 0;
  float mReductionNow = 0.f;
  grid_ui::Toggle* mEnabled = nullptr;
  std::array<IControl*, 2> mNAMKnobs{};
  std::array<IControl*, 6> mFXKnobs{};
  std::array<IControl*, 7> mEQKnobs{};
  std::array<IControl*, kUtilityControls> mUtilityKnobs{};
  static double Now()
  { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
  bool mLastSync = false, mLastEnabled = true, mLastLoaded = false;
  SlotMeterData mMeter;
  double mTick = Now(), mInputClip = 0., mOutputClip = 0.;
  IPopupMenu mMenu;
  std::shared_ptr<const nam_library::Snapshot> mMenuLibrary;
};

// Opaque full-page settings: child controls and their background share one coordinate system.
class NAMGridSettingsControl : public IContainerBase
{
public:
  NAMGridSettingsControl(const IRECT& r)
  : IContainerBase(r)
  {
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_ui;
    g.FillRect(background, mRECT);
    g.DrawText(Text(23.f), "Library & settings", IRECT(32, 24, 650, 64));
    g.DrawText(Text(13.f, muted), "Add NAM and IR folders. Subfolders become nested menus in every module selector.",
               IRECT(32, 76, 950, 104));
    const auto folders = PLUG()->GetCaptureFolders();
    mScroll = std::min(mScroll, std::max(0, static_cast<int>(folders.size()) - 7));
    for (int row = 0; row < 7; ++row)
    {
      const int index = mScroll + row;
      const float top = 164.f + row * 43.f;
      const IRECT r(32, top, 968, top + 36);
      if (index >= static_cast<int>(folders.size()))
        break;
      g.FillRoundRect(surface, r, 3.f);
      g.DrawText(Text(12.f, muted), std::to_string(index + 1).c_str(), IRECT(43, top, 74, top + 36));
      g.DrawText(Text(13.f), folders[index].c_str(), IRECT(80, top, 860, top + 36));
      g.DrawText(Text(12.f, muted, EAlign::Center), "Remove", IRECT(866, top, 962, top + 36));
    }
    if (folders.empty())
      g.DrawText(
        Text(14.f, muted), "No library folders yet. Add a folder above to get started.", IRECT(52, 166, 840, 198));
    g.DrawText(Text(12.f, muted), PLUG()->GetCaptureStatus().c_str(), IRECT(32, 476, 960, 506));
    if (folders.size() > 7)
      g.DrawText(Text(11.f, muted), "Scroll the list to see more folders (32 maximum).", IRECT(32, 505, 960, 523));
    g.DrawLine(border, 32, 525, 968, 525);
    g.DrawText(Text(14.f), "Input calibration / Pre 2", IRECT(32, 528, 460, 554));
    g.DrawText(Text(12.f, muted),
               "Interface reference level, separate from musical input gain. Raw keeps the model output unchanged.",
               IRECT(32, 556, 960, 578));
    g.DrawText(
      Text(12.f, muted), "All new gate, EQ and cabinet processing belongs in grid modules.", IRECT(32, 688, 950, 710));
  }
  void OnAttached() override
  {
    using namespace grid_ui;
    AddChildControl(new Button(IRECT(862, 28, 968, 60), [this](IControl*) { Hide(true); }, "Close"));
    mAdd = new Button(
      IRECT(32, 116, 190, 148),
      [this](IControl*) {
        WDL_String folder;
        GetUI()->PromptForDirectory(folder, [plugin = PLUG()](const WDL_String&, const WDL_String& chosen) {
          if (chosen.GetLength())
            plugin->AddCaptureFolder(chosen.Get());
        });
      },
      "+ Add folder");
    AddChildControl(mAdd);
    AddChildControl(
      new Button(IRECT(200, 116, 320, 148), [this](IControl*) { PLUG()->RescanCaptureFolder(); }, "Rescan all"));
    mStartupScan = new Button(
      IRECT(340, 116, 570, 148), [this](IControl*) { PLUG()->SetRescanOnStartup(!PLUG()->GetRescanOnStartup()); },
      "Startup rescan: ON");
    mStartupScan->SetTooltip(
      "Automatically index library folders when opening an instance or project. When off, choose Rescan all manually.");
    AddChildControl(mStartupScan);
    AddChildControl(new Knob(IRECT(32, 583, 135, 685), kInputCalibrationLevel, "Reference (dBu)"));
    AddChildControl(new Toggle(IRECT(158, 625, 277, 657), kCalibrateInput));
    AddChildControl(new IVSwitchControl(IRECT(317, 610, 504, 672), kOutputMode, "Pre 2 output mode", grid_ui::Style()));
    // Expose inherited processing only for recalled projects that still use it.
    mLegacy[0] =
      AddChildControl(new IVToggleControl(IRECT(545, 610, 666, 672), kNoiseGateActive, "Legacy gate", grid_ui::Style()));
    mLegacy[1] = AddChildControl(new IVToggleControl(IRECT(686, 610, 807, 672), kEQActive, "Legacy EQ", grid_ui::Style()));
    mLegacy[2] = AddChildControl(new IVToggleControl(IRECT(827, 610, 948, 672), kIRToggle, "Legacy IR", grid_ui::Style()));
  }
  void Hide(bool hide) override
  {
    IContainerBase::Hide(hide);
    if (!hide)
    {
      const bool legacy = GetDelegate()->GetParam(kNoiseGateActive)->Bool()
                          || GetDelegate()->GetParam(kEQActive)->Bool() || GetDelegate()->GetParam(kIRToggle)->Bool();
      for (auto* control : mLegacy)
        control->Hide(!legacy);
    }
    if (GetUI())
      GetUI()->SetAllControlsDirty();
  }
  bool IsDirty() override
  {
    const char* scanLabel = PLUG()->GetRescanOnStartup() ? "Startup rescan: ON" : "Startup rescan: OFF";
    if (mStartupScan && std::strcmp(mStartupScan->GetLabelStr(), scanLabel) != 0)
      mStartupScan->SetLabelStr(scanLabel);
    if (mAdd && mAdd->IsDisabled() != (PLUG()->GetCaptureFolders().size() >= 32))
      mAdd->SetDisabled(PLUG()->GetCaptureFolders().size() >= 32);
    return IContainerBase::IsDirty();
  }
  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (x < 866 || x > 962 || y < 164 || y >= 465)
      return;
    const int row = static_cast<int>((y - 164) / 43);
    if (y > 164 + row * 43 + 36)
      return;
    PLUG()->RemoveCaptureFolder(static_cast<size_t>(mScroll + row));
    SetDirty(false);
  }
  void OnMouseWheel(float, float y, const IMouseMod&, float delta) override
  {
    if (y < 160 || y > 470)
      return;
    mScroll = std::clamp(
      mScroll + (delta < 0 ? 1 : -1), 0, std::max(0, static_cast<int>(PLUG()->GetCaptureFolders().size()) - 7));
    SetDirty(false);
  }
  bool OnKeyDown(float, float, const IKeyPress& key) override
  {
    if (key.VK == kVK_ESCAPE)
    {
      Hide(true);
      return true;
    }
    return false;
  }

private:
  int mScroll = 0;
  grid_ui::Button* mAdd = nullptr;
  grid_ui::Button* mStartupScan = nullptr;
  std::array<IControl*, 3> mLegacy{};
};


class NAMGridTunerControl : public IContainerBase
{
public:
  explicit NAMGridTunerControl(const IRECT& r)
  : IContainerBase(r)
  {
  }
  void OnAttached() override
  {
    using namespace grid_ui;
    AddChildControl(new Button(IRECT(862, 28, 968, 60), [this](IControl*) { Hide(true); }, "Close"));
    AddChildControl(new Knob(IRECT(350, 510, 470, 630), kTunerReference, "Reference"));
    AddChildControl(new Toggle(IRECT(530, 545, 650, 590), kTunerMute, "Mute output"));
  }
  void Hide(bool hide) override
  {
    IContainerBase::Hide(hide);
    PLUG()->SetTunerOpen(!hide);
    if (GetUI())
      GetUI()->SetAllControlsDirty();
  }
  bool IsDirty() override
  {
    if (!IsHidden())
      SetDirty(false);
    return IContainerBase::IsDirty();
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_ui;
    g.FillRect(background, mRECT);
    g.DrawText(Text(23.f), "Tuner", IRECT(32, 24, 650, 64));
    g.DrawText(Text(13.f, muted), "Play one string. Clean input is measured before the grid.", IRECT(32, 76, 950, 104));
    const double frequency = PLUG()->TunerFrequency();
    const double reference = GetDelegate()->GetParam(kTunerReference)->Value();
    if (frequency > 0.)
    {
      const double pitch = 69. + 12. * std::log2(frequency / reference);
      const int note = static_cast<int>(std::round(pitch));
      const double cents = 100. * (pitch - note);
      const char* notes[]{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
      const std::string label = std::string(notes[(note % 12 + 12) % 12]) + std::to_string(note / 12 - 1);
      g.DrawText(
        Text(88.f, std::abs(cents) < 3. ? accent : text, EAlign::Center), label.c_str(), IRECT(280, 165, 720, 285));
      WDL_String value;
      value.SetFormatted(64, "%+.1f cents   /   %.1f Hz", cents, frequency);
      g.DrawText(Text(20.f, text, EAlign::Center), value.Get(), IRECT(240, 292, 760, 332));
      const float x = 500.f + 6.f * static_cast<float>(std::clamp(cents, -50., 50.));
      g.FillRoundRect(accent, IRECT(x - 3, 365, x + 3, 415), 2.f);
    }
    else
      g.DrawText(Text(30.f, muted, EAlign::Center), "Play a note", IRECT(200, 190, 800, 310));
    g.DrawLine(border, 200, 390, 800, 390);
    g.DrawLine(text, 500, 350, 500, 430);
    const char* ticks[]{"-50", "-25", "0", "+25", "+50"};
    for (int i = 0; i < 5; ++i)
    {
      const float x = 200.f + 150.f * i;
      g.DrawLine(border, x, 385, x, 395);
      g.DrawText(Text(13.f, muted, EAlign::Center), ticks[i], IRECT(x - 35, 438, x + 35, 470));
    }
    g.DrawText(Text(13.f, muted, EAlign::Center), "Close or press Escape to return to the grid and unmute.",
               IRECT(170, 666, 830, 704));
  }
};
