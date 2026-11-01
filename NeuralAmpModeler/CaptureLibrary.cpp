#include "CaptureLibrary.h"
#include "RigPreset.h"
#include "LibraryBrowser.h"
#include "FilenameHintVocabulary.h"
#include <regex>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <functional>
#include <sstream>
#include <string_view>
#include <map>
#include <set>
#include <utility>
#include <unordered_set>
#include <unordered_map>
#include "json.hpp"
#include "../iPlug2/WDL/sha.h"
#include <array>

namespace nam_library
{
namespace
{
std::string Lower(std::string value)
{
  for (auto& ch : value)
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  return value;
}

std::string Token(std::string value)
{
  value = Lower(std::move(value));
  std::erase_if(value, [](unsigned char ch) { return !std::isalnum(ch); });
  return value;
}

Gear Category(const std::string& text)
{
  const auto value = Token(text);
  if (value == "ampcab" || value == "ampandcab" || value == "fullrig" || value == "amppedalcab")
    return Gear::AmpCab;
  if (value == "amphead" || value == "ampheads" || value == "head" || value == "directamp")
    return Gear::AmpHead;
  if (value == "cab" || value == "cabinet" || value == "cabinets")
    return Gear::Cabinet;
  if (value == "pedal" || value == "pedals" || value == "stompbox")
    return Gear::Pedal;
  if (value == "outboard" || value == "studio")
    return Gear::Outboard;
  if (value == "space" || value == "spaces" || value == "room")
    return Gear::Spaces;
  // "amp" alone does not establish whether the capture includes a cabinet.
  return Gear::Unknown;
}

Gear FolderCategory(const std::filesystem::path& folder)
{
  const auto text = PathText(folder);
  const auto explicitCategory = Category(text);
  if (explicitCategory != Gear::Unknown)
    return explicitCategory;
  const auto value = Token(text);
  if (value == "fullrigs" || value == "ampcabs" || value == "ampandcabs" || value == "ampcabinet")
    return Gear::AmpCab;
  if (value == "directamps" || value == "heads")
    return Gear::AmpHead;
  if (value == "cabs" || value == "irs" || value == "impulseresponses")
    return Gear::Cabinet;
  if (value == "stompboxes" || value == "stomps")
    return Gear::Pedal;
  if (value == "acousticline" || value == "acousticandline" || value == "studiopreamps" || value == "rackgear"
      || value == "outboardgear")
    return Gear::Outboard;
  if (value == "rooms" || value == "reverbs")
    return Gear::Spaces;
  return Gear::Unknown;
}

std::string FilenameWords(std::string text)
{
  text = Lower(std::move(text));
  for (auto& ch : text)
    if (!std::isalnum(static_cast<unsigned char>(ch)))
      ch = ' ';
  std::istringstream input(text);
  std::string words = " ";
  for (std::string word; input >> word;)
    words += word + " ";
  return words;
}

// Parse only through the root metadata object. Stop before the usually much larger weights array.
// Metadata can appear after config/weights, in which case SAX skips those values without retaining them.
struct MetadataReader : nlohmann::json_sax<nlohmann::json>
{
  int depth = 0;
  bool inMetadata = false, complete = false;
  std::string rootKey, field;
  std::map<std::string, std::string> values;
  bool null() override { return true; }
  bool boolean(bool) override { return true; }
  bool number_integer(number_integer_t) override { return true; }
  bool number_unsigned(number_unsigned_t) override { return true; }
  bool number_float(number_float_t, const string_t&) override { return true; }
  bool string(string_t& value) override
  {
    if (inMetadata && depth == 2 && value.size() <= 4096)
      values[field] = value;
    return true;
  }
  bool binary(binary_t&) override { return true; }
  bool start_object(std::size_t) override
  {
    if (depth == 1 && rootKey == "metadata")
      inMetadata = true;
    ++depth;
    return depth < 128;
  }
  bool key(string_t& key) override
  {
    if (depth == 1)
      rootKey = key;
    if (depth == 2 && inMetadata)
      field = key;
    return true;
  }
  bool end_object() override
  {
    if (depth == 2 && inMetadata)
    {
      complete = true;
      return false; // Intentional early stop, not a validation of the inference model.
    }
    --depth;
    return true;
  }
  bool start_array(std::size_t) override { return ++depth < 128; }
  bool end_array() override
  {
    --depth;
    return true;
  }
  bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override { return false; }
};
} // namespace

std::string PathText(const std::filesystem::path& path)
{
  const auto text = path.u8string();
  return {text.begin(), text.end()};
}


namespace
{
// One path segment against one .ignore pattern segment: `*`, `?`, `[...]` classes and
// `\` escapes, ignoring ASCII case.
bool MatchSegment(std::string_view pattern, std::string_view text)
{
  const auto fold = [](char c) { return static_cast<char>(std::tolower(static_cast<unsigned char>(c))); };
  // Length of the pattern element at `at` when it matches `c`, otherwise 0.
  const auto element = [&](size_t at, char c) -> size_t {
    if (pattern[at] == '?')
      return 1;
    if (pattern[at] == '\\' && at + 1 < pattern.size())
      return fold(pattern[at + 1]) == fold(c) ? 2 : 0;
    if (pattern[at] == '[')
    {
      size_t i = at + 1;
      const bool negate = i < pattern.size() && (pattern[i] == '!' || pattern[i] == '^');
      if (negate)
        ++i;
      bool found = false;
      for (bool first = true; i < pattern.size() && (first || pattern[i] != ']'); first = false)
      {
        const char low = fold(pattern[i++]);
        char high = low;
        if (i + 1 < pattern.size() && pattern[i] == '-' && pattern[i + 1] != ']')
        {
          high = fold(pattern[i + 1]);
          i += 2;
        }
        found |= fold(c) >= low && fold(c) <= high;
      }
      if (i < pattern.size())
        return found != negate ? i + 1 - at : 0;
      // An unclosed '[' is an ordinary character.
    }
    return fold(pattern[at]) == fold(c) ? 1 : 0;
  };
  size_t p = 0, t = 0, star = std::string_view::npos, starText = 0;
  while (t < text.size())
  {
    if (p < pattern.size() && pattern[p] == '*')
    {
      star = ++p;
      starText = t;
      continue;
    }
    if (p < pattern.size())
      if (const size_t length = element(p, text[t]))
      {
        p += length;
        ++t;
        continue;
      }
    if (star == std::string_view::npos)
      return false;
    p = star;
    t = ++starText;
  }
  while (p < pattern.size() && pattern[p] == '*')
    ++p;
  return p == pattern.size();
}

// `**` matches any number of folders; a trailing `**` matches everything inside a folder
// but not the folder itself.
bool MatchSegments(const std::vector<std::string>& pattern, size_t p, const std::vector<std::string>& path, size_t t)
{
  if (p == pattern.size())
    return t == path.size();
  if (pattern[p] == "**")
  {
    for (size_t next = p + 1 == pattern.size() ? t + 1 : t; next <= path.size(); ++next)
      if (MatchSegments(pattern, p + 1, path, next))
        return true;
    return false;
  }
  return t < path.size() && MatchSegment(pattern[p], path[t]) && MatchSegments(pattern, p + 1, path, t + 1);
}

// Path components of `path` below `root`; empty when it is not inside. Case-insensitive
// on Windows, where library folders and saved paths may differ in case.
std::vector<std::string> SegmentsBelow(const std::filesystem::path& path, const std::filesystem::path& root)
{
  std::vector<std::string> segments, rootParts, pathParts;
  for (const auto& part : root.lexically_normal())
    if (!part.empty()) rootParts.push_back(PathText(part));
  for (const auto& part : path.lexically_normal())
    if (!part.empty()) pathParts.push_back(PathText(part));
  if (pathParts.size() <= rootParts.size())
    return segments;
  for (size_t i = 0; i < rootParts.size(); ++i)
  {
#ifdef _WIN32
    if (Lower(rootParts[i]) != Lower(pathParts[i]))
#else
    if (rootParts[i] != pathParts[i])
#endif
      return segments;
  }
  segments.assign(pathParts.begin() + static_cast<std::ptrdiff_t>(rootParts.size()), pathParts.end());
  return segments;
}
} // namespace

const std::vector<IgnoreRules::Pattern>& IgnoreRules::Patterns(const std::filesystem::path& folder)
{
  const auto [found, inserted] = mFolders.try_emplace(folder);
  auto& patterns = found->second;
  if (!inserted)
    return patterns;
  for (const char* name : {".ignore", ".ignore.txt"})
  {
    std::error_code error;
    const auto file = folder / name;
    if (!std::filesystem::is_regular_file(file, error) || std::filesystem::file_size(file, error) > 65536 || error)
      continue;
    std::ifstream input(file, std::ios::binary);
    std::string line;
    for (bool first = true; std::getline(input, line); first = false)
    {
      if (first && line.starts_with("\xEF\xBB\xBF"))
        line.erase(0, 3); // Notepad's UTF-8 byte order mark.
      if (!line.empty() && line.back() == '\r')
        line.pop_back();
      // Trailing spaces end a pattern unless escaped with a backslash.
      while (!line.empty() && line.back() == ' ' && !(line.size() > 1 && line[line.size() - 2] == '\\'))
        line.pop_back();
      if (line.empty() || line[0] == '#')
        continue;
      Pattern pattern;
      if (line[0] == '!')
      {
        pattern.negate = true;
        line.erase(0, 1);
      }
      else if (line.size() > 1 && line[0] == '\\' && (line[1] == '#' || line[1] == '!'))
        line.erase(0, 1);
      if (!line.empty() && line.back() == '/')
      {
        pattern.directoryOnly = true;
        line.pop_back();
      }
      // A slash anywhere else anchors the pattern to this folder; otherwise it matches a
      // name at any depth.
      if (line.find('/') == std::string::npos)
        pattern.segments.push_back("**");
      for (size_t start = 0; start < line.size();)
      {
        const auto end = std::min(line.find('/', start), line.size());
        if (end > start)
          pattern.segments.push_back(line.substr(start, end - start));
        start = end + 1;
      }
      if (!pattern.segments.empty() && !(pattern.segments.size() == 1 && pattern.segments[0] == "**"))
        patterns.push_back(std::move(pattern));
    }
  }
  return patterns;
}

bool IgnoreRules::Matches(const std::filesystem::path& path, bool directory)
{
  const auto segments = SegmentsBelow(path, mRoot);
  if (segments.empty())
    return false;
  bool ignored = false;
  auto folder = mRoot;
  for (size_t depth = 0; depth < segments.size(); ++depth)
  {
    // Each folder's patterns see the entry relative to that folder; deeper ones win.
    const std::vector<std::string> below(segments.begin() + static_cast<std::ptrdiff_t>(depth), segments.end());
    for (const auto& pattern : Patterns(folder))
      if ((directory || !pattern.directoryOnly) && MatchSegments(pattern.segments, 0, below, 0))
        ignored = !pattern.negate;
    folder /= std::filesystem::u8path(segments[depth]);
  }
  return ignored;
}

bool IgnoreRules::Ignored(const std::filesystem::path& path, bool directory)
{
  const auto segments = SegmentsBelow(path, mRoot);
  auto folder = mRoot;
  for (size_t i = 0; i + 1 < segments.size(); ++i)
  {
    folder /= std::filesystem::u8path(segments[i]);
    if (Matches(folder, true))
      return true; // A file inside an excluded folder cannot be re-included.
  }
  return !segments.empty() && Matches(path, directory);
}

const char* GearName(Gear gear)
{
  switch (gear)
  {
    case Gear::AmpCab: return "Amp + Cab";
    case Gear::AmpHead: return "Amp Head";
    case Gear::Cabinet: return "Cabinet";
    case Gear::Pedal: return "Pedal";
    case Gear::Outboard: return "Outboard";
    case Gear::Spaces: return "Spaces";
    default: return "Unknown";
  }
}

std::vector<std::string> NormalizeFolders(const std::vector<std::string>& folders)
{
  std::vector<std::string> result, keys;
  for (const auto& folder : folders)
  {
    if (folder.empty() || folder.size() > 32768 || folder.find('\0') != std::string::npos)
      continue;
    std::error_code error;
    auto path = std::filesystem::weakly_canonical(std::filesystem::u8path(folder), error);
    if (error)
      path = std::filesystem::u8path(folder).lexically_normal();
    while (path.has_relative_path() && path.filename().empty()) path = path.parent_path();
    auto key = PathText(path);
#ifdef _WIN32
    key = Lower(key);
#endif
    if (std::find(keys.begin(), keys.end(), key) == keys.end())
    {
      keys.push_back(key);
      result.push_back(PathText(path));
    }
    if (result.size() == 32)
      break;
  }
  return result;
}

Settings ReadSettings(const std::filesystem::path& settingsFile)
{
  Settings result;
  try
  {
    if (!std::filesystem::exists(settingsFile) || std::filesystem::file_size(settingsFile) > 1024 * 1024)
      return result;
    std::ifstream stream(settingsFile, std::ios::binary);
    const auto config = nlohmann::json::parse(stream);
    result.namFolders = NormalizeFolders(config.value("NamFolders", std::vector<std::string>{}));
    result.irFolders = NormalizeFolders(config.value("IrFolders", std::vector<std::string>{}));
    result.rescanOnStartup = config.value("RescanOnStartup", true);
    result.showVisualizations = config.value("ShowVisualizations", true);
    result.showMainMacros = config.value("ShowMainMacros", true);
    result.presetCrossfade = config.value("PresetCrossfade", false);
    result.presetFolders = NormalizeFolders(config.value("PresetFolders", std::vector<std::string>{}));
    result.namChosen = config.contains("NamFolders");
    result.irChosen = config.contains("IrFolders");
    result.presetChosen = config.contains("PresetFolders");
    return result;
  }
  catch (const std::exception&)
  {
    return {};
  }
}
bool WriteSettings(const std::filesystem::path& settingsFile, const Settings& settings)
{
  try
  {
    std::filesystem::create_directories(settingsFile.parent_path());
    rig_preset::WriteAtomically(settingsFile,
      nlohmann::json{{"NamFolders", settings.namFolders}, {"IrFolders", settings.irFolders},
                     {"RescanOnStartup", settings.rescanOnStartup},
                     {"ShowVisualizations", settings.showVisualizations},
                     {"ShowMainMacros", settings.showMainMacros},
                     {"PresetCrossfade", settings.presetCrossfade},
                     {"PresetFolders", settings.presetFolders}}.dump(2));
    return true;
  }
  catch (const std::exception&)
  {
    return false;
  }
}
UserSettings PrepareUserSettings(const std::filesystem::path& settingsFile, const std::filesystem::path& legacyFile,
                                 const std::filesystem::path& contentFolder)
{
  UserSettings result;
  auto& settings = result.settings;
  std::error_code error;
  const bool current = !settingsFile.empty() && std::filesystem::exists(settingsFile, error);
  bool changed = !current;
  if (current)
    settings = ReadSettings(settingsFile);
  else if (!legacyFile.empty() && std::filesystem::exists(legacyFile, error))
  {
    settings = ReadSettings(legacyFile);
    // Older builds had no default folders, so an empty list there was never a choice.
    settings.namChosen = !settings.namFolders.empty();
    settings.irChosen = !settings.irFolders.empty();
    settings.presetChosen = !settings.presetFolders.empty();
  }
  const auto adopt = [&](std::vector<std::string>& folders, bool& chosen, const char* name) {
    if (chosen || contentFolder.empty()) return std::filesystem::path{};
    const auto folder = contentFolder / name;
    std::filesystem::create_directories(folder, error);
    if (error) return std::filesystem::path{};
    // A commented .ignore shows that hiding files is possible; comments match nothing.
    if (const auto ignore = folder / ".ignore"; !std::filesystem::exists(ignore, error))
      std::ofstream(ignore, std::ios::binary)
        << "# Hide files and folders from NAM Grid's library: one pattern per line, as in .gitignore.\n"
           "# Lines starting with # are comments. Rescan after editing. Examples:\n"
           "# Old captures/\n"
           "# *DI.nam\n";
    folders = {PathText(folder)};
    chosen = changed = true;
    return folder;
  };
  result.newNamFolder = adopt(settings.namFolders, settings.namChosen, "NAM Captures");
  adopt(settings.irFolders, settings.irChosen, "IRs");
  result.newPresetFolder = adopt(settings.presetFolders, settings.presetChosen, "Presets");
  if (changed && !settingsFile.empty())
    result.saved = WriteSettings(settingsFile, settings);
  return result;
}
std::vector<std::string> ReadFolderSettings(const std::filesystem::path& settingsFile)
{ return ReadSettings(settingsFile).namFolders; }
bool WriteFolderSettings(const std::filesystem::path& settingsFile, const std::vector<std::string>& folders)
{
  auto settings = ReadSettings(settingsFile);
  settings.namFolders = folders;
  return WriteSettings(settingsFile, settings);
}

std::string ReadFolderSetting(const std::filesystem::path& settingsFile)
{
  const auto folders = ReadFolderSettings(settingsFile);
  return folders.empty() ? std::string{} : folders.front();
}
bool WriteFolderSetting(const std::filesystem::path& settingsFile, const std::string& folder)
{
  return WriteFolderSettings(
    settingsFile, folder.empty() ? std::vector<std::string>{} : std::vector<std::string>{folder});
}

std::vector<LibraryHint> InferFilenameHints(const Capture& capture)
{
  if (capture.kind != FileKind::NAM && capture.kind != FileKind::IR) return {};
  const auto normalize = [](const std::string& text) {
    std::string result = " ";
    for (const unsigned char c : text)
      if (std::isalnum(c)) result += static_cast<char>(std::tolower(c));
      else if (result.back() != ' ') result += ' ';
    if (result.back() != ' ') result += ' ';
    return result;
  };
  const auto stem = PathText(capture.path.stem());
  const auto words = normalize(stem);
  std::vector<std::string> fields;
  for (size_t start = 0;;)
  {
    const auto end = stem.find(" - ", start);
    fields.push_back(stem.substr(start, end == std::string::npos ? end : end - start));
    if (end == std::string::npos) break;
    start = end + 3;
  }
  const bool structured = fields.size() >= 3;
  const auto setting = structured ? fields[2] : stem;
  const auto settingsWords = normalize(setting);
  std::vector<LibraryHint> hints;
  const auto add = [&](const std::string& key, const std::string& value) {
    if (std::none_of(hints.begin(), hints.end(), [&](const auto& h) { return h.key == key && h.value == value; }))
      hints.push_back({key, value});
  };
  const auto contains = [&](const std::string& token) { return words.find(normalize(token)) != std::string::npos; };
  // Device brands precede the first spaced separator; cabinet and creator
  // brands in subsequent segments do not identify the captured device.
  const auto deviceName = stem.substr(0, stem.find(" - "));
  const auto device = normalize(deviceName);
  for (size_t start = 0;;)
  {
    const auto end = deviceName.find(" + ", start);
    auto part = normalize(deviceName.substr(start, end == std::string::npos ? end : end - start));
    const auto slotEnd = part.find(' ', 1);
    const auto slot = part.substr(1, slotEnd - 1);
    if (slot.size() == 1 || (slot.size() == 2 && std::isdigit(static_cast<unsigned char>(slot[0])) && std::isdigit(static_cast<unsigned char>(slot[1]))))
      part.erase(0, slotEnd);
    for (const auto& alias : filename_hints::vendors)
      if (part.find(normalize(alias.token)) == 0) add(alias.key, alias.label);
    if (end == std::string::npos) break;
    start = end + 3;
  }
  if (capture.kind == FileKind::NAM)
  {
    std::vector<std::pair<size_t, size_t>> modelSpans;
    for (const auto& alias : filename_hints::models)
    {
      const auto token = normalize(alias.token);
      const auto pos = device.find(token);
      if (pos == std::string::npos || std::any_of(modelSpans.begin(), modelSpans.end(), [&](const auto& s) {
        return pos + 1 < s.second && pos + token.size() - 1 > s.first;
      })) continue;
      modelSpans.push_back({pos + 1, pos + token.size() - 1});
      add("model", alias.label);
      if (alias.vendor[0]) add("vendor", alias.vendor);
      else for (const auto& vendor : filename_hints::vendors)
        if (normalize(alias.label).find(normalize(vendor.token)) != std::string::npos) add("vendor", vendor.label);
    }
  }
  // Whole original names are fallback evidence, not substring matches against
  // generic settings such as "All 5" or "Lead" in an unrelated capture.
  if (capture.kind == FileKind::NAM && std::none_of(hints.begin(), hints.end(), [](const auto& h) { return h.key == "model"; }))
    for (const auto& alias : filename_hints::originalModels)
      if (words == normalize(alias.token))
      { add("model", alias.label); if (alias.vendor[0]) add("vendor", alias.vendor); break; }
  for (const auto& alias : filename_hints::creators)
  {
    const auto token = normalize(alias.token);
    // Short creator codes are suffixes, never arbitrary model/knob tokens.
    if ((std::string(alias.token).size() > 3 && contains(alias.token))
        || (capture.kind == FileKind::NAM && words.size() >= token.size()
            && words.compare(words.size() - token.size(), token.size(), token) == 0))
      add(alias.key, alias.label);
  }
  static const auto settings = [&] {
    std::vector<filename_hints::Alias> result(std::begin(filename_hints::settings), std::end(filename_hints::settings));
    std::stable_sort(result.begin(), result.end(), [&](const auto& a, const auto& b) {
      return normalize(a.token).size() > normalize(b.token).size();
    });
    return result;
  }();
  // Longest aliases win where phrases overlap within one category.
  std::map<std::string, std::vector<std::pair<size_t, size_t>>> used;
  for (const auto& alias : settings)
  {
    const std::string key = alias.key;
    if (capture.kind == FileKind::IR && key != "eq" && key != "mic/cab" && key != "speaker") continue;
    const auto token = normalize(alias.token);
    const auto& text = settingsWords;
    const auto pos = text.find(token);
    if (pos == std::string::npos) continue;
    auto& spans = used[key];
    if (std::any_of(spans.begin(), spans.end(), [&](const auto& s) { return pos + 1 < s.second && pos + token.size() - 1 > s.first; })) continue;
    spans.push_back({pos + 1, pos + token.size() - 1});
    add(key, alias.label);
  }
  // In the canonical grammar Gain/Function is its own field. A model's
  // "Top Boost" or a channel/boost setting must never introduce another gain.
  if (capture.kind == FileKind::NAM)
    for (const auto& alias : filename_hints::gains)
      if (structured ? normalize(fields[1]) == normalize(alias.token)
                     : contains(alias.token) && !(normalize(alias.token) == " boost " && (contains("Top Boost") || contains("TopBoost"))))
      { add("gain", alias.label); break; }
  static const std::regex channel(R"(\bCh(?:annel)?\s*([1-9][0-9]*|[A-D])\b)", std::regex::icase);
  for (auto it = std::sregex_iterator(setting.begin(), setting.end(), channel); it != std::sregex_iterator(); ++it)
  {
    auto number = (*it)[1].str();
    for (auto& c : number) c = static_cast<char>(std::toupper(c));
    add("channel", "Channel " + number);
  }
  // Expand compact model codes only in the slot/make/model/gain export grammar.
  static const std::regex compact(R"(^([A-Za-z]|[0-9]{2}) +([A-Za-z0-9]+) +([A-Za-z0-9]+) +(CL|ED|CR|DR|HG|BS)( +DI)?$)", std::regex::icase);
  std::smatch match;
  if (capture.kind == FileKind::NAM && std::regex_match(stem, match, compact))
  {
    for (const auto& alias : filename_hints::compactModels)
      if (normalize(match[3].str()) == normalize(alias.token)) add(alias.key, alias.label);
    const std::map<std::string, std::string> gains{{" cl ", "Clean"}, {" ed ", "Edge of breakup"}, {" cr ", "Crunch"},
      {" dr ", "Drive"}, {" hg ", "High gain"}, {" bs ", "Boost"}};
    add("gain", gains.at(normalize(match[4].str())));
    add("format", match[5].matched ? "Direct input (no cabinet)" : "Amp + Cab");
  }
  const auto claimed = [&](const std::string& token, bool includeSettings) {
    for (const auto& alias : filename_hints::settings)
      if ((includeSettings || std::string(alias.key) != "settings")
          && settingsWords.find(normalize(alias.token)) != std::string::npos
          && normalize(alias.token).find(normalize(token)) != std::string::npos) return true;
    return false;
  };
  static const std::regex knob(R"(\b(Gain|Drive|Tone|Bass|Mid|Middle|Treble|Presence|Master|Volume|Level|Blend|Resonance|Input|Output|GR|Ratio|Attack|Release)\s*=?\s*(-?[0-9]+(?:[.,][0-9]+)?)(pct|%|dB|ms|Hz)?(?![A-Za-z0-9]))", std::regex::icase);
  for (auto it = std::sregex_iterator(setting.begin(), setting.end(), knob); it != std::sregex_iterator(); ++it)
  {
    if (claimed(it->str(), false)) continue;
    auto label = Lower((*it)[1].str()); label[0] = static_cast<char>(std::toupper(label[0]));
    if (label == "Gr") label = "Gain reduction";
    auto unit = Lower((*it)[3].str());
    unit = unit == "pct" || unit == "%" ? "%" : unit == "db" ? " dB" : unit.empty() ? "" : " " + unit;
    add("settings", label + " " + (*it)[2].str() + unit);
  }
  static const std::regex compactKnob(R"(\b([GVMBCPTD])(-?[0-9]+(?:[.,][0-9]+)?)\b)", std::regex::icase);
  const std::map<char, std::string> knobNames{{'G', "Gain"}, {'V', "Volume"}, {'M', "Master"}, {'B', "Bass"},
    {'C', "Tone cut"}, {'P', "Presence"}, {'T', "Tone / treble"}, {'D', "Drive"}};
  for (auto it = std::sregex_iterator(setting.begin(), setting.end(), compactKnob); it != std::sregex_iterator(); ++it)
    if (!claimed(it->str(), true))
      add("settings", knobNames.at(static_cast<char>(std::toupper((*it)[1].str()[0]))) + " " + (*it)[2].str());
  if (capture.kind == FileKind::NAM)
  {
    const auto format = structured ? normalize(fields[fields.size() - 2]) : words;
    const auto hasFormat = [&](const std::string& token) { return structured ? format == normalize(token) : contains(token); };
    if (hasFormat("No Cab") || hasFormat("DI") || hasFormat("Direct") || hasFormat("Head")) add("format", "Direct input (no cabinet)");
    else if (hasFormat("Cab") || hasFormat("Full Rig")) add("format", "Amp + Cab");
  }
  // Expand all spellings for search, while displaying one readable value.
  for (auto& hint : hints)
  {
    const auto append = [&](const auto& table) {
      for (const auto& alias : table)
        if (hint.key == alias.key && hint.value == alias.label) hint.aliases += " " + std::string(alias.token);
    };
    append(filename_hints::vendors); append(filename_hints::creators); append(filename_hints::settings);
    append(filename_hints::gains); append(filename_hints::compactModels);
    if (hint.key == "model")
      for (const auto& alias : filename_hints::models)
        if (hint.value.find(alias.label) != std::string::npos) hint.aliases += " " + std::string(alias.token);
  }
  return hints;
}

std::vector<LibraryHint> InferLibraryHints(const Capture& capture)
{
  auto hints = InferFilenameHints(capture);
  if ((capture.kind != FileKind::NAM && capture.kind != FileKind::IR)
      || capture.source.empty() || !Within(capture.path, capture.source)) return hints;
  const auto filenameHints = hints;
  // Only the configured source and its descendants supply context. Never use
  // arbitrary ancestors, and let filename evidence win over broader folders.
  for (auto folder = capture.path.parent_path(); Within(folder, capture.source); folder = folder.parent_path())
  {
    Capture label; label.path = folder.filename(); label.kind = capture.kind;
    // Dots in a folder name are not filename extensions.
    label.path += capture.kind == FileKind::IR ? ".wav" : ".nam";
    auto candidates = InferFilenameHints(label);
    for (const auto& alias : filename_hints::creators)
      if (std::string(alias.token).size() > 3 && Lower(PathText(folder.filename())) == Lower(alias.token))
        candidates.push_back({alias.key, alias.label});
    if (capture.kind == FileKind::IR)
      for (const auto& name : {"OwnHammer", "Celestion", "York Audio"})
        if (Lower(PathText(folder.filename())) == Lower(name)) candidates.push_back({"creator", name});
    auto category = FolderCategory(folder.filename());
    if (capture.gear == Gear::Unknown || capture.inferred || category == capture.gear)
    {
      if (category != Gear::Unknown) candidates.push_back({"category", GearName(category)});
      if (Lower(PathText(folder.filename())) == "acoustic & line") candidates.push_back({"category", "Acoustic & Line"});
    }
    const auto folderToken = Token(PathText(folder.filename()));
    // Explicit folder roles also support names absent from the shipped alias
    // dictionary, e.g. Creators/My Studio or Packs/My Collection.
    const auto parentToken = Token(PathText(folder.parent_path().filename()));
    if (!SamePath(folder, capture.source))
    {
      const std::map<std::string, std::string> roles{{"creators", "creator"}, {"capturevendors", "creator"},
        {"vendors", "vendor"}, {"brands", "vendor"}, {"models", "model"}, {"packs", "collection"},
        {"collections", "collection"}};
      const auto role = roles.find(parentToken);
      if (role != roles.end())
      {
        if (role->second != "collection")
          candidates.erase(std::remove_if(candidates.begin(), candidates.end(), [&](const auto& h) {
            return h.key != role->second && !(role->second == "model" && h.key == "vendor");
          }), candidates.end());
        if (std::none_of(candidates.begin(), candidates.end(), [&](const auto& h) { return h.key == role->second; }))
          candidates.push_back({role->second, PathText(folder.filename())});
      }
    }
    if ((folderToken == "amp" || folderToken == "amps" || folderToken == "amplifiers")
        && (capture.gear == Gear::Unknown || capture.gear == Gear::AmpHead || capture.gear == Gear::AmpCab))
      candidates.push_back({"category", "Amp"});
    const auto nearerHints = hints;
    for (const auto& hint : candidates)
    {
      if (hint.key == "settings" || hint.key == "format") continue;
      if (std::any_of(filenameHints.begin(), filenameHints.end(), [&](const auto& f) { return f.key == hint.key; })) continue;
      // A nearer folder already established this category: do not broaden it.
      if (std::any_of(nearerHints.begin(), nearerHints.end(), [&](const auto& h) { return h.key == hint.key; })) continue;
      if (std::none_of(hints.begin(), hints.end(), [&](const auto& h) { return h.key == hint.key && h.value == hint.value; }))
        hints.push_back({hint.key, hint.value, true, hint.aliases});
    }
    if (SamePath(folder, capture.source)) break;
  }
  return hints;
}

std::string HintText(const Capture& capture, const std::string& key, int origin)
{
  std::string result;
  for (const auto& hint : capture.hints)
    if ((key.empty() || hint.key == key) && (origin < 0 || static_cast<int>(hint.fromFolder) == origin))
      result += (result.empty() ? "" : "\n") + hint.key + ": " + hint.value;
  return result;
}

std::string HintSearchText(const Capture& capture)
{
  std::string result;
  for (const auto& hint : capture.hints) result += " " + hint.key + ": " + hint.value + " " + hint.aliases;
  return result;
}

Capture Inspect(const std::filesystem::path& file, const std::filesystem::path& root)
{
  Capture result;
  result.path = file;
  result.source = root;
  result.relative = file.lexically_relative(root);
  if (Lower(PathText(file.extension())) == ".wav")
  {
    result.isIR = true;
    result.kind = FileKind::IR;
    result.gear = Gear::Cabinet;
    result.hints = InferLibraryHints(result);
    return result; // Validate WAV contents only when loading; library scanning never decodes audio.
  }
  std::ifstream stream(file, std::ios::binary);
  if (!stream)
    throw std::runtime_error("Cannot open capture");
  MetadataReader reader;
  nlohmann::json::sax_parse(stream, &reader);
  if (reader.complete)
  {
    result.name = reader.values["name"];
    result.maker = reader.values["gear_make"];
    result.model = reader.values["gear_model"];
    result.gear = Category(reader.values["gear_type"]);
  }
  if (result.gear == Gear::Unknown)
  {
    // Whole-word markers work before the extension and across spaces, hyphens or underscores.
    const auto words = FilenameWords(PathText(file.stem()));
    const bool noCab = words.find(" no cab ") != std::string::npos;
    const bool direct = noCab || words.find(" di ") != std::string::npos || words.find(" direct ") != std::string::npos || (Lower(PathText(file.stem())).find(" - head - ") != std::string::npos || (words.size() >= 6 && words.compare(words.size() - 6, 6, " head ") == 0));
    const bool cabinet =
      (!noCab && words.find(" cab ") != std::string::npos) || words.find(" full rig ") != std::string::npos;
    bool ampContext = reader.complete && Token(reader.values["gear_type"]) == "amp";
    auto inheritFolder = [&](const std::filesystem::path& folder) {
      const auto category = FolderCategory(folder);
      if (category != Gear::Unknown)
        result.gear = category;
      const auto token = Token(PathText(folder));
      ampContext |= token == "amp" || token == "amps" || token == "amplifiers";
    };
    auto rootFolder = root.lexically_normal();
    if (rootFolder.filename().empty())
      rootFolder = rootFolder.parent_path();
    inheritFolder(rootFolder.filename());
    for (const auto& directory : result.relative.parent_path())
      inheritFolder(directory);
    ampContext |= result.gear == Gear::AmpCab || result.gear == Gear::AmpHead;
    const bool unresolvedOrAmp =
      result.gear == Gear::Unknown || result.gear == Gear::AmpCab || result.gear == Gear::AmpHead;

    if (cabinet && direct)
      result.gear = Gear::Unknown;
    else if (cabinet && unresolvedOrAmp)
      result.gear = Gear::AmpCab;
    else if (direct && ampContext && unresolvedOrAmp)
      result.gear = Gear::AmpHead;
    result.inferred = result.gear != Gear::Unknown;
  }
  result.hints = InferLibraryHints(result);
  return result;
}

bool Filter::Matches(const Capture& capture) const
{
  if (gear >= 0 && static_cast<int>(capture.gear) != gear)
    return false;
  const auto haystack = Lower(PathText(capture.relative) + " " + capture.name + " " + capture.maker + " "
                              + capture.model + " " + GearName(capture.gear) + " " + HintSearchText(capture) + (capture.isIR ? " IR WAV" : " NAM"));
  std::istringstream words(Lower(search));
  for (std::string word; words >> word;)
    if (haystack.find(word) == std::string::npos)
      return false;
  return true;
}

std::string ContentHash(const std::filesystem::path& file, const std::function<bool()>& cancelled)
{
  std::ifstream input(file, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot read capture/IR for content hash");
  WDL_SHA1 hash;
  std::array<char, 65536> buffer;
  while (input)
  {
    if (cancelled && cancelled()) return {};
    input.read(buffer.data(), buffer.size());
    hash.add(buffer.data(), static_cast<int>(input.gcount()));
  }
  if (!input.eof()) throw std::runtime_error("Cannot complete capture/IR content hash");
  std::array<unsigned char, WDL_SHA1SIZE> bytes{}; hash.result(bytes.data());
  static constexpr char hex[] = "0123456789abcdef";
  std::string result; result.reserve(2 * bytes.size());
  for (auto byte : bytes) { result += hex[byte >> 4]; result += hex[byte & 15]; }
  return result;
}
const Capture* FindHash(const Snapshot& library, const std::string& hash, bool isIR)
{
  if (hash.empty()) return nullptr;
  const Capture* fallback = nullptr;
  for (const auto& capture : library.captures)
    if (capture.hash == hash && capture.kind == (isIR ? FileKind::IR : FileKind::NAM))
    {
      std::error_code error;
      // Identical copies can be indexed side by side; relink to one that still exists.
      if (std::filesystem::is_regular_file(capture.path, error)) return &capture;
      if (!fallback) fallback = &capture;
    }
  return fallback;
}
std::vector<std::filesystem::path> SiblingFiles(const std::filesystem::path& selected, bool isIR,
                                                const std::vector<std::string>& roots)
{
  std::vector<std::filesystem::path> result;
  std::vector<IgnoreRules> rules;
  for (const auto& root : roots)
    rules.emplace_back(std::filesystem::u8path(root));
  std::error_code error;
  for (std::filesystem::directory_iterator it(selected.parent_path(), error), end; !error && it != end; it.increment(error))
    if (it->is_regular_file(error) && Lower(PathText(it->path().extension())) == (isIR ? ".wav" : ".nam")
        && std::none_of(rules.begin(), rules.end(), [&](auto& rule) { return rule.Ignored(it->path(), false); }))
      result.push_back(it->path());
  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    const auto first = Lower(PathText(a.filename())), second = Lower(PathText(b.filename()));
    return first == second ? PathText(a) < PathText(b) : first < second;
  });
  return result;
}
std::string StarKey(const Capture& file, const std::filesystem::path& favoritesFolder)
{
  switch (file.kind)
  {
    case FileKind::Module: return "Module:" + std::to_string(file.module);
    // Same bytes, same star: the content hash rigs already use to relink a moved capture.
    case FileKind::NAM: return file.hash.empty() ? std::string{} : "NAM:" + file.hash;
    case FileKind::IR: return file.hash.empty() ? std::string{} : "IR:" + file.hash;
    case FileKind::RigPreset:
    {
      // The scan records each preset relative to its source folder, in the file system's spelling.
      // Forward slashes keep a relative key the same on every platform.
      const bool local = !favoritesFolder.empty() && !file.relative.empty() && SamePath(file.source, favoritesFolder);
      const auto& path = local ? file.relative : file.path;
      return "RigPreset:" + PathText(std::filesystem::path(path.lexically_normal().generic_u8string()));
    }
    default: return {};
  }
}

