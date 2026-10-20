#pragma once
#include "CaptureMenu.h"
#include <chrono>

namespace grid_ui
{
const IColor background = COLOR_BLACK;
const IColor surface(255, 17, 19, 22), border(255, 47, 51, 57), muted(255, 139, 146, 156);
const IColor accent(255, 113, 199, 178), text(255, 232, 235, 239);
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
class Toggle : public IVToggleControl
{
public:
  Toggle(const IRECT& r, int param)
  : IVToggleControl(
      r, param, "", Style().WithShowLabel(false).WithValueText(Text(12.f, text, EAlign::Center)), "Bypassed", "Enabled")
  {
  }
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
        const float x = 296.f + 220.f * col, y = 191.f + 105.f * row;
        g.DrawLine(border, x, y, x + 12, y);
      }
    g.DrawLine(border, 956, 191, 973, 191);
    g.DrawLine(border, 973, 191, 973, 243);
    g.DrawLine(border, 973, 243, 69, 243);
    g.DrawLine(border, 69, 243, 69, 401);
    g.DrawLine(border, 69, 296, 88, 296);
    g.DrawLine(border, 69, 401, 88, 401);
    g.DrawLine(border, 956, 296, 982, 296);
    g.DrawLine(border, 982, 296, 982, 453);
    g.DrawLine(border, 956, 401, 982, 401);
    g.DrawLine(border, 982, 453, 69, 453);
    g.DrawLine(border, 69, 453, 69, 506);
    g.DrawLine(border, 69, 506, 88, 506);
  }
};
} // namespace grid_ui

// A selectable card with an independently clipped module editor below the grid.
class NAMGridSlotControl : public IControl
{
public:
  NAMGridSlotControl(const IRECT& bounds, size_t slot)
  : IControl(bounds)
  , mSlot(slot)
  {
  }

