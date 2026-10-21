#pragma once
#include "ModuleMenu.h"
#include "GridLayout.h"
#include "EQResponse.h"
#include "LevelReadout.h"
#include "EffectPreview.h"
#include <chrono>

namespace grid_ui
{
const IColor background = COLOR_BLACK;
const IColor surface(255, 23, 26, 30), border(255, 78, 85, 95), muted(255, 183, 190, 200);
const IColor accent(255, 113, 199, 178), text(255, 232, 235, 239);
// Stable semantic palette. Future effects already have reserved colors.
const IColor ampColor(255, 238, 164, 90), cabColor(255, 109, 174, 255), pedalColor(255, 135, 216, 107);
const IColor compColor(255, 205, 210, 216), gateColor(255, 205, 210, 216), eqColor(255, 229, 140, 182);
const IColor boostColor(255, 238, 126, 95), modulationColor(255, 168, 153, 245);
const IColor delayColor(255, 104, 206, 232), reverbColor(255, 199, 143, 237), neutralColor(255, 170, 179, 192);
const IColor creativeColor(255, 235, 137, 219);
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
  if (effect == SlotEffect::CosmicEcho)
    return {delayColor, reverbColor, "Echo / Reverb"};
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
  if (effect == SlotEffect::Freeze || effect == SlotEffect::Stutter || effect == SlotEffect::LoFiTape)
    return {creativeColor, creativeColor, "Creative"};
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

// Lift a hue toward white without draining its colour, unlike a plain interpolation
// to white that turns every selected tile into the same grey.
inline IColor Brighten(IColor color, float amount)
{
  return IColor::LinearInterpolateBetween(color, COLOR_WHITE, std::clamp(amount, 0.f, 1.f));
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
    .WithColor(kX1, text);
}
class Knob : public IVKnobControl
{
public:
  Knob(const IRECT& r, int parameter, const char* label, bool compact = false, int slot = -1)
  : IVKnobControl(r, parameter, label,
                  (compact ? Style().WithWidgetFrac(.82f).WithLabelText(Text(15.f, muted, EAlign::Center))
                           : Style()).WithValueText(Text(14.f, text, EAlign::Center).WithVAlign(EVAlign::Bottom)), true, false)
  , mSlot(slot)
  {
  }
  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (mSlot < 0) { IVKnobControl::OnMouseDblClick(x, y, mod); return; }
    const auto indices = SlotParamIndices(mSlot);
    const auto defaults = SlotDefaultValues(PLUG()->GetSlotEffect(mSlot));
    for (size_t i = 0; i < indices.size(); ++i)
      if (indices[i] == GetParamIdx())
      {
        SetValue(GetParam()->ToNormalized(defaults[i]));
        SetDirty(true);
        return;
      }
  }
  void DrawHandle(IGraphics& g, const IRECT& r) override { g.FillCircle(surface, r.MW(), r.MH(), r.W() * .5f); }
  void DrawIndicatorTrack(IGraphics& g, float angle, float cx, float cy, float radius) override
  {
    g.DrawArc(border, cx, cy, radius, -135.f, 135.f, &mBlend, 2.f);
    g.DrawArc(muted, cx, cy, radius, -135.f, angle, &mBlend, 2.f);
  }
  void DrawPointer(IGraphics& g, float angle, float cx, float cy, float radius) override
  { g.DrawRadialLine(text, cx, cy, angle, radius * .55f, radius * .9f, &mBlend, 2.f); }
private:
  int mSlot;
};
class Button : public IVButtonControl
{
public:
  Button(const IRECT& r, IActionFunction action, const char* label)
  : IVButtonControl(r, action, label, Style().WithLabelText(Text(13.f, text, EAlign::Center)))
  {
  }
};
class DownButton : public Button
{
public:
  using Button::Button;
  void Draw(IGraphics& g) override
  {
    Button::Draw(g);
    g.DrawLine(muted, mRECT.MW() - 4, mRECT.MH() - 2, mRECT.MW(), mRECT.MH() + 2);
    g.DrawLine(muted, mRECT.MW(), mRECT.MH() + 2, mRECT.MW() + 4, mRECT.MH() - 2);
  }
};
inline float CategoryBadges(IGraphics& g, SlotEffect effect, nam_library::Gear gear, const IRECT& r,
                            float font, bool right = false, const char* overrideLabel = nullptr)
{
  const auto tone = Tone(effect, gear);
  std::array<std::pair<const char*, IColor>, 2> badges{{{tone.label, tone.first}, {"", tone.second}}};
  int count = 1;
  if (overrideLabel) badges[0] = {overrideLabel, muted};
  else if (effect == SlotEffect::CabinetIR || (effect == SlotEffect::NAM && gear == nam_library::Gear::Cabinet))
    badges[0] = {"Cab", cabColor};
  else if (effect == SlotEffect::NAM)
  {
    if (gear == nam_library::Gear::AmpCab) { badges[0] = {"Amp", ampColor}; badges[1] = {"Cab", cabColor}; count = 2; }
    else if (gear == nam_library::Gear::AmpHead) badges[0] = {"Amp", ampColor};
    else if (gear == nam_library::Gear::Unknown) badges[0] = {"Capture", neutralColor};
  }
  std::array<float, 2> widths{};
  float total = 0.;
  for (int i = 0; i < count; ++i)
  {
    IRECT measured; g.MeasureText(Text(font), badges[i].first, measured);
    widths[i] = measured.W() + 12.f; total += widths[i] + (i ? 5.f : 0.f);
  }
  float x = right ? r.R - total : r.L;
  const float left = x;
  for (int i = 0; i < count; ++i)
  {
    const IRECT box(x, r.T, x + widths[i], r.B);
    g.FillRoundRect(badges[i].second.WithOpacity(.10f), box, 3.f);
    g.DrawRoundRect(badges[i].second.WithOpacity(.55f), box, 3.f);
    g.DrawText(Text(font, badges[i].second, EAlign::Center), badges[i].first, box);
    x = box.R + 5.f;
  }
  return left;
}
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
    g.FillRoundRect(on ? IColor(255, 49, 53, 59) : surface, button, 3.f);
    g.DrawRoundRect(on ? muted : border, button, 3.f);
    const std::string caption = !tall && !mLabel.empty() ? mLabel + (on ? ": ON" : ": OFF") : on ? "ON" : "OFF";
    g.DrawText(Text(13.f, text, EAlign::Center), caption.c_str(), button);
  }
  bool IsHit(float x, float y) const override { return mRECT.Contains(x, y); }

