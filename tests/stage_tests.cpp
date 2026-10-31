#include "A2Fixture.h"
#include "A2PlanarTests.h"
#include <array>
#include <chrono>
#include <cassert>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <new>
#include <thread>
#include "NAMStage.h"
#include "../NeuralAmpModelerCore/NAM/get_dsp.h"
#include "../NeuralAmpModelerCore/NAM/activations.h"

thread_local bool tracking = false;
thread_local int allocations = 0, deletions = 0;
void* operator new(size_t n)
{
  if (tracking)
    ++allocations;
  auto* p = std::malloc(n ? n : 1);
  if (!p)
    throw std::bad_alloc();
  return p;
}
void* operator new[](size_t n)
{ return ::operator new(n); }
void operator delete(void* p) noexcept
{
  if (tracking && p)
    ++deletions;
  std::free(p);
}
void operator delete[](void* p) noexcept
{ ::operator delete(p); }
void operator delete(void* p, size_t) noexcept
{ ::operator delete(p); }
void operator delete[](void* p, size_t) noexcept
{ ::operator delete(p); }

std::unique_ptr<NAMStage::Prepared> prepare(nlohmann::json j, double rate)
{
  nam::dspData config;
  config.version = j.value("version", "0.7.0");
  config.architecture = j["architecture"];
  config.config = j["config"];
  config.weights = j["weights"].get<std::vector<float>>();
  config.expected_sample_rate = j.value("sample_rate", 48000.);
  auto p = std::make_unique<NAMStage::Prepared>();
  p->model = std::make_unique<ResamplingNAM>(nam::get_dsp(config), rate);
  p->Reset(rate, 512);
  return p;
}
#include "GridTests.h"
#include "AudioControlCacheTests.h"
#include "RigTransitionTests.h"
#include "DynamicsTests.h"
#include "SlotFeaturesTests.h"
#include "IRControlsTests.h"
#include "TimeEffectsTests.h"
#include "TunerTempoUtilityTests.h"
#include "RoadmapEffectsTests.h"
#include "ModulationMatrixTests.h"
#include "HQEffectsTests.h"
#include "StereoGuitarEffectsTests.h"
#include "DSPReviewTests.h"
#include "DistortionTests.h"
#include "PedalDriveTests.h"
#include "PedalCompTests.h"
#include "DynamicEQTests.h"
#include "ModulePresetTests.h"
#include "DriveAntialiasTests.h"
#include "NoisePrintTests.h"
#include "LoFiTapeTests.h"
#include "ModuleControlsTests.h"
#include "EQResponseTests.h"
#include "FXMusicalTests.h"
#include "VisualizationTests.h"
#include "ReverbDelayCandidatesTests.h"
#include "CosmicEchoTests.h"
#include "MultiHeadStereoTests.h"
#include "OutboardTests.h"
#include "MeteringTests.h"
#include "SmoothControlsTests.h"
#include "ClickTests.h"
#include "MixLawTests.h"
#include "DelayTimeChangeTests.h"
#include "ChorusVoicesTests.h"
#include "SpringTankTests.h"
#include "TrailsTests.h"
#include "DSPBehaviorTests.h"