int NextStar(int level)
{
  return level < 0 || level >= kStarLevels ? 0 : level + 1;
}

std::filesystem::path FavoritesFolder(const std::vector<std::string>& presetFolders)
{
  return presetFolders.empty() ? std::filesystem::path{} : std::filesystem::u8path(presetFolders.front());
}

std::filesystem::path FavoritesFile(const std::vector<std::string>& presetFolders,
                                    const std::filesystem::path& configFolder)
{
  const auto folder = presetFolders.empty() ? configFolder : FavoritesFolder(presetFolders);
  return folder.empty() ? folder : folder / "favorites.json";
}

std::optional<Stars> ReadFavorites(const std::filesystem::path& file)
{
  Stars result;
  try
  {
    std::error_code error;
    if (file.empty() || !std::filesystem::exists(file, error)) return error ? std::nullopt : std::optional(result);
    std::ifstream input(file, std::ios::binary);
    if (!input) return std::nullopt;
    const auto config = nlohmann::json::parse(input);
    if (!config.is_object()) return std::nullopt;
    const auto stars = config.find("Stars");
    if (stars == config.end()) return result;
    if (!stars->is_object()) return std::nullopt;
    for (auto entry = stars->begin(); entry != stars->end() && result.size() < 8192; ++entry)
    {
      // A damaged or hand-edited entry costs only that star, never the rest of the file.
      const auto& value = entry.value();
      if (entry.key().empty() || entry.key().size() > 2048 || !value.is_object()) continue;
      const auto level = value.find("Level");
      if (level == value.end() || !level->is_number_integer()) continue;
      Star star{level->get<int>()};
      if (star.level < 1 || star.level > kStarLevels) continue;
      if (const auto name = value.find("Name"); name != value.end() && name->is_string() && name->size() <= 512)
        star.name = name->get<std::string>();
      result[entry.key()] = std::move(star);
    }
  }
  catch (const std::exception&)
  {
    return std::nullopt;
  }
  return result;
}

