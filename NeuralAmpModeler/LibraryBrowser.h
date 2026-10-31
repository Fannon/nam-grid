#pragma once
#include "CaptureLibrary.h"
#include <algorithm>
#include <cctype>
#include <set>
#ifdef _WIN32
#include <windows.h>
#endif

namespace nam_library
{
inline std::string BoundedQuery(std::string query)
{
  if (query.size() <= 512) return query;
  size_t end = 512;
  while (end > 0 && (static_cast<unsigned char>(query[end]) & 0xc0) == 0x80) --end;
  query.resize(end);
  return query;
}
enum class BrowserContext { Library, Cell, Rig };
inline bool Allows(BrowserContext context, FileKind kind)
{
  if (kind == FileKind::SlotPreset) return false;
  return context == BrowserContext::Library
         || (context == BrowserContext::Rig ? kind == FileKind::RigPreset
                                            : kind == FileKind::NAM || kind == FileKind::IR || kind == FileKind::Module);
}

// Fold ASCII and common Latin accents without changing the original UTF-8 paths/labels.
inline std::string SearchText(const std::string& text)
{
  std::string result;
  for (size_t i = 0; i < text.size(); ++i)
  {
    auto c = static_cast<unsigned char>(text[i]);
    if (c == 0xc3 && i + 1 < text.size())
    {
      const auto next = static_cast<unsigned char>(text[i + 1]);
      const char* fold = nullptr;
      if ((next >= 0x80 && next <= 0x85) || (next >= 0xa0 && next <= 0xa5)) fold = "a";
      else if (next == 0x87 || next == 0xa7) fold = "c";
      else if ((next >= 0x88 && next <= 0x8b) || (next >= 0xa8 && next <= 0xab)) fold = "e";
      else if ((next >= 0x8c && next <= 0x8f) || (next >= 0xac && next <= 0xaf)) fold = "i";
      else if (next == 0x91 || next == 0xb1) fold = "n";
      else if ((next >= 0x92 && next <= 0x96) || (next >= 0xb2 && next <= 0xb6)
               || next == 0x98 || next == 0xb8) fold = "o";
      else if ((next >= 0x99 && next <= 0x9c) || (next >= 0xb9 && next <= 0xbc)) fold = "u";
      else if (next == 0x9d || next == 0xbd || next == 0xbf) fold = "y";
      else if (next == 0x9f) fold = "ss";
      if (fold) { result += fold; ++i; continue; }
    }
    result += c < 128 ? static_cast<char>(std::tolower(c)) : static_cast<char>(c);
  }
  return result;
}

class Search
{
public:
  explicit Search(const std::string& query)
  {
    const auto text = SearchText(BoundedQuery(query));
    for (size_t i = 0; i < text.size();)
    {
      while (i < text.size() && std::isspace(static_cast<unsigned char>(text[i]))) ++i;
      if (i == text.size()) break;
      Term term;
      if (text[i] == '-') { term.exclude = true; ++i; }
      bool quoted = false;
      while (i < text.size())
      {
        const char c = text[i++];
        if (c == '"') { quoted = !quoted; continue; }
        if (!quoted && std::isspace(static_cast<unsigned char>(c))) break;
        term.text += c;
      }
      const auto colon = term.text.find(':');
      if (colon != std::string::npos)
      {
        const auto field = term.text.substr(0, colon);
        if (field == "name" || field == "maker" || field == "model" || field == "path" || field == "gear")
        { term.field = field; term.text.erase(0, colon + 1); }
      }
      if (!term.text.empty()) mTerms.push_back(std::move(term));
    }
  }
  int Score(const Capture& file) const
  {
    const auto filename = SearchText(PathText(file.path.stem()));
    const auto name = SearchText(file.name), maker = SearchText(file.maker), model = SearchText(file.model);
    const auto path = SearchText(PathText(file.path));
    const auto gear = file.kind == FileKind::NAM ? SearchText(GearName(file.gear)) : "";
    const auto all = filename + " " + name + " " + maker + " " + model + " " + path + " " + gear;
    int score = 0;
    for (const auto& term : mTerms)
    {
      const auto& haystack = term.field == "name" ? filename : term.field == "maker" ? maker
                             : term.field == "model" ? model : term.field == "path" ? path
                             : term.field == "gear" ? gear : all;
      const bool found = haystack.find(term.text) != std::string::npos;
      if (found == term.exclude) return -1;
      if (!term.exclude)
        score += filename == term.text ? 100 : filename.find(term.text) == 0 ? 60
                 : filename.find(term.text) != std::string::npos ? 30 : 5;
    }
    return score;
  }
private:
  struct Term { std::string text, field; bool exclude = false; };
  std::vector<Term> mTerms;
};

inline bool SamePath(std::filesystem::path first, std::filesystem::path second)
{
  first = first.lexically_normal().make_preferred();
  second = second.lexically_normal().make_preferred();
#ifdef _WIN32
  return CompareStringOrdinal(first.c_str(), -1, second.c_str(), -1, TRUE) == CSTR_EQUAL;
#else
  return first == second;
#endif
}

inline bool Within(const std::filesystem::path& file, const std::filesystem::path& folder)
{
  if (folder.empty()) return true;
  auto a = file.begin(), b = folder.begin();
  for (; b != folder.end(); ++a, ++b)
  {
    if (a == file.end()) return false;
#ifdef _WIN32
    if (CompareStringOrdinal(a->c_str(), -1, b->c_str(), -1, TRUE) != CSTR_EQUAL) return false;
#else
    if (*a != *b) return false;
#endif
  }
  return true;
}

inline std::vector<size_t> Browse(const Snapshot& library, FileKind kind, const std::string& query,
                                  const std::filesystem::path& folder = {}, int gear = -1, bool relevance = true)
{
  const Search search(query);
  struct Match { size_t index; int score; std::string name, path; };
  std::vector<Match> matches;
  for (size_t i = 0; i < library.captures.size(); ++i)
  {
    const auto& file = library.captures[i];
    if (file.kind != kind || !Within(file.path, folder)
        || (kind == FileKind::NAM && gear >= 0 && static_cast<int>(file.gear) != gear)) continue;
    const int score = search.Score(file);
    if (score >= 0) matches.push_back({i, score, SearchText(PathText(file.path.stem())), PathText(file.path)});
  }
  std::sort(matches.begin(), matches.end(), [relevance](const auto& a, const auto& b) {
    if (relevance && a.score != b.score) return a.score > b.score;
    return a.name == b.name ? a.path < b.path : a.name < b.name;
  });
  std::vector<size_t> result;
  for (const auto& match : matches) result.push_back(match.index);
  return result;
}

// The browser's Starred filter: keeps the results that carry any star, in their order.
inline std::vector<size_t> KeepStarred(const Snapshot& library, std::vector<size_t> results,
                                       const std::map<std::string, int>& stars)
{
  results.erase(std::remove_if(results.begin(), results.end(), [&](size_t index) {
    const auto& file = library.captures[index];
    return file.kind == FileKind::RigPreset || !stars.count(StarKey(file.kind, file.path, file.module));
  }), results.end());
  return results;
}

struct BrowserFolder { std::filesystem::path path; std::string label; int depth = 0, rootIndex = -1; };
inline std::vector<BrowserFolder> BrowserFolders(const Snapshot& library, FileKind kind,
                                                const std::vector<std::string>& roots)
{
  std::vector<BrowserFolder> result{{{}, "All folders"}};
  for (size_t i = 0; i < roots.size(); ++i)
  {
    const auto root = std::filesystem::u8path(roots[i]);
    // Hide roots that only hold other kinds (e.g. an IR-only folder in NAM captures).
    // Empty, missing or unscanned roots stay listed so they can still be removed.
    bool other = false, matching = false;
    for (const auto& file : library.captures)
      if (Within(file.path, root)) (file.kind == kind ? matching : other) = true;
    if (other && !matching) continue;
    auto label = PathText(root.filename());
    if (label.empty()) label = roots[i];
    // Same basenames stay distinguishable, including empty or unavailable sources.
    for (size_t j = 0; j < roots.size(); ++j)
      if (i != j && SearchText(PathText(std::filesystem::u8path(roots[j]).filename())) == SearchText(label))
      { label += " (" + std::to_string(i + 1) + ")"; break; }
    result.push_back({root, label, 0, static_cast<int>(i)});
    std::set<std::filesystem::path> children;
    for (const auto& file : library.captures)
      if (file.kind == kind && Within(file.path, root))
        for (auto parent = file.path.parent_path(); parent != root && Within(parent, root); parent = parent.parent_path())
          children.insert(parent);
    for (const auto& child : children)
    {
      const auto relative = child.lexically_relative(root);
      result.push_back({child, PathText(child.filename()),
                        static_cast<int>(std::distance(relative.begin(), relative.end())), -1});
    }
  }
  return result;
}
} // namespace nam_library
