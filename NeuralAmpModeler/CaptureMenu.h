#pragma once
#include "CaptureLibrary.h"
#include "IGraphicsStructs.h"
#include "IGraphicsPopupMenu.h"
#include <map>

namespace nam_library
{
// Menu tags index the immutable snapshot, so identically named files in different folders remain distinct.
inline void FillMenu(iplug::igraphics::IPopupMenu& menu, const Snapshot& library, const Filter& filter,
                     const std::string& selected)
{
  using Menu = iplug::igraphics::IPopupMenu;
  menu.Clear();
  std::map<std::filesystem::path, Menu*> directories{{{}, &menu}};
  for (size_t i = 0; i < library.captures.size(); ++i)
  {
    const auto& capture = library.captures[i];
    if (!filter.Matches(capture))
      continue;
    Menu* parent = &menu;
    std::filesystem::path path;
    for (const auto& directory : capture.relative.parent_path())
    {
      path /= directory;
      auto found = directories.find(path);
      if (found == directories.end())
      {
        auto* child = new Menu;
        parent->AddItem(PathText(directory).c_str(), child);
        directories[path] = child;
        parent = child;
      }
      else
        parent = found->second;
    }
    const auto label = PathText(capture.path.stem()) + (capture.isIR ? "  [IR / " : "  [NAM / ")
                       + GearName(capture.gear) + (capture.inferred ? ", inferred]" : "]");
    parent->AddItem(new Menu::Item(
      label.c_str(), PathText(capture.path) == selected ? Menu::Item::kChecked : 0, static_cast<int>(i)));
  }
  if (menu.NItems() == 0)
  {
    const auto label = library.captures.empty() ? std::string("Library has no indexed files")
                       : !filter.search.empty()
                         ? "No matches for search: " + filter.search
                         : "No matches for gear: " + std::string(GearName(static_cast<Gear>(filter.gear)));
    menu.AddItem(label.c_str(), -1, Menu::Item::kDisabled);
  }
}
} // namespace nam_library