bool WriteFavorites(const std::filesystem::path& file, const Stars& stars)
{
  try
  {
    if (file.empty()) return false;
    std::filesystem::create_directories(file.parent_path());
    auto entries = nlohmann::json::object();
    for (const auto& [key, star] : stars)
    {
      auto entry = nlohmann::json{{"Level", star.level}};
      if (!star.name.empty()) entry["Name"] = star.name;
      entries[key] = std::move(entry);
    }
    rig_preset::WriteAtomically(file, nlohmann::json{{"Format", "nam-grid-favorites"}, {"Version", 1}, {"Stars", entries}}.dump(2));
    return true;
  }
  catch (const std::exception&)
  {
    return false;
  }
}

// Content identity exists so a renamed or moved capture can be found again after a rescan.
// It must never shrink the menu: keep every indexed path and only count repeats, so a pack
// folder stays complete even when a curated copy of the same bytes lives elsewhere.
static void CountContentDuplicates(Snapshot& snapshot)
{
  std::set<std::pair<FileKind, std::string>> identities;
  size_t duplicates = 0;
  for (const auto& capture : snapshot.captures)
    if (!capture.hash.empty() && !identities.emplace(capture.kind, capture.hash).second) ++duplicates;
  snapshot.duplicates = duplicates;
  std::stable_sort(snapshot.captures.begin(), snapshot.captures.end(), [](const auto& a, const auto& b) {
    return Lower(PathText(a.relative)) < Lower(PathText(b.relative));
  });
}
// Bit mask of the file kinds a source list indexes.
static constexpr unsigned Bit(FileKind kind) { return 1u << static_cast<unsigned>(kind); }
static constexpr unsigned kCaptureKinds = Bit(FileKind::NAM) | Bit(FileKind::IR);
static FileKind FileKindOf(const std::string& extension)
{ return extension == ".wav" ? FileKind::IR : extension == ".ngrid" ? FileKind::RigPreset : FileKind::NAM; }
static bool Indexes(unsigned kinds, const std::string& extension)
{
  return (kinds & Bit(FileKind::NAM) && extension == ".nam") || (kinds & Bit(FileKind::IR) && extension == ".wav")
         || (kinds & Bit(FileKind::RigPreset) && extension == ".ngrid");
}
static std::string IndexKey(const std::filesystem::path& path)
{
  auto key = PathText(path.lexically_normal());
#ifdef _WIN32
  key = Lower(key);
#endif
  return key;
}
IndexEntries ReadIndex(const std::filesystem::path& file)
{
  IndexEntries entries;
  try
  {
    std::error_code error;
    if (file.empty() || !std::filesystem::exists(file, error) || std::filesystem::file_size(file, error) > 256u * 1024 * 1024)
      return entries;
    std::ifstream stream(file, std::ios::binary);
    const auto index = nlohmann::json::parse(stream);
    if (index.value("Version", 0) != kIndexVersion)
      return entries;
    for (const auto& item : index.at("Files"))
    {
      IndexEntry entry;
      entry.size = item.at("Size").get<std::uintmax_t>();
      entry.modified = item.at("Modified").get<long long>();
      auto& capture = entry.capture;
      capture.path = std::filesystem::u8path(item.at("Path").get<std::string>());
      const int kind = item.at("Kind").get<int>(), gear = item.at("Gear").get<int>();
      if (kind < static_cast<int>(FileKind::NAM) || kind > static_cast<int>(FileKind::RigPreset)
          || gear < static_cast<int>(Gear::Unknown) || gear > static_cast<int>(Gear::Spaces))
        continue;
      capture.kind = static_cast<FileKind>(kind);
      capture.isIR = capture.kind == FileKind::IR;
      capture.gear = static_cast<Gear>(gear);
      capture.inferred = item.value("Inferred", false);
      capture.name = item.value("Name", std::string{});
      capture.maker = item.value("Maker", std::string{});
      capture.model = item.value("Model", std::string{});
      capture.hash = item.value("Hash", std::string{});
      capture.hints = InferFilenameHints(capture);
      entries[IndexKey(capture.path)] = std::move(entry);
    }
  }
  catch (const std::exception&)
  {
    entries.clear(); // A damaged index is ignored; the next scan rewrites it.
  }
  return entries;
}
bool WriteIndex(const std::filesystem::path& file, const IndexEntries& entries)
{
  try
  {
    auto files = nlohmann::json::array();
    for (const auto& [key, entry] : entries)
    {
      const auto& capture = entry.capture;
      files.push_back({{"Path", PathText(capture.path)}, {"Size", entry.size}, {"Modified", entry.modified},
                       {"Kind", static_cast<int>(capture.kind)}, {"Gear", static_cast<int>(capture.gear)},
                       {"Inferred", capture.inferred}, {"Name", capture.name}, {"Maker", capture.maker},
                       {"Model", capture.model}, {"Hash", capture.hash}});
    }
    std::filesystem::create_directories(file.parent_path());
    rig_preset::WriteAtomically(file, nlohmann::json{{"Version", kIndexVersion}, {"Files", std::move(files)}}.dump());
    return true;
  }
  catch (const std::exception&)
  {
    return false;
  }
}
static Snapshot ScanSource(const std::filesystem::path& root, unsigned kinds, const std::function<bool()>& cancelled,
                           IndexCache* index = nullptr)
{
  Snapshot result;
  result.root = root;
  std::error_code error;
  if (root.empty())
    return result;
  if (!std::filesystem::is_directory(root, error))
  {
    result.error = "Library folder is missing or inaccessible";
    return result;
  }
  IgnoreRules ignore(root);
  using Iterator = std::filesystem::recursive_directory_iterator;
  // Surface denied directories as an incomplete scan rather than silently saving a
  // reduced index. The worker preserves the last good cache whenever error is set.
  Iterator it(root, std::filesystem::directory_options::none, error), end;
  for (; it != end && !error; it.increment(error))
  {
    if (cancelled && cancelled())
      return result;
    // Never follow directory links/junctions out of the selected tree.
    std::error_code statusError;
    if (it->is_symlink(statusError))
    {
      it.disable_recursion_pending();
      continue;
    }
    const bool directory = it->is_directory(statusError);
    if (ignore.Matches(it->path(), directory))
    {
      if (directory)
        it.disable_recursion_pending();
      continue;
    }
    const auto extension = Lower(PathText(it->path().extension()));
    if (!it->is_regular_file(statusError) || !Indexes(kinds, extension))
      continue;
    try
    {
      Capture capture;
      // An unchanged file (same size and modification time) reuses what the index knows.
      std::error_code statError;
      const auto size = it->file_size(statError);
      const long long modified = statError ? 0 : it->last_write_time(statError).time_since_epoch().count();
      const bool indexed = index && !statError;
      const auto key = indexed ? IndexKey(it->path()) : std::string{};
      const auto known = indexed ? index->previous.find(key) : IndexEntries::const_iterator{};
      if (indexed && known != index->previous.end() && known->second.size == size && known->second.modified == modified
          && known->second.capture.kind == FileKindOf(extension))
        capture = known->second.capture;
      else if (extension == ".ngrid")
      {
        // Validate the bounded file envelope on the worker; never instantiate DSP here.
        const auto rig = rig_preset::Read(it->path());
        capture.name = rig.value("PresetName", std::string{});
        if (capture.name.size() > 512 || capture.name.find('\0') != std::string::npos) capture.name.clear();
        capture.kind = FileKind::RigPreset;
      }
      else
      {
        capture = Inspect(it->path(), root);
        capture.hash = ContentHash(capture.path, cancelled);
        if (capture.hash.empty()) return result;
      }
      capture.path = it->path(); capture.source = root;
      capture.relative = it->path().lexically_relative(root);
      capture.hints = InferLibraryHints(capture);
      if (indexed) index->next[key] = {size, modified, capture};
      const auto parent = capture.relative.parent_path();
      capture.folderDepth = static_cast<size_t>(std::distance(parent.begin(), parent.end()));
      result.captures.push_back(std::move(capture));
    }
    catch (const std::exception&)
    {
      ++result.unreadable;
    }
  }
  if (error)
    result.error = "Some capture folders could not be read";
  std::sort(result.captures.begin(), result.captures.end(), [](const Capture& a, const Capture& b) {
    const auto first = PathText(a.relative), second = PathText(b.relative);
    const auto lowFirst = Lower(first), lowSecond = Lower(second);
    return lowFirst == lowSecond ? first < second : lowFirst < lowSecond;
  });
  CountContentDuplicates(result);
  return result;
}

