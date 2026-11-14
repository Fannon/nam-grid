#pragma once
// NAM Grid for MPC: the engine shared by the MPC plugins (Amp, Pedals, Delay, Cosmic Echo).
//
// A plugin is a fixed series of sections. Each section is one desktop NAMStage running the
// same DSP as NAM Grid; its Type menu chooses the module (or a module's voice/style).
// Signal flow: stereo in -> section 1 -> ... -> section N -> Output -> stereo out.
//
// Host parameters (ids are permanent; append new ones at the end):
//   Output, [Rescan], then per section: [Type] [On] [Folder File] Option 1..n Knob 1..n.
// Knob and option names follow the chosen module (MPCLayout.h); controls a module does not
// use report an empty name, so the skin hides them. Every section's control values, file
// reference and Smart Gate profile are saved as named values in the extra state.
//
// Threads: the audio thread only reads atomics and runs the stages. A worker thread scans
// the fixed library folders, loads captures and IRs and publishes prepared modules through
// NAMStage's lock-free exchange. Host UI calls (names, text, state) never touch audio data.
#if defined(_WIN32)
  // Native Windows test builds: WDL's UTF-8 wrappers need these first, and redefine
  // fopen, so the standard streams must be included before any WDL header.
  #include <windows.h>
  #include <commdlg.h>
  #include <shellapi.h>
#endif
#include <fstream>

#include "mpc_plugin.h"

#include "MPCLayout.h"
#include "MPCLibrary.h"
#include "SlotControlMap.h"
#include "SlotOperations.h"
#include "../NeuralAmpModelerCore/NAM/activations.h"
#include "json.hpp"

