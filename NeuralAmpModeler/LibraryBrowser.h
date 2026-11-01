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
inline std::vector<std::string> DetailValues(const Capture& capture, const std::string& key)
{
  std::vector<std::string> values;
  for (const auto& hint : capture.hints)
    if (hint.key == key)
    {
      size_t start = 0;
      do
      {
        const auto end = key == "model" ? hint.value.find(" + ", start) : std::string::npos;
        const auto value = hint.value.substr(start, end == std::string::npos ? end : end - start);
        if (!value.empty() && std::find(values.begin(), values.end(), value) == values.end()) values.push_back(value);
        if (end == std::string::npos) break;
        start = end + 3;
      } while (start < hint.value.size());
    }
  return values;
}

inline std::string DetailQuery(const std::string& key, const std::string& value)
{
  std::string query = (key == "vendor" ? "maker" : key) + ":\"";
  for (const auto c : value) { if (c == '"' || c == '\\') query += '\\'; query += c; }
  return query + '"';
}

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
        if (c == '\\' && i < text.size() && (text[i] == '"' || text[i] == '\\')) { term.text += text[i++]; continue; }
        if (c == '"') { quoted = !quoted; continue; }
        if (!quoted && std::isspace(static_cast<unsigned char>(c))) break;
        term.text += c;
      }
      const auto colon = term.text.find(':');
      if (colon != std::string::npos)
      {
        const auto field = term.text.substr(0, colon);
        if (field == "name" || field == "maker" || field == "model" || field == "path" || field == "gear"
            || field == "vendor" || field == "creator" || field == "eq" || field == "gain" || field == "category" || field == "collection"
            || field == "channel" || field == "chain" || field == "settings" || field == "format" || field == "platform" || field == "mic/cab" || field == "speaker")
        { term.field = field; term.text.erase(0, colon + 1); }
      }
      if (!term.text.empty()) mTerms.push_back(std::move(term));
    }
  }
  int Score(const Capture& file) const
  {
    const auto hintValues = [&](const std::string& key) {
      std::string result;
      for (const auto& hint : file.hints) if (hint.key == key) result += " " + hint.value;
      return SearchText(result);
    };
    const auto filename = SearchText(PathText(file.path.stem()));
    const auto name = SearchText(file.name), maker = SearchText(file.maker) + hintValues("vendor"),
               model = SearchText(file.model) + hintValues("model");
    const auto path = SearchText(PathText(file.path));
    const auto gear = file.kind == FileKind::NAM ? SearchText(GearName(file.gear)) : "";
    const auto all = filename + " " + name + " " + maker + " " + model + " " + path + " " + gear + " " + SearchText(HintText(file));
    int score = 0;
    for (const auto& term : mTerms)
    {
      const auto& haystack = term.field == "name" ? filename : term.field == "maker" ? maker
                             : term.field == "model" ? model : term.field == "path" ? path
                             : term.field == "gear" ? gear : term.field.empty() ? all : hintValues(term.field);
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

inline std::vector<std::string> ContentFolders(const Snapshot& library, const Capture& selected)
{
  std::vector<std::filesystem::path> folders{selected.path.parent_path()};
  if (!selected.hash.empty())
    for (const auto& file : library.captures)
      if (file.kind == selected.kind && file.hash == selected.hash
          && std::none_of(folders.begin(), folders.end(), [&](const auto& folder) { return SamePath(folder, file.path.parent_path()); }))
        folders.push_back(file.path.parent_path());
  std::vector<std::string> result;
  for (const auto& folder : folders) result.push_back(PathText(folder));
  return result;
}

inline std::filesystem::path ContentCopyInFolder(const Snapshot& library, const Capture& selected,
                                                 const std::filesystem::path& folder)
{
  if (SamePath(selected.path.parent_path(), folder)) return selected.path;
  std::filesystem::path result;
  if (!selected.hash.empty())
    for (const auto& file : library.captures)
      if (file.kind == selected.kind && file.hash == selected.hash && SamePath(file.path.parent_path(), folder)
          && (result.empty() || file.path < result)) result = file.path;
  return result;
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
inline std::vector<size_t> KeepStarred(const Snapshot& library, std::vector<size_t> results, const Stars& stars,
                                       const std::filesystem::path& favoritesFolder)
{
  results.erase(std::remove_if(results.begin(), results.end(), [&](size_t index) {
    const auto key = StarKey(library.captures[index], favoritesFolder);
    return key.empty() || !stars.count(key);
  }), results.end());
  return results;
}

struct BrowserFolder { std::filesystem::path path; std::string label; int depth = 0, rootIndex = -1; std::string hintKey, hintValue; };
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
    std::vector<std::filesystem::path> ordered(children.begin(), children.end());
    // Compare each path component so underscore folders lead among siblings,
    // while every parent stays immediately before its own descendants.
    std::sort(ordered.begin(), ordered.end(), [&](const auto& a, const auto& b) {
      const auto ar = a.lexically_relative(root), br = b.lexically_relative(root);
      auto ai = ar.begin(), bi = br.begin();
      for (; ai != ar.end() && bi != br.end(); ++ai, ++bi)
      {
        const auto an = SearchText(PathText(*ai)), bn = SearchText(PathText(*bi));
        if (an == bn) { if (*ai != *bi) return PathText(*ai) < PathText(*bi); continue; }
        const bool au = !an.empty() && an.front() == '_', bu = !bn.empty() && bn.front() == '_';
        return au != bu ? au : an < bn;
      }
      return ai == ar.end() && bi != br.end();
    });
    for (const auto& child : ordered)
    {
      const auto relative = child.lexically_relative(root);
      result.push_back({child, PathText(child.filename()),
                        static_cast<int>(std::distance(relative.begin(), relative.end())), -1});
    }
  }
  return result;
}

inline std::string HintLabel(const std::string& key)
{
  if (key == "eq") return "EQ";
  if (key == "mic/cab") return "Mic / Cab";
  if (key == "chain") return "Captured FX";
  auto label = key;
  if (!label.empty()) label[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(label[0])));
  return label;
}

inline std::vector<BrowserFolder> BrowserHintFilters(const Snapshot& library, FileKind kind)
{
  std::set<std::pair<std::string, std::string>> values;
  for (const auto& file : library.captures)
    if (file.kind == kind)
      for (const auto& hint : file.hints)
        if (hint.key != "settings" && hint.key != "model") values.insert({hint.key, hint.value});
  std::vector<BrowserFolder> result;
  for (const auto& [key, value] : values) result.push_back({{}, HintLabel(key) + ": " + value, 0, -1, key, value});
  return result;
}

inline std::vector<size_t> KeepHint(const Snapshot& library, std::vector<size_t> results,
                                    const std::string& key, const std::string& value)
{
  if (!key.empty()) results.erase(std::remove_if(results.begin(), results.end(), [&](size_t i) {
    const auto& hints = library.captures[i].hints;
    return std::none_of(hints.begin(), hints.end(), [&](const auto& hint) { return hint.key == key && hint.value == value; });
  }), results.end());
  return results;
}
} // namespace nam_library