Snapshot Scan(const std::filesystem::path& root, const std::function<bool()>& cancelled)
{ return ScanSource(root, kCaptureKinds, cancelled); }

// Restoring an index does no directory walks, file reads or hashing. Root-relative fields
// are rebuilt because the same cache can be used by instances with different folder lists.
static Snapshot CachedSource(const std::filesystem::path& root, unsigned kinds, const IndexEntries& entries)
{
  Snapshot result;
  result.root = root;
  for (const auto& [key, entry] : entries)
  {
    const auto& file = entry.capture;
    if (!(kinds & Bit(file.kind)) || !Within(file.path, root)) continue;
    auto capture = file;
    capture.source = root;
    capture.relative = file.path.lexically_relative(root);
    capture.hints = InferLibraryHints(capture);
    const auto parent = capture.relative.parent_path();
    capture.folderDepth = static_cast<size_t>(std::distance(parent.begin(), parent.end()));
    result.captures.push_back(std::move(capture));
  }
  std::sort(result.captures.begin(), result.captures.end(), [](const Capture& a, const Capture& b) {
    const auto first = PathText(a.relative), second = PathText(b.relative);
    const auto lowFirst = Lower(first), lowSecond = Lower(second);
    return lowFirst == lowSecond ? first < second : lowFirst < lowSecond;
  });
  CountContentDuplicates(result);
  return result;
}

