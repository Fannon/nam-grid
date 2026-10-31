// On-demand A/B audit of one NAM Grid capture slot against the official
// NeuralAmpModelerPlugin (Gateway) shell.
//
// Both plugins are driven through CLAP with neutral settings: one NAM capture,
// no gate, no tone stack, Raw output mode and unity gains. Any remaining
// difference is the capture path itself (model preparation, resampling, gain
// staging, DC blocker or latency).
//
// The reference plugin is an external artifact, so this executable runs only
// when CMake is configured with -DNAM_REFERENCE_CLAP=<official CLAP build>, or
// when it is invoked by hand with --reference.
#include "ClapTestHost.h"
#include "../NeuralAmpModeler/ParameterLayout.h"
#include "../NeuralAmpModeler/SlotDefaults.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
constexpr double kGridTolerance = 1.e-6;

std::string Utf8(const std::filesystem::path& path)
{
  const auto value = path.u8string();
  return std::string(reinterpret_cast<const char*>(value.data()), value.size());
}

void Check(bool ok, const std::string& message)
{
  if (!ok)
    throw std::runtime_error(message);
}

#ifdef _WIN32
using LibraryHandle = HMODULE;
#else
using LibraryHandle = void*;
#endif

class Library
{
public:
  LibraryHandle handle = nullptr;
  const clap_plugin_entry* entry = nullptr;
  const clap_plugin_factory* factory = nullptr;
  explicit Library(const std::filesystem::path& path)
  {
#ifdef _WIN32
    handle = LoadLibraryW(path.c_str());
    if (handle)
      entry = reinterpret_cast<const clap_plugin_entry*>(GetProcAddress(handle, "clap_entry"));
#else
    handle = dlopen(path.c_str(), RTLD_NOW);
    if (handle)
      entry = reinterpret_cast<const clap_plugin_entry*>(dlsym(handle, "clap_entry"));
#endif
    Check(entry && entry->init(Utf8(path).c_str()), "Cannot load/initialize CLAP plugin: " + Utf8(path));
    factory = static_cast<const clap_plugin_factory*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
    Check(factory && factory->get_plugin_count(factory) >= 1, "Expected at least one CLAP plugin");
  }
  ~Library()
  {
    if (entry)
      entry->deinit();
#ifdef _WIN32
    if (handle)
      FreeLibrary(handle);
#else
    if (handle)
      dlclose(handle);
#endif
  }
};

class Instance
{
public:
  Host host;
  const clap_plugin* plugin = nullptr;
  const clap_plugin_state* state = nullptr;
  bool active = false, started = false;
  explicit Instance(const Library& library)
  {
    plugin = library.factory->create_plugin(
      library.factory, &host.host, library.factory->get_plugin_descriptor(library.factory, 0)->id);
    Check(plugin && plugin->init(plugin), "Cannot create plugin instance");
    state = static_cast<const clap_plugin_state*>(plugin->get_extension(plugin, CLAP_EXT_STATE));
    Check(state != nullptr, "Missing state extension");
  }
  ~Instance()
  {
    if (started)
      plugin->stop_processing(plugin);
    if (active)
      plugin->deactivate(plugin);
    if (plugin)
      plugin->destroy(plugin);
  }
  void Activate(double rate, int buffer)
  {
    Check(plugin->activate(plugin, rate, 2, buffer), "Activation failed");
    active = true;
  }
  void Deactivate()
  {
    plugin->deactivate(plugin);
    active = false;
  }
  Stream Save()
  {
    Stream bytes;
    clap_ostream stream{&bytes, Stream::write};
    Check(state->save(plugin, &stream), "State save failed");
    return bytes;
  }
  void Load(const Stream& bytes)
  {
    clap_istream stream{const_cast<Stream*>(&bytes), Stream::read};
    Check(state->load(plugin, &stream), "State load failed");
  }
  int Latency() const
  {
    const auto* latency = static_cast<const clap_plugin_latency*>(plugin->get_extension(plugin, CLAP_EXT_LATENCY));
    return latency ? latency->get(plugin) : 0;
  }
};

