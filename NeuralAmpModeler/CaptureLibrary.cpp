#include "CaptureLibrary.h"
#include "RigPreset.h"
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
    const bool direct = noCab || words.find(" di ") != std::string::npos || words.find(" direct ") != std::string::npos;
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
  return result;
}

bool Filter::Matches(const Capture& capture) const
{
  if (gear >= 0 && static_cast<int>(capture.gear) != gear)
    return false;
  const auto haystack = Lower(PathText(capture.relative) + " " + capture.name + " " + capture.maker + " "
                              + capture.model + " " + GearName(capture.gear) + (capture.isIR ? " IR WAV" : " NAM"));
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
  Iterator it(root, std::filesystem::directory_options::skip_permission_denied, error), end;
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

static Snapshot ScanRoots(const std::vector<std::string>& roots, unsigned kinds, const std::function<bool()>& cancelled,
                          IndexCache* index = nullptr)
{
  Snapshot result;
  std::unordered_map<std::string, size_t> seen;
  const auto folders = NormalizeFolders(roots);
  std::unordered_set<std::string> labels;
  for (const auto& folder : folders)
  {
    if (cancelled && cancelled())
      return result;
    auto part = ScanSource(std::filesystem::u8path(folder), kinds, cancelled, index);
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
                     const std::vector<std::string>& presetRoots)
{
  {
    std::lock_guard<std::mutex> lock(mMutex);
    mCancelRequested = false;
    mNamRoots = namRoots;
    mIrRoots = irRoots;
    mPresetRoots = presetRoots;
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
    try
    {
      *result = mScanner(namRoots, irRoots, presetRoots, cancelled, indexed ? &mIndex : nullptr);
    }
    catch (const std::exception&)
    {
      result->error = "Capture folder scan failed";
    }
    // Serialize publication with Request/Cancel: an obsolete scan must neither
    // overwrite the index nor consume a cancellation intended for the next revision.
    lock.lock();
    // A scan that ran to the end replaces the index, so it lists exactly the files it found.
    // Missing folders simply contribute nothing; their files are read again when they return.
    if (indexed && !cancelled())
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
