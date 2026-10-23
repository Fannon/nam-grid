// Windows VST3 controller smoke test: inspect the built plugin rather than
// assuming CLAP metadata proves VST3 normalization/automation compatibility.
#include <windows.h>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <json.hpp>
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "../NeuralAmpModeler/ParameterLayout.h"

using namespace Steinberg;
using namespace Steinberg::Vst;
using json = nlohmann::json;

int main(int argc, char** argv)
{
  assert(argc == 3);
  auto library = LoadLibraryW(std::filesystem::path(argv[1]).c_str());
  assert(library);
  auto initialize = reinterpret_cast<bool (*)()>(GetProcAddress(library, "InitDll"));
  auto terminate = reinterpret_cast<bool (*)()>(GetProcAddress(library, "ExitDll"));
  if (initialize)
    assert(initialize());
  auto getFactory = reinterpret_cast<GetFactoryProc>(GetProcAddress(library, "GetPluginFactory"));
  assert(getFactory);
  auto* factory = getFactory();
  assert(factory);
  IEditController* controller = nullptr;
  for (int i = 0; i < factory->countClasses(); ++i)
  {
    PClassInfo info{};
    assert(factory->getClassInfo(i, &info) == kResultOk);
    if (factory->createInstance(info.cid, INLINE_UID_OF(IEditController), reinterpret_cast<void**>(&controller))
        == kResultOk)
      break;
  }
  assert(controller && controller->initialize(nullptr) == kResultOk);
  // VST3 inserts a framework bypass parameter before the plugin parameters.
  // Enumeration order is not identity: index the controller metadata by ID.
  std::vector<ParameterInfo> parameters(kNumParams);
  std::vector<bool> seen(kNumParams);
  int bypassCount = 0;
  for (int index = 0; index < controller->getParameterCount(); ++index)
  {
    ParameterInfo info{};
    assert(controller->getParameterInfo(index, info) == kResultOk);
    if (info.flags & ParameterInfo::kIsBypass)
    {
      ++bypassCount;
      continue;
    }
    assert(info.id < kNumParams && !seen[info.id]);
    parameters[info.id] = info;
    seen[info.id] = true;
  }
  assert(bypassCount == 1 && std::all_of(seen.begin(), seen.end(), [](bool found) { return found; }));
  json snapshot;
  std::ifstream(argv[2]) >> snapshot;
  auto verify = [&](int id, json expected, int control) {
    const auto& info = parameters[id];
    assert(info.id == id);
    std::string name;
    for (auto* c = info.title; *c; ++c)
      name.push_back(static_cast<char>(*c));
    assert(name == expected["name"].get<std::string>());
    const bool algorithm = IsAlgorithmMode(control);
    assert(bool(info.flags & ParameterInfo::kCanAutomate) == !algorithm);
    if (algorithm)
      return; // Algorithms may expand; normalized positions are not automated.
    const bool stepped = expected["stepped"].get<bool>();
    const double minimum = expected["minimum"], maximum = expected["maximum"];
    const double initial = expected["default"];
    const double normalizedDefault = stepped ? (initial - minimum) / (maximum - minimum) : initial;
    assert(std::abs(info.defaultNormalizedValue - normalizedDefault) < 1.e-12);
    assert(info.stepCount == (stepped ? maximum - minimum : 0.));
    if (stepped)
    {
      // CLAP enum values are native integers; arbitrary fractional native
      // inputs do not have display labels. Verify VST3 quantization directly
      // instead of comparing those inputs with normalized VST3 strings.
      for (double normalized : {0., .125, .25, .5, .75, .875, 1.})
        assert(controller->normalizedParamToPlain(id, normalized)
               == std::round(minimum + normalized * (maximum - minimum)));
      for (double value = minimum; value <= maximum; value += 1.)
        assert(std::abs(controller->plainParamToNormalized(id, value) - (value - minimum) / (maximum - minimum))
               < 1.e-12);
    }
    int sample = 0;
    for (double normalized : {0., .125, .25, .5, .75, .875, 1.})
    {
      String128 text{};
      assert(controller->getParamStringByValue(id, normalized, text) == kResultOk);
      std::string label;
      for (auto* c = text; *c; ++c)
        label.push_back(static_cast<char>(*c));
      // CLAP appends units to the value string; VST3 reports units separately.
      std::string units;
      for (auto* c = info.units; *c; ++c)
        units.push_back(static_cast<char>(*c));
      if (!units.empty())
        label += " " + units;
      if (label != expected["mapping"][sample].get<std::string>())
        std::cerr << "VST3 mapping changed: " << name << " at " << normalized << " got " << label << " expected "
                  << expected["mapping"][sample] << '\n';
      assert(label == expected["mapping"][sample++].get<std::string>());
    }
  };
  for (int slot = 0; slot < 16; ++slot)
    for (int p = 0; p < snapshot["utility"].size(); ++p)
    {
      auto expected = snapshot["utility"][p];
      expected["name"] = GridSlotName(slot) + " " + expected["name"].get<std::string>().substr(6);
      verify(SlotUtilityParam(slot, p), expected, p);
    }
  for (const auto& expected : snapshot["global"])
    verify(expected["id"], expected, -1);
  assert(controller->terminate() == kResultOk);
  controller->release();
  factory->release();
  if (terminate)
    assert(terminate());
  FreeLibrary(library);
  std::cout
    << "VST3 parameter compatibility: all frozen IDs, normalization mappings and state-only algorithm flags passed\n";
}