#include <array>
#include <atomic>
#include <functional>
#include <limits>
#include <chrono>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace mpc_grid
{
using parameter_layout::kSlotEnabled;
using parameter_layout::kSlotFullQuality;

struct SectionSpec
{
  const char* id;   // Parameter id prefix and state key. Permanent.
  const char* name; // Screen name, e.g. "Gate", "Voice", "Delay".
  std::vector<Choice> choices; // The Type menu. Released order is permanent: append only.
  int defaultChoice = 0;
  bool switchable = false; // Has an On parameter.
  bool defaultOn = true;
  const char* typeName = nullptr; // Screen name of the Type menu (default: the section name).
  bool dcBlock = false; // A 5 Hz DC blocker after the section (NAM captures can have a DC offset).
};
struct ProgramSpec
{
  const char* name;
  std::vector<int> choices; // Per section.
  std::vector<int> on;      // Per section: 1 on, 0 off, -1 the section's default.
};
struct PluginSpec
{
  const char* name;   // Must match plugin.json and the folder name.
  uint32_t uid;       // Must match plugin.json. Permanent.
  const char* stateFormat; // Permanent.
  std::vector<SectionSpec> sections;
  std::vector<ProgramSpec> programs; // None: the MPC shows one "Default" program.
  // Process in mono: the input is summed, every section runs on one channel and the result
  // goes to both outputs. For guitar rigs around a (mono) NAM capture: halves gate and cabinet
  // work for no audible difference with a mono source.
  bool mono = false;
};
inline constexpr double kNormalizedLoudness = -18.; // dB, the target of the Normalize switch.

inline constexpr int kStateSchema = 1;
inline const char* ControlKey(int control)
{
  return IsUtility(control) ? kEffectParameters[UtilityControl(control)].name : parameter_layout::slotControlNames[control];
}
// A choice's starting values: the module's insertion defaults and its style.
inline SlotValues ChoiceDefaults(const Choice& choice)
{
  auto values = SlotDefaultValues(choice.effect);
  values[standardControls + uNoiseLearn] = 0.;
  // Slimmable (A2) captures start at Lite on the MPC: Full is heavy for its CPUs.
  if (choice.effect == SlotEffect::NAM && choice.files == FileKind::Capture) values[kSlotFullQuality] = 0.;
  if (choice.styleControl >= 0) values[choice.styleControl] = choice.style;
  return values;
}
inline FileKind SectionFiles(const SectionSpec& section)
{
  for (const auto& c : section.choices)
    if (c.files != FileKind::None) return c.files;
  return FileKind::None;
}
// How many knobs and options a section needs across its choices and tempo-sync states.
// The section's options in fixed places: every option any of its choices uses, the most
// common first, so an option keeps its place (and its switch) across module changes.
inline std::vector<int> SectionOptionOrder(const SectionSpec& section)
{
  std::vector<int> order, count;
  for (const auto& choice : section.choices)
  {
    std::vector<int> seen;
    for (bool sync : {false, true})
    {
      auto values = ChoiceDefaults(choice);
      values[standardControls + uTempoSync] = sync ? 1. : 0.;
      for (int c : BaseLayout(choice, values, true).options)
        if (c >= 0 && std::find(seen.begin(), seen.end(), c) == seen.end()) seen.push_back(c);
    }
    for (int c : seen)
    {
      const auto it = std::find(order.begin(), order.end(), c);
      if (it == order.end())
      {
        order.push_back(c);
        count.push_back(1);
      }
      else
        ++count[it - order.begin()];
    }
  }
  std::vector<size_t> index(order.size());
  for (size_t i = 0; i < index.size(); ++i) index[i] = i;
  std::stable_sort(index.begin(), index.end(), [&](size_t a, size_t b) { return count[a] > count[b]; });
  std::vector<int> sorted;
  for (size_t i : index) sorted.push_back(order[i]);
  if (sorted.size() > kOptions) sorted.resize(kOptions);
  return sorted;
}
// How many knobs a section needs across its choices and tempo-sync states.
inline int SectionKnobCount(const SectionSpec& section)
{
  int knobs = 0;
  for (const auto& choice : section.choices)
    for (bool sync : {false, true})
    {
      auto values = ChoiceDefaults(choice);
      values[standardControls + uTempoSync] = sync ? 1. : 0.;
      const auto layout = MakeLayout(choice, values, true);
      for (int k = 0; k < kKnobs; ++k)
        if (layout.knobs[k] >= 0) knobs = std::max(knobs, k + 1);
    }
  return knobs;
}

class GridPlugin : public mpc::Plugin
{
public:
  enum class Role { Global, Type, On, Folder, File, Option, Knob };

  explicit GridPlugin(const PluginSpec& spec) : mpc::Plugin(Info(spec)), mSpec(spec), mSections(spec.sections.size())
  {
    nam::activations::Activation::enable_fast_tanh();
    mStaticNames = std::getenv("NAM_GRID_MPC_STATIC_NAMES") != nullptr;
    mOutputParam = addParam(mpc::floatParam("out", "Output", -40.f, 12.f, 0.f, "dB", 1));
    bool files = false;
    for (const auto& section : spec.sections) files = files || SectionFiles(section) != FileKind::None;
    if (files) mRescanParam = addParam(mpc::boolParam("rescan", "Rescan", false));
    bool captures = false;
    for (const auto& section : spec.sections)
      captures = captures || SectionFiles(section) == FileKind::Capture;
    // Levels every capture to the same loudness from its metadata (where the capture has it).
    if (captures) mNormalizeParam = addParam(mpc::boolParam("normalize", "Normalize", true));
    for (size_t s = 0; s < spec.sections.size(); ++s)
    {
      const auto& sectionSpec = spec.sections[s];
      auto& section = mSections[s];
      section.spec = &sectionSpec;
      section.remembered.resize(sectionSpec.choices.size());
      section.rememberedValid.assign(sectionSpec.choices.size(), 0);
      const std::string id = std::string(sectionSpec.id) + "_", name = std::string(sectionSpec.name) + " ";
      const auto add = [&](const mpc::Param& p, Role role, int n = 0) {
        const int index = addParam(p);
        mRoles.push_back({static_cast<int>(s), role, n});
        return index;
      };
      while (static_cast<int>(mRoles.size()) < numParams()) mRoles.push_back({-1, Role::Global, 0});
      if (sectionSpec.choices.size() > 1)
      {
        std::vector<std::string> names;
        for (const auto& c : sectionSpec.choices) names.push_back(c.name);
        section.typeParam = add(mpc::choiceParam((id + "type").c_str(), (name + "Type").c_str(), names, sectionSpec.defaultChoice), Role::Type);
      }
      if (sectionSpec.switchable)
        section.onParam = add(mpc::boolParam((id + "on").c_str(), (name + "On").c_str(), sectionSpec.defaultOn), Role::On);
      if (SectionFiles(sectionSpec) != FileKind::None)
      {
        section.folderParam = add(mpc::floatParam((id + "folder").c_str(), (name + "Folder").c_str(), 0.f, 1.f, 0.f, "", 3), Role::Folder);
        section.fileParam = add(mpc::floatParam((id + "file").c_str(), (name + "File").c_str(), 0.f, 1.f, 0.f, "", 3), Role::File);
      }
      section.optionOrder = SectionOptionOrder(sectionSpec);
      const int knobs = SectionKnobCount(sectionSpec), options = static_cast<int>(section.optionOrder.size());
      for (int o = 0; o < options; ++o)
        section.optionParams.push_back(add(mpc::floatParam((id + "o" + std::to_string(o + 1)).c_str(),
                                                           (name + "Opt " + std::to_string(o + 1)).c_str(), 0.f, 1.f, 0.f, "", 3),
                                           Role::Option, o));
      for (int k = 0; k < knobs; ++k)
        section.knobParams.push_back(add(mpc::floatParam((id + "k" + std::to_string(k + 1)).c_str(),
                                                         (name + "Knob " + std::to_string(k + 1)).c_str(), 0.f, 1.f, .5f, "", 3),
                                         Role::Knob, k));
    }
    mRoles.resize(numParams(), {-1, Role::Global, 0});
    for (size_t s = 0; s < mSections.size(); ++s)
    {
      auto& section = mSections[s];
      section.choice.store(section.spec->defaultChoice);
      StoreValues(section, ChoiceDefaults(Current(section)));
      RefreshLayout(static_cast<int>(s));
    }
    AddPrograms();
    WriteManifest();
    mWorker = std::thread([this] { WorkerLoop(); });
  }
  ~GridPlugin() override
  {
    mQuit.store(true);
    if (mWorker.joinable()) mWorker.join();
  }

  static mpc::PluginInfo Info(const PluginSpec& spec)
  {
    mpc::PluginInfo info;
    info.name = spec.name;
    info.vendor = "Simon Heimler";
    info.uniqueId = spec.uid;
    info.version = 100; // 0.1.0.0
    info.isInstrument = false;
    info.numInputs = 2;
    info.numOutputs = 2;
    return info;
  }

  // ----- host-facing names (dispatcher wrapper) ------------------------------------------
  std::string ParameterName(int index)
  {
    const auto role = mRoles[index];
    if (role.section < 0 || mStaticNames) return param(index).name;
    const auto& section = mSections[role.section];
    const auto& choice = Current(section);
    switch (role.role)
    {
      case Role::Type: return section.spec->typeName ? section.spec->typeName : section.spec->name;
      case Role::On: return std::string(section.spec->name) + " On";
      case Role::Folder: return choice.files == FileKind::None ? "" : "Folder";
      case Role::File: return choice.files == FileKind::Capture ? "Capture" : choice.files == FileKind::IR ? "IR" : "";
      default: break;
    }
    const int control = ControlOf(index);
    return control < 0 ? std::string() : ControlLabel(choice, control, LoadValues(section));
  }
  void BeginStateLoad()
  {
    mExtraApplied = false;
    mLoadingState.store(true);
  }
  void EndStateLoad(bool loaded)
  {
    mLoadingState.store(false);
    if (loaded && !mExtraApplied) ApplyParametersAsEdits(); // A parameter-only chunk.
  }
  // The SDK applies parameters while parsing the chunk. Validate it first so a truncated,
  // foreign or malformed preset cannot partly change the current rig or throw through VST.
  bool CanLoadState(const uint8_t* data, size_t size) const
  {
    if (!data || size < 12 || std::memcmp(data, "MPCP", 4) != 0) return false;
    size_t pos = 4;
    const auto u32 = [&](uint32_t& value) {
      if (size - pos < 4) return false;
      value = 0;
      for (int i = 0; i < 4; ++i) value |= uint32_t(data[pos++]) << (8 * i);
      return true;
    };
    uint32_t version = 0, count = 0;
    if (!u32(version) || (version != 1 && version != 2) || !u32(count)) return false;
    for (uint32_t i = 0; i < count; ++i)
    {
      if (pos == size) return false;
      const size_t length = data[pos++];
      if (length > size - pos) return false;
      pos += length;
      for (int field = 0; field < (version == 1 ? 1 : 2); ++field)
      {
        uint32_t bits;
        if (!u32(bits)) return false;
        float value;
        std::memcpy(&value, &bits, sizeof value);
        if (!std::isfinite(value)) return false;
      }
    }
    uint32_t extraSize;
    if (!u32(extraSize) || extraSize != size - pos) return false;
    if (extraSize == 0) return true;
    try { return ValidExtraState(nlohmann::json::parse(data + pos, data + size)); }
    catch (...) { return false; }
  }
  const PluginSpec& Spec() const { return mSpec; }

  // ----- mpc::Plugin -----------------------------------------------------------------
  void prepare(double sampleRate, int maxBlockSize) override
  {
    // The host stops processing around prepare, so this thread may reset the stages.
    std::lock_guard<std::mutex> publish(mPublishMutex);
    mPrepared.store(false);
    mRate = sampleRate > 0. ? sampleRate : 44100.;
    mBlock = std::max(32, maxBlockSize);
    mLeft.assign(mBlock, 0.);
    mRight.assign(mBlock, 0.);
    {
      std::lock_guard<std::mutex> lock(mControlMutex);
      ScanLibraryLocked();
    }
    int latency = 0;
    for (size_t s = 0; s < mSections.size(); ++s)
    {
      mSections[s].rebuild.store(false);
      Prepare(static_cast<int>(s));
      mSections[s].stage.ResetStopped(mRate, mBlock);
      latency += mSections[s].stage.Latency();
    }
    // Report the capture resampling delay; the MPC reads it when it prepares the insert.
    if (effect_) effect_->initialDelay = latency;
    mOutGain = -1.;
    mDcState.assign(mSections.size(), {0., 0.});
    mDcCoefficient = std::exp(-2. * 3.141592653589793 * 5. / mRate);
    mPrepared.store(true);
    mNamesDirty.store(true);
  }
  void reset() override { mOutGain = -1.; }

  void process(mpc::ProcessContext& ctx) override
  {
    const int frames = ctx.numFrames;
    float* outL = ctx.outputs[0];
    float* outR = ctx.numOutputs > 1 ? ctx.outputs[1] : nullptr;
    const float* inL = ctx.inputs && ctx.numInputs > 0 ? ctx.inputs[0] : nullptr;
    const float* inR = ctx.inputs && ctx.numInputs > 1 ? ctx.inputs[1] : inL;
    if (!mPrepared.load(std::memory_order_acquire) || mLeft.empty() || !inL)
    {
      if (outL && outL != inL) std::fill_n(outL, frames, 0.f);
      if (outR && outR != inR) std::fill_n(outR, frames, 0.f);
      return;
    }
    const double tempo = ctx.transport.tempoValid && ctx.transport.bpm > 0. ? ctx.transport.bpm : 120.;
    for (auto& section : mSections)
    {
      auto& c = section.controls;
      grid_modulation::VisitSlotControls(c, [&](int control, auto& field) {
        using T = std::remove_reference_t<decltype(field)>;
        field = grid_modulation::FieldValue<T>(section.values[control].load(std::memory_order_relaxed));
      });
      c.enabled = section.onParam < 0 || getBool(section.onParam);
      if (mNormalizeParam >= 0 && getBool(mNormalizeParam) && Current(section).effect == SlotEffect::NAM)
        if (const double loudness = section.loudness.load(); std::isfinite(loudness))
          c.trimDB += kNormalizedLoudness - loudness;
      c.utility.tempo = tempo;
      c.utility.constantDry = false;
      c.utility.trails = false;
    }
    const double outTarget = std::pow(10., get(mOutputParam) * .05);
    if (mOutGain < 0.) mOutGain = outTarget;
    for (int start = 0; start < frames;)
    {
      const int n = std::min(frames - start, mBlock);
      // Read the whole chunk before writing it: the MPC processes in place.
      for (int i = 0; i < n; ++i)
      {
        mLeft[i] = mSpec.mono ? .5 * (inL[start + i] + inR[start + i]) : inL[start + i];
        mRight[i] = inR[start + i];
      }
      for (size_t s = 0; s < mSections.size(); ++s)
      {
        auto& section = mSections[s];
        const auto& c = section.controls;
        double* right = mSpec.mono ? nullptr : mRight.data();
        section.stage.Process(mLeft.data(), n, c.enabled, c.inputDB, c.trimDB, c.dynamics, c.eq, c.utility, right);
        if (section.spec->dcBlock)
        {
          auto& state = mDcState[s];
          for (int i = 0; i < n; ++i)
          {
            const double x = mLeft[i];
            state[1] = x - state[0] + mDcCoefficient * state[1];
            state[0] = x;
            mLeft[i] = state[1];
          }
          if (!mSpec.mono) // Stereo plugins: the capture's output is mono, the same on both sides.
            std::copy_n(mLeft.data(), n, mRight.data());
        }
      }
      if (mSpec.mono) std::copy_n(mLeft.data(), n, mRight.data());
      const double outStep = (outTarget - mOutGain) / n;
      for (int i = 0; i < n; ++i)
      {
        mOutGain += outStep;
        const double l = mLeft[i] * mOutGain, r = mRight[i] * mOutGain;
        outL[start + i] = static_cast<float>(std::isfinite(l) ? std::clamp(l, -8., 8.) : 0.);
        if (outR) outR[start + i] = static_cast<float>(std::isfinite(r) ? std::clamp(r, -8., 8.) : 0.);
      }
      start += n;
    }
  }

  void parameterChanged(int index, float plainValue) override
  {
    if (mLoadingState.load()) return; // loadExtraState() applies the saved state as a whole.
    if (index == mRescanParam)
    {
      if (plainValue >= .5f) mRescan.store(true);
      return;
    }
    const auto role = mRoles[index];
    if (role.section < 0) return;
    auto& section = mSections[role.section];
    switch (role.role)
    {
      case Role::Type: SelectChoice(role.section, static_cast<int>(std::lround(plainValue))); return;
      case Role::Folder:
        section.folderNorm.store(getNormalised(index));
        section.fileDirty.store(false); // A later Folder turn supersedes an earlier pending File turn.
        section.folderDirty.store(true);
        mRescan.store(true); // Files copied while the plugin runs appear when browsing folders.
        return;
      case Role::File:
        section.fileNorm.store(getNormalised(index));
        section.fileDirty.store(true);
        return;
      case Role::Option: case Role::Knob: break;
      default: return;
    }
    const int control = ControlOf(index);
    if (control < 0) return;
    section.values[control].store(FromNormalized(control, getNormalised(index)));
    if (NeedsPreparation(control)) section.rebuild.store(true);
    if (ChangesLayout(control))
    {
      RefreshLayout(role.section);
      mNamesDirty.store(true);
    }
  }

  bool formatValue(int index, float plainValue, std::string& out) override
  {
    (void)plainValue;
    const auto role = mRoles[index];
    if (index == mOutputParam)
    {
      out = FormatNumber(plainValue, .1, "dB"); // The MPC shows no unit labels.
      return true;
    }
    if (role.section < 0 || role.role == Role::Type || role.role == Role::On) return false;
    auto& section = mSections[role.section];
    if (role.role == Role::Folder || role.role == Role::File)
    {
      std::lock_guard<std::mutex> lock(mControlMutex);
      out = Current(section).files == FileKind::None ? std::string()
            : role.role == Role::Folder              ? section.folderStatus
                                                     : section.fileStatus;
      return true;
    }
    const int control = ControlOf(index);
    out = control < 0 ? std::string() : FormatValue(control, section.values[control].load());
    return true;
  }

  void saveExtraState(std::vector<uint8_t>& out) override
  {
    nlohmann::json state;
    state["format"] = mSpec.stateFormat;
    state["schema"] = kStateSchema;
    auto& sections = state["sections"] = nlohmann::json::object();
    std::lock_guard<std::mutex> lock(mControlMutex);
    for (size_t s = 0; s < mSections.size(); ++s)
    {
      auto& section = mSections[s];
      // A save can arrive before the worker sees the latest Folder/File turn. Save that
      // selection by name now, and leave its preparation queued for the worker.
      if (section.folderDirty.load() || section.fileDirty.load())
      {
        ResolveFileLocked(static_cast<int>(s));
        section.rebuild.store(true);
      }
      const auto& choice = Current(section);
      nlohmann::json j;
      j["module"] = choice.key;
      auto values = LoadValues(section);
      values[standardControls + uNoiseLearn] = 0.; // Learning is transient.
      auto& named = j["values"] = nlohmann::json::object();
      for (int c = 0; c < slotControls; ++c)
        if (c != kSlotEnabled) named[ControlKey(c)] = values[c];
      if (choice.files != FileKind::None && !section.fileRelative.empty())
        j["file"] = {{"root", section.fileRoot}, {"path", section.fileRelative}};
      // The learned profile survives a switch to another gate, so keep it while it is valid.
      const auto profile = section.stage.GetNoiseProfile(section.noiseEpoch, section.noise);
      if (profile.valid) j["noise"] = profile.power;
      sections[section.spec->id] = std::move(j);
    }
    const auto text = state.dump();
    out.assign(text.begin(), text.end());
  }

  void loadExtraState(const uint8_t* data, size_t size) override
  {
    nlohmann::json state;
    try
    {
      state = nlohmann::json::parse(data, data + size);
    }
    catch (...)
    {
      return;
    }
    if (!ValidExtraState(state)) return;
    std::lock_guard<std::mutex> lock(mControlMutex);
    const auto& sections = state["sections"];
    for (size_t s = 0; s < mSections.size(); ++s)
    {
      auto& section = mSections[s];
      const auto it = sections.find(section.spec->id);
      const nlohmann::json j = it != sections.end() && it->is_object() ? *it : nlohmann::json::object();
      const std::string key = j.contains("module") && j["module"].is_string() ? j["module"].get<std::string>() : "";
      // Values first: a voice or style is identified by its module and style value.
      int choice = section.spec->defaultChoice;
      for (int c = 0; c < static_cast<int>(section.spec->choices.size()); ++c)
        if (key == section.spec->choices[c].key)
        {
          choice = c;
          break;
        }
      auto values = ChoiceDefaults(section.spec->choices[choice]);
      if (j.contains("values") && j["values"].is_object())
        for (int c = 0; c < slotControls; ++c)
          if (auto v = j["values"].find(ControlKey(c)); v != j["values"].end() && v->is_number())
          {
            const auto spec = mpc_grid::Spec(c);
            const double x = v->get<double>();
            if (std::isfinite(x)) values[c] = std::clamp(x, spec.minimum, spec.maximum);
          }
      for (int c = 0; c < static_cast<int>(section.spec->choices.size()); ++c)
      {
        const auto& candidate = section.spec->choices[c];
        if (key == candidate.key && candidate.styleControl >= 0 && std::lround(values[candidate.styleControl]) == candidate.style)
        {
          choice = c;
          break;
        }
      }
      values[standardControls + uNoiseLearn] = 0.;
      values[kSlotEnabled] = 1.;
      std::fill(section.rememberedValid.begin(), section.rememberedValid.end(), 0);
      section.choice.store(choice);
      StoreValues(section, values);
      section.fileRoot.clear();
      section.fileRelative.clear();
      if (j.contains("file") && j["file"].is_object())
      {
        section.fileRoot = j["file"].value("root", "");
        section.fileRelative = j["file"].value("path", "");
      }
      section.noise = {};
      if (j.contains("noise") && j["noise"].is_array() && j["noise"].size() == section.noise.power.size())
      {
        bool valid = true;
        for (size_t b = 0; b < section.noise.power.size(); ++b)
        {
          const auto& v = j["noise"][b];
          valid = valid && v.is_number() && std::isfinite(v.get<double>()) && v.get<double>() >= 0.;
          if (valid) section.noise.power[b] = v.get<double>();
        }
        section.noise.valid = valid;
        if (!valid) section.noise = {};
      }
      section.noiseReset.store(false);
      section.noiseEpoch = ++mNextNoiseEpoch; // Ignore audio snapshots until this project's profile is published.
      section.folderDirty.store(false);
      section.fileDirty.store(false);
      section.rebuild.store(true);
      if (section.typeParam >= 0)
        values_[section.typeParam]->store(param(section.typeParam).toNormalised(static_cast<float>(choice)));
      RefreshLayout(static_cast<int>(s));
      RefreshFileViewsLocked(static_cast<int>(s));
    }
    mExtraApplied = true;
    mNamesDirty.store(true);
  }

private:
  bool ValidExtraState(const nlohmann::json& state) const
  {
    if (!state.is_object() || !state.contains("format") || !state["format"].is_string()
        || state["format"] != mSpec.stateFormat || !state.contains("schema") || !state["schema"].is_number_integer()
        || state["schema"] < 1 || state["schema"] > kStateSchema || !state.contains("sections")
        || !state["sections"].is_object()) return false;
    for (const auto& section : mSections)
    {
      const auto it = state["sections"].find(section.spec->id);
      if (it == state["sections"].end()) continue; // Missing sections use their defaults.
      const auto& j = *it;
      if (!j.is_object() || (j.contains("module") && !j["module"].is_string())) return false;
      if (j.contains("values"))
      {
        if (!j["values"].is_object()) return false;
        for (int c = 0; c < slotControls; ++c)
          if (const auto v = j["values"].find(ControlKey(c)); v != j["values"].end())
            if (!v->is_number() || !std::isfinite(v->get<double>())) return false;
      }
      if (j.contains("file"))
      {
        const auto& f = j["file"];
        if (!f.is_object() || (f.contains("root") && !f["root"].is_string())
            || (f.contains("path") && !f["path"].is_string())) return false;
      }
      if (j.contains("noise"))
      {
        const auto& noise = j["noise"];
        if (!noise.is_array() || noise.size() != noise_print::Profile{}.power.size()) return false;
        for (const auto& v : noise)
          if (!v.is_number() || !std::isfinite(v.get<double>()) || v.get<double>() < 0.) return false;
      }
    }
    return true;
  }
  struct ParamRole
  {
    int section;
    Role role;
    int n;
  };
  struct Section
  {
    const SectionSpec* spec = nullptr;
    int typeParam = -1, onParam = -1, folderParam = -1, fileParam = -1;
    std::vector<int> optionParams, knobParams;
    std::vector<int> optionOrder; // Fixed option places (SectionOptionOrder).
    std::atomic<int> choice{0};
    std::array<std::atomic<double>, slotControls> values{};
    std::array<std::atomic<int>, kKnobs> knobs{};
    std::array<std::atomic<int>, kOptions> options{};
    std::atomic<bool> slimmable{false}, rebuild{true}, folderDirty{false}, fileDirty{false}, noiseReset{false};
    std::atomic<double> folderNorm{0.}, fileNorm{0.};
    std::atomic<double> loudness{std::numeric_limits<double>::quiet_NaN()}; // The loaded capture's, in dB.
    // Each module's last settings in this section, by the index of its first choice, so a
    // Type change and back (or a None voice in between) returns to them. Preallocated.
    std::vector<SlotValues> remembered;
    std::vector<char> rememberedValid;
    // Control data: worker and host UI threads, under mControlMutex.
    std::string fileRoot, fileRelative, filePath, fileStatus, folderStatus, preparedKey, failedPath;
    noise_print::Profile noise;
    uint64_t noiseEpoch = 0;
    NAMStage stage;
    FixedNAMGrid::Controls controls{}; // Audio thread only.
  };

  static const mpc_grid::Choice& Current(const Section& section)
  {
    const auto& choices = section.spec->choices;
    return choices[std::clamp(section.choice.load(), 0, static_cast<int>(choices.size()) - 1)];
  }
  static void StoreValues(Section& section, const SlotValues& values)
  {
    for (int c = 0; c < slotControls; ++c) section.values[c].store(values[c], std::memory_order_relaxed);
  }
  static SlotValues LoadValues(const Section& section)
  {
    SlotValues values{};
    for (int c = 0; c < slotControls; ++c) values[c] = section.values[c].load(std::memory_order_relaxed);
    return values;
  }
  int ControlOf(int index) const
  {
    const auto role = mRoles[index];
    if (role.section < 0) return -1;
    const auto& section = mSections[role.section];
    if (role.role == Role::Knob) return section.knobs[role.n].load();
    if (role.role == Role::Option) return section.options[role.n].load();
    return -1;
  }

  static int FirstChoiceOf(const Section& section, SlotEffect effect, FileKind files)
  {
    const auto& choices = section.spec->choices;
    for (int c = 0; c < static_cast<int>(choices.size()); ++c)
      if (choices[c].effect == effect && choices[c].files == files) return c;
    return 0;
  }
  // A Type edit. Another voice or style of the same module keeps the other settings, like
  // a pedal's voice switch. Another module returns to its last settings in this section, or
  // starts from its own starting settings.
  void SelectChoice(int s, int choiceIndex)
  {
    auto& section = mSections[s];
    choiceIndex = std::clamp(choiceIndex, 0, static_cast<int>(section.spec->choices.size()) - 1);
    if (choiceIndex == section.choice.load()) return;
    const auto& previous = Current(section);
    const auto& next = section.spec->choices[choiceIndex];
    section.choice.store(choiceIndex);
    if (next.effect == previous.effect && next.files == previous.files)
    {
      if (next.styleControl >= 0) section.values[next.styleControl].store(next.style);
    }
    else
    {
      const int from = FirstChoiceOf(section, previous.effect, previous.files);
      const int to = FirstChoiceOf(section, next.effect, next.files);
      if (!IsEmpty(previous))
      {
        section.remembered[from] = LoadValues(section);
        section.rememberedValid[from] = 1;
      }
      auto values = section.rememberedValid[to] ? section.remembered[to] : ChoiceDefaults(next);
      if (next.styleControl >= 0) values[next.styleControl] = next.style;
      values[standardControls + uNoiseLearn] = 0.;
      StoreValues(section, values);
      // A Smart Gate keeps its learned profile when another gate is tried in between.
      section.folderNorm.store(0.);
      section.fileNorm.store(0.);
      if (section.folderParam >= 0) values_[section.folderParam]->store(0.f);
      if (section.fileParam >= 0) values_[section.fileParam]->store(0.f);
      section.folderDirty.store(next.files != FileKind::None);
      section.rebuild.store(true);
    }
    RefreshLayout(s);
    mNamesDirty.store(true);
  }

  // Recompute which controls the knobs and options show, and point their host values at them.
  // Any thread; writes only atomics. Does not notify the host.
  void RefreshLayout(int s)
  {
    auto& section = mSections[s];
    const auto values = LoadValues(section);
    const auto layout = MakeLayout(Current(section), values, section.slimmable.load(), &section.optionOrder);
    for (size_t k = 0; k < section.knobParams.size(); ++k)
    {
      section.knobs[k].store(layout.knobs[k]);
      const int c = layout.knobs[k];
      values_[section.knobParams[k]]->store(c < 0 ? 0.f : static_cast<float>(ToNormalized(c, values[c])));
    }
    for (size_t o = 0; o < section.optionParams.size(); ++o)
    {
      section.options[o].store(layout.options[o]);
      const int c = layout.options[o];
      values_[section.optionParams[o]]->store(c < 0 ? 0.f : static_cast<float>(ToNormalized(c, values[c])));
    }
  }

  // After a parameter-only state (no extra state from this plugin): treat every value as an edit.
  void ApplyParametersAsEdits()
  {
    // RefreshLayout writes the host values. Keep the incoming values until options (which
    // can replace Time/Rate with Division) and then knobs have been interpreted.
    std::vector<float> incoming(numParams());
    for (int i = 0; i < numParams(); ++i) incoming[i] = getNormalised(i);
    for (size_t s = 0; s < mSections.size(); ++s)
    {
      auto& section = mSections[s];
      const int choice = section.typeParam >= 0 ? getInt(section.typeParam) : 0;
      section.choice.store(std::clamp(choice, 0, static_cast<int>(section.spec->choices.size()) - 1));
      std::fill(section.rememberedValid.begin(), section.rememberedValid.end(), 0);
      StoreValues(section, ChoiceDefaults(Current(section)));
      RefreshLayout(static_cast<int>(s));
      for (size_t o = 0; o < section.optionParams.size(); ++o)
        if (const int c = section.options[o].load(); c >= 0) section.values[c].store(FromNormalized(c, incoming[section.optionParams[o]]));
      RefreshLayout(static_cast<int>(s));
      for (size_t k = 0; k < section.knobParams.size(); ++k)
        if (const int c = section.knobs[k].load(); c >= 0) section.values[c].store(FromNormalized(c, incoming[section.knobParams[k]]));
      RefreshLayout(static_cast<int>(s));
      if (section.folderParam >= 0)
      {
        section.folderNorm.store(incoming[section.folderParam]);
        section.fileNorm.store(incoming[section.fileParam]);
        section.folderDirty.store(true);
        section.fileDirty.store(true);
      }
      section.rebuild.store(true);
    }
    mNamesDirty.store(true);
  }

  // ----- programs and the menu manifest ------------------------------------------------
  void AddPrograms()
  {
    for (const auto& program : mSpec.programs)
    {
      std::vector<float> values(numParams(), 0.f);
      for (size_t s = 0; s < mSections.size(); ++s)
      {
        const auto& section = mSections[s];
        const int choiceIndex = s < program.choices.size() ? program.choices[s] : section.spec->defaultChoice;
        const auto& choice = section.spec->choices[choiceIndex];
        const auto defaults = ChoiceDefaults(choice);
        const auto layout = MakeLayout(choice, defaults, false, &section.optionOrder);
        if (section.typeParam >= 0) values[section.typeParam] = static_cast<float>(choiceIndex);
        if (section.onParam >= 0)
        {
          const int on = s < program.on.size() ? program.on[s] : -1;
          values[section.onParam] = on < 0 ? (section.spec->defaultOn ? 1.f : 0.f) : static_cast<float>(on);
        }
        for (size_t o = 0; o < section.optionParams.size(); ++o)
          values[section.optionParams[o]] =
            layout.options[o] < 0 ? 0.f : static_cast<float>(ToNormalized(layout.options[o], defaults[layout.options[o]]));
        for (size_t k = 0; k < section.knobParams.size(); ++k)
          values[section.knobParams[k]] =
            layout.knobs[k] < 0 ? 0.f : static_cast<float>(ToNormalized(layout.knobs[k], defaults[layout.knobs[k]]));
      }
      addProgram(program.name, std::move(values));
    }
  }
  // The build's skin generator infers parameter kinds by probing the current display text,
  // which misreads long menus and module-dependent controls, so the plugin describes them
  // when NAM_GRID_MPC_MANIFEST names a file.
  void WriteManifest() const
  {
    const char* path = std::getenv("NAM_GRID_MPC_MANIFEST");
    if (!path || !*path) return;
    // "kinds": every parameter whose kind the probe could misread, by index: a list of
    // menu entries, or "float" for controls whose meaning follows the chosen module.
    nlohmann::json manifest;
    auto& kinds = manifest["kinds"] = nlohmann::json::object();
    for (const auto& section : mSections)
    {
      if (section.typeParam >= 0)
      {
        auto& names = kinds[std::to_string(section.typeParam)] = nlohmann::json::array();
        for (const auto& c : section.spec->choices) names.push_back(c.name);
      }
      if (section.folderParam >= 0)
      {
        kinds[std::to_string(section.folderParam)] = "float";
        kinds[std::to_string(section.fileParam)] = "float";
      }
      const auto describe = [&](int index, int place, bool option) {
        // The controls this place shows across every choice and tempo-sync state.
        std::vector<int> controls;
        for (const auto& choice : section.spec->choices)
          for (bool sync : {false, true})
          {
            auto values = ChoiceDefaults(choice);
            values[standardControls + uTempoSync] = sync ? 1. : 0.;
            const auto layout = MakeLayout(choice, values, true, &section.optionOrder);
            const int c = option ? layout.options[place] : layout.knobs[place];
            if (c >= 0 && std::find(controls.begin(), controls.end(), c) == controls.end()) controls.push_back(c);
          }
        const auto spec = controls.size() == 1 ? mpc_grid::Spec(controls[0]) : ControlSpec{};
        if (controls.size() != 1 || (spec.choices == 0 && !spec.toggle))
        {
          kinds[std::to_string(index)] = "float";
          return;
        }
        auto& names = kinds[std::to_string(index)] = nlohmann::json::array();
        const int count = spec.toggle ? 2 : spec.choices;
        for (int i = 0; i < count; ++i) names.push_back(FormatValue(controls[0], spec.minimum + i));
      };
      for (size_t o = 0; o < section.optionParams.size(); ++o) describe(section.optionParams[o], static_cast<int>(o), true);
      for (size_t k = 0; k < section.knobParams.size(); ++k) describe(section.knobParams[k], static_cast<int>(k), false);
    }
    // The plugin-wide Output is only offered when no module has an output of its own.
    bool ownOutput = false;
    for (const auto& section : mSpec.sections)
      for (const auto& choice : section.choices)
      {
        const auto values = ChoiceDefaults(choice);
        for (int c : MakeLayout(choice, values, true).knobs)
          if (c >= 0)
          {
            const auto label = ControlLabel(choice, c, values);
            ownOutput = ownOutput || label == "Output" || label == "Level";
          }
      }
    manifest["showOutput"] = !ownOutput;
    // "shown": for every section parameter, its section and the choices in which it has a
    // name (is visible). The skin hides a column's panel when its anchor control is hidden.
    auto& shown = manifest["shown"] = nlohmann::json::object();
    for (const auto& section : mSections)
    {
      const auto& choices = section.spec->choices;
      const auto record = [&](int index, const std::function<bool(int)>& visible) {
        if (index < 0) return;
        auto list = nlohmann::json::array();
        for (int c = 0; c < static_cast<int>(choices.size()); ++c)
          if (visible(c)) list.push_back(c);
        shown[std::to_string(index)] = {{"section", section.spec->id}, {"choices", list}};
      };
      const auto layoutOf = [&](int c, bool sync) {
        auto values = ChoiceDefaults(choices[c]);
        values[standardControls + uTempoSync] = sync ? 1. : 0.;
        return MakeLayout(choices[c], values, true, &section.optionOrder);
      };
      record(section.typeParam, [](int) { return true; });
      record(section.onParam, [](int) { return true; });
      record(section.folderParam, [&](int c) { return choices[c].files != FileKind::None; });
      record(section.fileParam, [&](int c) { return choices[c].files != FileKind::None; });
      for (size_t k = 0; k < section.knobParams.size(); ++k)
        record(section.knobParams[k], [&](int c) { return layoutOf(c, false).knobs[k] >= 0 || layoutOf(c, true).knobs[k] >= 0; });
      for (size_t o = 0; o < section.optionParams.size(); ++o)
        record(section.optionParams[o], [&](int c) { return layoutOf(c, false).options[o] >= 0 || layoutOf(c, true).options[o] >= 0; });
    }
    std::ofstream(path) << manifest.dump(2);
  }

  // ----- library and preparation (worker or prepare(), under mControlMutex) ----------
  std::vector<LibraryRoot> Roots() const
  {
    std::vector<LibraryRoot> roots;
    const auto& plugin = mpc::pluginFolder();
    if (const char* env = std::getenv("NAM_GRID_MPC_LIBRARY"); env && *env)
      roots.push_back({"user", env});
    else
      roots.push_back({"user", DriveLibraryFolder(plugin)});
    // Then a "NAM Grid" library on any other drive (SD card, USB, NVMe), labelled with its name.
    const auto own = PluginDrive(plugin);
    for (const auto& drive : OtherDrives())
      if (drive != own && !drive.empty())
        roots.push_back({"drive:" + DriveName(drive), drive + "/NAM Grid"});
    roots.push_back({"factory", plugin.empty() ? std::string() : plugin + "/Content"});
    return roots;
  }
  void ScanLibraryLocked()
  {
    bool captures = false, irs = false;
    for (const auto& section : mSpec.sections)
    {
      captures = captures || SectionFiles(section) == FileKind::Capture;
      irs = irs || SectionFiles(section) == FileKind::IR;
    }
    const auto roots = Roots();
    mCaptures = captures ? ScanLibrary(roots, "Captures", ".nam") : std::vector<LibraryEntry>{};
    mIRs = irs ? ScanLibrary(roots, "IRs", ".wav") : std::vector<LibraryEntry>{};
    mCaptureFolders = LibraryFolders(mCaptures);
    mIRFolders = LibraryFolders(mIRs);
  }
  const std::vector<LibraryEntry>& Entries(FileKind kind) const { return kind == FileKind::IR ? mIRs : mCaptures; }
  const std::vector<LibraryFolder>& Folders(FileKind kind) const { return kind == FileKind::IR ? mIRFolders : mCaptureFolders; }

  // Turns Folder/File edits, or the saved reference, into the file to load.
  void ResolveFileLocked(int s)
  {
    auto& section = mSections[s];
    const auto kind = Current(section).files;
    section.filePath.clear();
    if (kind == FileKind::None)
    {
      section.fileStatus.clear();
      section.folderStatus.clear();
      return;
    }
    const auto& entries = Entries(kind);
    const auto& folders = Folders(kind);
    const bool folderEdit = section.folderDirty.exchange(false), fileEdit = section.fileDirty.exchange(false);
    int chosen = -1;
    if (folderEdit || fileEdit || section.fileRelative.empty())
    {
      const int folder = IndexFromNormalized(section.folderNorm.load(), folders.size());
      if (folder >= 0)
      {
        // A new folder starts at its first file; a File edit steps within the folder.
        const int position = fileEdit ? IndexFromNormalized(section.fileNorm.load(), folders[folder].count) : 0;
        chosen = folders[folder].first + std::max(0, position);
      }
      section.fileRoot = chosen >= 0 ? entries[chosen].root : std::string();
      section.fileRelative = chosen >= 0 ? entries[chosen].relative : std::string();
    }
    const int index = FindEntry(entries, section.fileRoot, section.fileRelative);
    if (index >= 0)
    {
      section.filePath = entries[index].path;
      section.fileStatus = section.filePath == section.failedPath ? "Load failed" : entries[index].display;
    }
    else if (!section.fileRelative.empty())
      section.fileStatus = "Missing: " + Stem(section.fileRelative);
    else
      section.fileStatus = kind == FileKind::IR ? "No IRs" : "No captures";
    RefreshFileViewsLocked(s);
  }
  // Points Folder and File at the current file (or keeps the folder when it is missing).
  void RefreshFileViewsLocked(int s)
  {
    auto& section = mSections[s];
    if (section.folderParam < 0) return;
    const auto kind = Current(section).files;
    const auto& entries = Entries(kind);
    const auto& folders = Folders(kind);
    const int index = kind == FileKind::None ? -1 : FindEntry(entries, section.fileRoot, section.fileRelative);
    const int folder = index >= 0 ? FolderOfEntry(folders, index) : -1;
    section.folderStatus = folder >= 0 ? folders[folder].display
                           : section.fileRelative.empty() ? std::string()
                                                          : FolderDisplay(section.fileRoot, FolderOf(section.fileRelative));
    const double folderNorm = folder >= 0 ? NormalizedFromIndex(folder, folders.size()) : 0.;
    const double fileNorm = folder >= 0 ? NormalizedFromIndex(index - folders[folder].first, folders[folder].count) : 0.;
    section.folderNorm.store(folderNorm);
    section.fileNorm.store(fileNorm);
    values_[section.folderParam]->store(static_cast<float>(folderNorm));
    values_[section.fileParam]->store(static_cast<float>(fileNorm));
  }
  std::string PreparedKey(int s) const
  {
    const auto& section = mSections[s];
    const auto values = LoadValues(section);
    return std::to_string(static_cast<int>(Current(section).effect)) + "|" + section.filePath + "|"
           + std::to_string(values[kSlotFullQuality] >= .5) + "|"
           + std::to_string(std::lround(values[standardControls + uIRLength]));
  }
  // Builds and publishes the section's module; caller holds mPublishMutex (the stage's only
  // producer). The file is loaded without mControlMutex, so host UI calls stay responsive.
  // Throws nothing: a file that fails to load leaves the section dry.
  void Prepare(int s)
  {
    auto& section = mSections[s];
    SlotConfig config;
    std::string key;
    {
      std::lock_guard<std::mutex> lock(mControlMutex);
      ResolveFileLocked(s);
      if (section.noiseReset.exchange(false)) section.noise = {};
      // Keep a profile learned since the last save for the rebuilt Smart Gate.
      section.noise = section.stage.GetNoiseProfile(section.noiseEpoch, section.noise);
      config.effect = Current(section).effect;
      config.values = LoadValues(section);
      config.path = section.filePath;
      config.noiseProfile = section.noise;
      key = PreparedKey(s);
    }
    PreparedSlot prepared;
    bool failed = false;
    try
    {
      // Read captures here rather than in NAM Core: its existence check uses std::filesystem,
      // which fails in 32-bit builds for files with large inode numbers.
      std::unique_ptr<nam::DSP> model;
      if (config.effect == SlotEffect::NAM && !config.path.empty())
      {
        std::string text;
        if (!ReadWholeFile(config.path, text)) throw std::runtime_error("Cannot read the capture");
        model = nam::get_dsp(nlohmann::json::parse(text));
      }
      prepared = PrepareSlot(config, mRate, mBlock, std::move(model));
    }
    catch (...)
    {
      failed = !config.path.empty();
      config.path.clear();
      prepared = PrepareSlot(config, mRate, mBlock);
    }
    const auto info = prepared.dsp->Info();
    const bool slimmable = info.slimmable;
    section.loudness.store(info.hasLoudness ? info.loudness : std::numeric_limits<double>::quiet_NaN());
    {
      std::lock_guard<std::mutex> lock(mControlMutex);
      section.noiseEpoch = ++mNextNoiseEpoch;
      prepared.dsp->noiseEpoch = section.noiseEpoch;
      section.preparedKey = key;
      section.failedPath = failed ? section.filePath : std::string();
      if (failed) section.fileStatus = "Load failed";
    }
    section.stage.Publish(std::move(prepared.dsp));
    if (slimmable != section.slimmable.exchange(slimmable)) RefreshLayout(s);
  }

  void WorkerLoop()
  {
    using namespace std::chrono_literals;
    while (!mQuit.load())
    {
      std::this_thread::sleep_for(20ms);
      if (!mPrepared.load()) continue;
      bool changed = false;
      {
        std::lock_guard<std::mutex> publish(mPublishMutex);
        if (!mPrepared.load()) continue;
        if (mRescan.exchange(false))
        {
          std::lock_guard<std::mutex> lock(mControlMutex);
          ScanLibraryLocked();
          // A section with a pending Folder or File edit keeps it: the edit is resolved against
          // the new lists below, and only then do its views follow.
          for (size_t s = 0; s < mSections.size(); ++s)
            if (!mSections[s].folderDirty.load() && !mSections[s].fileDirty.load()) RefreshFileViewsLocked(static_cast<int>(s));
          if (mRescanParam >= 0) values_[mRescanParam]->store(0.f);
          changed = true;
        }
        for (size_t s = 0; s < mSections.size() && !mQuit.load(); ++s)
        {
          auto& section = mSections[s];
          const bool fileEdit = section.folderDirty.load() || section.fileDirty.load();
          const bool rebuild = section.rebuild.exchange(false);
          if (!fileEdit && !rebuild && !changed) continue;
          bool needed = rebuild;
          {
            std::lock_guard<std::mutex> lock(mControlMutex);
            const auto previous = section.fileStatus + "|" + section.folderStatus;
            ResolveFileLocked(static_cast<int>(s));
            needed = needed || PreparedKey(static_cast<int>(s)) != section.preparedKey;
            changed = changed || section.fileStatus + "|" + section.folderStatus != previous;
          }
          if (needed)
          {
            Prepare(static_cast<int>(s));
            changed = true;
          }
        }
        for (auto& section : mSections) section.stage.Collect();
      }
      if (mNamesDirty.exchange(false) || changed) requestDisplayUpdate();
    }
  }

  const PluginSpec& mSpec;
  std::vector<Section> mSections;
  std::vector<ParamRole> mRoles;
  int mOutputParam = 0, mRescanParam = -1, mNormalizeParam = -1;
  std::vector<std::array<double, 2>> mDcState; // Per section: the DC blocker's input and output memory.
  double mDcCoefficient = .9993;
  std::vector<double> mLeft, mRight;
  double mRate = 44100., mOutGain = -1.;
  int mBlock = 128;
  std::atomic<bool> mPrepared{false}, mQuit{false}, mRescan{false}, mNamesDirty{false}, mLoadingState{false};
  bool mExtraApplied = false, mStaticNames = false;
  std::mutex mPublishMutex; // Serialises module preparation and publishing (prepare() and the worker).
  std::mutex mControlMutex; // Guards file references, library lists and status text.
  std::vector<LibraryEntry> mCaptures, mIRs;
  std::vector<LibraryFolder> mCaptureFolders, mIRFolders;
  uint64_t mNextNoiseEpoch = 0;
  std::thread mWorker;
};

// Dynamic parameter names and the state-load bracket, around the SDK dispatcher.
inline VstIntPtr Dispatcher(AEffect* e, VstInt32 opcode, VstInt32 index, VstIntPtr value, void* ptr, float opt)
{
  auto* plugin = static_cast<GridPlugin*>(mpc::detail::self(e));
  if (plugin != nullptr && index >= 0 && index < plugin->numParams())
  {
    if (opcode == effGetParamName && ptr != nullptr)
    {
      mpc::copyString(static_cast<char*>(ptr), 32, plugin->ParameterName(index).c_str());
      return 0;
    }
    if (opcode == effGetParameterProperties && ptr != nullptr)
    {
      const auto result = mpc::detail::dispatcher(e, opcode, index, value, ptr, opt);
      auto* properties = static_cast<VstParameterProperties*>(ptr);
      const auto name = plugin->ParameterName(index);
      mpc::copyString(properties->label, sizeof(properties->label), name.c_str());
      mpc::copyString(properties->shortLabel, sizeof(properties->shortLabel), name.c_str());
      return result;
    }
  }
  if (plugin != nullptr && opcode == effSetChunk)
  {
    if (value < 0 || !plugin->CanLoadState(static_cast<const uint8_t*>(ptr), static_cast<size_t>(value))) return 0;
    plugin->BeginStateLoad();
    const auto result = mpc::detail::dispatcher(e, opcode, index, value, ptr, opt);
    plugin->EndStateLoad(result != 0);
    return result;
  }
  return mpc::detail::dispatcher(e, opcode, index, value, ptr, opt);
}

inline AEffect* CreateEffect(const PluginSpec& spec, audioMasterCallback host)
{
  if (host != nullptr && host(nullptr, audioMasterVersion, 0, 0, nullptr, 0.0f) == 0) return nullptr;
  AEffect* effect = mpc::detail::createEffect(new GridPlugin(spec), host);
  if (effect != nullptr) effect->dispatcher = &Dispatcher;
  return effect;
}
} // namespace mpc_grid

// Ends a plugin's source file: `SpecFunction` returns its PluginSpec (a function-local static).
#if defined(MPC_GRID_NO_ENTRY)
  #define MPC_GRID_PLUGIN(SpecFunction)
#else
  #define MPC_GRID_PLUGIN(SpecFunction)                                                    \
    extern "C" MPC_EXPORT AEffect* VSTPluginMain(audioMasterCallback host)               \
    {                                                                                    \
      return mpc_grid::CreateEffect(SpecFunction(), host);                               \
    }
#endif
