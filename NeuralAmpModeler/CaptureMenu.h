#pragma once
#include "CaptureLibrary.h"
#include "IGraphicsStructs.h"
#include "IGraphicsPopupMenu.h"
#include <map>
#include <iterator>
#include <algorithm>
#include <cctype>
#include <vector>

namespace nam_library
{
// Menu tags index the immutable snapshot, so identically named files in different folders remain distinct.
inline void FillMenu(iplug::igraphics::IPopupMenu& menu, const Snapshot& library, const Filter& filter,
                     const std::string& selected, int fileType = -1)
{
  using Menu = iplug::igraphics::IPopupMenu;
  menu.Clear();
  const auto matches = [&](const auto& capture) {
    return filter.Matches(capture) && (fileType < 0 || capture.isIR == (fileType == 1));
  };
  // Typed categories already identify the collection. Skip shared folder levels
  // until the first branch (or a file), retaining the snapshot's original tags.
  std::filesystem::path common;
  bool first = true;
  if (fileType >= 0)
    for (const auto& capture : library.captures)
    {
      if (!matches(capture)) continue;
      const auto parent = capture.relative.parent_path();
      if (first) { common = parent; first = false; }
      else
      {
        std::filesystem::path prefix;
        auto a = common.begin(), b = parent.begin();
        for (; a != common.end() && b != parent.end() && *a == *b; ++a, ++b) prefix /= *a;
        common = std::move(prefix);
      }
      if (common.empty()) break;
    }
  const auto levels = std::distance(common.begin(), common.end());
  // Sort path components once, then create every directory before adding files.
  // This keeps folders first at every level without quadratic sorted insertion.
  std::vector<std::pair<std::vector<std::string>, size_t>> order;
  for (size_t i = 0; i < library.captures.size(); ++i)
    if (matches(library.captures[i]))
    {
      std::vector<std::string> key;
      for (const auto& part : library.captures[i].relative)
      {
        auto label = PathText(part);
        for (auto& ch : label) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        key.push_back(std::move(label));
      }
      order.emplace_back(std::move(key), i);
    }
  std::sort(order.begin(), order.end());
  std::map<std::filesystem::path, Menu*> directories{{{}, &menu}};
  std::vector<std::pair<Menu*, size_t>> leaves;
  for (const auto& entry : order)
  {
    const auto i = entry.second;
    const auto& capture = library.captures[i];
    Menu* parent = &menu;
    std::filesystem::path path;
    int skipped = 0;
    for (const auto& directory : capture.relative.parent_path())
    {
      if (skipped++ < levels) continue;
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
    leaves.emplace_back(parent, i);
  }
  for (const auto& [parent, i] : leaves)
  {
    const auto& capture = library.captures[i];
    const auto label = PathText(capture.path.stem());
    parent->AddItem(new Menu::Item(
      label.c_str(), PathText(capture.path) == selected ? Menu::Item::kChecked : 0, static_cast<int>(i)));
  }
  if (menu.NItems() == 0)
  {
    const auto label = fileType >= 0 ? std::string(fileType == 1 ? "No indexed cabinet IRs" : "No indexed NAM captures")
                       : library.captures.empty() ? std::string("Library has no indexed files")
                       : !filter.search.empty()
                         ? "No matches for search: " + filter.search
                         : "No matches for gear: " + std::string(GearName(static_cast<Gear>(filter.gear)));
    menu.AddItem(label.c_str(), -1, Menu::Item::kDisabled);
  }
}
} // namespace nam_library
