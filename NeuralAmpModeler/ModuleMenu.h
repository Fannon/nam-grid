#pragma once
#include "CaptureMenu.h"
#include "ModuleCatalog.h"

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
    files->AddSeparator(0);
    files->AddItem(new IPopupMenu::Item(type == 0 ? "Browse NAM captures..." : "Browse IR files...", 0, type == 0 ? -17 : -18), 0);
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
  for (const auto& group : kModuleGroups) addGroup(group.name, group.effects);
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
