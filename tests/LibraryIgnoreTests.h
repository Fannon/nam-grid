#pragma once
#include "../NeuralAmpModeler/CaptureLibrary.h"
#include <set>

inline void TestLibraryIgnoreFiles(const std::filesystem::path& fixtureRoot)
{
  using namespace nam_library;
  const auto root = fixtureRoot / "ignore-fixtures";
  std::filesystem::remove_all(root);
  const auto write = [&](const std::string& relative, const std::string& text = "{}") {
    const auto path = root / std::filesystem::u8path(relative);
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
  };
  write(".ignore", "# Library exclusions\n"
                   "Catalog/\n"
                   "*DI.nam\n"
                   "!Keep DI.nam\n"
                   "/top-only.nam\n"
                   "Packs/**/old/\n"
                   "[Tt]emp*\n"
                   "\\#hash.nam\n"
                   "trailing.nam   \n");
  write("Amps/.ignore", "!Fender DI.nam\n");   // A deeper file re-includes one name.
  write("Catalog/.ignore", "!a.nam\n");        // Cannot re-include inside an excluded folder.
  write("Pedals/.ignore.txt", "\xEF\xBB\xBF" "fuzz.nam\r\n# keep the rest\r\n");
  for (const char* file : {"Catalog/a.nam", "Catalog/sub/b.nam", "Amps/clean.nam", "Amps/Marshall DI.nam",
                           "Amps/Keep DI.nam", "Amps/Fender DI.nam", "top-only.nam", "Amps/top-only.nam",
                           "Packs/x/old/c.nam", "Packs/old/d.nam", "Packs/new/e.nam", "Amps/temp1.nam",
                           "Amps/TEMP2.NAM", "#hash.nam", "trailing.nam", "Pedals/fuzz.nam", "Pedals/drive.nam",
                           "Cabs/cab.wav", "Cabs/old/cab.wav"})
    write(file);
  const std::set<std::string> expected{"Amps/clean.nam", "Amps/Keep DI.nam", "Amps/Fender DI.nam", "Amps/top-only.nam",
                                       "Packs/new/e.nam", "Pedals/drive.nam", "Cabs/cab.wav", "Cabs/old/cab.wav"};
  const auto scanned = ScanFolders({PathText(root)});
  std::set<std::string> found;
  for (const auto& capture : scanned.captures)
  {
    auto relative = PathText(capture.path.lexically_relative(root));
    std::replace(relative.begin(), relative.end(), '\\', '/');
    found.insert(relative);
  }
  for (const auto& name : found)
    if (!expected.count(name)) std::cerr << "Not ignored: " << name << '\n';
  for (const auto& name : expected)
    if (!found.count(name)) std::cerr << "Wrongly ignored: " << name << '\n';
  assert(found == expected && scanned.unreadable == 0);

  // Direct checks, including folders and a path outside the library.
  IgnoreRules rules(root);
  assert(rules.Ignored(root / "Catalog", true) && !rules.Ignored(root / "Catalog", false));
  assert(rules.Ignored(root / "Catalog" / "sub" / "b.nam", false));
  assert(!rules.Ignored(root / "Packs" / "old.nam", false)); // `old/` only matches folders.
  assert(!rules.Ignored(fixtureRoot / "elsewhere.nam", false));
#ifdef _WIN32
  IgnoreRules differentCase(root.parent_path() / "IGNORE-FIXTURES"); // Saved folder spelled differently.
  assert(differentCase.Ignored(root / "Amps" / "temp1.nam", false) && !differentCase.Ignored(root / "Amps" / "clean.nam", false));
#endif

  // The previous/next arrows skip what the library folder excludes.
  const auto siblings = SiblingFiles(root / "Amps" / "clean.nam", false, {PathText(root)});
  std::set<std::string> names;
  for (const auto& file : siblings) names.insert(PathText(file.filename()));
  assert((names == std::set<std::string>{"clean.nam", "Keep DI.nam", "Fender DI.nam", "top-only.nam"}));
  assert(SiblingFiles(root / "Amps" / "clean.nam", false).size() == 7); // No library folder: no rules.
  std::filesystem::remove_all(root);
  std::cout << ".ignore files: folders, negation, anchoring, wildcards, nesting, .ignore.txt and siblings passed\n";
}
