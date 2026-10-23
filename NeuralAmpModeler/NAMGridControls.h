#pragma once
#include "IControls.h"
#include "ModuleMenu.h"
#include "GridLayout.h"
#include "EQResponse.h"
#include "EQSignalSpectrum.h"
#include "LevelReadout.h"
#include "EffectPreview.h"
#include <chrono>

#define PLUG() static_cast<PLUG_CLASS_NAME*>(GetDelegate())

using namespace iplug;
using namespace igraphics;

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
  if (effect == SlotEffect::NoiseGate || effect == SlotEffect::NoisePrint || effect == SlotEffect::DigitalBlack)
    return {gateColor, gateColor, "Gate"};
  if (effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp || effect == SlotEffect::Limiter || IsOutboardCompressor(effect))
    return {compColor, compColor, "Compressor"};
  if (effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ || effect == SlotEffect::ParametricEQ || effect == SlotEffect::ConsoleEQ)
    return {eqColor, eqColor, "EQ"};
  if (effect == SlotEffect::Boost || effect == SlotEffect::TubeBoost)
    return {boostColor, boostColor, "Boost"};
  if (effect == SlotEffect::Distortion || effect == SlotEffect::ConsoleColour)
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

inline IText Text(float size = 14.f, IColor color = text, EAlign align = EAlign::Near)
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
// Up to two lines, vertically centred: line one breaks after the last space, '-' or '_'
// that fits (filenames often have no spaces), and line two ends in "..." if still too long.
inline void TwoLineText(IGraphics& g, const IText& style, const std::string& label, const IRECT& bounds)
{
  IRECT measured;
  g.MeasureText(style, label.c_str(), measured);
  if (measured.W() <= bounds.W()) { g.DrawText(style, label.c_str(), bounds); return; }
  size_t fit = 0, breakAt = 0;
  for (size_t end = 1; end <= label.size(); ++end)
  {
    if (end < label.size() && (static_cast<unsigned char>(label[end]) & 0xc0) == 0x80) continue; // Whole UTF-8 characters.
    g.MeasureText(style, label.substr(0, end).c_str(), measured);
    if (measured.W() > bounds.W()) break;
    fit = end;
    const char last = label[end - 1];
    if (last == ' ' || last == '-' || last == '_') breakAt = end;
  }
  const size_t split = breakAt > fit / 2 ? breakAt : fit;
  auto first = label.substr(0, split), second = label.substr(split);
  while (!first.empty() && first.back() == ' ') first.pop_back();
  while (!second.empty() && second.front() == ' ') second.erase(second.begin());
  const float line = bounds.H() * .5f;
  g.DrawText(style, first.c_str(), IRECT(bounds.L, bounds.T, bounds.R, bounds.T + line));
  FittedText(g, style, second, IRECT(bounds.L, bounds.T + line, bounds.R, bounds.B));
}
inline IVStyle Style()
{
  return DEFAULT_STYLE.WithLabelText(Text(14.f, muted, EAlign::Center))
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
  void DrawLabel(IGraphics& g) override
  {
    if (mSlot < 0) { IVKnobControl::DrawLabel(g); return; }
    if (mLabelBounds.H() && mStyle.showLabel)
    {
      auto label = mStyle.labelText;
      label.mFGColor = Tone(PLUG()->GetSlotEffect(mSlot), PLUG()->GetSlotGear(mSlot)).first;
      g.DrawText(label, GetLabelStr(), mLabelBounds, &mBlend);
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
  : IVButtonControl(r, action, label, Style().WithLabelText(Text(14.f, text, EAlign::Center)))
  {
  }
};
class HintLabel : public ITextControl
{
public:
  HintLabel(const IRECT& r, const char* label, const IText& style, const char* hint)
  : ITextControl(r, label, style)
  {
    SetIgnoreMouse(false);
    SetTooltip(hint);
  }
};
class ModuleButton : public Button
{
public:
  using Button::Button;
  void Draw(IGraphics& g) override
  {
    FittedText(g, Text(17.f), GetLabelStr(), IRECT(mRECT.L, mRECT.T, mRECT.R - 16, mRECT.B));
    g.DrawLine(muted, mRECT.R - 11, mRECT.MH() - 2, mRECT.R - 7, mRECT.MH() + 2);
    g.DrawLine(muted, mRECT.R - 7, mRECT.MH() + 2, mRECT.R - 3, mRECT.MH() - 2);
  }
};
class BrowserButton : public Button
{
public:
  using Button::Button;
  void Draw(IGraphics& g) override
  {
    Button::Draw(g);
    const float x = mRECT.MW(), y = mRECT.MH();
    g.DrawRoundRect(muted, IRECT(x - 8, y - 6, x + 8, y + 6), 1.5f);
    g.DrawLine(muted, x - 3, y - 6, x - 3, y + 6);
    g.DrawLine(muted, x, y - 2, x + 5, y - 2);
    g.DrawLine(muted, x, y + 2, x + 5, y + 2);
  }
};
inline float CategoryBadges(IGraphics& g, SlotEffect effect, nam_library::Gear gear, const IRECT& r,
                            float font, bool right = false, const char* overrideLabel = nullptr, float opacity = 1.f)
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
    g.FillRoundRect(badges[i].second.WithOpacity(.10f * opacity), box, 3.f);
    g.DrawRoundRect(badges[i].second.WithOpacity(.55f * opacity), box, 3.f);
    g.DrawText(Text(font, badges[i].second.WithOpacity(opacity), EAlign::Center), badges[i].first, box);
    x = box.R + 5.f;
  }
  return left;
}
// Flow controls share numeric entry below the dial and double-click reset.
class FlowKnob : public Knob
{
public:
  using Knob::Knob;
};

inline IRECT ControlBounds(int position)
{
  const float left = grid_layout::parameterLeft + grid_layout::parameterWidth * (position % 4);
  const float top = grid_layout::knobTop + (position / 4) * grid_layout::knobRowPitch;
  return IRECT(left + 4.f, top, left + grid_layout::parameterWidth - 4.f, top + grid_layout::knobBottom - grid_layout::knobTop);
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
    {
      const int slot = SlotControlOwner(GetParamIdx());
      const auto labelColor = slot < 0 ? muted : Tone(PLUG()->GetSlotEffect(slot), PLUG()->GetSlotGear(slot)).first;
      g.DrawText(Text(15.f, labelColor, EAlign::Center), mLabel.c_str(), mRECT.GetFromTop(22));
    }
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

inline void DisableTunerFromUI(IGraphics* ui)
{
  auto* button = ui->GetControlWithTag(kCtrlTagTunerButton);
  auto* delegate = button->GetDelegate();
  delegate->BeginInformHostOfParamChangeFromUI(kTunerActive);
  delegate->SendParameterValueFromUI(kTunerActive, 0.);
  button->SetValue(0.);
  button->SetDirty(false);
  delegate->EndInformHostOfParamChangeFromUI(kTunerActive);
}

class Choice : public IControl
{
public:
  Choice(const IRECT& bounds, int parameter) : IControl(bounds, parameter) {}
  void Draw(IGraphics& g) override
  {
    WDL_String label; GetParam()->GetDisplay(GetValue(), true, label);
    g.FillRoundRect(surface, mRECT, 3.f); g.DrawRoundRect(border, mRECT, 3.f);
    FittedText(g, Text(13.f, text, EAlign::Center), label.Get(), mRECT.GetHPadded(-4.f));
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    if (!mod.L) return;
    const int value = static_cast<int>(GetParam()->FromNormalized(GetValue()));
    const int next = value >= GetParam()->GetMax() ? 0 : value + 1;
    SetValue(GetParam()->ToNormalized(next)); SetDirty(true);
  }
};

// One peak scale for the grid IN/OUT and every module meter: -60 to +6 dBFS. The line marks
// 0 dBFS; level above it draws red, and a clip lamp keeps the overs red for a while.
inline constexpr double kMeterFloorDB = -60., kMeterCeilingDB = 6., kClipHoldSeconds = 2.;
inline float MeterY(const IRECT& r, double level)
{
  const double db = 20. * std::log10(std::max(1.e-8, level));
  return r.B - r.H() * static_cast<float>(std::clamp((db - kMeterFloorDB) / (kMeterCeilingDB - kMeterFloorDB), 0., 1.));
}
inline void DrawMeterBar(IGraphics& g, const IRECT& r, double level, IColor fill, bool clipped)
{
  g.FillRect(surface, r); // Opaque track covers wiring drawn behind it.
  g.DrawRect(border, r);
  const float zero = MeterY(r, 1.), y = MeterY(r, level);
  g.FillRect(fill, IRECT(r.L, std::max(y, zero), r.R, r.B));
  if (clipped) g.FillRect(COLOR_RED, IRECT(r.L, r.T, r.R, zero));
  else if (y < zero) g.FillRect(COLOR_RED, IRECT(r.L, y, r.R, zero));
  g.DrawLine(text.WithOpacity(.8f), r.L - 1.f, zero, r.R + 1.f, zero);
}
class FlowMeter : public IControl
{
public:
  FlowMeter(const IRECT& bar, const IRECT& readout)
  : IControl(IRECT(std::min(bar.L, readout.L), bar.T, std::max(bar.R, readout.R), readout.B)), mBar(bar), mReadout(readout)
  {
    mIgnoreMouse = true;
    SetTooltip("Stereo peak meter (left, right) from -60 to +6 dBFS; the line marks 0 dBFS. Overs stay red for 2 seconds. "
               "The value is the higher channel's peak in dBFS, held 1.5 seconds, then falling at 12 dB/second.");
  }
  void OnMsgFromDelegate(int message, int size, const void* data) override
  {
    using Packet = iplug::ISenderData<1, std::pair<float, float>>;
    if (message == 0 && data && size == sizeof(Packet))
    {
      const auto& values = static_cast<const Packet*>(data)->vals[0];
      mLevels[0].Receive(0., values.first); mLevels[1].Receive(0., values.second);
      if (std::max(values.first, values.second) >= 1.f) mClipUntil = Now() + kClipHoldSeconds;
      SetDirty(false);
    }
  }
  bool IsDirty() override
  {
    const double now = Now();
    for (auto& level : mLevels) level.Tick(std::max(0., now - mTick), false);
    mTick = now;
    if (mLevels[0].peak > 0. || mLevels[1].peak > 0. || now < mClipUntil + .1) SetDirty(false);
    return IControl::IsDirty();
  }
  void Draw(IGraphics& g) override
  {
    const bool clipped = Now() < mClipUntil;
    const float width = (mBar.W() - 1.f) * .5f;
    for (int channel = 0; channel < 2; ++channel)
    {
      const float left = mBar.L + channel * (width + 1.f);
      DrawMeterBar(g, IRECT(left, mBar.T, left + width, mBar.B), mLevels[channel].peak, muted, clipped);
    }
    g.FillRect(background, mReadout);
    const double peak = std::max(mLevels[0].peak, mLevels[1].peak);
    WDL_String held;
    if (peak < 3.16227766e-5) held.Set("-inf");
    else held.SetFormatted(40, "%.1f", 20. * std::log10(peak));
    g.DrawText(Text(14.f, clipped ? COLOR_RED : text, EAlign::Center), held.Get(), mReadout);
  }
private:
  static double Now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
  IRECT mBar, mReadout;
  std::array<LevelReadout, 2> mLevels;
  double mTick = Now(), mClipUntil = 0.;
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
    // Connectors meet card centres; returns run through the middle of the gap between rows.
    const auto centre = [](int row) { return grid_layout::cardTop + grid_layout::rowPitch * row + grid_layout::cardHeight * .5f; };
    const auto gap = [](int row) { return grid_layout::cardTop + grid_layout::rowPitch * row + (grid_layout::cardHeight + grid_layout::rowPitch) * .5f; };
    for (int row = 0; row < 4; ++row)
      for (int col = 0; col < 3; ++col)
      {
        const float x = 276.f + 212.f * col;
        g.DrawLine(border, x, centre(row), x + 24, centre(row));
      }
    g.DrawLine(border, 67, centre(0), 88, centre(0));
    g.DrawLine(border, 912, centre(3), 928, centre(3));
    g.DrawLine(border, 912, centre(0), 928, centre(0));
    g.DrawLine(border, 928, centre(0), 928, gap(0));
    g.DrawLine(border, 928, gap(0), 69, gap(0));
    g.DrawLine(border, 69, gap(0), 69, centre(2));
    g.DrawLine(border, 69, centre(1), 88, centre(1));
    g.DrawLine(border, 69, centre(2), 88, centre(2));
    g.DrawLine(border, 912, centre(1), 930, centre(1));
    g.DrawLine(border, 930, centre(1), 930, gap(2));
    g.DrawLine(border, 912, centre(2), 930, centre(2));
    g.DrawLine(border, 930, gap(2), 69, gap(2));
    g.DrawLine(border, 69, gap(2), 69, centre(3));
    g.DrawLine(border, 69, centre(3), 88, centre(3));
  }
};
} // namespace grid_ui