int main(int argc, char** argv)
try
{
  assert(argc >= 2 && argc <= 6);
  nam::activations::Activation::enable_fast_tanh();
  const auto run = [](const char* name, void (*test)()) {
    const auto start = std::chrono::steady_clock::now();
    std::cout << "[ RUN ] " << name << std::endl;
    test();
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    std::cout << "[ PASS ] " << name << " (" << elapsed << " s)" << std::endl;
  };
  const std::pair<const char*, void (*)()> cases[] = {
    {"a2-planar", TestA2Planar},
    {"metering-transparency", TestMeteringTransparency},
    {"settled-smoothing", TestSettledSmoothing},
    {"fixed-grid", TestFixedGrid},
    {"audio-control-cache", TestAudioControlCache},
    {"rig-transition", TestRigTransition},
    {"dynamics", TestDynamics},
    {"slot-features", TestSlotFeatures},
    {"ir-controls", TestIRControls},
    {"time-effects", TestTimeEffects},
    {"tuner-tempo-utility", TestTunerTempoUtility},
    {"roadmap-effects", TestRoadmapEffects},
    {"modulation-matrix", TestModulationMatrix},
    {"hq-effects", TestHQEffects},
    {"dsp-behavior", TestDSPBehavior},
    {"stereo-guitar-effects", TestStereoGuitarEffects},
    {"dsp-review-fixes", TestDSPReviewFixes},
    {"distortion-and-mono-meters", TestDistortionAndMonoMeters},
    {"capture-seams", TestCaptureSeams},
    {"noise-print", TestNoisePrint},
    {"lo-fi-tape", TestLoFiTape},
    {"module-controls", TestModuleControls},
    {"eq-response", TestEQResponse},
    {"dual-delay-musical-controls", TestDualDelayMusicalControls},
    {"distortion-tone-preview", TestDistortionTonePreview},
    {"fx-visualization-math", TestFXVisualizationMath},
    {"visualization-interactions", TestVisualizationInteractions},
    {"measured-visualizations", TestMeasuredVisualizations},
    {"distortion-mode-harmonics", TestDistortionModeHarmonics},
    {"pedal-drive", TestPedalDrive},
    {"pedal-comp", TestPedalComp},
    {"dynamic-eq", TestDynamicEQ},
    {"factory-vibe-vibrato", TestFactoryVibeVibrato},
    {"module-preset-coverage", TestModulePresetCoverage},
    {"module-preset-shared-controls", TestModulePresetSharedControls},
    {"smart-gate-user-preset", TestSmartGateUserPreset},
    {"named-user-preset", TestNamedUserPreset},
    {"module-presets", TestModulePresets},
    {"drive-antialiasing", TestDriveAntialiasing},
    {"oversampled-wet-dry-blend", TestOversampledWetDryBlend},
    {"oversampled-mix-automation", TestOversampledMixAutomation},
    {"reverb-delay-candidates", TestReverbDelayCandidates},
    {"cosmic-echo", TestCosmicEcho},
    {"cosmic-bypass-drain", TestCosmicBypassDrain},
    {"multi-head-stereo", TestMultiHeadStereo},
    {"outboard", TestOutboard},
    {"air-dynamics", TestAirDynamics},
    {"mix-law", TestMixLaw},
    {"delay-time-change", TestDelayTimeChange},
    {"chorus-voices", TestChorusVoices},
    {"spring-tank", TestSpringTank},
    {"trails", TestTrails},
    {"clicks", TestClicks},
  };
  if (argc == 2 && std::string(argv[1]) == "--list")
  {
    for (const auto& entry : cases) std::cout << entry.first << '\n';
    return 0;
  }
  if (argc >= 2 && std::string(argv[1]) == "--case")
  {
    if (argc == 3)
      for (const auto& entry : cases)
        if (std::string(argv[2]) == entry.first) { run(entry.first, entry.second); return 0; }
    std::cerr << "Use --case NAME with a name from --list\n";
    return 2;
  }
  if (std::string(argv[1]) == "--module-presets-only") { TestNamedUserPreset(); TestSmartGateUserPreset(); TestModulePresetCoverage(); TestModulePresetSharedControls(); TestModulePresets(); return 0; }
  if (std::string(argv[1]) == "--mix-law-only") { TestMixLaw(); return 0; }
  if (std::string(argv[1]) == "--delay-time-change-only") { TestDelayTimeChange(); return 0; }
  if (std::string(argv[1]) == "--chorus-voices-only") { TestChorusVoices(); return 0; }
  if (std::string(argv[1]) == "--trails-only") { TestTrails(); return 0; }
  if (std::string(argv[1]) == "--clicks-only") { TestClicks(); return 0; }
  run("a2-planar", TestA2Planar);
  run("metering-transparency", TestMeteringTransparency);
  run("settled-smoothing", TestSettledSmoothing);
  nam::activations::ActivationPReLU prelu(std::vector<float>{.01f, .05f});
  Eigen::MatrixXf activationInput(2, 3);
  activationInput.setConstant(-1.f);
  allocations = deletions = 0;
  tracking = true;
  prelu.apply(activationInput);
  tracking = false;
  assert(allocations == 0 && deletions == 0);
  assert(std::abs(activationInput(0, 0) + .01f) < 1e-6 && std::abs(activationInput(1, 0) + .05f) < 1e-6);
  std::ifstream lstmFile(std::filesystem::path(argv[1]) / "NeuralAmpModelerCore/example_models/lstm.nam");
  auto lstmJson = nlohmann::json::parse(lstmFile);
  NAMStage lstm;
  lstm.Publish(prepare(lstmJson, 48000.));
  lstm.ResetStopped(48000., 512);
  std::array<double, 512> lstmInput{};
  tracking = true;
  Eigen::internal::set_is_malloc_allowed(false);
  lstm.Process(lstmInput.data(), 512, true, 0, 0);
  Eigen::internal::set_is_malloc_allowed(true);
  tracking = false;
  assert(allocations == 0 && deletions == 0);
  const nlohmann::json linear{{"architecture", "Linear"},
                              {"config", {{"receptive_field", 1}, {"bias", true}}},
                              {"weights", {2., .1}},
                              {"sample_rate", 48000.}};
  auto second = linear;
  second["weights"] = {3., .4};
  NAMStage drive, amp;
  drive.Publish(prepare(linear, 48000.));
  amp.Publish(prepare(second, 48000.));
  drive.ResetStopped(48000., 512);
  amp.ResetStopped(48000., 512);
  std::array<double, 512> data{};
  data.fill(.05);
  drive.Process(data.data(), 512, true, 0, 0);
  amp.Process(data.data(), 512, true, 0, 0);
  assert(std::abs(data[500] - 1.) < 1e-6); // 3*(2*.05+.1)+.4; reversed order yields 1.2.
  // Gain changes and settled bypass preserve independent stage controls.
  for (int b = 0; b < 5; ++b)
  {
    data.fill(.05);
    drive.Process(data.data(), 512, false, 0, 0);
  }
  assert(std::abs(data[500] - .05) < 1e-9);
  for (int b = 0; b < 5; ++b)
  {
    data.fill(.05);
    drive.Process(data.data(), 512, true, 6.020599913, -6.020599913);
  }
  assert(std::abs(data[500] - .15) < 1e-6);
  auto a2 = fixture::model(3, 123), other = fixture::model(8, 456);
  for (double rate : {48000., 44100., 96000.})
  {
    NAMStage one, two;
    one.Publish(prepare(a2, rate));
    two.Publish(prepare(other, rate));
    one.ResetStopped(rate, 512);
    two.ResetStopped(rate, 512);
    auto ref1 = prepare(a2, rate), ref2 = prepare(other, rate);
    std::array<double, 512> expected{}, intermediate{}, output{};
    for (int b = 0; b < 12; ++b)
    {
      int n = std::array<int, 6>{512, 1, 127, 64, 256, 3}[b % 6];
      for (int i = 0; i < n; ++i)
        data[i] = expected[i] = .1 * std::sin((b * 512 + i) * .03);
      double* in = expected.data();
      double* mid = intermediate.data();
      double* out = output.data();
      ref1->model->process(&in, &mid, n);
      ref2->model->process(&mid, &out, n);
      allocations = deletions = 0;
      tracking = true;
      Eigen::internal::set_is_malloc_allowed(false);
      one.Process(data.data(), n, true, 0, 0);
      two.Process(data.data(), n, true, 0, 0);
      Eigen::internal::set_is_malloc_allowed(true);
      tracking = false;
      if (allocations || deletions)
        std::cerr << "rate=" << rate << " block=" << b << " n=" << n << " new/delete=" << allocations << "/"
                  << deletions << '\n';
      assert(allocations == 0 && deletions == 0);
      for (int i = 0; i < n; ++i)
        assert(std::isfinite(data[i]) && std::abs(data[i] - output[i]) < 1e-6);
    }
    std::cout << "A2 series/reference, variable blocks, no new/delete or Eigen allocation at " << rate << " Hz\n";
    NAMStage bypass;
    bypass.Publish(prepare(a2, rate));
    bypass.ResetStopped(rate, 512);
    data.fill(0.);
    data[0] = 1.;
    bypass.Process(data.data(), 512, false, 0, 0);
    for (int i = 0; i < 512; ++i)
      assert(data[i] == (i == bypass.Latency() ? 1. : 0.));
    // A host may advertise an actual maximum of one frame, not just vary blocks below 512.
    bypass.ResetStopped(rate, 1);
    data[0] = .05;
    bypass.Process(data.data(), 1, true, 0, 0);
    assert(std::isfinite(data[0]));
  }
  run("fixed-grid", TestFixedGrid);
  run("audio-control-cache", TestAudioControlCache);
  run("rig-transition", TestRigTransition);
  run("dynamics", TestDynamics);
  run("slot-features", TestSlotFeatures);
  run("ir-controls", TestIRControls);
  run("time-effects", TestTimeEffects);
  run("tuner-tempo-utility", TestTunerTempoUtility);
  run("roadmap-effects", TestRoadmapEffects);
  run("modulation-matrix", TestModulationMatrix);
  run("hq-effects", TestHQEffects);
  run("dsp-behavior", TestDSPBehavior);
  run("stereo-guitar-effects", TestStereoGuitarEffects);
  run("dsp-review-fixes", TestDSPReviewFixes);
  run("distortion-and-mono-meters", TestDistortionAndMonoMeters);
  run("capture-seams", TestCaptureSeams);
  run("noise-print", TestNoisePrint);
  run("lo-fi-tape", TestLoFiTape);
  run("module-controls", TestModuleControls);
  run("eq-response", TestEQResponse);
  run("dual-delay-musical-controls", TestDualDelayMusicalControls);
  run("distortion-tone-preview", TestDistortionTonePreview);
  run("fx-visualization-math", TestFXVisualizationMath);
  run("visualization-interactions", TestVisualizationInteractions);
  run("measured-visualizations", TestMeasuredVisualizations);
  run("distortion-mode-harmonics", TestDistortionModeHarmonics);
  run("pedal-drive", TestPedalDrive);
  run("pedal-comp", TestPedalComp);
  run("dynamic-eq", TestDynamicEQ);
  run("factory-vibe-vibrato", TestFactoryVibeVibrato);
  run("module-preset-coverage", TestModulePresetCoverage);
  run("module-preset-shared-controls", TestModulePresetSharedControls);
  run("smart-gate-user-preset", TestSmartGateUserPreset);
  run("named-user-preset", TestNamedUserPreset);
  run("module-presets", TestModulePresets);
  run("drive-antialiasing", TestDriveAntialiasing);
  run("oversampled-wet-dry-blend", TestOversampledWetDryBlend);
  run("oversampled-mix-automation", TestOversampledMixAutomation);
  run("reverb-delay-candidates", TestReverbDelayCandidates);
  run("cosmic-echo", TestCosmicEcho);
  run("cosmic-bypass-drain", TestCosmicBypassDrain);
  run("multi-head-stereo", TestMultiHeadStereo);
  run("outboard", TestOutboard);
  run("air-dynamics", TestAirDynamics);
  run("mix-law", TestMixLaw);
  run("delay-time-change", TestDelayTimeChange);
  run("chorus-voices", TestChorusVoices);
  run("spring-tank", TestSpringTank);
  run("trails", TestTrails);
  run("clicks", TestClicks);
  if (argc > 2)
  {
    TestRealGrid(argc - 2, argv + 2);
    TestRealCaptureQuality(argv[2]);
  }
  // Stress ownership: a producer can supersede pending items while audio adopts/retirement waits.
  struct Item
  {
    int id;
    static void check() { assert(!tracking); }
    ~Item() { check(); }
  };
  RealtimeExchange<Item> exchange;
  std::atomic<bool> done{false};
  std::thread audio([&] {
    tracking = true;
    while (!done.load())
      exchange.Adopt();
    tracking = false;
  });
  for (int i = 0; i < 10000; ++i)
    exchange.Publish(std::make_unique<Item>(Item{i}));
  done = true;
  audio.join();
  exchange.Collect();
  std::cout << "10,000 publication/reclamation operations passed\n";
}
catch (const std::exception& e)
{
  std::cerr << e.what() << '\n';
  return 1;
}