int32_t ReadInt(const std::vector<uint8_t>& bytes, size_t& pos)
{
  int32_t value = 0;
  Check(pos + sizeof(value) <= bytes.size(), "Truncated state chunk");
  std::memcpy(&value, bytes.data() + pos, sizeof(value));
  pos += sizeof(value);
  return value;
}

std::string ReadString(const std::vector<uint8_t>& bytes, size_t& pos)
{
  const int32_t length = ReadInt(bytes, pos);
  Check(length >= 0 && pos + static_cast<size_t>(length) <= bytes.size(), "Invalid string in state chunk");
  std::string value(reinterpret_cast<const char*>(bytes.data() + pos), static_cast<size_t>(length));
  pos += static_cast<size_t>(length);
  return value;
}

void PutInt(std::vector<uint8_t>& bytes, int32_t value)
{
  const auto* p = reinterpret_cast<const uint8_t*>(&value);
  bytes.insert(bytes.end(), p, p + sizeof(value));
}

// The official shell stores the model path inside its iPlug2 chunk. Saving the
// default state gives us the exact parameter payload for this build; replace
// the empty path and force neutral settings in the same chunk.
std::vector<uint8_t> PrepareReferenceState(Instance& reference, const std::filesystem::path& model, bool lite)
{
  const auto saved = reference.Save();
  const auto& bytes = saved.bytes;
  size_t pos = 0;
  const auto header = ReadString(bytes, pos);
  (void)ReadString(bytes, pos); // Plugin version; its position matters, not its value.
  Check(header == "###NeuralAmpModeler###", "Unexpected reference state header: " + header);
  const size_t namLengthPos = pos;
  const auto namPath = ReadString(bytes, pos);
  const size_t irLengthPos = pos;
  const auto irPath = ReadString(bytes, pos);
  Check(namPath.empty() && irPath.empty(), "Default reference state unexpectedly contains file paths");
  const size_t paramsPos = pos;
  constexpr size_t parameterCount = 13; // Input ... Slim in NeuralAmpModeler.h
  Check(bytes.size() - paramsPos == parameterCount * sizeof(double),
        "Unexpected reference parameter payload size");

  const auto path = Utf8(model);
  std::vector<uint8_t> prepared(bytes.begin(), bytes.begin() + namLengthPos);
  PutInt(prepared, static_cast<int32_t>(path.size()));
  prepared.insert(prepared.end(), path.begin(), path.end());
  prepared.insert(prepared.end(), bytes.begin() + irLengthPos, bytes.end());

  const size_t newParamsPos = prepared.size() - parameterCount * sizeof(double);
  const auto set = [&](size_t index, double value) {
    std::memcpy(prepared.data() + newParamsPos + index * sizeof(double), &value, sizeof(value));
  };
  // kInputLevel, kOutputLevel, kNoiseGateActive, kEQActive, kCalibrateInput,
  // kOutputMode, kSlim. Raw passthrough with a full-size slimmable model.
  set(0, 0.);
  set(5, 0.);
  set(6, 0.);
  set(7, 0.);
  set(9, 0.);
  set(11, 0.);
  set(12, lite ? 0. : 1.);
  return prepared;
}

json PrepareGridState(Instance& grid, const std::filesystem::path& model, bool lite)
{
  auto config = grid.Save().config();
  config["Tuner"] = 0.;
  config["Tuner Mute"] = 0.;
  config["Pre 2 NAM Input"] = config["Output"] = 0.;
  config["SlotPaths"] = std::vector<std::string>(16, "");
  config["SlotHashes"] = std::vector<std::string>(16, "");
  config["SlotEffects"] = std::vector<int>(16, static_cast<int>(SlotEffect::NAM));
  const auto defaults = SlotDefaultValues(SlotEffect::NAM);
  for (int control = 0; control < parameter_layout::standardControls; ++control)
    config[std::string("Pre 1 ") + parameter_layout::slotControlNames[control]] = defaults[control];
  for (int p = 0; p < kUtilityControls; ++p)
    config[std::string("Pre 1 ") + kEffectParameters[p].name] = defaults[17 + p];
  if (lite)
    config["Pre 1 Full Quality"] = 0.;
  config["SlotPaths"][0] = Utf8(model);
  return config;
}