#include "EffectPlot.h"
#include "ModulationDisplay.h"
#include "EffectSetupDisplay.h"
#include "EffectSpaceDisplay.h"
#include "EffectDriveDisplay.h"

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
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    const auto path = PLUG()->GetSlotPath(mSlot);
    const bool empty = path.empty() && effect == SlotEffect::NAM;
    const bool enabled = !empty && GetDelegate()->GetParam(SlotActiveParam(mSlot))->Bool();
    const auto tone = Tone(effect, PLUG()->GetSlotGear(mSlot));
    const bool occupied = !empty;
    g.FillRoundRect(enabled ? surface : IColor(255, 13, 15, 18), mCardBounds, 4.f);
    if (occupied)
      Gradient(g, mCardBounds.GetPadded(-1.f), tone.first, tone.second,
               enabled ? selected ? .18f : .1f : selected ? .025f : .012f, 4.f);
    auto first = occupied ? (enabled ? tone.first : IColor::LinearInterpolateBetween(border, tone.first, .12f)) : border;
    auto second = occupied ? (enabled ? tone.second : IColor::LinearInterpolateBetween(border, tone.second, .12f)) : border;
    if (selected)
    {
      first = Brighten(first, occupied ? enabled ? .25f : .1f : .5f);
      second = Brighten(second, occupied ? enabled ? .25f : .1f : .5f);
    }
    const auto outline = IPattern::CreateLinearGradient(mCardBounds.L, 0, mCardBounds.R, 0, {{first, 0.f}, {second, 1.f}});
    g.PathRoundRect(mCardBounds, 4.f); g.PathStroke(outline, selected ? 2.f : occupied ? 1.5f : 1.f);
    auto slotName = GridSlotName(mSlot);
    slotName.erase(std::remove(slotName.begin(), slotName.end(), ' '), slotName.end());
    // The tile ID carries the module's own hue, matching the module panel header below.
    const auto idColor = !occupied ? muted
                         : !enabled ? IColor::LinearInterpolateBetween(border, tone.first, .15f)
                         : selected ? Brighten(tone.first, .3f) : tone.first;
    // Header row: ID with its category badges, and bypass. It selects and drags; the name
    // button below is the only part that opens the browser.
    IRECT idText; g.MeasureText(Text(16.f), slotName.c_str(), idText);
    g.DrawText(Text(16.f, idColor), slotName.c_str(),
               IRECT(mCardBounds.L + 12, mCardBounds.T + 6, mCardBounds.L + 14 + idText.W(), mCardBounds.T + 26));
    if (!empty)
      CategoryBadges(g, effect, PLUG()->GetSlotGear(mSlot),
                     IRECT(mCardBounds.L + 22 + idText.W(), mCardBounds.T + 7, mCardBounds.R - 28, mCardBounds.T + 25), 12.f, false,
                     !path.empty() && !PLUG()->IsSlotLoaded(mSlot) ? "Missing file" : nullptr, enabled ? 1.f : .4f);
    if (!empty)
    {
      // Equal 12 px inset from the top and right, with a larger 32 px hit target.
      const float toggleX=mCardBounds.R-12.f, toggleY=mCardBounds.T+12.f;
      g.DrawCircle(enabled ? tone.first.WithOpacity(.7f) : border, toggleX, toggleY, 7.f, nullptr, 1.2f);
      if (enabled) g.FillCircle(tone.first, toggleX, toggleY, 4.5f);
      else g.DrawLine(muted.WithOpacity(.65f),toggleX-3.f,toggleY,toggleX+3.f,toggleY,nullptr,1.5f);
    }
    const auto label = empty          ? std::string("+ Add module")
                       : path.empty() ? std::string(SlotEffectName(effect))
                                      : nam_library::PathText(std::filesystem::u8path(path).stem());
    const auto button = NameButton();
    g.FillRoundRect(mNameHover ? IColor(255, 44, 48, 55) : background.WithOpacity(.35f), button, 3.f);
    g.DrawRoundRect(mNameHover ? muted : border.WithOpacity(.8f), button, 3.f);
    // Captures and IRs often have long names: they get two lines before an ellipsis.
    TwoLineText(g, Text(13.f, empty ? muted : enabled ? text : IColor(255, 113, 119, 130)), label,
                IRECT(button.L + 8, button.T + 4, button.R - 26, button.B - 4));
    const float iconX = button.R - 15, iconY = button.MH(); // Same library icon as the editor's browser button.
    g.DrawRoundRect(muted, IRECT(iconX - 7, iconY - 5, iconX + 7, iconY + 5), 1.5f);
    g.DrawLine(muted, iconX - 3, iconY - 5, iconX - 3, iconY + 5);
    g.DrawLine(muted, iconX, iconY - 2, iconX + 4, iconY - 2);
    g.DrawLine(muted, iconX, iconY + 2, iconX + 4, iconY + 2);
    if (enabled && PLUG()->IsSlotLoaded(mSlot))
    {
      if (mSlot != 0) // The global IN meter already covers the first slot's input.
        DrawMeterBar(g, IRECT(mCardBounds.L - 7, mCardBounds.T + 5, mCardBounds.L - 4, mCardBounds.B - 5), mMeter.input,
                     tone.first, Now() < mInputClip);
      DrawMeterBar(g, IRECT(mCardBounds.R + 4, mCardBounds.T + 5, mCardBounds.R + 7, mCardBounds.B - 5), mMeter.output,
                   tone.first, Now() < mOutputClip);
    }
    DrawDrop(g);
  }
  void DrawEditor(IGraphics& g)
  {
    using namespace grid_ui;
    auto slotName = GridSlotName(mSlot);
    slotName.erase(std::remove(slotName.begin(), slotName.end(), ' '), slotName.end());
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    const auto path = PLUG()->GetSlotPath(mSlot);
    const bool empty = path.empty() && effect == SlotEffect::NAM;
    const auto tone = Tone(effect, PLUG()->GetSlotGear(mSlot));
    g.DrawText(Text(22.f, tone.first), slotName.c_str(), IRECT(24, 484, 84, 508));
    if (!empty) CategoryBadges(g, effect, PLUG()->GetSlotGear(mSlot), IRECT(444, 484, 620, 508), 13.f, true);
    const bool enabled = GetDelegate()->GetParam(SlotActiveParam(mSlot))->Bool();
    const int distortionStyle = GetDelegate()->GetParam(SlotUtilityParam(mSlot, uDistStyle))->Int();
    const auto hint = std::string(enabled ? "" : "Bypassed | ")
      + (effect == SlotEffect::Distortion ? effect_presentation::Help(effect, uDistStyle, distortionStyle)
                                         : effect_presentation::Sound(effect));
    // The description stops before any option shown on its row (quality, tempo sync, modes).
    float hintRight = 620.f;
    for (const IControl* option : std::initializer_list<const IControl*>{mQualityButton, mUtilityKnobs[uTempoSync],
           mUtilityKnobs[uTapeHeads], mUtilityKnobs[uCosmicStyle], mUtilityKnobs[uColourDesk], mUtilityKnobs[uBlackMode],
           mUtilityKnobs[uDelayPingPong], mUtilityKnobs[uReverbStereo]})
      if (option && !option->IsHidden() && option->GetRECT().T >= 509.f && option->GetRECT().B <= 535.f)
        hintRight = std::min(hintRight, option->GetRECT().L - 10.f);
    if (!empty) FittedText(g, Text(14.f, muted), hint, IRECT(24, 510, hintRight, 533));
    if (HasVisualization() && PLUG()->GetShowVisualizations()) DrawVisualization(g);
  }
  bool IsHit(float x, float y) const override { return mCardBounds.Contains(x, y); }

  // The header row (ID and bypass) only selects. The name and badges below open the
  // browser on release, unless the press became a drag to another tile.
  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    PLUG()->SelectSlot(mSlot);
    mPressX = x; mPressY = y; mDragging = mPressBrowse = mPressDrag = false;
    if (mod.R)
      ShowEditMenu();
    else if (Occupied(mSlot) && x > mCardBounds.R - 32 && y < mCardBounds.T + 32)
    {
      auto* param = GetDelegate()->GetParam(SlotActiveParam(mSlot));
      const double value = param->Bool() ? 0. : 1.;
      GetDelegate()->BeginInformHostOfParamChangeFromUI(SlotActiveParam(mSlot));
      GetDelegate()->SendParameterValueFromUI(SlotActiveParam(mSlot), value);
      GetDelegate()->EndInformHostOfParamChangeFromUI(SlotActiveParam(mSlot));
      mEnabled->SetValueFromDelegate(value);
      SetDirty(false);
    }
    else
    {
      mPressBrowse = NameButton().Contains(x, y);
      mPressDrag = Occupied(mSlot);
    }
  }
  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    const bool hover = NameButton().Contains(x, y);
    if (hover != mNameHover) { mNameHover = hover; SetDirty(false); }
    IControl::OnMouseOver(x, y, mod);
  }
  void OnMouseOut() override
  {
    if (mNameHover) { mNameHover = false; SetDirty(false); }
    IControl::OnMouseOut();
  }
  void OnMouseDrag(float x, float y, float, float, const IMouseMod& mod) override
  {
    if (!mPressDrag || (!mDragging && std::hypot(x - mPressX, y - mPressY) < 5.f)) return;
    mDragging = true;
    UpdateDrop(x, y, mod);
  }
  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    const bool browse = mPressBrowse && !mDragging;
    if (mDragging)
    {
      UpdateDrop(x, y, mod); // Modifiers pressed or released since the last move still count.
      const auto drop = sDrop;
      sDrop = {}; DirtySlots();
      if (drop.target >= 0)
      {
        const auto error = PLUG()->EditSlot(mSlot, static_cast<size_t>(drop.target), drop.mode);
        if (error.empty()) PLUG()->SelectSlot(static_cast<size_t>(drop.target));
        ShowError(error);
      }
    }
    mDragging = mPressBrowse = mPressDrag = false;
    if (browse) OpenBrowser();
  }

  void OnMouseDblClick(float, float, const IMouseMod&) override
  {
    PLUG()->SelectSlot(mSlot);
    OpenBrowser();
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
    mModelButton = new grid_ui::ModuleButton(IRECT(92, 484, 372, 508), [this](IControl*) { ShowMenu(); }, "Choose module...");
    mEditor->AddChildControl(mModelButton);
    mBrowserButton = new grid_ui::BrowserButton(IRECT(340, 484, 372, 508), [this](IControl*) { OpenBrowser(); }, "");
    mBrowserButton->SetTooltip("Browse this module's category or file folder. The current item is selected.");
    mEditor->AddChildControl(mBrowserButton);
    mStepButtons[0] = new grid_ui::Button(IRECT(380, 484, 404, 508), [this](IControl*) { Step(-1); }, "<");
    mStepButtons[1] = new grid_ui::Button(IRECT(410, 484, 434, 508), [this](IControl*) { Step(1); }, ">");
    for (auto* button : mStepButtons) mEditor->AddChildControl(button);
    mQualityButton = new grid_ui::Button(IRECT(444, 510, 620, 534), [this](IControl*) {
      PLUG()->ToggleSlotQuality(mSlot); Refresh();
    }, "Quality: A2");
    mQualityButton->SetTooltip("Click to switch between full A2 and lower-CPU A2 Lite for this capture.");
    mEditor->AddChildControl(mQualityButton);
    mEnabled = new grid_ui::Toggle(grid_ui::ControlBounds(7), SlotActiveParam(mSlot), "Enabled");
    mEditor->AddChildControl(mEnabled);
    mNAMKnobs[0] = new grid_ui::Knob(grid_ui::ControlBounds(0), SlotInputParam(mSlot), "Input", true, mSlot);
    // Gain staging stays in the left column: Input on top, Output below it.
    mNAMKnobs[1] = new grid_ui::Knob(grid_ui::ControlBounds(4), SlotTrimParam(mSlot), "Output", true, mSlot);
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
    const char* eqLabels[] = {"Bass", "Mid", "Treble", "Low cut", "High cut", "Presence", "Output"};
    for (int i = 0; i < 7; ++i)
    {
      mEQKnobs[i] = new grid_ui::Knob(grid_ui::ControlBounds(i == 6 ? effect_presentation::kOutputPosition : i % 3), SlotEQParam(mSlot, i), eqLabels[i], true, mSlot);
      mEditor->AddChildControl(mEQKnobs[i]);
    }
    for (int i = 0; i < kUtilityControls; ++i)
    {
      const auto& p = kEffectParameters[i];
      if (i == uTempoSync)
        mUtilityKnobs[i] = new grid_ui::Toggle(IRECT(380, 510, 490, 534), SlotUtilityParam(mSlot, i), "Tempo sync");
      else if (i == uTapeHeads || i == uCosmicStyle || i == uColourDesk || i == uBlackMode)
        mUtilityKnobs[i] = new grid_ui::Choice(IRECT(496, 510, 620, 534), SlotUtilityParam(mSlot, i));
      else if (i == uDelayPingPong || i == uReverbStereo)
        mUtilityKnobs[i] = new grid_ui::Toggle(IRECT(496, 510, 620, 534), SlotUtilityParam(mSlot, i), p.label);
      else if (EffectChoices(i).empty() && p.minimum == 0. && p.maximum == 1. && p.step == 1.)
        mUtilityKnobs[i] = new grid_ui::Toggle(grid_ui::ControlBounds(p.position), SlotUtilityParam(mSlot, i), p.label);
      else
        mUtilityKnobs[i] = new grid_ui::Knob(grid_ui::ControlBounds(p.position), SlotUtilityParam(mSlot, i), p.label, true, mSlot);
      mEditor->AddChildControl(mUtilityKnobs[i]);
    }
    SetTooltip("Click the ID row to select; click the name button to browse. Drag onto another tile to swap; "
               "Shift-drag replaces, Alt-drag duplicates. Right-click for Clear / Copy / Move / Swap. Top-right toggle switches bypass.");
    Refresh();
  }

  void OnMsgFromDelegate(int message, int size, const void* data) override
  {
    if (message == 0 && data && size == sizeof(iplug::ISenderData<1, grid_eq_signal::Window>))
    {
      if (!ShowsSignalSpectrum(PLUG()->GetSlotEffect(mSlot))) return;
      const auto& window = static_cast<const iplug::ISenderData<1, grid_eq_signal::Window>*>(data)->vals[0];
      if (PLUG()->GetSlotEffect(mSlot) == SlotEffect::Utility)
      {
        // The stereo scope reads the newest samples of both channels.
        std::copy(window.samples.end() - window.right.size(), window.samples.end(), mScopeLeft.begin());
        mScopeRight = window.right;
      }
      const auto analyzed = grid_eq_signal::Analyze(window.samples,
        PLUG()->GetSampleRate() > 0. ? PLUG()->GetSampleRate() : 48000.);
      // Instant rise, gentle fall: transients stay visible without flicker.
      const bool fresh = Now() - mSpectrumTime < .5;
      for (size_t i = 0; i < mSpectrum.size(); ++i)
        mSpectrum[i] = fresh ? std::max(analyzed[i], .65f * mSpectrum[i] + .35f * analyzed[i]) : analyzed[i];
      mSpectrumTime = Now();
      if (mEditor && PLUG()->GetSelectedSlot() == mSlot) mEditor->SetDirty(false);
      return;
    }
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
      const auto monitoredEffect = PLUG()->GetSlotEffect(mSlot);
      mReductionNow = meter.reduction;
      mInputHistory[mReductionWrite] = meter.input;
      mReductionHistory[mReductionWrite] = meter.reduction;
      const bool compressor=monitoredEffect==SlotEffect::SmoothComp || monitoredEffect==SlotEffect::PunchComp;
      mOutputHistory[mReductionWrite] = compressor ? meter.beforeMakeup : meter.output;
      mReductionWrite = (mReductionWrite + 1) % mInputHistory.size();
      if (mEditor && PLUG()->GetSelectedSlot() == mSlot && HasVisualization() && PLUG()->GetShowVisualizations())
        mEditor->SetDirty(false);
      if (meter.input >= 1.f)
        mInputClip = Now() + grid_ui::kClipHoldSeconds;
      if (meter.output >= 1.f)
        mOutputClip = Now() + grid_ui::kClipHoldSeconds;
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
    else if (item->GetTag() == -17 || item->GetTag() == -18)
      PLUG()->OpenLibraryBrowser(nam_library::BrowserContext::Cell,
        item->GetTag() == -17 ? nam_library::FileKind::NAM : nam_library::FileKind::IR, mSlot);
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
    const IRECT bounds(644, 484, 976, 696);
    g.FillRoundRect(IColor(255, 17, 20, 24), bounds, 3.f);
    const bool enabled = GetDelegate()->GetParam(SlotActiveParam(mSlot))->Bool();
    const double mix = GetDelegate()->GetParam(SlotUtilityParam(mSlot, uLoFiMix))->Value() * .01;
    const float opacity = enabled ? static_cast<float>(.3 + .7 * mix) : .25f;
    const auto& history = mMeter.lofi;
    const char* status = !enabled ? "BYPASSED" : mix == 0. ? "DRY" : history.sequence == 0 ? "AWAITING AUDIO" : "PITCH MOTION";
    const IRECT plot(656,542,964,658);
    PlotTitle(g,plot,Tone(SlotEffect::LoFiTape,{}).first,status);
    PlotDetail(g,plot,"Measured head motion | +/-60 cents");
    PlotFooter(g,plot,"Speed also changes bandwidth","2.56 s");
    const auto trace = [&](const char* name, const auto& values, const IColor& color, float center) {
      g.DrawText(Text(13.f, color.WithOpacity(opacity)), name, IRECT(656, center - 38, 790, center - 21));
      constexpr float start = 656.f, end = 964.f, height = 14.f;
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
      g.DrawText(Text(13.f, color.WithOpacity(opacity), EAlign::Far), amount.Get(), IRECT(850, center - 38, 964, center - 21));
    };
    trace("WOW", history.wow, Tone(SlotEffect::LoFiTape,{}).first, 577.f);
    trace("FLUTTER", history.flutter, ampColor, 643.f);
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
    const IRECT box(644, 484, 976, 696), plot(656, 542, 964, 658);
    g.FillRoundRect(IColor(255, 17, 20, 24), box, 3.f);
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    const auto color = Tone(effect, PLUG()->GetSlotGear(mSlot)).first;
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
      // Left: the capture's measured level transfer; Input moves the cloud along
      // it. Right: the neutral post-capture EQ the Bass/Mid/Treble/cuts apply.
      const IRECT scatter(plot.L, plot.T, plot.L + plot.H(), plot.B), eqPlot(scatter.R + 16.f, plot.T, plot.R, plot.B);
      PlotTitle(g,plot,color,"Capture | measured IN > OUT");
      const auto info = PLUG()->GetSlotModelInfo(mSlot);
      WDL_String detail;
      if (info.hasInput) detail.SetFormatted(80, "Peaks 0 / -60 dB | capture cal. %.1f dBu", info.input);
      else detail.Set("Peaks 0 / -60 dB | right: post EQ");
      PlotDetail(g,plot,detail.Get());
      DrawTransferScatter(g, scatter, color, -60.f);
      const auto eqValue = [&](int p) { return GetDelegate()->GetParam(SlotEQParam(mSlot, p))->Value(); };
      const double rate = PLUG()->GetSampleRate() > 0. ? PLUG()->GetSampleRate() : 48000.;
      const double upper = std::min(20000., .49 * rate);
      EQControls eq; eq.bass = eqValue(0); eq.mid = eqValue(1); eq.treble = eqValue(2);
      eq.highPass = eqValue(3); eq.lowPass = eqValue(4); eq.presence = 0.;
      const grid_eq_response::Response tone(rate, SlotEffect::ToneEQ, eq, {});
      const grid_eq_response::Response cuts(rate, SlotEffect::CutEQ, eq, {});
      PlotGrid(g, eqPlot);
      DrawSignalSpectrum(g, eqPlot);
      PlotCurve(g, eqPlot, [&](double x) {
        const double hz = 20. * std::pow(upper / 20., x);
        return 20. * std::log10(std::max(1.e-9, tone.Magnitude(hz) * cuts.Magnitude(hz)));
      }, -12., 12., color);
      PlotFooter(g,plot,"Faint = older | diagonal = unity","Post EQ +/-12");
    }
    else if (effect == SlotEffect::OutboardChannel)
      DrawOutboardChannel(g, plot, color);
    else if (IsOutboardCompressor(effect) || effect == SlotEffect::ConsoleColour || IsAirDynamics(effect))
    {
      WDL_String caption;
      if (effect == SlotEffect::ConsoleColour)
      {
        const int desk = GetDelegate()->GetParam(SlotUtilityParam(mSlot, uColourDesk))->Int();
        caption.SetFormatted(80, "%s | Input / output", effect_modes::deskNames[std::clamp(desk,0,7)]);
      }
      else if(effect==SlotEffect::DigitalBlack)
        caption.SetFormatted(80,"Thr %.0f | Atten %.1f dB",GetDelegate()->GetParam(SlotUtilityParam(mSlot,uBlackThreshold))->Value(),mReductionNow);
      else caption.SetFormatted(80,"Detector GR %.1f dB | IN / OUT",mReductionNow);
      PlotTitle(g,plot,color,caption.Get());
      // Program-dependent compressors have no static curve to draw: their gain reduction
      // over time is what Compress and speed change, so it gets the left half.
      if (effect != SlotEffect::ConsoleColour && effect != SlotEffect::DigitalBlack)
      {
        const IRECT reduction(plot.L, plot.T, plot.MW() - 8.f, plot.B), history(plot.MW() + 8.f, plot.T, plot.R, plot.B);
        DrawReductionHistory(g, reduction);
        levels(-60.f, history);
        PlotDetail(g,plot,"Left: gain reduction | right: gray IN, colour OUT");
        PlotFooter(g,plot,"GR 0 - 24 dB | IN / OUT 0 / -60 dB","Older > now");
        return;
      }
      if (effect == SlotEffect::DigitalBlack)
      {
        g.FillRect(surface,IRECT(plot.L,538,plot.R,541));
        g.FillRect(ampColor,IRECT(plot.L,538,plot.L+plot.W()*std::clamp(mReductionNow/24.f,0.f,1.f),541));
      }
      // The measured IN > OUT cloud shows how the desk / trims or the gate bend the levels.
      const float floorDB = effect==SlotEffect::DigitalBlack ? -90.f : -60.f;
      const IRECT scatter(plot.L, plot.T, plot.L + plot.H(), plot.B), history(scatter.R + 12.f, plot.T, plot.R, plot.B);
      DrawTransferScatter(g, scatter, color, floorDB);
      levels(floorDB, history);
      PlotDetail(g,plot,effect == SlotEffect::DigitalBlack ? "GR before trim | IN > OUT, then history" : "Left: IN > OUT | right: gray IN, colour OUT");
      PlotFooter(g,plot,effect==SlotEffect::ConsoleColour ? "0 dB top / -60 dB floor" : "0 / -90 dB | GR bar 0 - 24 dB","Older > now");
      if(effect==SlotEffect::DigitalBlack)
      {
        const double threshold=GetDelegate()->GetParam(SlotUtilityParam(mSlot,uBlackThreshold))->Value();
        const float y=history.T+history.H()*static_cast<float>(std::clamp(-threshold/90.,0.,1.));
        for(float x=history.L;x<history.R;x+=8.f) g.DrawLine(ampColor,x,y,std::min(x+4.f,history.R),y);
        const float x=scatter.L+scatter.W()*static_cast<float>(std::clamp(1.+threshold/90.,0.,1.));
        for(float py=scatter.T;py<scatter.B;py+=8.f) g.DrawLine(ampColor,x,py,x,std::min(py+4.f,scatter.B));
      }
    }
    else if (effect == SlotEffect::CosmicEcho)
    {
      DrawCosmicSetup(g,plot,[&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot,p))->Value(); },
        PLUG()->GetSampleRate()>0. ? PLUG()->GetSampleRate() : 48000.);
    }
    else if (effect == SlotEffect::NoiseGate || effect == SlotEffect::NoisePrint
        || effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp || effect == SlotEffect::Limiter)
    {
      const bool gate = effect == SlotEffect::NoiseGate;
      const bool smart = effect == SlotEffect::NoisePrint;
      const double threshold = smart ? GetDelegate()->GetParam(SlotUtilityParam(mSlot, uNoiseGateThreshold))->Value()
                             : effect == SlotEffect::Limiter ? GetDelegate()->GetParam(SlotUtilityParam(mSlot, uLimiterCeiling))->Value()
                             : GetDelegate()->GetParam(SlotDynamicsParam(mSlot, gate ? 0 : 3))->Value();
      const bool off = smart && threshold <= -100.;
      WDL_String caption;
      if (off) caption.SetFormatted(64, "Gate Off | GR %.1f dB", mReductionNow);
      else caption.SetFormatted(64, "Thr %.0f | GR %.1f dB", threshold, mReductionNow);
      PlotTitle(g,plot,color,caption.Get());
      const bool compressor=effect==SlotEffect::SmoothComp || effect==SlotEffect::PunchComp;
      const IRECT history(smart ? plot.L : plot.MW()+8,plot.T,plot.R,plot.B);
      const IRECT reductionBar(plot.L,538,plot.R,541);
      g.FillRect(surface,reductionBar);
      g.FillRect(ampColor.WithOpacity(.8f),IRECT(reductionBar.L,reductionBar.T,
        reductionBar.L+reductionBar.W()*std::clamp(mReductionNow/24.f,0.f,1.f),reductionBar.B));
      const float floor=gate || smart ? -100.f : -60.f;
      if(!smart)
      {
        const IRECT transfer(plot.L,plot.T,plot.MW()-8,plot.B);
        PlotGrid(g,transfer);
        const double mix=GetDelegate()->GetParam(SlotUtilityParam(mSlot,uCompMix))->Value()*.01;
        const double makeup=GetDelegate()->GetParam(SlotDynamicsParam(mSlot,5))->Value();
        const double range=GetDelegate()->GetParam(SlotDynamicsParam(mSlot,2))->Value();
        const bool tight=GetDelegate()->GetParam(SlotUtilityParam(mSlot,uGateMode))->Int()==1;
        PlotCurve(g,transfer,[&](double x) { return floor*(1.-x); },floor,18.,muted.WithOpacity(.5f),1.f);
        const auto response=[&](double input) {
          if(gate) return input-(input>=threshold ? 0. : tight ? range : std::min(range,threshold-input));
          if(!compressor) return std::min(input,threshold);
          const double reduction=SlotDynamics::CompressionDB(input,threshold,effect==SlotEffect::PunchComp ? 4. : 2.5,effect==SlotEffect::PunchComp ? 3. : 9.);
          return input+20.*std::log10(1.-mix+mix*std::pow(10.,(makeup-reduction)*.05));
        };
        PlotCurve(g,transfer,[&](double x) { return response(floor*(1.-x)); },floor,18.,color);
        // Where the current input peak sits on the curve: threshold, knee, Mix
        // and the upstream Input level all move this dot.
        const float inputDB=20.f*std::log10(std::max(mMeter.input,1.e-6f));
        if(inputDB>floor)
        {
          const float px=transfer.L+transfer.W()*std::clamp(1.f-inputDB/floor,0.f,1.f);
          const float py=transfer.B-transfer.H()*static_cast<float>(std::clamp((response(inputDB)-floor)/(18.-floor),0.,1.));
          g.FillCircle(text,px,py,3.f);
        }
        WDL_String info;
        const double release=compressor ? GetDelegate()->GetParam(SlotDynamicsParam(mSlot,4))->Value()
                            : gate ? GetDelegate()->GetParam(SlotDynamicsParam(mSlot,1))->Value()
                            : GetDelegate()->GetParam(SlotUtilityParam(mSlot,uLimiterRelease))->Value();
        if(compressor) info.Set("Dot: live input | history before makeup");
        else if(gate) info.SetFormatted(80,"Dot: live | closes 3 dB below | %.0f ms",release);
        else info.SetFormatted(80,"Dot: live input | release %.0f ms",release);
        PlotDetail(g,plot,info.Get());
        info.SetFormatted(64,"IN %.0f..0 / OUT %.0f..+18 dB",floor,floor);
        PlotFooter(g,plot,info.Get(),gate ? "Live 0..-100" : "Live 0..-60");
      }
      else
      {
        PlotDetail(g,plot,"Gray IN / colour OUT | gate line dashed");
        PlotFooter(g,plot,"0 / -100 dB | GR bar 0 - 24 dB","Older > now");
      }
      levels(gate || smart ? -100.f : -60.f, history);
      if (gate)
      {
        // Opens at the threshold, closes only 3 dB below it (after the hold).
        const auto bandY = [&](double db) { return history.T+history.H()*std::clamp(static_cast<float>(db)/floor,0.f,1.f); };
        g.FillRect(ampColor.WithOpacity(.12f),IRECT(history.L,bandY(threshold),history.R,bandY(threshold-3.)));
      }
      if (!off)
      {
        const float y = history.T+history.H()*std::clamp(static_cast<float>(threshold)/floor,0.f,1.f);
        for (float x = history.L; x < history.R; x += 8.f)
          g.DrawLine(ampColor, x, y, std::min(x + 4.f, history.R), y, nullptr, 1.5f);
      }
    }
    else if (effect == SlotEffect::AutoSwell)
    {
      const auto value = [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); };
      const double rise = value(uSwellRise), release = value(uSwellRelease), threshold = value(uSwellThreshold);
      constexpr double hold = 40.;
      const IRECT sketch(plot.L, plot.T, plot.MW() - 8.f, plot.B), history(plot.MW() + 8.f, plot.T, plot.R, plot.B);
      WDL_String text;
      text.SetFormatted(64, "Trigger %.0f dB | gain = OUT / IN", threshold); PlotTitle(g,plot,color,text.Get());
      text.SetFormatted(80, "Rise %.0f | hold 40 | release %.0f ms", rise, release); PlotDetail(g,plot,text.Get());
      // One shared time axis: Rise and Release change the shape, not only labels.
      const double total = rise + hold + 3. * release;
      const float riseEnd = sketch.L + sketch.W() * static_cast<float>(rise / total);
      const float holdEnd = std::max(riseEnd + 2.f, sketch.L + sketch.W() * static_cast<float>((rise + hold) / total));
      PlotGrid(g, sketch);
      g.FillRect(color.WithOpacity(.14f), IRECT(riseEnd, sketch.T, holdEnd, sketch.B));
      PlotCurve(g, sketch, [&](double x) {
        const double t = x * total;
        return t < rise ? t / rise : t < rise + hold ? 1. : std::exp(-(t - rise - hold) / release);
      }, 0., 1., color);
      // Live: OUT / IN is the swell gain. Legato notes inside the release keep
      // the current gain instead of swelling again.
      levels(-72.f, history);
      const auto historyY = [&](double db) { return history.T + history.H() * static_cast<float>(std::clamp(db / -72., 0., 1.)); };
      g.FillRect(ampColor.WithOpacity(.12f), IRECT(history.L, historyY(threshold), history.R, historyY(threshold - 3.)));
      for (float x = history.L; x < history.R; x += 8.f)
        g.DrawLine(ampColor, x, historyY(threshold), std::min(x + 4.f, history.R), historyY(threshold), nullptr, 1.5f);
      text.SetFormatted(64, "Sketch %.1f s | live 0..-72 dB", total * .001);
      PlotFooter(g,plot,text.Get(),"Older > now");
    }
    else if (effect == SlotEffect::ConsoleEQ)
      DrawConsoleEQ(g, plot, color);
    else if (effect == SlotEffect::Utility)
      DrawUtility(g, plot, color);
    else if (effect == SlotEffect::Wah)
    {
      DrawSignalSpectrum(g, plot); // Behind the response curve, as on the other EQ / Filter views.
      DrawSetup(g, effect, plot, color);
    }
    else if (DrawSetup(g, effect, plot, color))
    {
    }
    else if (IsReverb(effect) || effect == SlotEffect::ShimmerReverb)
    {
      DrawSpaceSetup(g,effect,plot,color,[&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot,p))->Value(); },
        PLUG()->GetSampleRate()>0. ? PLUG()->GetSampleRate() : 48000.);
    }
    else if (DrawModulationSetup(g, effect, plot, color,
               [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); },
               PLUG()->GetTempo(), PLUG()->GetSampleRate() > 0. ? PLUG()->GetSampleRate() : 48000.))
    {
    }
    else if (DrawDriveSetup(g, effect, plot, color,
               [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); },
               GetDelegate()->GetParam(SlotEQParam(mSlot, 3))->Value(),
               PLUG()->GetSampleRate() > 0. ? PLUG()->GetSampleRate() : 48000., mMeter.input))
    {
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
      PlotTitle(g,plot,color,caption.Get());
      WDL_String detail;
      if (filter) detail.Set("Shaded: envelope range, full at -12 dBFS");
      else if (effect == SlotEffect::ParametricEQ && value(uEQMode) < .5)
        detail.SetFormatted(80, "Response +/-24 dB | shaded: Q width %.2f oct",
          2. / std::log(2.) * std::asinh(1. / (2. * std::clamp(value(uQ), .1, 20.))));
      else detail.Set("Response +/-24 dB | fill: output signal");
      PlotDetail(g,plot,detail.Get());
      PlotFrequencyAxis(g,plot,upper,"-24 / +24 dB");
      const auto y = [&](float db) { return plot.T + plot.H() * (24.f - std::clamp(db, -24.f, 24.f)) / 48.f; };
      for (int db : {-24, -12, 0, 12, 24})
        g.DrawLine(border.WithOpacity(db == 0 ? .9f : .4f), plot.L, y(static_cast<float>(db)), plot.R, y(static_cast<float>(db)));
      for (double hz : {100., 1000., 10000.})
      {
        const float x = plot.L + static_cast<float>(plot.W() * std::log(hz / 20.) / span);
        if (hz < upper) g.DrawLine(border.WithOpacity(.35f), x, plot.T, x, plot.B);
      }
      const auto frequencyX = [&](double hz) {
        return plot.L + plot.W() * static_cast<float>(std::log(std::clamp(hz, 20., upper) / 20.) / span);
      };
      if (effect == SlotEffect::ParametricEQ && value(uEQMode) < .5)
      {
        // RBJ bell: bandwidth in octaves between the -3 dB-from-peak points.
        const double center = std::min(value(uFrequency), .4 * rate);
        const double octaves = 2. / std::log(2.) * std::asinh(1. / (2. * std::clamp(value(uQ), .1, 20.)));
        g.FillRect(color.WithOpacity(.08f), IRECT(frequencyX(center * std::exp2(-.5 * octaves)), plot.T,
                                                  frequencyX(center * std::exp2(.5 * octaves)), plot.B));
      }
      DrawSignalSpectrum(g, plot);
      for (size_t i = 1; i < mEQCurve.size(); ++i)
      {
        if (filter && (std::abs(key[12] - key[14]) > 1. || std::abs(key[13] - key[15]) > .01))
          g.DrawLine(color.WithOpacity(.5f), plot.L + plot.W() * mEQCurve[i - 1][0], y(mEQCurve[i - 1][2]),
                                           plot.L + plot.W() * mEQCurve[i][0], y(mEQCurve[i][2]), nullptr, 1.f);
        g.DrawLine(color, plot.L + plot.W() * mEQCurve[i - 1][0], y(mEQCurve[i - 1][1]),
                          plot.L + plot.W() * mEQCurve[i][0], y(mEQCurve[i][1]), nullptr, 1.5f);
      }
      if(filter)
      {
        // Cutoff x 2^(Envelope x min(1, 4 x input peak)): the full shift needs
        // -12 dBFS peaks, so quieter playing uses only part of this range.
        const double rest=std::clamp(value(uFilterCutoff),20.,std::min(upper,.4*rate));
        const double full=std::clamp(value(uFilterCutoff)*std::exp2(value(uFilterEnvelope)),20.,std::min(upper,.4*rate));
        g.FillRect(ampColor.WithOpacity(.08f),IRECT(frequencyX(rest),plot.T,frequencyX(full),plot.B));
      }
      if(filter)
        for(double hz : {value(uFilterCutoff),value(uFilterCutoff)*std::exp2(value(uFilterEnvelope))})
        {
          hz=std::clamp(hz,20.,std::min(upper,.4*rate));
          const float x=plot.L+plot.W()*static_cast<float>(std::log(hz/20.)/span);
          for(float py=plot.T;py<plot.B;py+=8.f) g.DrawLine(ampColor.WithOpacity(.6f),x,py,x,std::min(py+4.f,plot.B));
        }
    }
    else
    {
      // A cabinet IR is a fixed filter: its frequency response is the useful view.
      // Computed once per file on the UI thread; audio is never touched.
      UpdateIRResponse();
      PlotTitle(g,plot,color,"IR | frequency response");
      WDL_String detail;
      detail.SetFormatted(80,"Relative to its peak | Level %+.1f dB",GetDelegate()->GetParam(SlotEQParam(mSlot, 6))->Value());
      PlotDetail(g,plot,detail.Get());
      PlotFrequencyAxis(g,plot,mIRUpper,"0 / -36 dB");
      if (mIRResponseValid)
        PlotCurve(g,plot,[&](double x) {
          const double position=x*(mIRResponse.size()-1);
          const size_t i=std::min(mIRResponse.size()-2,static_cast<size_t>(position));
          return mIRResponse[i]+(position-i)*(mIRResponse[i+1]-mIRResponse[i]);
        },-36.,0.,color);
      else
        g.DrawText(Text(13.f,muted,EAlign::Center),"IR file unavailable",plot);
    }
  }
  bool DrawSetup(IGraphics& g, SlotEffect effect, const IRECT& plot, IColor color)
  {
    return grid_ui::DrawEffectSetup(g, effect, plot, color,
      [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); },
      PLUG()->GetTempo(), PLUG()->GetSampleRate() > 0. ? PLUG()->GetSampleRate() : 48000.);
  }
  // Utility reshapes stereo, so it shows the live output as a goniometer (mid up, side across)
  // with a correlation meter: mono is a vertical line, Width spreads it, Balance tilts it and
  // one inverted channel turns it horizontal with negative correlation.
  void DrawUtility(IGraphics& g, const IRECT& plot, IColor color)
  {
    using namespace grid_ui;
    const auto value = [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); };
    WDL_String caption;
    caption.SetFormatted(96, "Width %+.0f%% | Balance %+.0f%% | Polarity %s / %s", value(uUtilityStereo),
                         value(uUtilityBalance), value(uUtilityInvertL) >= .5 ? "-" : "+", value(uUtilityInvertR) >= .5 ? "-" : "+");
    PlotTitle(g, plot, color, caption.Get());
    PlotDetail(g, plot, "Goniometer: up = mono, across = side | correlation");
    const IRECT scope(plot.L, plot.T, plot.L + plot.H(), plot.B), meters(scope.R + 18.f, plot.T, plot.R, plot.B);
    g.DrawRect(border.WithOpacity(.5f), scope);
    g.DrawLine(border.WithOpacity(.6f), scope.MW(), scope.T, scope.MW(), scope.B);
    g.DrawLine(border.WithOpacity(.35f), scope.L, scope.T, scope.R, scope.B);
    g.DrawLine(border.WithOpacity(.35f), scope.R, scope.T, scope.L, scope.B);
    g.DrawText(Text(11.f, muted), "L", IRECT(scope.L + 3.f, scope.T, scope.L + 20.f, scope.T + 13.f));
    g.DrawText(Text(11.f, muted, EAlign::Far), "R", IRECT(scope.R - 20.f, scope.T, scope.R - 3.f, scope.T + 13.f));
    const bool live = Now() - mSpectrumTime < .5;
    double peak = 1.e-4, sumL = 0., sumR = 0., product = 0.;
    for (size_t i = 0; i < mScopeLeft.size(); ++i)
    {
      peak = std::max({peak, std::abs(static_cast<double>(mScopeLeft[i])), std::abs(static_cast<double>(mScopeRight[i]))});
      sumL += mScopeLeft[i] * mScopeLeft[i]; sumR += mScopeRight[i] * mScopeRight[i]; product += mScopeLeft[i] * mScopeRight[i];
    }
    const bool signal = live && peak > 1.e-3;
    if (signal)
    {
      // Auto-scaled so the shape stays readable at any level; level is on the meters.
      const float half = scope.W() * .5f * .92f;
      for (size_t i = 0; i < mScopeLeft.size(); ++i)
      {
        const double l = mScopeLeft[i] / peak, r = mScopeRight[i] / peak;
        const float x = scope.MW() + half * static_cast<float>((r - l) * .70710678);
        const float y = scope.MH() - half * static_cast<float>((l + r) * .70710678);
        g.FillRect(color.WithOpacity(.55f), IRECT(x - .75f, y - .75f, x + .75f, y + .75f));
      }
    }
    else g.DrawText(Text(12.f, muted, EAlign::Center), "Play to see", scope);
    // Correlation, then left and right RMS on one dB scale.
    const double correlation = signal && sumL > 0. && sumR > 0. ? product / std::sqrt(sumL * sumR) : 0.;
    const auto bar = [&](float top, const char* label) {
      const IRECT r(meters.L + 22.f, top, meters.R, top + 10.f);
      g.DrawText(Text(11.f, muted), label, IRECT(meters.L, top - 2.f, meters.L + 20.f, top + 12.f));
      g.FillRect(surface, r); g.DrawRect(border, r);
      return r;
    };
    const IRECT corr = bar(meters.T + 10.f, "Cor");
    g.DrawLine(muted, corr.MW(), corr.T - 3.f, corr.MW(), corr.B + 3.f);
    if (signal)
    {
      const float x = corr.MW() + corr.W() * .5f * static_cast<float>(std::clamp(correlation, -1., 1.));
      g.FillRect(correlation < 0. ? ampColor : color, IRECT(std::min(x, corr.MW()), corr.T, std::max(x, corr.MW()), corr.B));
    }
    g.DrawText(Text(11.f, muted), "-1", IRECT(corr.L, corr.B + 2.f, corr.L + 20.f, corr.B + 14.f));
    g.DrawText(Text(11.f, muted, EAlign::Far), "+1", IRECT(corr.R - 20.f, corr.B + 2.f, corr.R, corr.B + 14.f));
    const auto level = [&](float top, const char* label, double sum) {
      const IRECT r = bar(top, label);
      const double rms = signal ? std::sqrt(sum / mScopeLeft.size()) : 0.;
      const double fill = std::clamp((20. * std::log10(std::max(1.e-6, rms)) + 60.) / 60., 0., 1.);
      g.FillRect(color, IRECT(r.L, r.T, r.L + r.W() * static_cast<float>(fill), r.B));
    };
    level(meters.T + 52.f, "L", sumL);
    level(meters.T + 72.f, "R", sumR);
    WDL_String footer;
    footer.SetFormatted(64, "Fine delay %.2f ms", value(uUtilityDelay));
    PlotFooter(g, plot, "Scope auto-scaled | RMS -60 to 0 dB", footer.Get());
  }
  // Outboard Channel is a cut EQ, a compressor and a saturator in series; each gets its own view.
  void DrawOutboardChannel(IGraphics& g, const IRECT& plot, IColor color)
  {
    using namespace grid_ui;
    const auto value = [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); };
    const double rate = PLUG()->GetSampleRate() > 0. ? PLUG()->GetSampleRate() : 48000.;
    WDL_String caption;
    caption.SetFormatted(96, "GR %.1f dB | Speed %.0f%% | Saturate %.0f%%", mReductionNow, value(uStripSpeed), value(uStripSaturate));
    PlotTitle(g, plot, color, caption.Get());
    PlotDetail(g, plot, "Cuts + output spectrum | saturation | GR");
    const IRECT eq(plot.L, plot.T, plot.L + 134.f, plot.B), shaper(eq.R + 10.f, plot.T, eq.R + 100.f, plot.B),
      reduction(shaper.R + 10.f, plot.T, plot.R, plot.B);

    // Cuts: the module's own DSP measured with compression and saturation off.
    UpdateMeasuredResponse(SlotEffect::OutboardChannel, {value(uStripLowCut), value(uStripHighCut), 0., 25., 0., 0., 100.}, rate);
    const double upper = std::min(20000., .49 * rate);
    PlotGrid(g, eq);
    for (double hz : {100., 1000., 10000.})
      if (hz < upper)
      {
        const float x = eq.L + eq.W() * static_cast<float>(std::log(hz / 20.) / std::log(upper / 20.));
        g.DrawLine(border.WithOpacity(.35f), x, eq.T, x, eq.B);
      }
    DrawSignalSpectrum(g, eq, false);
    PlotCurve(g, eq, [&](double x) {
      return MeasuredResponse(x);
    }, -24., 6., color);

    // Saturation: CStrip2's static Spiral stage at the current Saturate, against unity.
    const double saturate = value(uStripSaturate) * .01;
    const auto spiral = [saturate](double x) {
      if (saturate <= 0.) return x; // The native stage is bypassed at unity output gain.
      const double raw = x * (1. + saturate), clipped = std::clamp(raw, -1., 1.) * 1.2533141373155;
      const double shaped = clipped == 0. ? 0. : std::sin(clipped * std::abs(clipped)) / std::abs(clipped);
      return raw * (1. - saturate) + shaped * saturate;
    };
    g.DrawRect(border.WithOpacity(.5f), shaper);
    g.DrawLine(border.WithOpacity(.6f), shaper.L, shaper.B, shaper.R, shaper.T);
    PlotCurve(g, shaper, [&](double x) { return spiral(x); }, 0., 1.2, color);
    const double input = std::min(1., static_cast<double>(mMeter.input));
    if (input > 1.e-3)
      g.FillCircle(text, shaper.L + shaper.W() * static_cast<float>(input),
                   shaper.B - shaper.H() * static_cast<float>(std::clamp(spiral(input) / 1.2, 0., 1.)), 2.5f);
    DrawReductionHistory(g, reduction);
    PlotFooter(g, plot, "Cuts -24 / +6 dB | curve 0 - 1.2, dot: input peak", "GR 0 - 24 dB");
  }
  // Console EQ: the measured small-signal response over the live output spectrum. Its band
  // density is level-dependent, so louder notes add harmonics this curve does not show.
  void DrawConsoleEQ(IGraphics& g, const IRECT& plot, IColor color)
  {
    using namespace grid_ui;
    const auto value = [&](int p) { return GetDelegate()->GetParam(SlotUtilityParam(mSlot, p))->Value(); };
    const double rate = PLUG()->GetSampleRate() > 0. ? PLUG()->GetSampleRate() : 48000.;
    std::array<double, 7> controls{};
    for (int i = 0; i < 7; ++i) controls[i] = value(uConsoleTreble + i);
    UpdateMeasuredResponse(SlotEffect::ConsoleEQ, controls, rate);
    WDL_String caption;
    caption.SetFormatted(96, "Bass %+.1f | Mid %+.1f | Treble %+.1f", value(uConsoleBass), value(uConsoleMid), value(uConsoleTreble));
    PlotTitle(g, plot, color, caption.Get());
    PlotDetail(g, plot, "Quiet-signal response | fill: output signal");
    PlotFrequencyAxis(g, plot, std::min(20000., .49 * rate), "-12 / +12 dB");
    DrawSignalSpectrum(g, plot);
    PlotCurve(g, plot, [&](double x) { return MeasuredResponse(x); }, -12., 12., color);
  }
  double MeasuredResponse(double x) const
  {
    const double position = x * (mResponse.size() - 1);
    const size_t i = std::min(mResponse.size() - 2, static_cast<size_t>(position));
    return mResponse[i] + (position - i) * (mResponse[i + 1] - mResponse[i]);
  }
  // Magnitude from a quiet impulse through a private copy of the module's DSP. With
  // Outboard Channel's compression and saturation off, and at Console EQ's quiet-signal
  // level, the strip is linear, so this is the response the controls actually produce.
  void UpdateMeasuredResponse(SlotEffect effect, const std::array<double, 7>& values, double rate)
  {
    std::array<double, 9> key{static_cast<double>(effect), rate};
    std::copy(values.begin(), values.end(), key.begin() + 2);
    if (key == mResponseKey) return;
    mResponseKey = key;
    SlotOutboard strip;
    strip.Reset(rate, effect);
    UtilityControls controls;
    (effect == SlotEffect::ConsoleEQ ? controls.consoleEQ : controls.strip) = values;
    constexpr int length = 4096;
    constexpr double level = 1.e-3;
    std::vector<double> silence(length, 0.), impulse(length, 0.);
    strip.Process(silence.data(), nullptr, length, controls); // Let control smoothing settle first.
    impulse[0] = level;
    strip.Process(impulse.data(), nullptr, length, controls);
    const double upper = std::min(20000., .49 * rate);
    for (size_t i = 0; i < mResponse.size(); ++i)
    {
      const double hz = 20. * std::pow(upper / 20., static_cast<double>(i) / (mResponse.size() - 1));
      const auto step = std::polar(1., -2. * MATH_PI * hz / rate);
      std::complex<double> phase(1., 0.), sum(0., 0.);
      for (int n = 0; n < length; ++n)
      {
        sum += impulse[n] * phase;
        phase *= step;
        if ((n & 1023) == 1023) phase /= std::abs(phase); // Keep the rotation on the unit circle.
      }
      mResponse[i] = static_cast<float>(20. * std::log10(std::max(1.e-12, std::abs(sum) / level)));
    }
  }
  // Gain reduction over time, hanging from 0 dB at the top like a console GR meter.
  void DrawReductionHistory(IGraphics& g, const IRECT& area)
  {
    using namespace grid_ui;
    constexpr float range = 24.f;
    g.DrawRect(border.WithOpacity(.5f), area);
    for (float db : {6.f, 12.f, 18.f})
    {
      const float y = area.T + area.H() * db / range;
      g.DrawLine(border.WithOpacity(.4f), area.L, y, area.R, y);
      WDL_String label; label.SetFormatted(8, "-%.0f", db);
      g.DrawText(Text(11.f, muted, EAlign::Far), label.Get(), IRECT(area.R - 26.f, y - 12.f, area.R - 3.f, y));
    }
    g.DrawText(Text(11.f, muted), "GR", IRECT(area.L + 3.f, area.T, area.L + 30.f, area.T + 13.f));
    const auto size = mReductionHistory.size();
    const float step = area.W() / static_cast<float>(size - 1);
    float previous = area.T;
    for (size_t i = 0; i < size; ++i)
    {
      const float reduction = mReductionHistory[(mReductionWrite + i) % size];
      const float y = area.T + area.H() * std::clamp(reduction / range, 0.f, 1.f);
      const float x = area.L + step * static_cast<float>(i);
      g.FillRect(ampColor.WithOpacity(.3f), IRECT(std::max(area.L, x - .5f * step), area.T, std::min(area.R, x + .5f * step), y));
      if (i) g.DrawLine(ampColor, x - step, previous, x, y, nullptr, 1.5f);
      previous = y;
    }
  }
  // Live output spectrum as a filled area on its own dBFS scale: a different
  // form from the response line. Stale spectra (no window for 0.5 s) are hidden.
  void DrawSignalSpectrum(IGraphics& g, const IRECT& area, bool labels = true)
  {
    using namespace grid_ui;
    if (Now() - mSpectrumTime >= .5) return;
    const auto signalY = [&](float db) {
      return area.B - area.H() * std::clamp((db + 72.f) / 72.f, 0.f, 1.f);
    };
    const float step = area.W() / static_cast<float>(mSpectrum.size() - 1);
    for (size_t i = 0; i < mSpectrum.size(); ++i)
    {
      const float x = area.L + step * static_cast<float>(i);
      g.FillRect(ampColor.WithOpacity(.2f), IRECT(std::max(area.L, x - .5f * step), signalY(mSpectrum[i]),
                                                  std::min(area.R, x + .5f * step), area.B));
    }
    if (!labels) return;
    g.DrawText(Text(11.f, ampColor, EAlign::Far), "0 dBFS", IRECT(area.R - 60.f, area.T, area.R - 2.f, area.T + 12.f));
    g.DrawText(Text(11.f, ampColor, EAlign::Far), "-72", IRECT(area.R - 60.f, area.B - 12.f, area.R - 2.f, area.B));
  }
  // Measured IN vs OUT peaks as a cloud: the module's real level transfer
  // emerges from playing. Newer points are brighter; the diagonal is unity.
  void DrawTransferScatter(IGraphics& g, const IRECT& area, IColor color, float floorDB)
  {
    using namespace grid_ui;
    g.DrawRect(border.WithOpacity(.5f), area);
    g.DrawLine(border.WithOpacity(.6f), area.L, area.B, area.R, area.T);
    const auto position = [&](float db) { return std::clamp(1.f - db / floorDB, 0.f, 1.f); };
    for (size_t i = 0; i < mInputHistory.size(); ++i)
    {
      const size_t index = (mReductionWrite + i) % mInputHistory.size();
      const float in = 20.f * std::log10(std::max(mInputHistory[index], 1.e-6f));
      const float out = 20.f * std::log10(std::max(mOutputHistory[index], 1.e-6f));
      if (in <= floorDB) continue;
      const float age = static_cast<float>(i) / static_cast<float>(mInputHistory.size() - 1);
      g.FillCircle(color.WithOpacity(.15f + .85f * age), area.L + area.W() * position(in), area.B - area.H() * position(out), 1.6f);
    }
    g.DrawText(Text(11.f, muted, EAlign::Far), "IN", IRECT(area.R - 22.f, area.B - 13.f, area.R - 2.f, area.B));
    g.DrawText(Text(11.f, muted), "OUT", IRECT(area.L + 2.f, area.T, area.L + 32.f, area.T + 13.f));
  }
  void UpdateIRResponse()
  {
    const auto path = PLUG()->GetSlotPath(mSlot);
    if (path == mIRResponsePath) return;
    mIRResponsePath = path; mIRResponseValid = false;
    std::vector<float> taps; double rate = 0.;
    if (path.empty() || dsp::wav::Load(path.c_str(), taps, rate) != dsp::wav::LoadReturnCode::SUCCESS || taps.empty() || !(rate > 0.))
      return;
    taps.resize(std::min<size_t>(taps.size(), 32768)); // Cabinet IRs are far shorter; cap pathological files.
    mIRUpper = std::min(20000., .49 * rate);
    float peak = -1.e9f;
    for (size_t i = 0; i < mIRResponse.size(); ++i)
    {
      const double hz = 20. * std::pow(mIRUpper / 20., static_cast<double>(i) / (mIRResponse.size() - 1));
      const auto step = std::polar(1., -2. * MATH_PI * hz / rate);
      std::complex<double> phase(1., 0.), sum(0., 0.);
      for (size_t n = 0; n < taps.size(); ++n)
      {
        sum += static_cast<double>(taps[n]) * phase;
        phase *= step;
        if ((n & 1023) == 1023) phase /= std::abs(phase); // Keep the rotation on the unit circle.
      }
      mIRResponse[i] = static_cast<float>(20. * std::log10(std::max(1.e-12, std::abs(sum))));
      peak = std::max(peak, mIRResponse[i]);
    }
    for (auto& value : mIRResponse) value -= peak;
    mIRResponseValid = std::isfinite(peak);
  }
  void Refresh()
  {
    if (!mModelButton)
      return;
    const auto path = PLUG()->GetSlotPath(mSlot);
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    if (effect != mPlotEffect || path != mPlotPath)
    {
      mPlotEffect = effect; mPlotPath = path; mInputHistory = {}; mOutputHistory = {}; mReductionHistory = {};
      mMeter.lofi = {}; mSpectrumTime = 0.;
      mReductionWrite = 0; mReductionNow = 0.f;
    }
    const bool nam = effect == SlotEffect::NAM;
    const bool gate = effect == SlotEffect::NoiseGate;
    const bool compressor = effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp;
    const bool ir = effect == SlotEffect::CabinetIR;
    const bool selected = PLUG()->GetSelectedSlot() == mSlot;
    mEditor->Hide(!selected || PLUG()->IsTunerActive());
    if (selected)
    {
      const bool empty = nam && path.empty();
      mEnabled->Hide(empty);
      for (auto* button : mStepButtons) button->Hide(empty);
      mBrowserButton->Hide(false);
      mBrowserButton->SetTooltip("Browse this module's category or file folder. The current item is selected.");
      mQualityButton->Hide(!PLUG()->CanToggleSlotQuality(mSlot));
      mQualityButton->SetLabelStr(GetDelegate()->GetParam(SlotFullParam(mSlot))->Bool() ? "Quality: A2" : "Quality: A2 Lite");
      mModelButton->SetRECT(IRECT(92, 484, 334, 508));
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
        mEQKnobs[3]->SetRECT(grid_ui::ControlBounds(5));
        mEQKnobs[4]->SetRECT(grid_ui::ControlBounds(6));
      }
      if (effect == SlotEffect::Boost || effect == SlotEffect::TubeBoost)
      {
        mEQKnobs[3]->Hide(false);
        mEQKnobs[3]->SetRECT(grid_ui::ControlBounds(2));
      }
      for (int i = 0; i < 3; ++i)
        mEQKnobs[i]->SetRECT(grid_ui::ControlBounds(nam ? i + 1 : i));
    }
    const auto moduleLabel = path.empty() ? nam ? std::string("+ Add module") : std::string(SlotEffectName(effect))
                                          : nam_library::PathText(std::filesystem::u8path(path).stem());
    mModelButton->SetLabelStr(moduleLabel.c_str());
    mModelButton->SetDirty(false);
    mModelButton->SetTooltip(("Choose or replace the module. " + moduleLabel).c_str());
    SetTooltip(path.empty() ? "Click to browse modules. Right-click for Clear / Copy / Move / Swap."
                            : path.c_str());
    if (mEditor && selected) mEditor->SetDirty(false);
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
  void OpenBrowser()
  {
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    PLUG()->OpenLibraryBrowser(nam_library::BrowserContext::Cell,
      effect == SlotEffect::CabinetIR ? nam_library::FileKind::IR
      : effect == SlotEffect::NAM && !PLUG()->GetSlotPath(mSlot).empty() ? nam_library::FileKind::NAM
      : nam_library::FileKind::Module, mSlot);
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
    const bool ir = effect == SlotEffect::CabinetIR;
    const auto files = nam_library::SiblingFiles(
      path, ir, PLUG()->GetLibraryFolders(ir ? nam_library::FileKind::IR : nam_library::FileKind::NAM));
    if (files.empty()) return;
    const auto found = std::find(files.begin(), files.end(), path);
    const int current = found == files.end() ? -1 : static_cast<int>(found - files.begin());
    const int count = static_cast<int>(files.size());
    const int next = current < 0 ? (direction > 0 ? 0 : count - 1) : (current + direction + count) % count;
    Load(nam_library::PathText(files[next]));
  }
  bool Occupied(size_t slot)
  { return PLUG()->GetSlotEffect(slot) != SlotEffect::NAM || !PLUG()->GetSlotPath(slot).empty(); }
  // Drag conventions follow DAW devices: plain drag swaps (or moves into an empty tile),
  // Shift replaces the target, Alt/Option duplicates (Ctrl/Cmd also, as in Ableton and Reaper).
  static SlotEditMode DropMode(const IMouseMod& mod, bool occupied)
  { return mod.A || mod.C ? SlotEditMode::Copy : mod.S || !occupied ? SlotEditMode::Move : SlotEditMode::Swap; }
  void UpdateDrop(float x, float y, const IMouseMod& mod)
  {
    Drop drop{GetUI(), static_cast<int>(mSlot)};
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
      if (slot != mSlot && GetUI()->GetControlWithTag(kCtrlTagGridSlotBase + static_cast<int>(slot))->IsHit(x, y))
      {
        drop.target = static_cast<int>(slot);
        drop.occupied = Occupied(slot);
        drop.mode = DropMode(mod, drop.occupied);
      }
    if (drop.target != sDrop.target || drop.mode != sDrop.mode || drop.source != sDrop.source) { sDrop = drop; DirtySlots(); }
  }
  void DirtySlots()
  {
    for (size_t slot = 0; slot < FixedNAMGrid::kSlots; ++slot)
      GetUI()->GetControlWithTag(kCtrlTagGridSlotBase + static_cast<int>(slot))->SetDirty(false);
  }
  void DrawDrop(IGraphics& g)
  {
    using namespace grid_ui;
    if (sDrop.ui != GetUI()) return;
    if (sDrop.source == static_cast<int>(mSlot))
      g.FillRoundRect(background.WithOpacity(.55f), mCardBounds, 4.f);
    if (sDrop.target != static_cast<int>(mSlot)) return;
    g.FillRoundRect(background.WithOpacity(.75f), mCardBounds, 4.f);
    g.DrawRoundRect(accent, mCardBounds.GetPadded(-1.f), 4.f, nullptr, 2.f);
    const char* action = sDrop.mode == SlotEditMode::Swap ? "Swap"
                       : sDrop.mode == SlotEditMode::Move ? (sDrop.occupied ? "Replace" : "Move here")
                       : sDrop.occupied ? "Replace with copy" : "Duplicate here";
    g.DrawText(Text(15.f, accent, EAlign::Center), action,
               IRECT(mCardBounds.L, mCardBounds.T + 16, mCardBounds.R, mCardBounds.T + 42));
    g.DrawText(Text(12.f, muted, EAlign::Center), "Shift replaces | Alt duplicates",
               IRECT(mCardBounds.L, mCardBounds.T + 44, mCardBounds.R, mCardBounds.T + 62));
  }
  IRECT NameButton() const { return IRECT(mCardBounds.L + 8, mCardBounds.T + 30, mCardBounds.R - 8, mCardBounds.B - 7); }
  bool mNameHover = false;
  // One drag at a time per editor; the owning UI keeps other plugin instances unaffected.
  struct Drop { const IGraphics* ui = nullptr; int source = -1, target = -1; SlotEditMode mode = SlotEditMode::Swap; bool occupied = false; };
  static inline Drop sDrop;
  float mPressX = 0.f, mPressY = 0.f;
  bool mPressBrowse = false, mPressDrag = false, mDragging = false;
  IRECT mCardBounds;
  size_t mSlot;
  grid_ui::Editor* mEditor = nullptr;
  IVButtonControl* mModelButton = nullptr;
  grid_ui::BrowserButton* mBrowserButton = nullptr;
  std::array<grid_ui::Button*, 2> mStepButtons{};
  grid_ui::Button* mQualityButton = nullptr;
  SlotEffect mPlotEffect = SlotEffect::NAM;
  std::string mPlotPath;
  std::array<float, 120> mInputHistory{}, mOutputHistory{}, mReductionHistory{};
  std::array<float, 96> mResponse{};
  std::array<float, grid_eq_signal::scopeSize> mScopeLeft{}, mScopeRight{};
  std::array<double, 9> mResponseKey{-1.};
  std::array<double, 17> mEQKey{};
  std::array<std::array<float, 3>, 162> mEQCurve{};
  std::array<float, grid_eq_signal::bands> mSpectrum{};
  double mSpectrumTime = 0.; // Last window received; stale spectra are hidden.
  std::string mIRResponsePath; // Cabinet IR whose response is cached below.
  std::array<float, 120> mIRResponse{};
  double mIRUpper = 20000.;
  bool mIRResponseValid = false;
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
    g.DrawText(Text(13.f, muted), "LIBRARY FOLDERS", IRECT(32, 160, 400, 190));
    const auto rows = Rows();
    mScroll = std::min(mScroll, std::max(0, static_cast<int>(rows.size()) - kRows));
    for (int row = 0; row < kRows && mScroll + row < static_cast<int>(rows.size()); ++row)
    {
      const auto& entry = rows[mScroll + row];
      const float top = kListTop + row * kRowPitch;
      g.FillRoundRect(surface, IRECT(32, top, 968, top + kRowHeight), 3.f);
      g.DrawText(Text(14.f, muted), KindLabel(entry.kind), IRECT(44, top, 124, top + kRowHeight));
      FittedText(g, Text(14.f), entry.path.c_str(), IRECT(128, top, 882, top + kRowHeight));
      g.DrawText(Text(14.f, muted, EAlign::Center), "Remove", IRECT(888, top, 962, top + kRowHeight));
    }
    if (rows.empty())
      g.DrawText(Text(14.f, muted), "No library folders. Add NAM, IR and preset folders; each is scanned only for its own file type.",
                 IRECT(44, kListTop, 960, kListTop + kRowHeight));
    g.DrawText(Text(14.f, muted), PLUG()->GetCaptureStatus().c_str(), IRECT(32, 476, 960, 506));
  }
  void OnAttached() override
  {
    using namespace grid_ui;
    AddChildControl(new HintLabel(IRECT(32, 24, 650, 64), "Library & settings", Text(23.f),
      "NAM, IR and preset folders are separate lists; each folder is scanned only for its own file type. Add a folder to several lists if it mixes types. Up to 32 folders per type."));
    AddChildControl(new Button(IRECT(862, 28, 968, 60), [this](IControl*) { Hide(true); }, "Close"));
    AddChildControl(new Button(IRECT(678, 28, 846, 60), [this](IControl*) {
      PLUG()->OpenLibraryBrowser(nam_library::BrowserContext::Library, nam_library::FileKind::NAM);
    }, "Browse library"));
    for (int i = 0; i < 3; ++i)
    {
      const auto kind = kKinds[i];
      mAdd[i] = new Button(kAddRects[i], [this, kind](IControl*) {
        WDL_String folder;
        GetUI()->PromptForDirectory(folder, [plugin = PLUG(), kind](const WDL_String&, const WDL_String& chosen) {
          if (chosen.GetLength()) plugin->AddLibraryFolder(kind, chosen.Get());
        });
      }, i == 0 ? "+ NAM folder" : i == 1 ? "+ IR folder" : "+ Preset folder");
      mAdd[i]->SetTooltip(i == 0 ? "Add a folder of .nam captures. Only .nam files in it and its subfolders are indexed."
                          : i == 1 ? "Add a folder of .wav impulse responses. Only .wav files in it and its subfolders are indexed."
                                   : "Add a rig preset folder. Only .ngrid files in it and its subfolders are indexed.");
      AddChildControl(mAdd[i]);
    }
    AddChildControl(
      new Button(IRECT(32, 116, 152, 148), [this](IControl*) { PLUG()->RescanCaptureFolder(); }, "Rescan all"))->SetTooltip(
      "Index NAM, IR and preset folders. Use this when the library is not indexed or files have changed.");
    mStartupScan = new Button(
      IRECT(168, 116, 398, 148), [this](IControl*) { PLUG()->SetRescanOnStartup(!PLUG()->GetRescanOnStartup()); },
      "Startup rescan: ON");
    mStartupScan->SetTooltip(
      "Automatically index library folders when opening an instance or project. When off, choose Rescan all manually.");
    AddChildControl(mStartupScan);
    mVisualizations = new Button(IRECT(414, 116, 650, 148), [this](IControl*) {
      PLUG()->SetShowVisualizations(!PLUG()->GetShowVisualizations());
    }, "Visualizations: ON");
    AddChildControl(mVisualizations);
    mVisualizations->SetTooltip("Show parameter plots and level displays beside the selected module's controls.");
    mPresetSwitch = new Button(IRECT(666, 116, 902, 148), [this](IControl*) {
      PLUG()->SetPresetCrossfade(!PLUG()->GetPresetCrossfade());
    }, "Preset switch: Short fade");
    AddChildControl(mPresetSwitch);
    mPresetSwitch->SetTooltip("How rig presets change while audio plays. Short fade dips the output for about 25 ms "
                              "and runs one rig at a time. Crossfade blends both rigs over 40 ms for a seamless change, "
                              "briefly using about twice the CPU.");
  }
  void Hide(bool hide) override
  {
    IContainerBase::Hide(hide);
    if (GetUI())
      GetUI()->SetAllControlsDirty();
  }
  bool IsDirty() override
  {
    const char* scanLabel = PLUG()->GetRescanOnStartup() ? "Startup rescan: ON" : "Startup rescan: OFF";
    const char* visualLabel = PLUG()->GetShowVisualizations() ? "Visualizations: ON" : "Visualizations: OFF";
    if (mVisualizations && std::strcmp(mVisualizations->GetLabelStr(), visualLabel) != 0)
      mVisualizations->SetLabelStr(visualLabel);
    const char* switchLabel = PLUG()->GetPresetCrossfade() ? "Preset switch: Crossfade" : "Preset switch: Short fade";
    if (mPresetSwitch && std::strcmp(mPresetSwitch->GetLabelStr(), switchLabel) != 0)
      mPresetSwitch->SetLabelStr(switchLabel);
    if (mStartupScan && std::strcmp(mStartupScan->GetLabelStr(), scanLabel) != 0)
      mStartupScan->SetLabelStr(scanLabel);
    for (int i = 0; i < 3; ++i)
    {
      const bool full = PLUG()->GetLibraryFolders(kKinds[i]).size() >= 32;
      if (mAdd[i] && mAdd[i]->IsDisabled() != full)
        mAdd[i]->SetDisabled(full);
    }
    return IContainerBase::IsDirty();
  }
  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (x < 888 || x > 962 || y < kListTop || y >= kListTop + kRows * kRowPitch)
      return;
    const int row = static_cast<int>((y - kListTop) / kRowPitch);
    if (y > kListTop + row * kRowPitch + kRowHeight)
      return;
    const auto rows = Rows();
    if (mScroll + row >= static_cast<int>(rows.size()))
      return;
    const auto& entry = rows[mScroll + row];
    PLUG()->RemoveLibraryFolder(entry.kind, entry.index);
    SetDirty(false);
  }
  void OnMouseWheel(float, float y, const IMouseMod&, float delta) override
  {
    if (y < kListTop || y > kListTop + kRows * kRowPitch)
      return;
    mScroll = std::clamp(mScroll + (delta < 0 ? 1 : -1), 0, std::max(0, static_cast<int>(Rows().size()) - kRows));
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
  // One list of all source folders, grouped by the single file kind each one indexes.
  static constexpr std::array<nam_library::FileKind, 3> kKinds{
    nam_library::FileKind::NAM, nam_library::FileKind::IR, nam_library::FileKind::RigPreset};
  inline static const std::array<IRECT, 3> kAddRects{IRECT(544, 160, 672, 190), IRECT(684, 160, 812, 190), IRECT(824, 160, 968, 190)};
  static constexpr float kListTop = 200.f, kRowPitch = 38.f, kRowHeight = 32.f;
  static constexpr int kRows = 7;
  struct Row { nam_library::FileKind kind; size_t index; std::string path; };
  std::vector<Row> Rows()
  {
    std::vector<Row> rows;
    for (const auto kind : kKinds)
    {
      const auto folders = PLUG()->GetLibraryFolders(kind);
      for (size_t i = 0; i < folders.size(); ++i) rows.push_back({kind, i, folders[i]});
    }
    return rows;
  }
  static const char* KindLabel(nam_library::FileKind kind)
  { return kind == nam_library::FileKind::NAM ? "NAM" : kind == nam_library::FileKind::IR ? "IR" : "Presets"; }
  int mScroll = 0;
  std::array<grid_ui::Button*, 3> mAdd{};
  grid_ui::Button* mStartupScan = nullptr;
  grid_ui::Button* mVisualizations = nullptr;
  grid_ui::Button* mPresetSwitch = nullptr;
};


