#pragma once
#include "CaptureMenu.h"
#include "SlotDynamics.h"

namespace nam_library
{
// Insert commands in order ahead of the existing library tree. Tags remain stable
// independently of optional quality, filtering and scanning entries.
inline void FillModuleMenu(iplug::igraphics::IPopupMenu& menu, const Snapshot& library, const Filter& filter,
                           const std::string& selected, SlotEffect effect, bool canToggleQuality, bool lite,
                           bool scanning)
{
  using IPopupMenu = iplug::igraphics::IPopupMenu;
  FillMenu(menu, library, filter, selected);
  int next = 0;
  menu.AddItem(new IPopupMenu::Item("Choose .nam file...", 0, -10), next++);
  menu.AddItem(new IPopupMenu::Item("Choose cabinet IR (.wav)...", 0, -12), next++);
  menu.AddItem(new IPopupMenu::Item("Clear slot", 0, -11), next++);
  auto* gates = new IPopupMenu;
  auto* compressors = new IPopupMenu;
  auto* eq = new IPopupMenu;
  for (int i = 1; i <= 6; ++i)
  {
    const auto type = static_cast<SlotEffect>(i);
    (i <= 2   ? gates
     : i <= 4 ? compressors
              : eq)
      ->AddItem(new IPopupMenu::Item(SlotEffectName(type), type == effect ? IPopupMenu::Item::kChecked : 0, -20 - i));
  }
  menu.AddItem("Noise Gate", gates, next++);
  menu.AddItem("Compressor", compressors, next++);
  eq->AddItem(new IPopupMenu::Item(SlotEffectName(SlotEffect::ParametricEQ),
                                   effect == SlotEffect::ParametricEQ ? IPopupMenu::Item::kChecked : 0, -28));
  menu.AddItem("EQ", eq, next++);
  menu.AddItem(
    new IPopupMenu::Item("Boost", effect == SlotEffect::Boost ? IPopupMenu::Item::kChecked : 0, -29), next++);
  const auto addGroup = [&](const char* title, SlotEffect first, SlotEffect last) {
    auto* group = new IPopupMenu;
    for (int i = static_cast<int>(first); i <= static_cast<int>(last); ++i)
    {
      const auto type = static_cast<SlotEffect>(i);
      group->AddItem(
        new IPopupMenu::Item(SlotEffectName(type), type == effect ? IPopupMenu::Item::kChecked : 0, -20 - i));
    }
    if (first == SlotEffect::RoomReverb || first == SlotEffect::SynthFilter)
    {
      const auto type = first == SlotEffect::RoomReverb ? SlotEffect::SpringReverb : SlotEffect::Wah;
      group->AddItem(new IPopupMenu::Item(
        SlotEffectName(type), type == effect ? IPopupMenu::Item::kChecked : 0, -20 - static_cast<int>(type)));
    }
    menu.AddItem(title, group, next++);
  };
  addGroup("Reverb", SlotEffect::RoomReverb, SlotEffect::CloudReverb);
  addGroup("Delay", SlotEffect::DigitalDelay, SlotEffect::TapeDelay);
  menu.AddItem(new IPopupMenu::Item("Slapback preset", 0, -16), next++);
  addGroup("Modulation", SlotEffect::Chorus, SlotEffect::Vibrato);
  addGroup("Rotary / Vibe", SlotEffect::Rotary, SlotEffect::UniVibe);
  addGroup("Dynamics / Pitch", SlotEffect::Limiter, SlotEffect::Doubler);
  addGroup("Creative", SlotEffect::ReverseDelay, SlotEffect::RingMod);
  addGroup("Filter / Volume", SlotEffect::SynthFilter, SlotEffect::AutoSwell);
  for (auto type : {SlotEffect::Tape, SlotEffect::TubeBoost, SlotEffect::Utility})
    menu.AddItem(new IPopupMenu::Item(
                   SlotEffectName(type), type == effect ? IPopupMenu::Item::kChecked : 0, -20 - static_cast<int>(type)),
                 next++);
  menu.AddSeparator(next++);
  if (canToggleQuality)
    menu.AddItem(new IPopupMenu::Item("A2 Lite", lite ? IPopupMenu::Item::kChecked : 0, -15), next++);
  if (!filter.search.empty() || filter.gear >= 0)
    menu.AddItem(new IPopupMenu::Item("Clear search and gear filter", 0, -13), 0);
  if (library.captures.empty())
    menu.AddItem(new IPopupMenu::Item(scanning ? "Scanning library..." : "Rescan library",
                                      scanning ? IPopupMenu::Item::kDisabled : 0, -14),
                 0);
}
} // namespace nam_library
