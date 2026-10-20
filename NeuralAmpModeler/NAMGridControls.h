#pragma once
#include "CaptureMenu.h"
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
  if (effect == SlotEffect::NaturalGate || effect == SlotEffect::TightGate)
    return {gateColor, gateColor, "Gate"};
  if (effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp)
    return {compColor, compColor, "Compressor"};
  if (effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ || effect == SlotEffect::ParametricEQ)
    return {eqColor, eqColor, "EQ"};
  if (effect == SlotEffect::Boost)
    return {boostColor, boostColor, "Boost"};
  if (IsDelay(effect))
    return {delayColor, delayColor, "Delay"};
  if (IsReverb(effect))
    return {reverbColor, reverbColor, "Reverb"};
  if (IsModulation(effect) || effect == SlotEffect::SynthFilter)
    return {modulationColor, modulationColor, "Modulation"};
  if (effect == SlotEffect::AutoSwell)
    return {compColor, compColor, "Volume"};
  if (effect == SlotEffect::Tape)
    return {boostColor, boostColor, "Tape"};
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
inline IVStyle Style()
{
  return DEFAULT_STYLE.WithLabelText(Text(12.f, muted, EAlign::Center))
    .WithValueText(Text(13.f, text, EAlign::Center))
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
  Knob(const IRECT& r, int parameter, const char* label)
  : IVKnobControl(r, parameter, label, Style(), true, false)
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
// All module controls occupy one of six fixed positions, including the bypass toggle.
inline IRECT ControlBounds(int position)
{
  const float left = 42.f + 152.f * position;
  return IRECT(left, 650, left + 126.f, 754);
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
      g.DrawText(Text(13.f, muted, EAlign::Center), mLabel.c_str(), mRECT.GetFromTop(22));
    const auto button = tall ? IRECT(mRECT.L + 12, mRECT.T + 40, mRECT.R - 12, mRECT.T + 74) : mRECT;
    const bool on = GetValue() >= .5;
    g.FillRoundRect(on ? IColor(255, 37, 68, 62) : surface, button, 3.f);
    g.DrawRoundRect(on ? accent : border, button, 3.f);
    g.DrawText(Text(13.f, on ? accent : text, EAlign::Center), on ? "ON" : "OFF", button);
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
        const float x = 296.f + 220.f * col, y = 160.f + 104.f * row;
        g.DrawLine(border, x, y, x + 12, y);
      }
    g.DrawLine(accent, 37, 160, 88, 160);
    g.DrawCircle(accent, 28, 160, 9.f);
    g.FillTriangle(accent, 24, 155, 24, 165, 32, 160);
    g.DrawText(Text(11.f, text, EAlign::Center), "IN", IRECT(9, 176, 47, 195));
    g.DrawLine(accent, 956, 472, 973, 472);
    g.DrawCircle(accent, 982, 472, 9.f);
    g.FillTriangle(accent, 978, 467, 978, 477, 986, 472);
    g.DrawText(Text(11.f, text, EAlign::Center), "OUT", IRECT(963, 487, 999, 507));
    g.DrawLine(border, 956, 160, 973, 160);
    g.DrawLine(border, 973, 160, 973, 212);
    g.DrawLine(border, 973, 212, 69, 212);
    g.DrawLine(border, 69, 212, 69, 368);
    g.DrawLine(border, 69, 264, 88, 264);
    g.DrawLine(border, 69, 368, 88, 368);
    g.DrawLine(border, 956, 264, 982, 264);
    g.DrawLine(border, 982, 264, 982, 420);
    g.DrawLine(border, 956, 368, 982, 368);
    g.DrawLine(border, 982, 420, 69, 420);
    g.DrawLine(border, 69, 420, 69, 472);
    g.DrawLine(border, 69, 472, 88, 472);
  }
};
} // namespace grid_ui