class NAMGridTunerControl : public IContainerBase
{
public:
  explicit NAMGridTunerControl(const IRECT& r) : IContainerBase(r) {}
  void OnAttached() override
  {
    using namespace grid_ui;
    AddChildControl(new Button(IRECT(896, 488, 976, 512), [this](IControl*) {
      DisableTunerFromUI(GetUI());
    }, "Close"));
    AddChildControl(new Knob(IRECT(216, 536, 340, 634), kTunerReference, "Reference"));
    AddChildControl(new Toggle(IRECT(216, 646, 340, 680), kTunerMute, "Mute output"));
  }
  void Refresh()
  {
    ForAllChildrenFunc([this](int, IControl* control) {
      const int parameter = control->GetParamIdx();
      if (parameter >= 0) control->SetValueFromDelegate(GetDelegate()->GetParam(parameter)->GetNormalized());
    });
  }
  bool IsDirty() override
  {
    if (!IsHidden()) SetDirty(false);
    return IContainerBase::IsDirty();
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_ui;
    g.FillRect(background, mRECT);
    g.DrawText(Text(17.f, accent), "Tuner", IRECT(24, 484, 124, 512));
    g.DrawText(Text(13.f, muted), "Clean input · before the grid", IRECT(124, 484, 370, 512));
    const double reference = GetDelegate()->GetParam(kTunerReference)->Value();
    const double frequency = PLUG()->TunerFrequency();
    const auto pitch = grid_tuner::ToPitch(frequency, reference);
    const auto pitchColor = [](double cents) {
      return std::abs(cents) <= 3. ? accent : std::abs(cents) <= 10. ? IColor(255, 238, 187, 97) : IColor(255, 235, 135, 119);
    };
    if (pitch.valid)
    {
      const char* notes[]{"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
      const std::string label = std::string(notes[(pitch.note % 12 + 12) % 12]) + std::to_string(pitch.note / 12 - 1);
      g.DrawText(Text(64.f, pitchColor(pitch.cents), EAlign::Center), label.c_str(), IRECT(24, 525, 196, 605));
      WDL_String value; value.SetFormatted(64, "%+.1f cents", pitch.cents);
      g.DrawText(Text(20.f, pitchColor(pitch.cents), EAlign::Center), value.Get(), IRECT(24, 604, 196, 634));
      value.SetFormatted(64, "%.1f Hz", frequency);
      g.DrawText(Text(13.f, muted, EAlign::Center), value.Get(), IRECT(24, 636, 196, 658));
      const float x = 110.f + 1.7f * static_cast<float>(pitch.cents);
      g.FillRoundRect(pitchColor(pitch.cents), IRECT(x - 2, 667, x + 2, 680), 1.f);
    }
    else g.DrawText(Text(23.f, muted, EAlign::Center), "Play a note", IRECT(24, 550, 196, 630));
    g.DrawLine(border, 24, 674, 196, 674);
    g.DrawLine(text, 110, 663, 110, 684);

    const IRECT panel(372, 520, 976, 696), plot(410, 538, 964, 674);
    g.FillRoundRect(IColor(255, 17, 21, 24), panel, 4.f);
    g.DrawText(Text(13.f, accent), "Pitch history", IRECT(372, 484, 570, 512));
    g.DrawText(Text(13.f, muted, EAlign::Far), "±25 cents · 6 seconds", IRECT(570, 488, 882, 512));
    // Zoomed to ±25 cents, so the last few cents of tuning are visible; larger offsets pin to the edge.
    constexpr double range = 25.;
    const auto yFor = [&](double cents) { return plot.MH() - plot.H() * .5f * static_cast<float>(std::clamp(cents, -range, range) / range); };
    g.FillRect(accent.WithOpacity(.08f), IRECT(plot.L, yFor(3.), plot.R, yFor(-3.)));
    for (int cents : {-25, -10, 0, 10, 25})
    {
      const float y = yFor(cents);
      g.DrawLine(cents == 0 ? muted.WithOpacity(.6f) : border.WithOpacity(.45f), plot.L, y, plot.R, y);
      WDL_String tick; tick.SetFormatted(16, "%+d", cents);
      g.DrawText(Text(13.f, muted, EAlign::Far), tick.Get(), IRECT(panel.L + 2, y - 8, plot.L - 7, y + 8));
    }
    const auto now = grid_tuner::Tuner::Now();
    grid_tuner::Pitch previous{};
    float previousX = 0, previousY = 0;
    long long previousTime = 0;
    for (const auto& point : PLUG()->TunerHistory())
    {
      const auto current = grid_tuner::ToPitch(point.frequency, reference);
      const auto age = now - point.milliseconds;
      if (!current.valid || age < 0 || age > grid_tuner::historyDurationMs) { previous = {}; continue; }
      const float x = plot.R - plot.W() * static_cast<float>(age) / grid_tuner::historyDurationMs;
      const float y = yFor(current.cents);
      const auto color = pitchColor(current.cents).WithOpacity(.35f + .65f * (1.f - static_cast<float>(age) / grid_tuner::historyDurationMs));
      if (grid_tuner::Connect(previous, current) && point.milliseconds - previousTime <= 150)
        g.DrawLine(color, previousX, previousY, x, y, nullptr, 2.f);
      else g.FillCircle(color, x, y, 1.5f);
      previous = current; previousTime = point.milliseconds; previousX = x; previousY = y;
    }
    g.DrawText(Text(13.f, muted), "6 s ago", IRECT(plot.L, 676, plot.L + 100, 696));
    g.DrawText(Text(13.f, muted, EAlign::Far), "Now", IRECT(plot.R - 80, 676, plot.R, 696));
    g.DrawText(Text(13.f, muted), "Play one string. Green = within ±3 cents. Escape / Tuner OFF restores output.", IRECT(24, 696, 976, 719));
  }
};