private:
  std::string mLabel;
};

class Choice : public IControl
{
public:
  Choice(const IRECT& bounds, int parameter) : IControl(bounds, parameter) {}
  void Draw(IGraphics& g) override
  {
    WDL_String label; GetParam()->GetDisplay(GetValue(), true, label);
    g.FillRoundRect(surface, mRECT, 3.f); g.DrawRoundRect(border, mRECT, 3.f);
    g.DrawText(Text(12.f, text, EAlign::Center), label.Get(), mRECT);
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    if (!mod.L) return;
    const int value = static_cast<int>(GetParam()->FromNormalized(GetValue()));
    const int next = value >= GetParam()->GetMax() ? 0 : value + 1;
    SetValue(GetParam()->ToNormalized(next)); SetDirty(true);
  }
};

class FlowMeter : public IControl
{
public:
  FlowMeter(const IRECT& bar, const IRECT& readout)
  : IControl(IRECT(std::min(bar.L, readout.L), bar.T, std::max(bar.R, readout.R), readout.B)), mBar(bar), mReadout(readout)
  { mIgnoreMouse = true; SetTooltip("Peak above, 300 ms RMS below, in dBFS. Measures the mono sum (L+R)/2; opposite stereo polarity can cancel in this meter. Peak holds 1.5 seconds then falls at 12 dB/second."); }
  void OnMsgFromDelegate(int message, int size, const void* data) override
  {
    using Packet = iplug::ISenderData<1, std::pair<float, float>>;
    if (message == 0 && data && size == sizeof(Packet))
    {
      const auto& values = static_cast<const Packet*>(data)->vals[0];
      mLevel.Receive(values.second, values.first); mReceived = Now(); SetDirty(false);
    }
  }
  bool IsDirty() override
  {
    const double now = Now(); mLevel.Tick(std::max(0., now - mTick), now - mReceived > .1); mTick = now;
    if (mLevel.peak > 0. || mLevel.average > 0.) SetDirty(false);
    return IControl::IsDirty();
  }
  void Draw(IGraphics& g) override
  {
    const auto db = [](double value) { return 20. * std::log10(std::max(1.e-8, value)); };
    const auto position = [&](double value) { return mBar.B - mBar.H() * static_cast<float>(std::clamp((db(value) + 60.) / 60., 0., 1.)); };
    g.FillRect(surface, mBar); g.DrawRect(border, mBar);
    g.FillRect(muted, IRECT(mBar.L, position(mLevel.average), mBar.R, mBar.B));
    const float peak = position(mLevel.peak);
    g.FillRect(mLevel.peak >= 1. ? COLOR_RED : text, IRECT(mBar.L, peak, mBar.R, std::min(mBar.B, peak + 1.5f)));
    g.FillRect(background, mReadout);
    WDL_String average, held;
    if (mLevel.average < 3.16227766e-5) average.Set("RMS -inf");
    else average.SetFormatted(40, "RMS %.1f", db(mLevel.average));
    if (mLevel.peak < 3.16227766e-5) held.Set("pk -inf");
    else held.SetFormatted(40, "pk %.1f", db(mLevel.peak));
    g.DrawText(Text(13.f, mLevel.peak >= 1. ? COLOR_RED : text, EAlign::Center), held.Get(), mReadout.GetFromTop(15));
    g.DrawText(Text(11.f, muted, EAlign::Center), average.Get(), mReadout.GetFromBottom(13));
  }
private:
  static double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
  IRECT mBar, mReadout;
  LevelReadout mLevel;
  double mTick = Now(), mReceived = Now();
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

#include "ModulationDisplay.h"
#include "EffectSetupDisplay.h"
#include "EffectSpaceDisplay.h"

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
      Gradient(g, mCardBounds.GetPadded(-1.f), tone.first, tone.second,
               selected ? .18f : enabled ? .1f : .035f, 4.f);
    auto first = occupied ? (enabled ? tone.first : IColor::LinearInterpolateBetween(border, tone.first, .35f)) : border;
    auto second = occupied ? (enabled ? tone.second : IColor::LinearInterpolateBetween(border, tone.second, .35f)) : border;
    if (selected)
    {
      first = Brighten(first, occupied ? .25f : .5f);
      second = Brighten(second, occupied ? .25f : .5f);
    }
    const auto outline = IPattern::CreateLinearGradient(mCardBounds.L, 0, mCardBounds.R, 0, {{first, 0.f}, {second, 1.f}});
    g.PathRoundRect(mCardBounds, 4.f); g.PathStroke(outline, selected ? 2.f : occupied ? 1.5f : 1.f);
    auto slotName = GridSlotName(mSlot);
    slotName.erase(std::remove(slotName.begin(), slotName.end(), ' '), slotName.end());
    // The tile ID carries the module's own hue, matching the module panel header below.
    const auto idColor = !occupied ? muted
                         : !enabled ? IColor::LinearInterpolateBetween(border, tone.first, .55f)
                         : selected ? Brighten(tone.first, .3f) : tone.first;
    g.DrawText(Text(14.f, idColor), slotName.c_str(),
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
    const IRECT category(mCardBounds.L + 12, mCardBounds.T + 56, mCardBounds.R - 12, mCardBounds.T + 74);
    if (empty) g.DrawText(Text(12.f, muted), "Empty", category);
    else CategoryBadges(g, effect, PLUG()->GetSlotGear(mSlot), category, 12.f, false,
                        !path.empty() && !PLUG()->IsSlotLoaded(mSlot) ? "Missing file" : nullptr);
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
    const float headerRight = HasVisualization() ? 768.f : 956.f;
    Gradient(g, IRECT(42, 539, headerRight, 541), tone.first, tone.second, .8f, 1.f);
    g.DrawText(Text(18.f, tone.first), slotName.c_str(), IRECT(42, 487, 110, 536));
    const float badgeLeft = empty ? headerRight : CategoryBadges(g, effect, PLUG()->GetSlotGear(mSlot),
                                                                IRECT(590, 499, headerRight, 523), 13.f, true);
    FittedText(g, Text(17.f), label, IRECT(118, 487, badgeLeft - 12, 513));
    const bool enabled = GetDelegate()->GetParam(SlotActiveParam(mSlot))->Bool();
    const int distortionStyle = GetDelegate()->GetParam(SlotUtilityParam(mSlot, uDistStyle))->Int();
    const auto hint = std::string(enabled ? "" : "Bypassed | ")
      + (effect == SlotEffect::Distortion ? effect_presentation::Help(effect, uDistStyle, distortionStyle)
                                         : effect_presentation::Sound(effect));
    if (!empty) FittedText(g, Text(11.f, muted), hint, IRECT(118, 514, badgeLeft - 12, 536));
    if (HasVisualization() && PLUG()->GetShowVisualizations()) DrawVisualization(g);
    std::array<bool, kControlPositions> used{};
    used.back() = true;
    if (effect == SlotEffect::NAM && !path.empty())
      used.fill(true);
    else if (effect == SlotEffect::CabinetIR)
      used[0] = true;
    else if (effect == SlotEffect::ParametricEQ)
    {
      used[0] = used[2] = used[3] = used[4] = true;
      used[1] = !GetDelegate()->GetParam(SlotUtilityParam(mSlot, uEQMode))->Bool();
    }
    else if (effect == SlotEffect::Boost || effect == SlotEffect::TubeBoost)
      used[2] = true;
    else if (effect == SlotEffect::NaturalGate || effect == SlotEffect::TightGate || effect == SlotEffect::SmoothComp
             || effect == SlotEffect::PunchComp || effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ)
      used[0] = used[1] = used[2] = true;
    const bool sync = GetDelegate()->GetParam(SlotUtilityParam(mSlot, uTempoSync))->Bool();
    const bool notch = GetDelegate()->GetParam(SlotUtilityParam(mSlot, uEQMode))->Bool();
    const bool vibrato = GetDelegate()->GetParam(SlotUtilityParam(mSlot, uVibeMode))->Bool();
    for (int p = 0; p < kUtilityControls; ++p)
      if (effect_presentation::Visible(effect, p, sync, notch, vibrato)
          && !effect_presentation::HeaderControl(p))
        used[effect_presentation::Position(effect, p)] = true;
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
    mSiblingButton = new grid_ui::DownButton(IRECT(380, 546, 416, 576), [this](IControl*) { ShowSiblings(); }, "");
    mSiblingButton->SetTooltip("Files in the same folder as this capture or IR.");
    mEditor->AddChildControl(mSiblingButton);
    mEditor->AddChildControl(new grid_ui::Button(IRECT(428, 546, 464, 576), [this](IControl*) { Step(-1); }, "<"));
    mEditor->AddChildControl(new grid_ui::Button(IRECT(472, 546, 508, 576), [this](IControl*) { Step(1); }, ">"));
    mQualityButton = new grid_ui::Button(IRECT(520, 546, 668, 576), [this](IControl*) {
      PLUG()->ToggleSlotQuality(mSlot); Refresh();
    }, "Quality: A2");
    mQualityButton->SetTooltip("Click to switch between full A2 and lower-CPU A2 Lite for this capture.");
    mEditor->AddChildControl(mQualityButton);
    mEnabled = new grid_ui::Toggle(grid_ui::ControlBounds(7), SlotActiveParam(mSlot), "Enabled");
    mEditor->AddChildControl(mEnabled);
    mNAMKnobs[0] = new grid_ui::Knob(grid_ui::ControlBounds(0), SlotInputParam(mSlot), "Input", true, mSlot);
    mNAMKnobs[1] = new grid_ui::Knob(grid_ui::ControlBounds(6), SlotTrimParam(mSlot), "Output trim", true, mSlot);
    for (auto* knob : mNAMKnobs)
      mEditor->AddChildControl(knob);
    const char* labels[] = {"Threshold", "Release", "Reduction", "Threshold", "Release", "Makeup"};
    for (int i = 0; i < 6; ++i)
    {
      mFXKnobs[i] = new grid_ui::Knob(grid_ui::ControlBounds(i % 3), SlotDynamicsParam(mSlot, i), labels[i], true, mSlot);
      mEditor->AddChildControl(mFXKnobs[i]);
      mFXKnobs[i]->SetTooltip(i == 0 ? "Input level that opens the gate; set above noise and below quiet notes."
        : i == 1 ? "Gate closing time; longer values preserve note tails."
        : i == 2 ? "Maximum attenuation when the gate closes; less reduction keeps natural room tone."
        : i == 3 ? "Level above which compression reduces dynamics."
        : i == 4 ? "Gain recovery time after a loud note."
                 : "Level compensation after compression; compare with bypass at similar loudness.");
    }
    const char* eqLabels[] = {"Bass", "Mid", "Treble", "High pass", "Low pass", "Presence", "Output"};
    for (int i = 0; i < 7; ++i)
    {
      mEQKnobs[i] = new grid_ui::Knob(grid_ui::ControlBounds(i == 6 ? 0 : i % 3), SlotEQParam(mSlot, i), eqLabels[i], true, mSlot);
      mEditor->AddChildControl(mEQKnobs[i]);
    }
    for (int i = 0; i < kUtilityControls; ++i)
    {
      const auto& p = kEffectParameters[i];
      if (i == uTempoSync)
        mUtilityKnobs[i] = new grid_ui::Toggle(IRECT(520, 546, 632, 576), SlotUtilityParam(mSlot, i), "Tempo sync");
      else if (i == uTapeHeads || i == uCosmicStyle)
        mUtilityKnobs[i] = new grid_ui::Choice(IRECT(640, 546, 768, 576), SlotUtilityParam(mSlot, i));
      else if (i == uDelayPingPong || i == uReverbStereo)
        mUtilityKnobs[i] = new grid_ui::Toggle(IRECT(640, 546, 768, 576), SlotUtilityParam(mSlot, i), p.label);
      else if (EffectChoices(i).empty() && p.minimum == 0. && p.maximum == 1. && p.step == 1.)
        mUtilityKnobs[i] = new grid_ui::Toggle(grid_ui::ControlBounds(p.position), SlotUtilityParam(mSlot, i), p.label);
      else
        mUtilityKnobs[i] = new grid_ui::Knob(grid_ui::ControlBounds(p.position), SlotUtilityParam(mSlot, i), p.label, true, mSlot);
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
      if (PLUG()->GetSlotEffect(mSlot) == SlotEffect::LoFiTape)
      {
        mMeter.lofi = meter.lofi;
        if (mEditor && PLUG()->GetSelectedSlot() == mSlot) mEditor->SetDirty(false);
      }
      mMeter.filterCutoff = meter.filterCutoff; mMeter.filterQ = meter.filterQ;
      mReductionNow = meter.reduction;
      mInputHistory[mReductionWrite] = meter.input;
      const auto monitoredEffect=PLUG()->GetSlotEffect(mSlot);
      const bool compressor=monitoredEffect==SlotEffect::SmoothComp || monitoredEffect==SlotEffect::PunchComp;
      mOutputHistory[mReductionWrite] = compressor ? meter.beforeMakeup : meter.output;
      mReductionWrite = (mReductionWrite + 1) % mInputHistory.size();
      if (mEditor && PLUG()->GetSelectedSlot() == mSlot && HasVisualization() && PLUG()->GetShowVisualizations())
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
    if (item->GetTag() <= -20000 && -item->GetTag() - 20000 < mSiblingFiles.size())
      Load(nam_library::PathText(mSiblingFiles[-item->GetTag() - 20000]));
    else if (item->GetTag() == -10)
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
    else if (item->GetTag() <= -21 && item->GetTag() >= -20 - static_cast<int>(SlotEffect::Last)
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
  void DrawTapeTransport(IGraphics& g)
  {
    using namespace grid_ui;
    const IRECT bounds(780, 488, 956, 576);
    g.FillRoundRect(IColor(255, 12, 16, 20), bounds, 4.f);
    g.DrawRoundRect(border, bounds, 3.f);
    const bool enabled = GetDelegate()->GetParam(SlotActiveParam(mSlot))->Bool();
    const double mix = GetDelegate()->GetParam(SlotUtilityParam(mSlot, uLoFiMix))->Value() * .01;
    const float opacity = enabled ? static_cast<float>(.3 + .7 * mix) : .25f;
    const auto& history = mMeter.lofi;
    const char* status = !enabled ? "BYPASSED" : mix == 0. ? "DRY" : history.sequence == 0 ? "AWAITING AUDIO" : "PITCH MOTION";
    g.DrawText(Text(8.f, muted), status, IRECT(788, 489, 875, 502));
    g.DrawText(Text(8.f, muted, EAlign::Far), "+/-60 ct", IRECT(880, 489, 948, 502));
    const auto trace = [&](const char* name, const auto& values, const IColor& color, float center) {
      g.DrawText(Text(9.f, color.WithOpacity(opacity)), name, IRECT(788, center - 22, 850, center - 10));
      constexpr float start = 788.f, end = 948.f, height = 9.f;
      g.DrawLine(border.WithOpacity(.5f), start, center, end, center);
      float previous = center;
      double peak = 0.;
      for (int i = 0; i < LoFiTapeHistory::points; ++i)
      {
        const double cents = values[(history.next + i) % LoFiTapeHistory::points];
        peak = std::max(peak, std::abs(cents));
        const float y = center - static_cast<float>(std::clamp(cents / 60., -1., 1.)) * height;
        const float x = start + (end - start) * i / (LoFiTapeHistory::points - 1);
        if (i) g.DrawLine(color.WithOpacity(opacity), x - (end - start) / (LoFiTapeHistory::points - 1), previous, x, y, nullptr, 1.4f);
        previous = y;
      }
      WDL_String amount; amount.SetFormatted(32, "%.1f ct", peak);
      g.DrawText(Text(9.f, color.WithOpacity(opacity), EAlign::Far), amount.Get(), IRECT(885, center - 22, 948, center - 10));
    };
    trace("WOW", history.wow, accent, 525.f);
    trace("FLUTTER", history.flutter, ampColor, 560.f);
  }

  bool HasVisualization()
  {
    if (!PLUG()->GetShowVisualizations()) return false;
    const auto e = PLUG()->GetSlotEffect(mSlot);
    return (e != SlotEffect::NAM && e != SlotEffect::CabinetIR) || !PLUG()->GetSlotPath(mSlot).empty();
  }
  void DrawVisualization(IGraphics& g)
  {
    using namespace grid_ui;
    if (PLUG()->GetSlotEffect(mSlot) == SlotEffect::LoFiTape)
    {
      DrawTapeTransport(g);
      return;
    }
    const IRECT box(780, 488, 956, 576), plot(788, 513, 948, 568);
    g.FillRoundRect(IColor(255, 17, 20, 24), box, 3.f);
    g.DrawRoundRect(border, box, 3.f);
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    const auto color = Tone(effect, PLUG()->GetSlotGear(mSlot)).first;
    const auto levelY = [&](float db, float floorDB) {
      return plot.T + plot.H() * std::clamp(db / floorDB, 0.f, 1.f);
    };
    const auto levels = [&](float floorDB, const IRECT& area) {
      for (int i = 0; i <= 3; ++i)
      {
        const float y = area.T + area.H() * i / 3.f;
        g.DrawLine(border.WithOpacity(.4f), area.L, y, area.R, y);
      }
      const auto trace = [&](const auto& history, IColor traceColor, float width) {
        const auto y = [&](size_t index) {
          const float level = history[(mReductionWrite + index) % history.size()];
          return area.T + area.H() * std::clamp(20.f * std::log10(std::max(level, 1.e-6f))/floorDB, 0.f, 1.f);
        };
        float previous = y(0);
        for (size_t i = 1; i < history.size(); ++i)
        {
          const float current = y(i);
          g.DrawLine(traceColor, area.L + area.W() * (i - 1) / (history.size() - 1), previous,
                                 area.L + area.W() * i / (history.size() - 1), current, nullptr, width);
          previous = current;
        }
      };
      trace(mInputHistory, muted.WithOpacity(.85f), 1.f);
      trace(mOutputHistory, color, 1.5f);
    };
    if (effect == SlotEffect::NAM)
    {
      g.DrawText(Text(12.f, color), "Capture | IN / OUT", IRECT(788, 490, 948, 510));
      levels(-60.f, plot);
    }
    else if (effect == SlotEffect::CosmicEcho)
    {
      DrawCosmicSetup(g,plot,[&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot,p))->Value(); },
        PLUG()->GetSampleRate()>0. ? PLUG()->GetSampleRate() : 48000.);
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
      const bool compressor=effect==SlotEffect::SmoothComp || effect==SlotEffect::PunchComp;
      const IRECT history(plot.L,529,plot.R,plot.B);
      const IRECT reductionBar(plot.L,514,plot.R,520);
      g.FillRect(surface,reductionBar);
      g.FillRect(ampColor.WithOpacity(.8f),IRECT(reductionBar.L,reductionBar.T,
        reductionBar.L+reductionBar.W()*std::clamp(mReductionNow/24.f,0.f,1.f),reductionBar.B));
      g.DrawText(Text(9.5f,muted),compressor ? "Input / reduced (no makeup)" : "Input / reduced",IRECT(plot.L,520,plot.R,530));
      levels(gate || smart ? -100.f : -60.f, history);
      if (!off)
      {
        const float floor=gate || smart ? -100.f : -60.f;
        const float y = history.T+history.H()*std::clamp(static_cast<float>(threshold)/floor,0.f,1.f);
        for (float x = plot.L; x < plot.R; x += 8.f)
          g.DrawLine(color, x, y, std::min(x + 4.f, plot.R), y, nullptr, 1.5f);
      }
    }
    else if (DrawEffectSetup(g, effect, plot, color,
               [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); },
               PLUG()->GetTempo(), PLUG()->GetSampleRate() > 0. ? PLUG()->GetSampleRate() : 48000.))
    {
    }
    else if (IsReverb(effect) || effect == SlotEffect::ShimmerReverb)
    {
      DrawSpaceSetup(g,effect,plot,color,[&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot,p))->Value(); });
    }
    else if (DrawModulationSetup(g, effect, plot, color,
               [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); },
               PLUG()->GetTempo(), PLUG()->GetSampleRate() > 0. ? PLUG()->GetSampleRate() : 48000.))
    {
    }
    else if (effect == SlotEffect::Distortion)
    {
      const auto value = [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); };
      const int style = static_cast<int>(value(uDistStyle));
      const double drive = std::pow(10., value(uDistDrive) / 20.), bias = value(uDistBias) * .004;
      const IRECT shape(plot.L, plot.T, plot.L + 76, plot.B - 10), tonePlot(plot.L + 84, plot.T, plot.R, plot.B - 10);
      const auto transfer = [&](double x) { return SlotDistortion::Transfer(x, style, drive, bias); };
      const auto xPos = [&](double x) { return shape.MW() + shape.W() * .5f * static_cast<float>(x); };
      const auto yPos = [&](double y) { return shape.MH() - shape.H() * .5f * static_cast<float>(y / 2.); };
      WDL_String caption; caption.Set("Clip / tone + mix");
      g.DrawText(Text(12.f, color), caption.Get(), IRECT(788, 490, 948, 510));
      g.DrawLine(border, shape.L, shape.MH(), shape.R, shape.MH());
      g.DrawLine(border, shape.MW(), shape.T, shape.MW(), shape.B);
      g.DrawLine(border.WithOpacity(.5f), xPos(-1.), yPos(-1.), xPos(1.), yPos(1.));
      const double level = std::clamp(double(mMeter.input), 0., 1.);
      const float activeLeft = xPos(-level), activeRight = xPos(level);
      g.FillRect(color.WithOpacity(.08f), IRECT(activeLeft, shape.T, activeRight, shape.B));
      // Sample densely enough to retain folds at maximum drive.
      constexpr int points = 512;
      for (int i = 1; i <= points; ++i)
      {
        const double a = -1. + 2. * (i - 1) / points, b = -1. + 2. * i / points;
        const bool active = level > 1.e-5 && b >= -level && a <= level;
        g.DrawLine(active ? color : color.WithOpacity(.5f), xPos(a), yPos(transfer(a)), xPos(b), yPos(transfer(b)),
                   nullptr, active ? 2.5f : 1.5f);
      }
      if (level > 1.e-5)
        for (double input : {-level, level})
        {
          g.DrawLine(muted.WithOpacity(.6f), xPos(input), shape.B, xPos(input), yPos(transfer(input)));
          g.FillCircle(text, xPos(input), yPos(transfer(input)), 2.5f);
        }
      UtilityControls controls;
      for (int i = 0; i < 6; ++i) controls.distortion[i] = value(uDistStyle + i);
      const double rate = PLUG()->GetSampleRate() > 0. ? PLUG()->GetSampleRate() : 48000.;
      const double upper = std::min(20000., .45 * rate);
      const auto toneY = [&](double hz) {
        const double db = 20. * std::log10(std::max(1.e-9, effect_preview::DistortionTone(hz, controls, rate)));
        return tonePlot.B - tonePlot.H() * static_cast<float>((std::clamp(db, -36., 12.) + 36.) / 48.);
      };
      const float unity = tonePlot.B - tonePlot.H() * .75f;
      g.DrawLine(border, tonePlot.L, unity, tonePlot.R, unity);
      double previousHz = 80.;
      for (int i = 1; i <= 80; ++i)
      {
        const double hz = 80. * std::pow(upper / 80., i / 80.);
        g.DrawLine(color, tonePlot.L + tonePlot.W() * (i - 1) / 80.f, toneY(previousHz),
                          tonePlot.L + tonePlot.W() * i / 80.f, toneY(hz));
        previousHz = hz;
      }
      g.DrawText(Text(9.f, muted), "Shape", IRECT(shape.L, plot.B - 9, shape.R, plot.B));
      g.DrawText(Text(9.f, muted, EAlign::Far), "80 Hz - 20k", IRECT(tonePlot.L, plot.B - 9, tonePlot.R, plot.B));
    }
    else if (effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ || effect == SlotEffect::ParametricEQ
             || effect == SlotEffect::SynthFilter)
    {
      const auto eqValue = [&](int p) { return GetDelegate()->GetParam(SlotEQParam(mSlot, p))->Value(); };
      const auto value = [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); };
      const double rate = PLUG()->GetSampleRate() > 0. ? PLUG()->GetSampleRate() : 48000.;
      const double upper = std::min(20000., .49 * rate), span = std::log(upper / 20.);
      const bool filter = effect == SlotEffect::SynthFilter;
      const auto cutoff = [&](int channel) {
        return mMeter.filterCutoff[channel] > 0.f ? double(mMeter.filterCutoff[channel])
                                                : std::min(.4 * rate, value(uFilterCutoff));
      };
      const auto resonance = [&](int channel) {
        return mMeter.filterQ[channel] > 0.f ? double(mMeter.filterQ[channel]) : value(uFilterResonance);
      };
      const std::array<double, 17> key{static_cast<double>(effect), rate, eqValue(0), eqValue(1), eqValue(2),
                                     eqValue(3), eqValue(4), eqValue(5), value(uFrequency), value(uGain), value(uQ), value(uEQMode),
                                     filter ? cutoff(0) : 0., filter ? resonance(0) : 0.,
                                     filter ? cutoff(1) : 0., filter ? resonance(1) : 0., filter ? value(uFilterMode) : 0.};
      if (key != mEQKey)
      {
        mEQKey = key;
        EQControls eq; eq.bass = key[2]; eq.mid = key[3]; eq.treble = key[4];
        eq.highPass = key[5]; eq.lowPass = key[6]; eq.presence = key[7];
        UtilityControls utility; utility.frequency = key[8]; utility.gain = key[9]; utility.q = key[10]; utility.eqMode = key[11];
        utility.cutoff = key[12]; utility.resonance = key[13];
        utility.filterMode = static_cast<int>(key[16]);
        const grid_eq_response::Response response(rate, effect, eq, utility);
        utility.cutoff = key[14]; utility.resonance = key[15];
        const grid_eq_response::Response rightResponse(rate, effect, eq, utility);
        for (size_t i = 0; i < mEQCurve.size(); ++i)
        {
          // Include the exact center so a narrow notch is visible between log-spaced samples.
          const double hz = i == mEQCurve.size() - 1 ? std::clamp(std::min(filter ? key[12] : utility.frequency, .4 * rate), 20., upper)
                                                    : 20. * std::exp(span * i / (mEQCurve.size() - 2));
          mEQCurve[i] = {static_cast<float>(std::log(hz / 20.) / span),
                        static_cast<float>(20. * std::log10(std::max(1.e-12, response.Magnitude(hz)))),
                        static_cast<float>(20. * std::log10(std::max(1.e-12, filter ? rightResponse.Magnitude(hz) : response.Magnitude(hz))))};
        }
        std::sort(mEQCurve.begin(), mEQCurve.end());
      }
      WDL_String caption;
      if (filter) caption.SetFormatted(64, "%s %.0f Hz", key[16] == 1 ? "HP" : key[16] == 2 ? "BP" : "LP", key[12]);
      else caption.Set("EQ response");
      g.DrawText(Text(13.f, color), caption.Get(), IRECT(788, 490, 948, 510));
      const auto y = [&](float db) { return plot.T + plot.H() * (24.f - std::clamp(db, -24.f, 24.f)) / 48.f; };
      for (int db : {-24, -12, 0, 12, 24})
        g.DrawLine(border.WithOpacity(db == 0 ? .9f : .4f), plot.L, y(static_cast<float>(db)), plot.R, y(static_cast<float>(db)));
      for (double hz : {100., 1000., 10000.})
      {
        const float x = plot.L + static_cast<float>(plot.W() * std::log(hz / 20.) / span);
        if (hz < upper) g.DrawLine(border.WithOpacity(.35f), x, plot.T, x, plot.B);
      }
      for (size_t i = 1; i < mEQCurve.size(); ++i)
      {
        if (filter && (std::abs(key[12] - key[14]) > 1. || std::abs(key[13] - key[15]) > .01))
          g.DrawLine(color.WithOpacity(.5f), plot.L + plot.W() * mEQCurve[i - 1][0], y(mEQCurve[i - 1][2]),
                                           plot.L + plot.W() * mEQCurve[i][0], y(mEQCurve[i][2]), nullptr, 1.f);
        g.DrawLine(color, plot.L + plot.W() * mEQCurve[i - 1][0], y(mEQCurve[i - 1][1]),
                          plot.L + plot.W() * mEQCurve[i][0], y(mEQCurve[i][1]), nullptr, 1.5f);
      }
    }
    else
    {
      // Reuse the existing mono meter packets: no audio capture or extra DSP work.
      g.DrawText(Text(13.f, muted), "IN", IRECT(788, 490, 818, 510));
      g.DrawText(Text(13.f, color), "OUT", IRECT(823, 490, 858, 510));
      g.DrawText(Text(11.f, muted, EAlign::Far), "2.4 s", IRECT(870, 490, 948, 510));
      levels(-60.f, plot);
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
      mMeter.lofi = {};
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
      const bool siblings = (nam || ir) && !path.empty();
      mSiblingButton->Hide(!siblings);
      mQualityButton->Hide(!PLUG()->CanToggleSlotQuality(mSlot));
      mQualityButton->SetLabelStr(GetDelegate()->GetParam(SlotFullParam(mSlot))->Bool() ? "Quality: A2" : "Quality: A2 Lite");
      mModelButton->SetRECT(IRECT(42, 546, siblings ? 370 : 416, 576));
      for (auto* knob : mNAMKnobs)
        knob->Hide(!nam || path.empty());
      for (int i = 0; i < 6; ++i)
        mFXKnobs[i]->Hide(i < 3 ? !gate : !compressor);
      for (int i = 0; i < 7; ++i)
        mEQKnobs[i]->Hide(i < 3   ? (effect != SlotEffect::ToneEQ && !(nam && !path.empty()))
                          : i < 6 ? effect != SlotEffect::CutEQ
                                  : !ir);
      const auto flag = [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Bool(); };
      const bool sync = flag(uTempoSync), notch = flag(uEQMode), vibrato = flag(uVibeMode);
      for (int i = 0; i < kUtilityControls; ++i)
      {
        const bool visible = effect_presentation::Visible(effect, i, sync, notch, vibrato);
        mUtilityKnobs[i]->Hide(!visible);
        if (!visible) continue;
        mUtilityKnobs[i]->SetTooltip(effect_presentation::Help(effect, i, GetDelegate()->GetParam(SlotUtilityParam(mSlot, i))->Int()));
        if (!effect_presentation::HeaderControl(i))
          mUtilityKnobs[i]->SetRECT(grid_ui::ControlBounds(effect_presentation::Position(effect, i)));
        if (auto* knob = dynamic_cast<grid_ui::Knob*>(mUtilityKnobs[i]))
          if (std::strcmp(knob->GetLabelStr(), effect_presentation::Label(effect, i)) != 0)
            knob->SetLabelStr(effect_presentation::Label(effect, i));
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
  void ShowSiblings()
  {
    const auto path = std::filesystem::u8path(PLUG()->GetSlotPath(mSlot));
    mSiblingFiles = nam_library::SiblingFiles(path, PLUG()->GetSlotEffect(mSlot) == SlotEffect::CabinetIR);
    mMenu.Clear();
    for (size_t i = 0; i < mSiblingFiles.size(); ++i)
      mMenu.AddItem(new IPopupMenu::Item(nam_library::PathText(mSiblingFiles[i].stem()).c_str(),
                    mSiblingFiles[i] == path ? IPopupMenu::Item::kChecked : 0, -20000 - static_cast<int>(i)));
    if (mSiblingFiles.empty()) mMenu.AddItem("No files in this folder", -1, IPopupMenu::Item::kDisabled);
    GetUI()->CreatePopupMenu(*this, mMenu, mSiblingButton->GetRECT());
  }
  void Step(int direction)
  {
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    if (effect != SlotEffect::NAM && effect != SlotEffect::CabinetIR)
    {
      PLUG()->SetSlotEffect(mSlot, nam_library::StepModule(effect, direction));
      Refresh(); return;
    }
    const auto path = std::filesystem::u8path(PLUG()->GetSlotPath(mSlot));
    if (path.empty()) return;
    const auto files = nam_library::SiblingFiles(path, effect == SlotEffect::CabinetIR);
    if (files.empty()) return;
    const auto found = std::find(files.begin(), files.end(), path);
    const int current = found == files.end() ? -1 : static_cast<int>(found - files.begin());
    const int count = static_cast<int>(files.size());
    const int next = current < 0 ? (direction > 0 ? 0 : count - 1) : (current + direction + count) % count;
    Load(nam_library::PathText(files[next]));
  }
  IRECT mCardBounds;
  size_t mSlot;
  grid_ui::Editor* mEditor = nullptr;
  IVButtonControl* mModelButton = nullptr;
  grid_ui::DownButton* mSiblingButton = nullptr;
  grid_ui::Button* mQualityButton = nullptr;
  std::vector<std::filesystem::path> mSiblingFiles;
  SlotEffect mPlotEffect = SlotEffect::NAM;
  std::string mPlotPath;
  std::array<float, 120> mInputHistory{}, mOutputHistory{};
  std::array<double, 17> mEQKey{};
  std::array<std::array<float, 3>, 162> mEQCurve{};
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
    mVisualizations = new Button(IRECT(590, 116, 826, 148), [this](IControl*) {
      PLUG()->SetShowVisualizations(!PLUG()->GetShowVisualizations());
    }, "Visualizations: ON");
    AddChildControl(mVisualizations);
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
    const char* visualLabel = PLUG()->GetShowVisualizations() ? "Visualizations: ON" : "Visualizations: OFF";
    if (mVisualizations && std::strcmp(mVisualizations->GetLabelStr(), visualLabel) != 0)
      mVisualizations->SetLabelStr(visualLabel);
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
  grid_ui::Button* mVisualizations = nullptr;
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