static Snapshot ScanRoots(const std::vector<std::string>& roots, unsigned kinds, const std::function<bool()>& cancelled,
                          IndexCache* index = nullptr, bool cachedOnly = false)
{
  Snapshot result;
  std::unordered_map<std::string, size_t> seen;
  const auto folders = NormalizeFolders(roots);
  std::unordered_set<std::string> labels;
  for (const auto& folder : folders)
  {
    if (cancelled && cancelled())
      return result;
    auto part = cachedOnly ? CachedSource(std::filesystem::u8path(folder), kinds, index->previous)
                           : ScanSource(std::filesystem::u8path(folder), kinds, cancelled, index);
    if (folders.size() == 1)
      result.root = part.root;
    result.unreadable += part.unreadable;
    if (!part.error.empty())
    {
      if (!result.error.empty())
        result.error += "; ";
      result.error += folder + ": " + part.error;
    }
    auto label = PathText(std::filesystem::u8path(folder).filename());
    if (label.empty())
      label = folder;
    const auto baseLabel = label;
    int occurrence = 1;
    while (!labels.insert(Lower(label)).second)
      label = baseLabel + " (" + std::to_string(++occurrence) + ")";
    for (auto& capture : part.captures)
    {
      auto key = PathText(capture.path.lexically_normal());
#ifdef _WIN32
      key = Lower(key);
#endif
      if (folders.size() > 1)
        capture.relative = std::filesystem::u8path(label) / capture.relative;
      const auto [found, inserted] = seen.emplace(key, result.captures.size());
      if (!inserted)
      {
        if (capture.folderDepth < result.captures[found->second].folderDepth)
          result.captures[found->second] = std::move(capture);
        continue;
      }
      result.captures.push_back(std::move(capture));
    }
  }
  CountContentDuplicates(result);
  return result;
}

