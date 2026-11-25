#include "NeuralAmpModeler.h"
#include "A2Fixture.h"
#include "ClapTestHost.h"
#include "IsolatedUserHome.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <thread>
#include <vector>

extern "C" const clap_plugin_entry clap_entry;

struct PluginLifecycleTestAccessor
{
  static NeuralAmpModeler* FromClap(const clap_plugin* plugin)
  {
    return static_cast<NeuralAmpModeler*>(static_cast<iplug::ClapPluginHelper*>(plugin->plugin_data));
  }
  static int EditGrid(const NeuralAmpModeler& plug) { return plug.mEditGrid; }
  static int AudibleGrid(const NeuralAmpModeler& plug) { return plug.mAudibleGrid; }
  static int OutgoingGrid(const NeuralAmpModeler& plug) { return plug.mOutgoingGrid; }
  static uint64_t ResetEpoch(const NeuralAmpModeler& plug) { return plug.mResetEpoch; }
  static double SampleRatePrepared(const NeuralAmpModeler& plug) { return plug.mSampleRatePrepared; }
  static double MasterCoefficient(const NeuralAmpModeler& plug) { return plug.mMasterCoefficient; }
  static const std::unique_ptr<PreparedSlot>& PreviewOriginal(const NeuralAmpModeler& plug)
  {
    return plug.mPreviewOriginal;
  }
  static void SetRawSampleRate(NeuralAmpModeler& plug, double rate) { plug.SetSampleRate(rate); }
  static void SetSlotPath(NeuralAmpModeler& plug, size_t slot, const char* text) { plug.mSlotPaths[slot].Set(text); }
  static void SetPresetName(NeuralAmpModeler& plug, std::string name) { plug.mPresetName = std::move(name); }
  static nlohmann::json CurrentRig(const NeuralAmpModeler& plug) { return plug._CurrentRig(); }
  static void ApplyRig(NeuralAmpModeler& plug, nlohmann::json rig, const std::string& name, const std::string& dir)
  {
    const auto guard = plug._LockControls();
    plug._ApplyRig(std::move(rig), name, dir);
  }
  static const noise_print::Profile& SavedNoiseProfile(const NeuralAmpModeler& plug, size_t slot)
  {
    return plug.mNoiseProfiles[slot];
  }
  static bool RigIdle(const NeuralAmpModeler& plug) { return plug.mRigTransition.Idle(); }
  static bool RigStarted(const NeuralAmpModeler& plug) { return plug.mRigTransition.Started(); }
  template <typename StopAudioFn>
  static void SimulateMidApplyReentrantReset(NeuralAmpModeler& plug, StopAudioFn&& stopAudio)
  {
    // Mirrors _ApplyRig's epoch-guarded block when OnReset() arrives mid-staging.
    const auto guard = plug._LockControls();
    const uint64_t resetEpoch = plug.mResetEpoch;
    const bool switching = plug.mRigTransition.Hold(std::chrono::milliseconds(250));
    assert(switching);
    plug.mEditGrid = 1 - plug.mEditGrid;
    // Host stops audio before invoking OnReset() (e.g., VST3 restartComponent -> setProcessing(false)/setupProcessing).
    stopAudio();
    plug.OnReset();
    assert(plug.mResetEpoch != resetEpoch);
    if (switching && plug.mResetEpoch == resetEpoch)
      assert(false && "Epoch guard should prevent ResetStopped/Start after re-entrant OnReset");
  }
};

