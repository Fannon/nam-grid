#pragma once
#include "CaptureMenu.h"
#include <chrono>

class NAMGridWiringControl : public IControl
{
public:
  NAMGridWiringControl(const IRECT& bounds)
  : IControl(bounds)
  { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override
  {
    const IColor line(255, 85, 113, 126);
    // Input row feeds the left edge of both parallel rows.
    g.DrawLine(line, 940, 250, 955, 250);
    g.DrawLine(line, 955, 250, 955, 317);
    g.DrawLine(line, 955, 317, 80, 317);
    g.DrawLine(line, 80, 317, 80, 520);
    g.DrawLine(line, 80, 385, 90, 385);
    g.DrawLine(line, 80, 520, 90, 520);
    g.FillCircle(line, 80, 385, 2.f);
    // Lane outputs merge before the output row. Blend is the A/B crossfade below the grid.
    g.DrawLine(line, 940, 385, 965, 385);
    g.DrawLine(line, 965, 385, 965, 587);
    g.DrawLine(line, 940, 520, 965, 520);
    g.FillCircle(line, 965, 520, 2.f);
    g.DrawLine(line, 965, 587, 80, 587);
    g.DrawLine(line, 80, 587, 80, 655);
    g.DrawLine(line, 80, 655, 90, 655);
    g.DrawLine(line, 940, 655, 970, 655);
    for (int row = 0; row < 4; ++row)
      for (int col = 0; col < 3; ++col)
      {
        const float right = 295.f + 215.f * col, center = 250.f + 135.f * row;
        g.FillTriangle(line, right + 2, center - 3, right + 2, center + 3, right + 8, center);
      }
  }
};

// A compact native card for a mono NAM capture or a three-knob built-in effect.
class NAMGridSlotControl : public IContainerBase
{
public:
  NAMGridSlotControl(const IRECT& bounds, size_t slot, const IVStyle& style)
  : IContainerBase(bounds)
  , mSlot(slot)
  , mStyle(style)
  {
  }

  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(IColor(255, 30, 31, 34), mRECT, 5.f);
    g.DrawRoundRect(IColor(255, 70, 72, 77), mRECT, 5.f);
    const auto text = IText(9.f, IColor(255, 180, 185, 190), "Roboto-Regular");
    const float top = mRECT.B - 12;
    const auto bar = [&](float left, float right, double value, bool clipped, bool reduction) {
      const IRECT bounds(left, top + 1, right, top + 7);
      g.FillRect(IColor(255, 15, 17, 20), bounds);
      const double fill = reduction ? std::clamp(value / 30., 0., 1.)
                                    : std::clamp((20. * std::log10(std::max(1.e-6, value)) + 60.) / 60., 0., 1.);
      g.FillRect(clipped     ? IColor(255, 250, 75, 65)
                 : reduction ? IColor(255, 235, 183, 73)
                             : IColor(255, 75, 190, 145),
                 IRECT(left, bounds.T, left + static_cast<float>(fill) * bounds.W(), bounds.B));
    };
    g.DrawText(text, "I", IRECT(mRECT.L + 5, top - 1, mRECT.L + 17, top + 9));
    g.DrawText(text, "O", IRECT(mRECT.L + 76, top - 1, mRECT.L + 88, top + 9));
    g.DrawText(text, "GR", IRECT(mRECT.L + 146, top - 1, mRECT.L + 165, top + 9));
    bar(mRECT.L + 18, mRECT.L + 73, mMeter.input, Now() < mInputClip, false);
    bar(mRECT.L + 89, mRECT.L + 144, mMeter.output, Now() < mOutputClip, false);
    bar(mRECT.L + 166, mRECT.R - 7, mMeter.reduction, false, true);
    WDL_String gr;
    gr.SetFormatted(16, "%.0f", mMeter.reduction);
    if (mMeter.reduction >= .5f)
      g.DrawText(text.WithFGColor(COLOR_WHITE), gr.Get(), IRECT(mRECT.L + 166, top - 2, mRECT.R - 7, top + 9));
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
      SetDirty(false);
    return IContainerBase::IsDirty();
  }

