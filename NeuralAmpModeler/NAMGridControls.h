#pragma once
#include "CaptureMenu.h"

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
  }

  void OnAttached() override
  {
    const auto text = IText(12.f, COLOR_WHITE, "Roboto-Regular");
    const auto cardStyle = mStyle.WithLabelText(text).WithValueText(text);
    AddChildControl(new ITextControl(IRECT(mRECT.L + 8, mRECT.T + 4, mRECT.L + 65, mRECT.T + 24),
                                     GridSlotName(mSlot).c_str(), text.WithAlign(EAlign::Near)));
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
      IRECT(mRECT.L + 8, mRECT.T + 57, mRECT.L + 94, mRECT.B - 3), SlotInputParam(mSlot), "In", cardStyle);
    mNAMKnobs[1] = new IVKnobControl(
      IRECT(mRECT.L + 106, mRECT.T + 57, mRECT.R - 8, mRECT.B - 3), SlotTrimParam(mSlot), "Trim", cardStyle);
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
        IRECT(left, mRECT.T + 57, left + 65.f, mRECT.B - 3), SlotDynamicsParam(mSlot, i), labels[i], cardStyle);
      mFXKnobs[i]->SetTooltip(tips[i]);
      AddChildControl(mFXKnobs[i]);
    }
    Refresh();
  }

  void OnMsgFromDelegate(int, int, const void*) override { Refresh(); }

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
    else if (item->GetTag() <= -21 && item->GetTag() >= -24)
      PLUG()->SetSlotEffect(mSlot, static_cast<SlotEffect>(-item->GetTag() - 20));
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
    for (auto* knob : mNAMKnobs)
      knob->Hide(!nam);
    for (int i = 0; i < 6; ++i)
      mFXKnobs[i]->Hide(nam || (i < 3) != gate);
    auto label = !nam           ? std::string(SlotEffectName(effect))
                 : path.empty() ? std::string("Empty - choose...")
                                : nam_library::PathText(std::filesystem::u8path(path).stem());
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
    mModelButton->SetTooltip(!nam           ? SlotEffectName(effect)
                             : path.empty() ? "Choose NAM, noise gate or compressor"
                                            : path.c_str());
    mModelButton->SetDirty(false);
  }
  void Load(const std::string& path)
  {
    const auto error = PLUG()->LoadSlot(mSlot, path.c_str());
    if (!error.empty())
    {
#ifdef OS_MAC
      GetUI()->ShowMessageBox("NAM slot load failed", error.c_str(), kMB_OK);
#else
      GetUI()->ShowMessageBox(error.c_str(), "NAM slot load failed", kMB_OK);
#endif
    }
    Refresh();
  }
  void ShowMenu()
  {
    mMenuLibrary = PLUG()->GetCaptureLibrary();
    nam_library::FillMenu(mMenu, *mMenuLibrary, PLUG()->GetCaptureFilter(), PLUG()->GetSlotPath(mSlot));
    mMenu.AddItem(new IPopupMenu::Item("Choose .nam file...", 0, -10), 0);
    mMenu.AddItem(new IPopupMenu::Item("Clear slot", 0, -11), 1);
    auto* gates = new IPopupMenu;
    auto* compressors = new IPopupMenu;
    const auto effect = PLUG()->GetSlotEffect(mSlot);
    for (int i = 1; i <= 4; ++i)
    {
      const auto type = static_cast<SlotEffect>(i);
      (i <= 2 ? gates : compressors)
        ->AddItem(new IPopupMenu::Item(SlotEffectName(type), type == effect ? IPopupMenu::Item::kChecked : 0, -20 - i));
    }
    mMenu.AddItem("Noise Gate", gates, 2);
    mMenu.AddItem("Compressor", compressors, 3);
    mMenu.AddSeparator(4);
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
  std::array<IControl*, 2> mNAMKnobs{};
  std::array<IControl*, 6> mFXKnobs{};
  IPopupMenu mMenu;
  std::shared_ptr<const nam_library::Snapshot> mMenuLibrary;
};