Snapshot ScanFolders(const std::vector<std::string>& roots, const std::function<bool()>& cancelled)
{ return ScanRoots(roots, kCaptureKinds, cancelled); }

static void Append(Snapshot& result, Snapshot part)
{
  result.captures.insert(result.captures.end(), std::make_move_iterator(part.captures.begin()),
                         std::make_move_iterator(part.captures.end()));
  result.unreadable += part.unreadable;
  result.duplicates += part.duplicates;
  if (!part.error.empty())
    result.error += (result.error.empty() ? "" : "; ") + part.error;
}

Snapshot ScanLibrary(const std::vector<std::string>& namRoots, const std::vector<std::string>& irRoots,
                     const std::vector<std::string>& presetRoots, const std::function<bool()>& cancelled,
                     IndexCache* index)
{
  // Identical NAM and IR lists share one directory walk.
  auto result = ScanRoots(namRoots, namRoots == irRoots ? kCaptureKinds : Bit(FileKind::NAM), cancelled, index);
  if (namRoots != irRoots)
    Append(result, ScanRoots(irRoots, Bit(FileKind::IR), cancelled, index));
  Append(result, ScanRoots(presetRoots, Bit(FileKind::RigPreset), cancelled, index));
  return result;
}

const char* KindName(FileKind kind)
{
  switch (kind)
  {
    case FileKind::NAM: return "NAM";
    case FileKind::IR: return "IR";
    case FileKind::RigPreset: return "Preset";
    case FileKind::SlotPreset: return "FX preset";
    case FileKind::Module: return "Module";
  }
  return "";
}