  void Draw(IGraphics& g) override
  {
    using namespace grid_ui;
    const bool selected = PLUG()->GetSelectedSlot() == mSlot;
    const bool enabled = GetDelegate()->GetParam(SlotActiveParam(mSlot))->Bool();
    g.FillRoundRect(surface, mRECT, 4.f);
    g.DrawRoundRect(selected ? accent : border, mRECT, 4.f, nullptr, selected ? 1.5f : 1.f);
    const auto slotName = GridSlotName(mSlot);
    g.DrawText(Text(11.f, selected ? accent : muted), slotName.c_str(),
               IRECT(mRECT.L + 12, mRECT.T + 8, mRECT.R - 36, mRECT.T + 25));
    g.FillCircle(enabled ? accent : muted, mRECT.R - 17, mRECT.T + 16, 3.f);
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    const auto path = PLUG()->GetSlotPath(mSlot);
    const bool empty = path.empty() && effect == SlotEffect::NAM;
    const auto label = empty          ? std::string("+ Add module")
                       : path.empty() ? std::string(SlotEffectName(effect))
                                      : nam_library::PathText(std::filesystem::u8path(path).stem());
    g.DrawText(
      Text(14.f, empty ? muted : text), label.c_str(), IRECT(mRECT.L + 12, mRECT.T + 29, mRECT.R - 12, mRECT.T + 52));
    const auto type = empty                                           ? "Empty"
                      : !path.empty() && !PLUG()->IsSlotLoaded(mSlot) ? "Missing file"
                      : effect == SlotEffect::CabinetIR               ? "WAV / IR"
                      : effect == SlotEffect::NAM                     ? "NAM"
                                                                      : "Effect";
    g.DrawText(Text(10.f, muted), type, IRECT(mRECT.L + 12, mRECT.T + 54, mRECT.R - 12, mRECT.T + 70));
    const auto bar = [&](float left, float right, double value, bool clipped) {
      const float top = mRECT.B - 8;
      g.FillRect(border, IRECT(left, top, right, top + 2));
      const double fill = std::clamp((20. * std::log10(std::max(1.e-6, value)) + 60.) / 60., 0., 1.);
      g.FillRect(
        clipped ? COLOR_RED : accent, IRECT(left, top, left + static_cast<float>(fill) * (right - left), top + 2));
    };
    bar(mRECT.L + 12, mRECT.MW() - 3, mMeter.input, Now() < mInputClip);
    bar(mRECT.MW() + 3, mRECT.R - 12, mMeter.output, Now() < mOutputClip);
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
    const IRECT panel(24, 592, 976, 780);
    g.FillRoundRect(surface, panel, 5.f);
    g.DrawRoundRect(border, panel, 5.f);
    g.DrawText(Text(12.f, accent), slotName.c_str(), IRECT(42, 606, 104, 629));
    g.DrawText(Text(16.f), label.c_str(), IRECT(110, 602, 790, 633));
    const auto description = empty ? "Choose a capture, impulse response or effect to get started."
                             : effect == SlotEffect::NAM         ? "NAM model / input gain and output trim"
                             : effect == SlotEffect::CabinetIR   ? "Cabinet IR  /  WAV impulse response"
                             : effect == SlotEffect::NaturalGate ? "Natural gate  /  gentle closing for sustained notes"
                             : effect == SlotEffect::TightGate   ? "Tight gate  /  fast closing for precise rhythm"
                             : effect == SlotEffect::SmoothComp  ? "Smooth compressor  /  gentle leveling"
                             : effect == SlotEffect::PunchComp   ? "Punch compressor  /  keep the attack"
                             : effect == SlotEffect::ToneEQ      ? "Tone EQ  /  broad bass, mid and treble bands"
                                                                 : "Cut EQ  /  high pass, low pass and presence";
    g.DrawText(Text(12.f, muted), description, IRECT(42, 635, 465, 673));
    g.DrawText(Text(11.f, muted), "Right-click a card for Copy / Move / Swap", IRECT(42, 751, 455, 770));
    if (mMeter.reduction >= .5f)
    {
      WDL_String gr;
      gr.SetFormatted(40, "Gain reduction: %.1f dB", mMeter.reduction);
      g.DrawText(Text(12.f, accent), gr.Get(), IRECT(500, 751, 900, 770));
    }
  }
  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    PLUG()->SelectSlot(mSlot);
    if (mod.R)
      ShowEditMenu();
    else if (x > mRECT.R - 32 && y < mRECT.T + 28)
    {
      auto* param = GetDelegate()->GetParam(SlotActiveParam(mSlot));
      const double value = param->Bool() ? 0. : 1.;
      GetDelegate()->BeginInformHostOfParamChangeFromUI(SlotActiveParam(mSlot));
      GetDelegate()->SendParameterValueFromUI(SlotActiveParam(mSlot), value);
      GetDelegate()->EndInformHostOfParamChangeFromUI(SlotActiveParam(mSlot));
      mEditor->GetChild(3)->SetValueFromDelegate(value);
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
    mEditor = new grid_ui::Editor(IRECT(24, 592, 976, 780), [this](IGraphics& g) { DrawEditor(g); });
    GetUI()->AttachControl(mEditor);
    // Controls live in a shared bottom editor; each slot keeps its original automation bindings.
    // Inactive editors are hidden, so switching selection never changes parameter ownership.
    mModelButton = new grid_ui::Button(IRECT(42, 686, 295, 717), [this](IControl*) { ShowMenu(); }, "Choose module...");
    mEditor->AddChildControl(mModelButton);
    mEditor->AddChildControl(new grid_ui::Button(IRECT(303, 686, 339, 717), [this](IControl*) { Step(-1); }, "<"));
    mEditor->AddChildControl(new grid_ui::Button(IRECT(345, 686, 381, 717), [this](IControl*) { Step(1); }, ">"));
    mEditor->AddChildControl(new grid_ui::Toggle(IRECT(835, 604, 956, 631), SlotActiveParam(mSlot)));
    mQuality =
      new grid_ui::Button(IRECT(389, 686, 465, 717), [this](IControl*) { PLUG()->ToggleSlotQuality(mSlot); }, "Full");
    mEditor->AddChildControl(mQuality);
    mNAMKnobs[0] = new grid_ui::Knob(IRECT(525, 641, 625, 747), SlotInputParam(mSlot), "Input");
    mNAMKnobs[1] = new grid_ui::Knob(IRECT(665, 641, 765, 747), SlotTrimParam(mSlot), "Output trim");
    for (auto* knob : mNAMKnobs)
      mEditor->AddChildControl(knob);
    const char* labels[] = {"Threshold", "Decay", "Depth", "Threshold", "Release", "Makeup"};
    const char* tips[] = {"Open above this input level (dBFS)", "Closing time in milliseconds",
                          "Maximum attenuation in dB",          "Compress above this input level (dBFS)",
                          "Gain recovery time in milliseconds", "Output gain in dB; match bypass loudness"};
    for (int i = 0; i < 6; ++i)
    {
      const float left = 515.f + (i % 3) * 140.f;
      mFXKnobs[i] = new grid_ui::Knob(IRECT(left, 641, left + 110, 747), SlotDynamicsParam(mSlot, i), labels[i]);
      mFXKnobs[i]->SetTooltip(tips[i]);
      mEditor->AddChildControl(mFXKnobs[i]);
    }
    const char* eqLabels[] = {"Bass", "Mid", "Treble", "High pass", "Low pass", "Presence", "Level"};
    const char* eqTips[] = {"150 Hz low shelf", "800 Hz broad mid band",  "3 kHz high shelf",     "0 Hz = off",
                            "20 kHz = off",     "3.5 kHz presence shelf", "IR output level in dB"};
    for (int i = 0; i < 7; ++i)
    {
      const float left = 515.f + (i == 6 ? 0 : i % 3) * 140.f;
      mEQKnobs[i] = new grid_ui::Knob(IRECT(left, 641, left + 110, 747), SlotEQParam(mSlot, i), eqLabels[i]);
      mEQKnobs[i]->SetTooltip(eqTips[i]);
      mEditor->AddChildControl(mEQKnobs[i]);
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
    else if (item->GetTag() <= -21 && item->GetTag() >= -26)
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
        mEQKnobs[i]->Hide(i < 3 ? effect != SlotEffect::ToneEQ : i < 6 ? effect != SlotEffect::CutEQ : !ir);
    }
    const bool scalable = PLUG()->CanToggleSlotQuality(mSlot);
    mQuality->Hide(!selected || !nam || !PLUG()->IsSlotLoaded(mSlot));
    mQuality->SetDisabled(!scalable);
    mQuality->SetLabelStr(!scalable                                                     ? "Fixed"
                          : GetDelegate()->GetParam(SlotFullParam(mSlot))->Value() < .5 ? "Lite"
                                                                                        : "Full");
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
    GetUI()->CreatePopupMenu(*this, mMenu, mRECT);
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
    mMenu.AddItem("EQ", eq, 5);
    mMenu.AddSeparator(6);
    GetUI()->CreatePopupMenu(*this, mMenu, mRECT);
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
  size_t mSlot;
  grid_ui::Editor* mEditor = nullptr;
  IVButtonControl* mModelButton = nullptr;
  IVButtonControl* mQuality = nullptr;
  std::array<IControl*, 2> mNAMKnobs{};
  std::array<IControl*, 6> mFXKnobs{};
  std::array<IControl*, 7> mEQKnobs{};
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
    g.DrawText(Text(14.f), "Input calibration / IN 2", IRECT(32, 556, 460, 582));
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
    AddChildControl(new Knob(IRECT(32, 617, 135, 725), kInputCalibrationLevel, "Reference (dBu)"));
    AddChildControl(new Toggle(IRECT(158, 657, 277, 689), kCalibrateInput));
    AddChildControl(new IVSwitchControl(IRECT(317, 638, 504, 700), kOutputMode, "IN 2 output mode", Style()));
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
    if (mAdd)
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
  std::array<IControl*, 3> mLegacy{};
};
