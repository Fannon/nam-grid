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
  menu.Clear();
  for (int type = 0; type < 2; ++type)
  {
    auto* files = new IPopupMenu;
    FillMenu(*files, library, filter, selected, type);
    menu.AddItem(type == 0 ? "NAM captures" : "Cabinet IRs", files);
  }
  int next = 0;
  menu.AddItem(new IPopupMenu::Item("Clear slot", 0, -11), next++);
  menu.AddItem(new IPopupMenu::Item("Choose .nam file...", 0, -10), next++);
  menu.AddItem(new IPopupMenu::Item("Choose cabinet IR (.wav)...", 0, -12), next++);
  menu.AddSeparator(next++);
  const auto addGroup = [&](const char* title, std::initializer_list<SlotEffect> types) {
    auto* group = new IPopupMenu;
    for (const auto type : types)
    {
      group->AddItem(
        new IPopupMenu::Item(SlotEffectName(type), type == effect ? IPopupMenu::Item::kChecked : 0,
                             -20 - static_cast<int>(type)));
    }
    if (std::string(title) == "Delay") group->AddItem(new IPopupMenu::Item("Slapback preset", 0, -16));
    menu.AddItem(title, group, next++);
  };
  using E = SlotEffect;
  addGroup("Distortion", {E::Distortion, E::Boost, E::TubeBoost, E::Tape, E::Bitcrusher});
  addGroup("Dynamics", {E::NaturalGate, E::TightGate, E::NoisePrint, E::SmoothComp, E::PunchComp, E::Limiter});
  addGroup("EQ / Filter", {E::ToneEQ, E::CutEQ, E::ParametricEQ, E::SynthFilter, E::Wah});
  addGroup("Modulation", {E::Chorus, E::JunoChorus, E::Flanger, E::Phaser, E::Tremolo, E::Vibrato,
                           E::Rotary, E::UniVibe, E::RingMod});
  addGroup("Pitch / Frequency", {E::Octaver, E::Doubler, E::FrequencyShifter});
  addGroup("Delay", {E::DigitalDelay, E::AnalogDelay, E::TapeDelay, E::ReverseDelay, E::DualDelay});
  addGroup("Reverb", {E::RoomReverb, E::PlateReverb, E::CloudReverb, E::SpringReverb, E::ShimmerReverb});
  addGroup("Creative", {E::Freeze, E::Stutter});
  addGroup("Utility / Volume", {E::Utility, E::AutoSwell});
  menu.AddSeparator(next++);
  if (canToggleQuality)
    menu.AddItem(new IPopupMenu::Item("A2 Lite", lite ? IPopupMenu::Item::kChecked : 0, -15), next++);
  if (!filter.search.empty() || filter.gear >= 0)
    menu.AddItem(new IPopupMenu::Item("Clear search and gear filter", 0, -13), next++);
  if (library.captures.empty())
    menu.AddItem(new IPopupMenu::Item(scanning ? "Scanning library..." : "Rescan library",
                                      scanning ? IPopupMenu::Item::kDisabled : 0, -14),
                 next++);
}
} // namespace nam_library