void LoadGridState(Instance& grid, const json& config)
{
  Stream bytes;
  bytes.string("###nam-grid###");
  bytes.string(config.dump());
  grid.Load(bytes);
}

struct Render
{
  std::vector<double> left, right;
  int latency = 0;
};

// Deterministic input with a slow level envelope. Both channels differ so the
// mono-collapse convention is exercised rather than bypassed.
Render Run(const std::filesystem::path& pluginPath, const std::filesystem::path& model, bool reference, bool lite,
           double rate, int buffer, double warmup, double seconds)
{
  Library library(pluginPath);
  Instance instance(library);
  // Loaders need the host rate and block limit before preparing the capture.
  instance.Activate(rate, buffer);
  instance.Deactivate();
  try
  {
    if (reference)
    {
      const auto prepared = PrepareReferenceState(instance, model, lite);
      Stream bytes;
      bytes.bytes = prepared;
      instance.Load(bytes);
    }
    else
    {
      LoadGridState(instance, PrepareGridState(instance, model, lite));
      const auto recalled = instance.Save().config();
      Check(recalled.at("SlotPaths")[0] == Utf8(model), "Grid did not recall the capture path");
      Check(recalled.at("SlotHashes")[0].is_string() && !recalled.at("SlotHashes")[0].get<std::string>().empty(),
            "Grid failed to load the capture");
    }
  }
  catch (const std::exception& e)
  {
    throw std::runtime_error(std::string(reference ? "reference" : "grid") + " state: " + e.what());
  }
  instance.Activate(rate, buffer);
  Check(instance.plugin->start_processing(instance.plugin), "Start processing failed");
  instance.started = true;

  Render render;
  const int blocks = static_cast<int>(std::ceil((warmup + seconds) * rate / buffer));
  const int frames = blocks * buffer;
  render.left.reserve(frames);
  render.right.reserve(frames);

  std::vector<double> inL(buffer), inR(buffer), outL(buffer), outR(buffer);
  double* inputs[]{inL.data(), inR.data()};
  double* outputs[]{outL.data(), outR.data()};
  clap_audio_buffer input{nullptr, inputs, 2, 0, 0}, output{nullptr, outputs, 2, 0, 0};
  clap_output_events sent{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
  clap_event_transport transport{};
  transport.header.size = sizeof(transport);
  transport.header.type = CLAP_EVENT_TRANSPORT;
  transport.flags = CLAP_TRANSPORT_HAS_TEMPO | CLAP_TRANSPORT_IS_PLAYING;
  transport.tempo = 120.;
  clap_process process{};
  process.audio_inputs = &input;
  process.audio_outputs = &output;
  process.audio_inputs_count = process.audio_outputs_count = 1;
  process.out_events = &sent;
  process.transport = &transport;

  int64_t position = 0;
  for (int block = 0; block < blocks; ++block)
  {
    for (int i = 0; i < buffer; ++i)
    {
      const double t = static_cast<double>(position + i) / rate;
      // Slow level changes, guitar-range fundamentals and inharmonic partials.
      const double envelope = .45 + .55 * std::pow(std::sin(3.141592653589793 * .7 * t), 2.);
      inL[i] = envelope * (.11 * std::sin(6.283185307179586 * 110. * t)
                           + .045 * std::sin(6.283185307179586 * 233.08 * t + .4)
                           + .025 * std::sin(6.283185307179586 * 1046.5 * t));
      inR[i] = envelope * (.1 * std::sin(6.283185307179586 * 164.81 * t + .3)
                           + .035 * std::sin(6.283185307179586 * 440. * t)
                           + .02 * std::sin(6.283185307179586 * 2637. * t + .7));
      outL[i] = outR[i] = std::numeric_limits<double>::quiet_NaN();
    }
    process.frames_count = buffer;
    process.steady_time = position;
    const auto status = instance.plugin->process(instance.plugin, &process);
    Check(status != CLAP_PROCESS_ERROR, "Plugin process failed");
    for (int i = 0; i < buffer; ++i)
    {
      Check(std::isfinite(outL[i]) && std::isfinite(outR[i]), "Plugin produced a non-finite sample");
      render.left.push_back(outL[i]);
      render.right.push_back(outR[i]);
    }
    position += buffer;
  }
  render.latency = instance.Latency();
  instance.plugin->stop_processing(instance.plugin);
  instance.started = false;
  instance.Deactivate();
  return render;
}

json Compare(const Render& reference, const Render& grid, double rate, double warmup)
{
  const size_t skip = static_cast<size_t>(std::llround(warmup * rate));
  const size_t refStart = skip + static_cast<size_t>(std::max(0, reference.latency));
  const size_t gridStart = skip + static_cast<size_t>(std::max(0, grid.latency));
  const size_t count = std::min(reference.left.size() - refStart, grid.left.size() - gridStart);
  Check(count > 0, "No overlapping rendered samples");
  double maxError = 0., refEnergy = 0., gridEnergy = 0., errorEnergy = 0., refPeak = 0., gridPeak = 0.;
  for (int channel = 0; channel < 2; ++channel)
  {
    const auto& referenceSamples = channel == 0 ? reference.left : reference.right;
    const auto& gridSamples = channel == 0 ? grid.left : grid.right;
    for (size_t i = 0; i < count; ++i)
    {
      const double r = referenceSamples[refStart + i];
      const double g = gridSamples[gridStart + i];
      const double error = r - g;
      maxError = std::max(maxError, std::abs(error));
      refEnergy += r * r;
      gridEnergy += g * g;
      errorEnergy += error * error;
      refPeak = std::max(refPeak, std::abs(r));
      gridPeak = std::max(gridPeak, std::abs(g));
    }
  }
  Check(refPeak > 1.e-3 && gridPeak > 1.e-3, "Render is silent; a capture did not load");
  const double refRms = std::sqrt(refEnergy / (2. * count)), gridRms = std::sqrt(gridEnergy / (2. * count));
  const double errorRms = std::sqrt(errorEnergy / (2. * count));
  // Best lag over a small window, for a wrong or unreported latency.
  int bestLag = 0;
  double bestError = std::numeric_limits<double>::infinity();
  for (int lag = -64; lag <= 64; ++lag)
  {
    double energy = 0.;
    for (size_t i = 64; i + 64 < count; ++i)
    {
      const auto j = static_cast<int64_t>(i) + lag;
      if (j < 0 || j >= static_cast<int64_t>(count)) continue;
      const double error = reference.left[refStart + i] - grid.left[gridStart + j];
      energy += error * error;
    }
    if (energy < bestError)
    {
      bestError = energy;
      bestLag = lag;
    }
  }
  return {{"samples", count},
          {"reference_latency", reference.latency},
          {"grid_latency", grid.latency},
          {"reference_peak", refPeak},
          {"grid_peak", gridPeak},
          {"reference_rms", refRms},
          {"grid_rms", gridRms},
          {"gain_db", 20. * std::log10(std::max(1.e-30, gridRms / std::max(1.e-30, refRms)))},
          {"max_abs_error", maxError},
          {"error_rms", errorRms},
          {"error_db", 20. * std::log10(std::max(1.e-30, errorRms / std::max(1.e-30, refRms)))},
          {"best_lag", bestLag}};
}
} // namespace

