// Headless metadata and visual-state capture. No native editor or user library is opened.
#include "../plugins/amp/Amp.cpp"
#include "../plugins/pedals/Pedals.cpp"
#include "../plugins/delay/Delay.cpp"
#include "../plugins/cosmic-echo/CosmicEcho.cpp"
#include "../plugins/lofi/LoFi.cpp"
#include "../plugins/mod/Mod.cpp"
#include "../plugins/console/Console.cpp"
#include "Contract.h"
#include <iostream>

using namespace mpc_grid;

VstIntPtr InspectHost(AEffect*, VstInt32 opcode, VstInt32, VstIntPtr, void*, float)
{ return opcode == audioMasterVersion ? 2400 : 0; }

void InspectWait(GridPlugin& plugin)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
  while (std::chrono::steady_clock::now() < deadline)
  {
    bool busy = false;
    for (int i = 0; i < plugin.numParams(); ++i)
    {
      const auto& id = plugin.param(i).id;
      if (id == "rescan" && plugin.getBool(i)) busy = true;
      if (id == "amp_info1" || id == "cab_info1")
      {
        const auto status = plugin.displayText(i);
        busy = busy || status == "Loading..." || status == "Selecting..." || status == "Waiting for library";
      }
    }
    if (!busy) return;
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
  }
  throw std::runtime_error("Gallery state did not settle");
}

nlohmann::json InspectSnapshot(GridPlugin& plugin, const std::string& label, const std::string& section = "")
{
  InspectWait(plugin);
  nlohmann::json snapshot{{"label", label}, {"section", section}};
  for (int i = 0; i < plugin.numParams(); ++i)
    snapshot["params"][plugin.param(i).id] = {{"staticName", plugin.param(i).name}, {"name", plugin.ParameterName(i)},
                                             {"display", plugin.displayText(i)}, {"norm", plugin.getNormalised(i)}};
  return snapshot;
}

void InspectSet(GridPlugin& plugin, const std::string& id, float normalized)
{
  for (int i = 0; i < plugin.numParams(); ++i)
    if (plugin.param(i).id == id) { plugin.setNormalised(i, normalized); return; }
}

void InspectWrite(const std::string& path, const std::string& content)
{
  std::filesystem::create_directories(std::filesystem::u8path(FolderOf(path)));
  std::ofstream file(std::filesystem::u8path(path), std::ios::binary);
  file << content;
  if (!file) throw std::runtime_error("Cannot write gallery fixture");
}

void InspectLibrary(GridPlugin& plugin, nlohmann::json& states)
{
  const char* root = std::getenv("NAM_GRID_MPC_LIBRARY");
  if (!root || !*root) throw std::runtime_error("Gallery requires an isolated library");
  std::string capture;
  if (!ReadWholeFile(mpc::pluginFolder() + "/Content/Captures/riffsmith-clean.nam", capture))
    throw std::runtime_error("Gallery factory capture missing");
  const std::string name = "A long capture name - clean guitar with bright pickups and a carefully levelled cabinet.nam";
  InspectWrite(std::string(root) + "/Captures/Examples/" + name, capture);
  auto metadataFree = nlohmann::json::parse(capture);
  metadataFree["metadata"].erase("loudness");
  InspectWrite(std::string(root) + "/Captures/Examples/no-loudness.nam", metadataFree.dump());
  InspectWrite(std::string(root) + "/Captures/Examples/broken.nam", "{ not JSON");
  InspectSet(plugin, "rescan", 1.f);
  InspectWait(plugin);
  const auto load = [&](const std::string& path, const std::string& label) {
    std::vector<uint8_t> bytes;
    plugin.saveExtraState(bytes);
    auto state = nlohmann::json::parse(bytes);
    state["sections"]["amp"]["file"] = {{"root", "user"}, {"path", path}};
    const auto text = state.dump();
    plugin.BeginStateLoad();
    plugin.loadExtraState(reinterpret_cast<const uint8_t*>(text.data()), text.size());
    plugin.EndStateLoad(true);
    states.push_back(InspectSnapshot(plugin, label, "library"));
  };
  load("Examples/" + name, "long-name");
  load("Examples/no-loudness.nam", "no-metadata");
  load("Examples/broken.nam", "load-error");
  load("Removed/missing.nam", "missing-drive");
}

int main(int argc, char** argv)
{
  const bool gallery = argc == 3 && std::string(argv[1]) == "--gallery";
  if (!gallery && (argc != 2 || std::string(argv[1]) != "--describe")) return 2;
  nlohmann::json output;
  for (const auto* spec : {&mpc_plugins::AmpSpec(), &mpc_plugins::PedalsSpec(), &mpc_plugins::DelaySpec(),
                           &mpc_plugins::CosmicEchoSpec(), &mpc_plugins::LoFiSpec(), &mpc_plugins::ModSpec(),
                           &mpc_plugins::ConsoleSpec()})
  {
    auto* e = mpc_grid::CreateEffect(*spec, &InspectHost);
    auto& plugin = *static_cast<mpc_grid::GridPlugin*>(mpc::detail::self(e));
    if (!gallery)
      output[spec->stateFormat] = {{"contract", DescribeContract(plugin)}, {"state", SavedParameters(plugin)}};
    else
    {
      e->dispatcher(e, effOpen, 0, 0, nullptr, 0.f);
      e->dispatcher(e, effSetSampleRate, 0, 0, nullptr, 44100.f);
      e->dispatcher(e, effSetBlockSize, 0, 128, nullptr, 0.f);
      e->dispatcher(e, effMainsChanged, 0, 1, nullptr, 0.f);
      auto& states = output[std::string(spec->stateFormat).substr(9)] = nlohmann::json::array();
      states.push_back(InspectSnapshot(plugin, "default"));
      for (const auto& section : spec->sections)
      {
        if (section.choices.size() < 2) continue;
        for (size_t c = 0; c < section.choices.size(); ++c)
        {
          InspectSet(plugin, std::string(section.id) + "_type", static_cast<float>(c) / (section.choices.size() - 1));
          states.push_back(InspectSnapshot(plugin, std::string(section.id) + "-" + std::to_string(c), section.id));
          for (int i = 0; i < plugin.numParams(); ++i)
            if (plugin.param(i).id.rfind(std::string(section.id) + "_o", 0) == 0 && plugin.ParameterName(i) == "Tempo sync")
            {
              plugin.setNormalised(i, 1.f);
              states.push_back(InspectSnapshot(plugin, std::string(section.id) + "-" + std::to_string(c) + "-synced", section.id));
              plugin.setNormalised(i, 0.f);
            }
        }
        InspectSet(plugin, std::string(section.id) + "_type", static_cast<float>(section.defaultChoice) / (section.choices.size() - 1));
      }
      if (std::string(spec->stateFormat) == "nam-grid-amp") InspectLibrary(plugin, states);
      e->dispatcher(e, effMainsChanged, 0, 0, nullptr, 0.f);
    }
    e->dispatcher(e, effClose, 0, 0, nullptr, 0.f);
  }
  if (gallery) std::ofstream(argv[2]) << output.dump(2) << '\n';
  else std::cout << output.dump(2) << '\n';
}