namespace
{
class ScopedAudioThread
{
public:
  explicit ScopedAudioThread(const clap_plugin* plugin) : mPlugin(plugin)
  {
    mLeft.fill(0.);
    mRight.fill(0.);
    mThread = std::thread([this] {
      double* in[]{mLeft.data(), mRight.data()};
      double* out[]{mOutLeft.data(), mOutRight.data()};
      clap_audio_buffer input{nullptr, in, 2, 0, 0}, output{nullptr, out, 2, 0, 0};
      clap_input_events inEvents{
        nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
        [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }};
      clap_output_events outEvents{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
      clap_process process{};
      process.audio_inputs = &input;
      process.audio_outputs = &output;
      process.audio_inputs_count = process.audio_outputs_count = 1;
      process.in_events = &inEvents;
      process.out_events = &outEvents;
      process.frames_count = 64;
      while (!mStop.load(std::memory_order_acquire))
      {
        mPlugin->process(mPlugin, &process);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
      }
    });
  }
  ~ScopedAudioThread()
  {
    mStop.store(true, std::memory_order_release);
    if (mThread.joinable())
      mThread.join();
  }

private:
  const clap_plugin* mPlugin;
  std::atomic<bool> mStop{false};
  std::array<double, 64> mLeft{}, mRight{}, mOutLeft{}, mOutRight{};
  std::thread mThread;
};

void WaitForRigIdle(const NeuralAmpModeler& plug)
{
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
  while (!PluginLifecycleTestAccessor::RigIdle(plug) && std::chrono::steady_clock::now() < deadline)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  assert(PluginLifecycleTestAccessor::RigIdle(plug));
}
} // namespace

int main(int argc, char** argv)
{
  std::cout << std::unitbuf;
  assert(argc >= 2);
  (void)argv;
  IsolatedUserHome isolated("nam-plugin-lifecycle");
  const auto fixtures = isolated.home / "fixtures";
  std::filesystem::create_directories(fixtures);
  const auto model1 = fixtures / "a2-1.nam";
  const auto model2 = fixtures / "a2-2.nam";
  std::ofstream(model1) << fixture::model(3, 123).dump();
  std::ofstream(model2) << fixture::model(8, 456).dump();

  assert(clap_entry.init("nam-plugin-tests"));
  auto* factory = static_cast<const clap_plugin_factory*>(clap_entry.get_factory(CLAP_PLUGIN_FACTORY_ID));
  assert(factory && factory->get_plugin_count(factory) == 1);
  const auto* descriptor = factory->get_plugin_descriptor(factory, 0);

  Host host;
  auto* clapPlugin = factory->create_plugin(factory, &host.host, descriptor->id);
  assert(clapPlugin && clapPlugin->init(clapPlugin));
  auto* plug = PluginLifecycleTestAccessor::FromClap(clapPlugin);
  auto* state = static_cast<const clap_plugin_state*>(clapPlugin->get_extension(clapPlugin, CLAP_EXT_STATE));
  auto* latency = static_cast<const clap_plugin_latency*>(clapPlugin->get_extension(clapPlugin, CLAP_EXT_LATENCY));
  assert(state && latency);

  // Clean user preferences (no default folders) so tests start deterministically.
  {
    nam_library::Settings settings;
    settings.showVisualizations = true;
    settings.presetCrossfade = false;
    assert(nam_library::WriteSettings(isolated.file, settings));
  }

  const auto saveStream = [&] {
    Stream s;
    clap_ostream out{&s, Stream::write};
    assert(state->save(clapPlugin, &out));
    return s;
  };
  const auto loadRawChunk = [&](const iplug::IByteChunk& chunk) {
    return plug->UnserializeState(chunk, 0);
  };

  // --- Item 4a: UnserializeState preserves active preview when chunk validation fails ---
  std::cout << "[plugin-lifecycle] Item 4a\n";
  {
    assert(plug->BeginSlotPreview(0).empty());
    assert(plug->PreviewSlot(SlotEffect::Boost, "").empty());
    assert(PluginLifecycleTestAccessor::PreviewOriginal(*plug) != nullptr);
    assert(plug->GetSlotEffect(0) == SlotEffect::Boost);

    iplug::IByteChunk badJson;
    badJson.PutStr("###nam-grid###");
    badJson.PutStr("{not valid json");
    assert(loadRawChunk(badJson) < 0);
    assert(PluginLifecycleTestAccessor::PreviewOriginal(*plug) != nullptr);
    assert(plug->GetSlotEffect(0) == SlotEffect::Boost);

    auto badSchemaJson = PluginLifecycleTestAccessor::CurrentRig(*plug);
    badSchemaJson["schema"] = 99999;
    iplug::IByteChunk badSchema;
    badSchema.PutStr("###nam-grid###");
    badSchema.PutStr(badSchemaJson.dump().c_str());
    assert(loadRawChunk(badSchema) < 0);
    assert(PluginLifecycleTestAccessor::PreviewOriginal(*plug) != nullptr);
    assert(plug->GetSlotEffect(0) == SlotEffect::Boost);

    auto deepText = PluginLifecycleTestAccessor::CurrentRig(*plug).dump();
    deepText.insert(deepText.size() - 1, std::string(",\"extra\":") + std::string(140, '[') + "0" + std::string(140, ']'));
    iplug::IByteChunk deepChunk;
    deepChunk.PutStr("###nam-grid###");
    deepChunk.PutStr(deepText.c_str());
    assert(loadRawChunk(deepChunk) < 0);
    assert(PluginLifecycleTestAccessor::PreviewOriginal(*plug) != nullptr);
    assert(plug->GetSlotEffect(0) == SlotEffect::Boost);

    plug->EndSlotPreview(false);
    assert(PluginLifecycleTestAccessor::PreviewOriginal(*plug) == nullptr);
    assert(plug->GetSlotEffect(0) == SlotEffect::NAM);
  }

  // --- Item 4b: _ApplyRig preserves mEditGrid & preview on invalid rig, cancels preview before Hold()/flip on valid rig ---
  std::cout << "[plugin-lifecycle] Item 4b\n";
  {
    assert(clapPlugin->activate(clapPlugin, 48000., 1, 64));
    assert(clapPlugin->start_processing(clapPlugin));
    {
      ScopedAudioThread audio(clapPlugin);
      const int initialGrid = PluginLifecycleTestAccessor::EditGrid(*plug);
      assert(plug->BeginSlotPreview(0).empty());
      assert(plug->PreviewSlot(SlotEffect::Boost, "").empty());

      auto invalidRig = PluginLifecycleTestAccessor::CurrentRig(*plug);
      invalidRig["schema"] = 99999;
      bool threw = false;
      try
      {
        PluginLifecycleTestAccessor::ApplyRig(*plug, invalidRig, "Bad", "");
      }
      catch (const std::exception&)
      {
        threw = true;
      }
      assert(threw);
      assert(PluginLifecycleTestAccessor::EditGrid(*plug) == initialGrid);
      assert(PluginLifecycleTestAccessor::PreviewOriginal(*plug) != nullptr);
      assert(plug->GetSlotEffect(0) == SlotEffect::Boost);

      const auto validEmpty = PluginLifecycleTestAccessor::CurrentRig(*plug);
      PluginLifecycleTestAccessor::ApplyRig(*plug, validEmpty, "Valid", "");
      WaitForRigIdle(*plug);
      assert(PluginLifecycleTestAccessor::EditGrid(*plug) == 1 - initialGrid);
      assert(PluginLifecycleTestAccessor::PreviewOriginal(*plug) == nullptr);
      assert(plug->GetSlotEffect(0) == SlotEffect::NAM);
    }
    clapPlugin->stop_processing(clapPlugin);
    clapPlugin->deactivate(clapPlugin);
  }

  // --- Item 4c: SerializeState / CLAP stateSave safely returns false on non-UTF-8 strings ---
  std::cout << "[plugin-lifecycle] Item 4c\n";
  {
    const auto saved = saveStream();
    PluginLifecycleTestAccessor::SetSlotPath(*plug, 0, "/tmp/\xFF\xFE/bad.nam");
    iplug::IByteChunk chunk;
    assert(!plug->SerializeState(chunk));
    assert(chunk.Size() == 0);

    Stream badOut;
    clap_ostream stream{&badOut, Stream::write};
    assert(!state->save(clapPlugin, &stream));
    assert(badOut.bytes.empty());

    PluginLifecycleTestAccessor::SetSlotPath(*plug, 0, "");
    PluginLifecycleTestAccessor::SetPresetName(*plug, "Bad\xFFName");
    iplug::IByteChunk chunk2;
    assert(!plug->SerializeState(chunk2));
    assert(chunk2.Size() == 0);

    clap_istream restoreIn{const_cast<Stream*>(&saved), Stream::read};
    assert(state->load(clapPlugin, &restoreIn));
  }

  // --- Item 2: Deferred latency during _ApplyRig / UnserializeState & mResetEpoch guard ---
  std::cout << "[plugin-lifecycle] Item 2\n";
  {
    auto twoNamRigA = PluginLifecycleTestAccessor::CurrentRig(*plug);
    twoNamRigA["Lane Mode"] = grid_performance::Parallel;
    twoNamRigA["SlotEffects"][4] = static_cast<int>(SlotEffect::NAM);
    twoNamRigA["SlotPaths"][4] = nam_library::PathText(model1);
    twoNamRigA["SlotHashes"][4] = nam_library::ContentHash(model1);

    auto twoNamRigB = PluginLifecycleTestAccessor::CurrentRig(*plug);
    twoNamRigB["Lane Mode"] = grid_performance::Parallel;
    twoNamRigB["SlotEffects"][8] = static_cast<int>(SlotEffect::NAM);
    twoNamRigB["SlotPaths"][8] = nam_library::PathText(model2);
    twoNamRigB["SlotHashes"][8] = nam_library::ContentHash(model2);

    assert(clapPlugin->activate(clapPlugin, 44100., 1, 64));
    assert(clapPlugin->start_processing(clapPlugin));
    host.callback = host.restart = false;
    {
      // Load via _ApplyRig while active; slot 4 (Lane A) adds resampling latency.
      PluginLifecycleTestAccessor::ApplyRig(*plug, twoNamRigA, "TwoNAM_A", "");
      const uint32_t expectedLatency = latency->get(clapPlugin);
      assert(expectedLatency > 0);
      // Now switch from Lane A to Lane B (same resampling latency) while audio runs;
      // _ClearModel(0..15) must NOT transiently drop GetLatency() to 0.
      ScopedAudioThread audio(clapPlugin);
      if (host.callback)
      {
        host.callback = false;
        clapPlugin->on_main_thread(clapPlugin);
      }
      host.callback = host.restart = false;
      std::atomic<bool> checkLatency{true};
      uint32_t minLatency = expectedLatency;
      std::thread watcher([&] {
        while (checkLatency.load(std::memory_order_acquire))
          minLatency = std::min(minLatency, latency->get(clapPlugin));
      });
      PluginLifecycleTestAccessor::ApplyRig(*plug, twoNamRigB, "TwoNAM_B", "");
      checkLatency.store(false, std::memory_order_release);
      watcher.join();
      WaitForRigIdle(*plug);
      if (host.callback)
      {
        host.callback = false;
        clapPlugin->on_main_thread(clapPlugin);
      }
      assert(minLatency == expectedLatency);
      assert(latency->get(clapPlugin) == expectedLatency);
      // switching == true ran ResetStopped on mEditGrid, so CLAP did not need to request a restart.
      assert(!host.restart);
    }
    // Verify OnReset() increments mResetEpoch and prevents mid-staging ResetStopped/Start.
    {
      auto audio = std::make_unique<ScopedAudioThread>(clapPlugin);
      const uint64_t before = PluginLifecycleTestAccessor::ResetEpoch(*plug);
      PluginLifecycleTestAccessor::SimulateMidApplyReentrantReset(*plug, [&] { audio.reset(); });
      assert(PluginLifecycleTestAccessor::ResetEpoch(*plug) == before + 1);
      assert(PluginLifecycleTestAccessor::AudibleGrid(*plug) == PluginLifecycleTestAccessor::EditGrid(*plug));
      assert(PluginLifecycleTestAccessor::OutgoingGrid(*plug) == PluginLifecycleTestAccessor::AudibleGrid(*plug));
      assert(PluginLifecycleTestAccessor::RigIdle(*plug));
    }
    clapPlugin->stop_processing(clapPlugin);
    clapPlugin->deactivate(clapPlugin);
  }

  // --- Item 5: RigTransition freezes outgoing InputGain / OutputLevel ---
  std::cout << "[plugin-lifecycle] Item 5\n";
  {
    plug->SetPresetCrossfade(false); // Dip: 12 ms out = 576 samples @ 48 kHz; first 256-frame block is 100% outgoing.
    auto rigA = PluginLifecycleTestAccessor::CurrentRig(*plug);
    for (int s = 0; s < 16; ++s)
    {
      rigA["SlotEffects"][s] = static_cast<int>(SlotEffect::NAM);
      rigA["SlotPaths"][s] = "";
      rigA["SlotHashes"][s] = "";
    }
    rigA["Input"] = -20.;
    rigA["Output"] = -20.;

    auto rigB = rigA;
    rigB["Input"] = 20.;
    rigB["Output"] = 20.;

    PluginLifecycleTestAccessor::ApplyRig(*plug, rigA, "Quiet", "");
    assert(clapPlugin->activate(clapPlugin, 48000., 1, 256));
    assert(clapPlugin->start_processing(clapPlugin));

    std::array<double, 256> inL{}, inR{}, outL{}, outR{};
    double* inPtrs[]{inL.data(), inR.data()};
    double* outPtrs[]{outL.data(), outR.data()};
    clap_audio_buffer inBuf{nullptr, inPtrs, 2, 0, 0}, outBuf{nullptr, outPtrs, 2, 0, 0};
    clap_input_events inEv{
      nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
      [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }};
    clap_output_events outEv{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
    clap_process proc{};
    proc.audio_inputs = &inBuf;
    proc.audio_outputs = &outBuf;
    proc.audio_inputs_count = proc.audio_outputs_count = 1;
    proc.in_events = &inEv;
    proc.out_events = &outEv;
    proc.frames_count = 256;

    std::atomic<bool> stopAudio{false};
    std::atomic<double> firstTransitionBlockPeak{-1.};
    std::atomic<bool> sawTransition{false};
    std::thread audio([&] {
      int pos = 0;
      while (!stopAudio.load(std::memory_order_acquire))
      {
        for (int i = 0; i < 256; ++i)
          inL[i] = inR[i] = std::sin((pos + i) * 0.05);
        pos += 256;
        clapPlugin->process(clapPlugin, &proc);
        if (PluginLifecycleTestAccessor::RigStarted(*plug) && !sawTransition.exchange(true))
        {
          double peak = 0.;
          for (double s : outL)
            peak = std::max(peak, std::abs(s));
          firstTransitionBlockPeak.store(peak, std::memory_order_release);
        }
        std::this_thread::sleep_for(std::chrono::microseconds(200));
      }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(15));
    PluginLifecycleTestAccessor::ApplyRig(*plug, rigB, "Loud", "");
    WaitForRigIdle(*plug);
    stopAudio.store(true, std::memory_order_release);
    audio.join();

    const double peak = firstTransitionBlockPeak.load();
    assert(peak >= 0. && peak < 0.025); // Steady-state Rig A is 0.01; un-frozen Rig B gain would be ~100.0.
    clapPlugin->stop_processing(clapPlugin);
    clapPlugin->deactivate(clapPlugin);
  }

  // --- Item 7: mRigTransition.Started() resets mPedalActiveCC / mPedalKnob ---
  std::cout << "[plugin-lifecycle] Item 7\n";
  {
    auto pedalRig = PluginLifecycleTestAccessor::CurrentRig(*plug);
    for (int s = 0; s < 16; ++s)
    {
      pedalRig["SlotEffects"][s] = static_cast<int>(SlotEffect::NAM);
      pedalRig["SlotPaths"][s] = "";
      pedalRig["SlotHashes"][s] = "";
    }
    pedalRig["Input"] = 0.;
    pedalRig["Output"] = 0.;
    pedalRig["Lane Mode"] = grid_performance::Sequential;
    pedalRig["SlotEffects"][0] = static_cast<int>(SlotEffect::VolumePedal);
    pedalRig[std::string("Pre 1 ") + kEffectParameters[uVolumePosition].name] = 100.;
    pedalRig[std::string("Pre 1 ") + kEffectParameters[uVolumeCC].name] = 11.;
    pedalRig[std::string("Pre 1 ") + kEffectParameters[uVolumeSmoothing].name] = 1.;

    PluginLifecycleTestAccessor::ApplyRig(*plug, pedalRig, "PedalA", "");
    assert(clapPlugin->activate(clapPlugin, 48000., 1, 256));
    assert(clapPlugin->start_processing(clapPlugin));

    std::array<double, 256> inL{}, inR{}, outL{}, outR{};
    double* inPtrs[]{inL.data(), inR.data()};
    double* outPtrs[]{outL.data(), outR.data()};
    clap_audio_buffer inBuf{nullptr, inPtrs, 2, 0, 0}, outBuf{nullptr, outPtrs, 2, 0, 0};
    clap_event_midi heel{};
    heel.header.size = sizeof(heel);
    heel.header.type = CLAP_EVENT_MIDI;
    heel.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    heel.data[0] = 0xB0;
    heel.data[1] = 11;
    heel.data[2] = 0;
    std::atomic<bool> sendHeel{true};
    clap_input_events inEv{
      &sendHeel,
      [](const clap_input_events* e) -> uint32_t {
        return static_cast<std::atomic<bool>*>(e->ctx)->exchange(false) ? 1u : 0u;
      },
      nullptr};
    inEv.get = [](const clap_input_events*, uint32_t) -> const clap_event_header* {
      static const clap_event_midi ev = [] {
        clap_event_midi m{};
        m.header.size = sizeof(m);
        m.header.type = CLAP_EVENT_MIDI;
        m.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
        m.data[0] = 0xB0;
        m.data[1] = 11;
        m.data[2] = 0;
        return m;
      }();
      return &ev.header;
    };
    clap_output_events outEv{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
    clap_process proc{};
    proc.audio_inputs = &inBuf;
    proc.audio_outputs = &outBuf;
    proc.audio_inputs_count = proc.audio_outputs_count = 1;
    proc.in_events = &inEv;
    proc.out_events = &outEv;
    proc.frames_count = 256;

    std::atomic<bool> stopAudio{false};
    std::atomic<double> settledRms{0.};
    std::thread audio([&] {
      int pos = 0;
      while (!stopAudio.load(std::memory_order_acquire))
      {
        for (int i = 0; i < 256; ++i)
          inL[i] = inR[i] = 0.2 * std::sin((pos + i) * 0.05);
        pos += 256;
        clapPlugin->process(clapPlugin, &proc);
        double sum = 0.;
        for (double s : outL)
          sum += s * s;
        settledRms.store(std::sqrt(sum / 256.), std::memory_order_release);
        std::this_thread::sleep_for(std::chrono::microseconds(200));
      }
    });
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    assert(settledRms.load() < 0.005); // Rig A muted by CC 11 = 0.

    PluginLifecycleTestAccessor::ApplyRig(*plug, pedalRig, "PedalB", "");
    WaitForRigIdle(*plug);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const double postSwitchRms = settledRms.load();
    stopAudio.store(true, std::memory_order_release);
    audio.join();

    // Rig B has Position = 100% and received no MIDI events, so it must play near unity (~0.1414 RMS).
    assert(postSwitchRms > 0.10);
    clapPlugin->stop_processing(clapPlugin);
    clapPlugin->deactivate(clapPlugin);
  }

  // --- Item 8: Per-grid mNoiseSeen tracking across grid switches ---
  std::cout << "[plugin-lifecycle] Item 8\n";
  {
    auto noiseRig = PluginLifecycleTestAccessor::CurrentRig(*plug);
    for (int s = 0; s < 16; ++s)
    {
      noiseRig["SlotEffects"][s] = static_cast<int>(SlotEffect::NAM);
      noiseRig["SlotPaths"][s] = "";
      noiseRig["SlotHashes"][s] = "";
    }
    noiseRig["SlotEffects"][0] = static_cast<int>(SlotEffect::NoisePrint);
    noiseRig[std::string("Pre 1 ") + kEffectParameters[uNoiseLearn].name] = 0.;

    PluginLifecycleTestAccessor::ApplyRig(*plug, noiseRig, "NoiseA", "");
    assert(clapPlugin->activate(clapPlugin, 48000., 1, 256));
    assert(clapPlugin->start_processing(clapPlugin));

    std::array<double, 256> inL{}, inR{}, outL{}, outR{};
    double* inPtrs[]{inL.data(), inR.data()};
    double* outPtrs[]{outL.data(), outR.data()};
    clap_audio_buffer inBuf{nullptr, inPtrs, 2, 0, 0}, outBuf{nullptr, outPtrs, 2, 0, 0};
    clap_input_events inEv{
      nullptr, [](const clap_input_events*) -> uint32_t { return 0; },
      [](const clap_input_events*, uint32_t) -> const clap_event_header* { return nullptr; }};
    clap_output_events outEv{nullptr, [](const clap_output_events*, const clap_event_header*) { return true; }};
    clap_process proc{};
    proc.audio_inputs = &inBuf;
    proc.audio_outputs = &outBuf;
    proc.audio_inputs_count = proc.audio_outputs_count = 1;
    proc.in_events = &inEv;
    proc.out_events = &outEv;
    proc.frames_count = 256;

    const auto learnAndSync = [&](double amplitude) {
      const int learnParam = SlotUtilityParam(0, uNoiseLearn);
      plug->SendParameterValueFromUI(learnParam, 1.);
      for (int b = 0; b < 80; ++b)
      {
        for (int i = 0; i < 256; ++i)
          inL[i] = inR[i] = amplitude * std::sin((b * 256 + i) * 0.13);
        clapPlugin->process(clapPlugin, &proc);
      }
      plug->SendParameterValueFromUI(learnParam, 0.);
      for (int b = 0; b < 4; ++b)
      {
        for (int i = 0; i < 256; ++i)
          inL[i] = inR[i] = amplitude * std::sin((b * 256 + i) * 0.13);
        clapPlugin->process(clapPlugin, &proc);
      }
    };

    // On Grid 0: initial Reset() publishes seq=2; learn once -> seq=4, OnIdle => mNoiseSeen[0] == 4.
    for (int i = 0; i < 256; ++i)
      inL[i] = inR[i] = 0.;
    clapPlugin->process(clapPlugin, &proc);
    plug->OnIdle();
    learnAndSync(0.03);
    plug->OnIdle();
    const auto grid0Profile = PluginLifecycleTestAccessor::SavedNoiseProfile(*plug, 0);
    assert(grid0Profile.valid);

    // Switch via _ApplyRig with audio running so mEditGrid flips to Grid 1!
    {
      ScopedAudioThread audio(clapPlugin);
      PluginLifecycleTestAccessor::ApplyRig(*plug, noiseRig, "NoiseB", "");
      WaitForRigIdle(*plug);
    }
    // Reset() on Grid 1 publishes seq=2; learn a louder profile -> seq=4 (collides with mNoiseSeen[0]==4 on main!).
    learnAndSync(0.10);
    host.dirty = false;
    plug->OnIdle();
    assert(host.dirty);
    const auto grid1Profile = PluginLifecycleTestAccessor::SavedNoiseProfile(*plug, 0);
    assert(grid1Profile.valid && !(grid1Profile == grid0Profile));

    // Test 8b: Switch back to a rig with a saved profile on Grid 0 -> no false dirtying!
    auto savedNoiseRig = noiseRig;
    savedNoiseRig["NoisePrintProfiles"][0] = noise_print::Save(grid0Profile);
    {
      ScopedAudioThread audio(clapPlugin);
      PluginLifecycleTestAccessor::ApplyRig(*plug, savedNoiseRig, "NoiseSaved", "");
      WaitForRigIdle(*plug);
    }
    host.dirty = false;
    plug->OnIdle();
    assert(!host.dirty);
    assert(PluginLifecycleTestAccessor::SavedNoiseProfile(*plug, 0) == grid0Profile);

    clapPlugin->stop_processing(clapPlugin);
    clapPlugin->deactivate(clapPlugin);
  }

  // --- Item 9: OnReset() guards invalid sampleRate (0 and NaN) with 48000. fallback ---
  std::cout << "[plugin-lifecycle] Item 9\n";
  {
    for (double badRate : {0., -48000., std::numeric_limits<double>::quiet_NaN()})
    {
      PluginLifecycleTestAccessor::SetRawSampleRate(*plug, badRate);
      plug->OnReset();
      assert(PluginLifecycleTestAccessor::SampleRatePrepared(*plug) == 48000.);
      assert(std::isfinite(PluginLifecycleTestAccessor::MasterCoefficient(*plug)));
      std::array<double, 64> inL{}, inR{}, outL{}, outR{};
      inL.fill(0.1);
      inR.fill(0.1);
      double* in[]{inL.data(), inR.data()};
      double* out[]{outL.data(), outR.data()};
      plug->ProcessBlock(in, out, 64);
      for (double s : outL)
        assert(std::isfinite(s));
    }
    PluginLifecycleTestAccessor::SetRawSampleRate(*plug, 48000.);
    plug->OnReset();
  }

  // --- Item 10: Concurrent control / UI / audio locking stress ---
  std::cout << "[plugin-lifecycle] Item 10\n";
  {
    assert(clapPlugin->activate(clapPlugin, 48000., 1, 64));
    assert(clapPlugin->start_processing(clapPlugin));
    std::atomic<bool> stop{false};
    ScopedAudioThread audio(clapPlugin);
    std::thread uiReader([&] {
      while (!stop.load(std::memory_order_acquire))
      {
        (void)plug->GetPresetCrossfade();
        (void)plug->GetShowVisualizations();
        (void)plug->GetShowMainMacros();
        (void)plug->GetRescanOnStartup();
        (void)plug->GetSelectedSlot();
        (void)plug->GetActiveScene();
        (void)plug->IsRoutingOpen();
        (void)plug->IsLibraryScanning();
        (void)plug->GetSlotModelInfo(0);
        (void)plug->MonitorGeneration();
        plug->ToggleSlotQuality(0);
        plug->BeginInformHostOfParamChangeFromUI(kOutputLevel);
        plug->SendParameterValueFromUI(kOutputLevel, 0.5);
        plug->EndInformHostOfParamChangeFromUI(kOutputLevel);
        plug->OnParamChangeUI(kOutputLevel, iplug::EParamSource::kUI);
      }
    });
    std::thread controlMutator([&] {
      for (int iter = 0; iter < 50; ++iter)
      {
        plug->SetPresetCrossfade((iter & 1) != 0);
        plug->SetShowVisualizations((iter & 1) == 0);
        plug->SetShowMainMacros((iter & 1) != 0);
        plug->SetRescanOnStartup((iter & 1) == 0);
        plug->SelectSlot(static_cast<size_t>(iter % 4));
        plug->SetRoutingOpen((iter & 1) != 0);
        if (iter % 10 == 0)
        {
          plug->BeginSlotPreview(0);
          plug->PreviewSlot(SlotEffect::Boost, "");
          plug->EndSlotPreview(false);
        }
        plug->OnIdle();
        iplug::IByteChunk chunk;
        assert(plug->SerializeState(chunk));
        if (iter % 20 == 0)
          assert(plug->UnserializeState(chunk, 0) >= 0);
      }
      stop.store(true, std::memory_order_release);
    });
    controlMutator.join();
    uiReader.join();
    clapPlugin->stop_processing(clapPlugin);
    clapPlugin->deactivate(clapPlugin);
  }

  clapPlugin->destroy(clapPlugin);
  clap_entry.deinit();
  std::cout << "Plugin lifecycle tests (Items 2, 4, 5, 7, 8, 9, 10) passed\n";
  return 0;
}