Worker::Worker(std::filesystem::path indexFile, Scanner scanner)
: mIndexFile(std::move(indexFile))
, mScanner(std::move(scanner))
, mThread([this] { Run(); })
{
}
Worker::~Worker()
{
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mStop.store(true);
  }
  mChanged.notify_one();
  mThread.join();
}
void Worker::Request(const std::string& root)
{ Request(root.empty() ? std::vector<std::string>{} : std::vector<std::string>{root}); }
void Worker::Request(const std::vector<std::string>& roots)
{ Request(roots, roots, {}); }
void Worker::Request(const std::vector<std::string>& namRoots, const std::vector<std::string>& irRoots,
                     const std::vector<std::string>& presetRoots, bool rescan,
                     const std::vector<std::string>& factoryFiles)
{
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mCancelRequested = false;
    mNamRoots = namRoots;
    mIrRoots = irRoots;
    mPresetRoots = presetRoots;
    mRescan = rescan;
    mFactoryFiles = factoryFiles;
    mResult.reset();
    ++mRevision;
  }
  mChanged.notify_one();
}
void Worker::Cancel()
{
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mCancelRequested = true;
    mResult.reset();
    ++mRevision;
  }
  mChanged.notify_one();
}
std::shared_ptr<const Snapshot> Worker::TakeResult()
{
  std::lock_guard<std::mutex> lock(mMutex);
  return std::exchange(mResult, {});
}
void Worker::Run()
{
  unsigned done = 0;
  while (!mStop.load())
  {
    std::unique_lock<std::mutex> lock(mMutex);
    mChanged.wait(lock, [&] { return mStop.load() || mRevision.load() != done; });
    if (mStop.load())
      return;
    if (mCancelRequested)
    {
      mCancelRequested = false;
      done = mRevision.load();
      continue;
    }
    const auto revision = mRevision.load();
    const auto namRoots = mNamRoots, irRoots = mIrRoots, presetRoots = mPresetRoots;
    const bool rescan = mRescan;
    const auto factoryFiles = mFactoryFiles;
    lock.unlock();
    auto cancelled = [&] { return mStop.load() || mRevision.load() != revision; };
    auto result = std::make_shared<Snapshot>();
    const bool indexed = !mIndexFile.empty();
    if (indexed && !mIndexLoaded)
    {
      mIndex.previous = ReadIndex(mIndexFile);
      mIndexLoaded = true;
    }
    mIndex.next.clear();
    bool scanned = true;
    try
    {
      if (indexed && !rescan)
      {
        *result = ScanRoots(namRoots, namRoots == irRoots ? kCaptureKinds : Bit(FileKind::NAM), cancelled, &mIndex, true);
        if (namRoots != irRoots)
          Append(*result, ScanRoots(irRoots, Bit(FileKind::IR), cancelled, &mIndex, true));
        Append(*result, ScanRoots(presetRoots, Bit(FileKind::RigPreset), cancelled, &mIndex, true));
        std::unordered_set<std::string> factory;
        for (const auto& file : factoryFiles) factory.insert(IndexKey(std::filesystem::u8path(file)));
        scanned = !std::any_of(result->captures.begin(), result->captures.end(), [&](const auto& file) {
          return !factory.contains(IndexKey(file.path));
        });
      }
      if (scanned)
        *result = mScanner(namRoots, irRoots, presetRoots, cancelled, indexed ? &mIndex : nullptr);
    }
    catch (const std::exception&)
    {
      result->error = "Capture folder scan failed";
    }
    // Serialize publication with Request/Cancel: an obsolete scan must neither
    // overwrite the index nor consume a cancellation intended for the next revision.
    lock.lock();
    // Only a complete, successful scan replaces the index. An offline drive or a read
    // failure must not erase the last good cache; still publish the scan's error to the UI.
    if (indexed && scanned && !cancelled() && result->error.empty() && result->unreadable == 0)
    {
      mIndex.previous = std::move(mIndex.next);
      WriteIndex(mIndexFile, mIndex.previous);
    }
    mIndex.next.clear();
    if (!cancelled())
      mResult = std::move(result);
    done = revision;
  }
}
} // namespace nam_library