  void OnAttached() override
  {
    const auto text = IText(12.f, COLOR_WHITE, "Roboto-Regular");
    const auto cardStyle = mStyle.WithLabelText(text).WithValueText(text);
    auto* edit = new IVButtonControl(
      IRECT(mRECT.L + 5, mRECT.T + 3, mRECT.L + 47, mRECT.T + 24), [this](IControl*) { ShowEditMenu(); },
      GridSlotName(mSlot).c_str(), cardStyle);
    edit->SetTooltip("Copy, move or swap this slot, including its settings");
    AddChildControl(edit);
    mQuality = new IVButtonControl(
      IRECT(mRECT.L + 50, mRECT.T + 3, mRECT.L + 102, mRECT.T + 24),
      [this](IControl*) { PLUG()->ToggleSlotQuality(mSlot); }, "Full", cardStyle);
    mQuality->SetTooltip("Full / Lite: change the model's compute size (brief switching fade)");
    AddChildControl(mQuality);
    AddChildControl(new IVButtonControl(
      IRECT(mRECT.R - 98, mRECT.T + 3, mRECT.R - 74, mRECT.T + 24), [this](IControl*) { Step(-1); }, "<", cardStyle));
    AddChildControl(new IVButtonControl(
      IRECT(mRECT.R - 72, mRECT.T + 3, mRECT.R - 48, mRECT.T + 24), [this](IControl*) { Step(1); }, ">", cardStyle));
    AddChildControl(new IVToggleControl(
      IRECT(mRECT.R - 44, mRECT.T + 3, mRECT.R - 5, mRECT.T + 24), SlotActiveParam(mSlot), "On", cardStyle));
    mModelButton = new IVButtonControl(
      IRECT(mRECT.L + 7, mRECT.T + 28, mRECT.R - 7, mRECT.T + 53), [this](IControl*) { ShowMenu(); },
      "Empty - choose...", cardStyle);
    AddChildControl(mModelButton);
    mNAMKnobs[0] = new IVKnobControl(
      IRECT(mRECT.L + 8, mRECT.T + 55, mRECT.L + 94, mRECT.B - 16), SlotInputParam(mSlot), "In", cardStyle);
    mNAMKnobs[1] = new IVKnobControl(
      IRECT(mRECT.L + 106, mRECT.T + 55, mRECT.R - 8, mRECT.B - 16), SlotTrimParam(mSlot), "Trim", cardStyle);
    for (auto* knob : mNAMKnobs)
      AddChildControl(knob);
    const char* labels[] = {"Thresh", "Decay", "Depth", "Thresh", "Release", "Makeup"};
    const char* tips[] = {"Open above this input level (dBFS)",
                          "Closing time constant in milliseconds",
                          "Maximum attenuation in dB; 0 dB leaves the signal open",
                          "Compress above this input level (dBFS)",
                          "Gain recovery time constant in milliseconds",
                          "Manual output gain in dB; start at 0 and match bypass loudness"};
    for (int i = 0; i < 6; ++i)
    {
      const float left = mRECT.L + 4.f + (i % 3) * 66.f;
      mFXKnobs[i] = new IVKnobControl(
        IRECT(left, mRECT.T + 55, left + 65.f, mRECT.B - 16), SlotDynamicsParam(mSlot, i), labels[i], cardStyle);
      mFXKnobs[i]->SetTooltip(tips[i]);
      AddChildControl(mFXKnobs[i]);
    }
    const char* eqLabels[] = {"Bass", "Mid", "Treble", "HP", "LP", "Presence", "Level"};
    const char* eqTips[] = {"150 Hz low shelf",
                            "800 Hz broad mid band",
                            "3 kHz high shelf",
                            "High pass: 0 Hz = off",
                            "Low pass: 20 kHz = off",
                            "3.5 kHz presence shelf",
                            "IR output gain in dB; -18 dB matches the original global cabinet level"};
    for (int i = 0; i < 7; ++i)
    {
      const float left = i == 6 ? mRECT.L + 60.f : mRECT.L + 4.f + (i % 3) * 66.f;
      mEQKnobs[i] = new IVKnobControl(IRECT(left, mRECT.T + 55, left + (i == 6 ? 85.f : 65.f), mRECT.B - 16),
                                      SlotEQParam(mSlot, i), eqLabels[i], cardStyle);
      mEQKnobs[i]->SetTooltip(eqTips[i]);
      AddChildControl(mEQKnobs[i]);
    }
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
    for (auto* knob : mNAMKnobs)
      knob->Hide(!nam);
    for (int i = 0; i < 6; ++i)
      mFXKnobs[i]->Hide(i < 3 ? !gate : !compressor);
    for (int i = 0; i < 7; ++i)
      mEQKnobs[i]->Hide(i < 3 ? effect != SlotEffect::ToneEQ : i < 6 ? effect != SlotEffect::CutEQ : !ir);
    const bool scalable = PLUG()->CanToggleSlotQuality(mSlot);
    mQuality->Hide(!nam || !PLUG()->IsSlotLoaded(mSlot));
    mQuality->SetDisabled(!scalable);
    mQuality->SetLabelStr(!scalable                                                     ? "Fixed"
                          : GetDelegate()->GetParam(SlotFullParam(mSlot))->Value() < .5 ? "Lite"
                                                                                        : "Full");
    auto label = !nam && !ir    ? std::string(SlotEffectName(effect))
                 : path.empty() ? std::string("Empty - choose...")
                                : nam_library::PathText(std::filesystem::u8path(path).stem());
    if (ir)
      label = "IR: " + label;
    if (!path.empty() && !PLUG()->IsSlotLoaded(mSlot))
      label = "Missing: " + label;
    if (label.size() > 27)
    {
      size_t end = 24;
      while (end > 0 && (static_cast<unsigned char>(label[end]) & 0xc0) == 0x80)
        --end;
      label = label.substr(0, end) + "...";
    }
    mModelButton->SetLabelStr(label.c_str());
    mModelButton->SetTooltip(!nam && !ir    ? SlotEffectName(effect)
                             : path.empty() ? "Choose NAM, gate, compressor, EQ or cabinet IR"
                                            : path.c_str());
    mModelButton->SetDirty(false);
  }
  void Load(const std::string& path)
  {
    ShowError(PLUG()->LoadSlot(mSlot, path.c_str()));
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
    GetUI()->CreatePopupMenu(*this, mMenu, IRECT(mRECT.L + 5, mRECT.T + 3, mRECT.L + 47, mRECT.T + 24));
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
    GetUI()->CreatePopupMenu(*this, mMenu, mModelButton->GetRECT());
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
  IVStyle mStyle;
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