// A selectable card with an independently clipped module editor below the grid.
class NAMGridSlotControl : public IControl
{
public:
  NAMGridSlotControl(const IRECT& bounds, size_t slot)
  : IControl(bounds.GetHPadded(6.f))
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
    g.DrawRoundRect(selected ? tone.first : border, mCardBounds, 4.f, nullptr, selected ? 1.5f : 1.f);
    if (occupied)
      Gradient(g, IRECT(mCardBounds.L + 8, mCardBounds.T + 1, mCardBounds.R - 8, mCardBounds.T + 4), tone.first,
               tone.second, enabled ? 1.f : .4f, 1.f);
    const auto slotName = GridSlotName(mSlot);
    g.DrawText(Text(11.f, selected ? tone.first : muted), slotName.c_str(),
               IRECT(mCardBounds.L + 12, mCardBounds.T + 8, mCardBounds.R - 36, mCardBounds.T + 25));
    g.FillCircle(enabled ? tone.first : muted, mCardBounds.R - 17, mCardBounds.T + 16, 3.f);
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    const auto path = PLUG()->GetSlotPath(mSlot);
    const bool empty = path.empty() && effect == SlotEffect::NAM;
    const auto label = empty          ? std::string("+ Add module")
                       : path.empty() ? std::string(SlotEffectName(effect))
                                      : nam_library::PathText(std::filesystem::u8path(path).stem());
    g.DrawText(Text(14.f, empty ? muted : text), label.c_str(),
               IRECT(mCardBounds.L + 12, mCardBounds.T + 29, mCardBounds.R - 12, mCardBounds.T + 52));
    const auto type = empty ? "Empty" : !path.empty() && !PLUG()->IsSlotLoaded(mSlot) ? "Missing file" : tone.label;
    g.DrawText(
      Text(10.f, muted), type, IRECT(mCardBounds.L + 12, mCardBounds.T + 54, mCardBounds.R - 12, mCardBounds.T + 70));
    const auto bar = [&](const IRECT& r, double value, bool clipped) {
      g.FillRect(border.WithOpacity(.55f), r);
      const double fill = std::clamp((20. * std::log10(std::max(1.e-6, value)) + 60.) / 60., 0., 1.);
      g.FillRect(clipped ? COLOR_RED : tone.first, IRECT(r.L, r.B - static_cast<float>(fill) * r.H(), r.R, r.B));
    };
    bar(IRECT(mCardBounds.L - 5, mCardBounds.T + 28, mCardBounds.L - 2, mCardBounds.B - 10), mMeter.input,
        Now() < mInputClip);
    bar(IRECT(mCardBounds.R + 2, mCardBounds.T + 28, mCardBounds.R + 5, mCardBounds.B - 10), mMeter.output,
        Now() < mOutputClip);
  }
  void DrawEditor(IGraphics& g)
  {
    using namespace grid_ui;
    const auto slotName = GridSlotName(mSlot);
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
    Gradient(g, IRECT(42, 599, 956, 601), tone.first, tone.second, .8f, 1.f);
    g.DrawText(Text(12.f, tone.first), slotName.c_str(), IRECT(42, 568, 104, 594));
    g.DrawText(Text(16.f), label.c_str(), IRECT(110, 565, 956, 598));
    std::array<bool, 6> used{false, false, false, false, false, true};
    if (effect == SlotEffect::NAM && !path.empty())
      used.fill(true);
    else if (effect == SlotEffect::CabinetIR)
      used[0] = true;
    else if (effect == SlotEffect::ParametricEQ)
      used[0] = used[1] = used[2] = used[3] = used[4] = true;
    else if (effect == SlotEffect::Boost)
      used[0] = used[1] = used[2] = true;
    else if (effect == SlotEffect::NaturalGate || effect == SlotEffect::TightGate || effect == SlotEffect::SmoothComp
             || effect == SlotEffect::PunchComp || effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ)
      used[0] = used[1] = used[2] = true;
    for (int p = 0; p < kUtilityControls; ++p)
      if (UsesUtility(effect, p))
        used[kEffectParameters[p].position] = true;
    for (int i = 0; i < 6; ++i)
    {
      const auto cell = ControlBounds(i).GetPadded(5.f);
      g.FillRoundRect(IColor(255, 17, 20, 24), cell, 4.f);
      if (!used[i])
        g.DrawText(Text(16.f, border, EAlign::Center), "-", cell);
    }
    const char* description = effect == SlotEffect::NAM         ? "NAM / post-capture tone EQ"
                              : effect == SlotEffect::CabinetIR ? "WAV / cabinet impulse response"
                                                                : SlotEffectName(effect);
    g.DrawText(Text(11.f, muted), description, IRECT(42, 757, 450, 777));
    if (mMeter.reduction >= .5f)
    {
      WDL_String gr;
      gr.SetFormatted(40, "Gain reduction: %.1f dB", mMeter.reduction);
      g.DrawText(Text(12.f, accent), gr.Get(), IRECT(500, 757, 956, 777));
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
  }

  void OnMouseDblClick(float, float, const IMouseMod&) override
  {
    PLUG()->SelectSlot(mSlot);
    ShowMenu();
  }

  bool IsDirty() override
  {
    const double now = Now(), dt = std::max(0., now - mTick);
    mTick = now;
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
    mEditor = new grid_ui::Editor(IRECT(24, 558, 976, 780), [this](IGraphics& g) { DrawEditor(g); });
    GetUI()->AttachControl(mEditor);
    // Controls live in a shared bottom editor; each slot keeps its original automation bindings.
    // Inactive editors are hidden, so switching selection never changes parameter ownership.
    mModelButton = new grid_ui::Button(IRECT(42, 606, 416, 636), [this](IControl*) { ShowMenu(); }, "Choose module...");
    mEditor->AddChildControl(mModelButton);
    mEditor->AddChildControl(new grid_ui::Button(IRECT(428, 606, 464, 636), [this](IControl*) { Step(-1); }, "<"));
    mEditor->AddChildControl(new grid_ui::Button(IRECT(472, 606, 508, 636), [this](IControl*) { Step(1); }, ">"));
    mEnabled = new grid_ui::Toggle(grid_ui::ControlBounds(5), SlotActiveParam(mSlot), "Enabled");
    mEditor->AddChildControl(mEnabled);
    mNAMKnobs[0] = new grid_ui::Knob(grid_ui::ControlBounds(0), SlotInputParam(mSlot), "Input");
    mNAMKnobs[1] = new grid_ui::Knob(grid_ui::ControlBounds(4), SlotTrimParam(mSlot), "Output trim");
    for (auto* knob : mNAMKnobs)
      mEditor->AddChildControl(knob);
    const char* labels[] = {"Threshold", "Decay", "Depth", "Threshold", "Release", "Makeup"};
    for (int i = 0; i < 6; ++i)
    {
      mFXKnobs[i] = new grid_ui::Knob(grid_ui::ControlBounds(i % 3), SlotDynamicsParam(mSlot, i), labels[i]);
      mEditor->AddChildControl(mFXKnobs[i]);
    }
    const char* eqLabels[] = {"Bass", "Mid", "Treble", "High pass", "Low pass", "Presence", "Level"};
    for (int i = 0; i < 7; ++i)
    {
      mEQKnobs[i] = new grid_ui::Knob(grid_ui::ControlBounds(i == 6 ? 0 : i % 3), SlotEQParam(mSlot, i), eqLabels[i]);
      mEditor->AddChildControl(mEQKnobs[i]);
    }
    for (int i = 0; i < kUtilityControls; ++i)
    {
      const auto& p = kEffectParameters[i];
      mUtilityKnobs[i] = new grid_ui::Knob(grid_ui::ControlBounds(p.position), SlotUtilityParam(mSlot, i), p.label);
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
    else if (item->GetTag() <= -21 && item->GetTag() >= -20 - static_cast<int>(SlotEffect::Tape))
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
  void Refresh()
  {
    if (!mModelButton)
      return;
    const auto path = PLUG()->GetSlotPath(mSlot);
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    const bool nam = effect == SlotEffect::NAM;
    const bool gate = effect == SlotEffect::NaturalGate || effect == SlotEffect::TightGate;
    const bool compressor = effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp;
    const bool ir = effect == SlotEffect::CabinetIR;
    const bool selected = PLUG()->GetSelectedSlot() == mSlot;
    mEditor->Hide(!selected);
    if (selected)
    {
      for (auto* knob : mNAMKnobs)
        knob->Hide(!nam || path.empty());
      for (int i = 0; i < 6; ++i)
        mFXKnobs[i]->Hide(i < 3 ? !gate : !compressor);
      for (int i = 0; i < 7; ++i)
        mEQKnobs[i]->Hide(i < 3   ? (effect != SlotEffect::ToneEQ && !(nam && !path.empty()))
                          : i < 6 ? effect != SlotEffect::CutEQ
                                  : !ir);
      for (int i = 0; i < kUtilityControls; ++i)
      {
        mUtilityKnobs[i]->Hide(!UsesUtility(effect, i));
        if (i == uDelayCharacter)
          mUtilityKnobs[i]->SetTooltip(
            effect == SlotEffect::DigitalDelay  ? "Ducks repeats while you play; echoes recover in the gaps."
            : effect == SlotEffect::AnalogDelay ? "More darkening and soft saturation on the repeats."
                                                : "More tape wobble and soft saturation on the repeats.");
        else if (i == uModCharacter)
          mUtilityKnobs[i]->SetTooltip(
            effect == SlotEffect::Phaser    ? "Raises the center of the phase sweep."
            : effect == SlotEffect::Flanger ? "Moves feedback from positive through zero to negative."
            : effect == SlotEffect::Tremolo ? "Sharper pulses; stronger playing speeds up the pulse."
                                            : "Warmer tone and more irregular pitch movement.");
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
      if (effect == SlotEffect::Boost)
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
    nam_library::FillMenu(mMenu, *mMenuLibrary, PLUG()->GetCaptureFilter(), PLUG()->GetSlotPath(mSlot));
    mMenu.AddItem(new IPopupMenu::Item("Choose .nam file...", 0, -10), 0);
    mMenu.AddItem(new IPopupMenu::Item("Choose cabinet IR (.wav)...", 0, -12), 1);
    mMenu.AddItem(new IPopupMenu::Item("Clear slot", 0, -11), 2);
    auto* gates = new IPopupMenu;
    auto* compressors = new IPopupMenu;
    auto* eq = new IPopupMenu;
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    for (int i = 1; i <= 6; ++i)
    {
      const auto type = static_cast<SlotEffect>(i);
      (i <= 2   ? gates
       : i <= 4 ? compressors
                : eq)
        ->AddItem(new IPopupMenu::Item(SlotEffectName(type), type == effect ? IPopupMenu::Item::kChecked : 0, -20 - i));
    }
    mMenu.AddItem("Noise Gate", gates, 3);
    mMenu.AddItem("Compressor", compressors, 4);
    eq->AddItem(new IPopupMenu::Item(SlotEffectName(SlotEffect::ParametricEQ),
                                     effect == SlotEffect::ParametricEQ ? IPopupMenu::Item::kChecked : 0, -28));
    mMenu.AddItem("EQ", eq, 5);
    mMenu.AddItem(new IPopupMenu::Item("Boost", effect == SlotEffect::Boost ? IPopupMenu::Item::kChecked : 0, -29), 6);
    const auto addGroup = [&](const char* title, SlotEffect first, SlotEffect last) {
      auto* group = new IPopupMenu;
      for (int i = static_cast<int>(first); i <= static_cast<int>(last); ++i)
      {
        const auto type = static_cast<SlotEffect>(i);
        group->AddItem(
          new IPopupMenu::Item(SlotEffectName(type), type == effect ? IPopupMenu::Item::kChecked : 0, -20 - i));
      }
      mMenu.AddItem(title, group, 7);
    };
    addGroup("Reverb", SlotEffect::RoomReverb, SlotEffect::CloudReverb);
    addGroup("Delay", SlotEffect::DigitalDelay, SlotEffect::TapeDelay);
    addGroup("Modulation", SlotEffect::Chorus, SlotEffect::Vibrato);
    addGroup("Filter / Volume", SlotEffect::SynthFilter, SlotEffect::AutoSwell);
    mMenu.AddItem(new IPopupMenu::Item("Airwindows Tape", effect == SlotEffect::Tape ? IPopupMenu::Item::kChecked : 0,
                                       -20 - static_cast<int>(SlotEffect::Tape)),
                  7);
    mMenu.AddSeparator(7);
    if (PLUG()->CanToggleSlotQuality(mSlot))
      mMenu.AddItem(
        new IPopupMenu::Item(
          "A2 Lite", GetDelegate()->GetParam(SlotFullParam(mSlot))->Value() < .5 ? IPopupMenu::Item::kChecked : 0, -15),
        6);
    const auto filter = PLUG()->GetCaptureFilter();
    if (!filter.search.empty() || filter.gear >= 0)
      mMenu.AddItem(new IPopupMenu::Item("Clear search and gear filter", 0, -13), 0);
    if (mMenuLibrary->captures.empty())
      mMenu.AddItem(new IPopupMenu::Item(PLUG()->IsLibraryScanning() ? "Scanning library..." : "Rescan library",
                                         PLUG()->IsLibraryScanning() ? IPopupMenu::Item::kDisabled : 0, -14),
                    0);
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
  grid_ui::Toggle* mEnabled = nullptr;
  std::array<IControl*, 2> mNAMKnobs{};
  std::array<IControl*, 6> mFXKnobs{};
  std::array<IControl*, 7> mEQKnobs{};
  std::array<IControl*, kUtilityControls> mUtilityKnobs{};
  static double Now()
  { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
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
      g.DrawText(Text(11.f, muted), "Scroll the list to see more folders (32 maximum).", IRECT(32, 507, 960, 528));
    g.DrawLine(border, 32, 545, 968, 545);
    g.DrawText(Text(14.f), "Input calibration / Pre 2", IRECT(32, 556, 460, 582));
    g.DrawText(Text(12.f, muted),
               "Interface reference level, separate from musical input gain. Raw keeps the model output unchanged.",
               IRECT(32, 585, 960, 609));
    g.DrawText(
      Text(12.f, muted), "All new gate, EQ and cabinet processing belongs in grid modules.", IRECT(32, 751, 950, 778));
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
    AddChildControl(new Knob(IRECT(32, 617, 135, 725), kInputCalibrationLevel, "Reference (dBu)"));
    AddChildControl(new Toggle(IRECT(158, 657, 277, 689), kCalibrateInput));
    AddChildControl(new IVSwitchControl(IRECT(317, 638, 504, 700), kOutputMode, "Pre 2 output mode", Style()));
    // Expose inherited processing only for recalled projects that still use it.
    mLegacy[0] =
      AddChildControl(new IVToggleControl(IRECT(545, 638, 666, 700), kNoiseGateActive, "Legacy gate", Style()));
    mLegacy[1] = AddChildControl(new IVToggleControl(IRECT(686, 638, 807, 700), kEQActive, "Legacy EQ", Style()));
    mLegacy[2] = AddChildControl(new IVToggleControl(IRECT(827, 638, 948, 700), kIRToggle, "Legacy IR", Style()));
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