int main(int argc, char** argv)
try
{
  std::cout << std::unitbuf;
  if (argc < 2 || std::string(argv[1]) == "--help")
  {
    std::cout << "Usage: " << argv[0]
              << " --grid PLUGIN --reference PLUGIN --model FILE [--model FILE ...]\n"
                 "       [--rate 48000] [--buffer 128] [--warmup 0.5] [--seconds 2]\n"
                 "       [--tolerance 1e-6] [--json REPORT]\n";
    return argc < 2 ? 2 : 0;
  }
  std::filesystem::path gridPath, referencePath, reportPath;
  std::vector<std::filesystem::path> models;
  std::vector<double> rates{48000.};
  std::vector<int> buffers{128};
  double warmup = .5, seconds = 2., tolerance = kGridTolerance;
  bool lite = false;
  for (int i = 1; i < argc; ++i)
  {
    const std::string arg = argv[i];
    const auto next = [&]() {
      Check(i + 1 < argc, "Missing value for " + arg);
      return std::string(argv[++i]);
    };
    if (arg == "--grid")
      gridPath = next();
    else if (arg == "--reference")
      referencePath = next();
    else if (arg == "--model")
      models.emplace_back(next());
    else if (arg == "--rate")
      rates.push_back(std::stod(next()));
    else if (arg == "--buffer")
      buffers.push_back(std::stoi(next()));
    else if (arg == "--warmup")
      warmup = std::stod(next());
    else if (arg == "--seconds")
      seconds = std::stod(next());
    else if (arg == "--tolerance")
      tolerance = std::stod(next());
    else if (arg == "--lite")
      lite = true;
    else if (arg == "--json")
      reportPath = next();
    else
      Check(false, "Unknown argument: " + arg);
  }
  Check(!gridPath.empty() && !referencePath.empty() && !models.empty(),
        "--grid, --reference and at least one --model are required");
  Check(std::filesystem::exists(gridPath), "Missing grid plugin: " + Utf8(gridPath));
  Check(std::filesystem::exists(referencePath), "Missing reference plugin: " + Utf8(referencePath));
  // The first value is a default; drop it when the user provided explicit ones.
  if (rates.size() > 1)
    rates.erase(rates.begin());
  if (buffers.size() > 1)
    buffers.erase(buffers.begin());

  json report = json::array();
  bool pass = true;
  for (const auto& model : models)
  {
    Check(std::filesystem::exists(model), "Missing model: " + Utf8(model));
    for (double rate : rates)
      for (int buffer : buffers)
      {
        const auto reference = Run(referencePath, model, true, lite, rate, buffer, warmup, seconds);
        const auto grid = Run(gridPath, model, false, lite, rate, buffer, warmup, seconds);
        auto row = Compare(reference, grid, rate, warmup);
        row["model"] = Utf8(model.filename());
        row["rate"] = rate;
        row["buffer"] = buffer;
        row["tolerance"] = tolerance;
        row["quality"] = lite ? "a2-lite" : "full";
        row["pass"] = row["max_abs_error"].get<double>() <= tolerance
                      && std::abs(row["gain_db"].get<double>()) <= 1.e-6 && row["best_lag"].get<int>() == 0;
        pass = pass && row["pass"].get<bool>();
        report.push_back(row);
        std::cout << std::left << std::setw(24) << Utf8(model.filename()) << " " << std::setw(7) << rate << "/"
                  << std::setw(4) << buffer << " max_error=" << std::scientific << std::setprecision(3)
                  << row["max_abs_error"].get<double>() << " error=" << std::fixed << std::setprecision(1)
                  << row["error_db"].get<double>() << " dB gain=" << std::showpos << row["gain_db"].get<double>()
                  << std::noshowpos << " dB latency=" << reference.latency << "/" << grid.latency
                  << " lag=" << row["best_lag"].get<int>() << " pass=" << (row["pass"].get<bool>() ? "yes" : "NO")
                  << "\n";
      }
  }
  if (!reportPath.empty())
  {
    std::ofstream out(reportPath);
    Check(out.good(), "Cannot write report");
    out << report << "\n";
  }
  std::cout << (pass ? "PASS" : "FAIL") << ": " << report.size() << " render comparison(s)\n";
  return pass ? 0 : 1;
}
catch (const std::exception& e)
{
  std::cerr << "nam-parity: " << e.what() << "\n";
  return 2;
}
