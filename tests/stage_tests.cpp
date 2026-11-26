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
#include "GridDCBlocker.h"
#include "../AudioDSPTools/dsp/RecursiveLinearFilter.h"
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
#include "TransientShaperTests.h"
#include "HarmonicTremoloTests.h"
#include "MultibandTests.h"
#include "GraphicEQTests.h"
#include "ModulePresetTests.h"
#include "DriveAntialiasTests.h"
#include "NoisePrintTests.h"
#include "AIGateTests.h"
#include "PedalDistortionTests.h"
#include "LoFiTapeTests.h"
#include "ModuleControlsTests.h"
#include "EQResponseTests.h"
#include "FXMusicalTests.h"
#include "VisualizationTests.h"
#include "ReverbDelayCandidatesTests.h"
#include "CosmicEchoTests.h"
#include "GranulatorTests.h"
#include "MultiHeadStereoTests.h"
#include "OutboardTests.h"
#include "MeteringTests.h"
#include "SignalMonitorTests.h"
#include "PitchShiftTests.h"
#include "SmoothControlsTests.h"
#include "ClickTests.h"
#include "ExpandedDelayTests.h"
#include "ExpandedDelayPreviewTests.h"
#include "DigitalDelayVoiceTests.h"
#include "MixLawTests.h"
#include "DelayTimeChangeTests.h"
#include "ChorusVoicesTests.h"
#include "SpringTankTests.h"
#include "TrailsTests.h"
#include "DSPBehaviorTests.h"
#include "ModuleUpgradeTests.h"
#include "DSPSafetyTests.h"

namespace
{
class StubSampleRateDSP final : public nam::DSP
{
public:
  explicit StubSampleRateDSP(double expectedSampleRate) : nam::DSP(1, 1, expectedSampleRate) {}
  void process(NAM_SAMPLE** input, NAM_SAMPLE** output, const int num_frames) override
  {
    for (int i = 0; i < num_frames; ++i)
      output[0][i] = input[0][i];
  }
};

void TestStageAdoptionAndSampleRate()
{
  // Item 6: Adopt() must set mFirst = true so a replaced module's input/output gains do not bleed.
  {
    const nlohmann::json unity{{"architecture", "Linear"},
                               {"config", {{"receptive_field", 1}, {"bias", false}}},
                               {"weights", {1.}},
                               {"sample_rate", 48000.}};
    const nlohmann::json avg8{{"architecture", "Linear"},
                              {"config", {{"receptive_field", 8}, {"bias", false}}},
                              {"weights", {0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125, 0.125}},
                              {"sample_rate", 48000.}};
    NAMStage stage;
    stage.Publish(prepare(unity, 48000.));
    stage.ResetStopped(48000., 512);
    std::array<double, 64> block{};
    for (int b = 0; b < 4; ++b)
    {
      block.fill(0.01);
      stage.Process(block.data(), 64, true, 20., 20.);
    }
    stage.Publish(prepare(avg8, 48000.));
    // Step 64-frame blocks until mSwitchGain hits 0 (4 * 64 = 256 samples > 240-sample 5 ms fade).
    for (int b = 0; b < 4; ++b)
    {
      block.fill(0.1);
      stage.Process(block.data(), 64, true, 20., 20.);
    }
    // Adoption block (8 samples): switching stays true until block end so mSwitchGain stays 0,
    // while mFirst = true must snap mIn/mOut/mMix to 0 dB before feeding the 8-tap NAM history.
    block.fill(0.1);
    stage.Process(block.data(), 8, true, 0., 0.);
    // First audible block after adoption: sample 0 averages feed steps 2..9 * step (avg = 5.5 * step).
    block.fill(0.1);
    stage.Process(block.data(), 8, true, 0., 0.);
    const double step = 1. / (48000. * .005);
    const double expected0 = 0.1 * (5.5 * step) * step;
    assert(std::abs(block[0] - expected0) < 1e-12);
  }
  // Item 9: GetNAMSampleRate must fall back to 48000.0 for NaN, +/-Inf, 0, and negative values.
  for (double badRate : {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity(),
                         -std::numeric_limits<double>::infinity(), 0., -44100.})
  {
    std::unique_ptr<nam::DSP> stub = std::make_unique<StubSampleRateDSP>(badRate);
    assert(GetNAMSampleRate(stub) == 48000.);
    ResamplingNAM resampled(std::make_unique<StubSampleRateDSP>(badRate), 44100.);
    assert(resampled.GetEncapsulatedSampleRate() == 48000.);
    std::array<NAM_SAMPLE, 64> in{}, out{};
    in.fill(0.25);
    NAM_SAMPLE* inPtr = in.data();
    NAM_SAMPLE* outPtr = out.data();
    resampled.process(&inPtr, &outPtr, 64);
    for (double sample : out)
      assert(std::isfinite(sample));
  }
  std::cout << "Stage adoption mFirst and NAM sample-rate fallback passed\n";
}

void TestPR4HotPathAndPrewarm()
{
  // 1. CabinetIR PrepareSlot + Publish + ResetStopped constructs AdaptiveIR only 2x (left + right),
  //    rebuilds after Process() to clear tails, and rebuilds on rate/block changes.
  {
    const auto dir = std::filesystem::current_path() / "pr4-stage-fixtures";
    std::filesystem::create_directories(dir);
    const auto irFile = dir / "impulse-stereo.wav";
    // 2-channel 16-bit 48 kHz WAV with impulse at sample 0 and delayed tap at sample 20 (creates tail).
    std::vector<uint32_t> samples(2 * 64, 0);
    samples[0] = 16384;  // L[0]
    samples[1] = 16384;  // R[0]
    samples[2 * 20] = 8192;
    samples[2 * 20 + 1] = 8192;
    wav_fixture::Write(irFile, 16, false, samples, 256, 256, false, 2);

    SlotConfig irConfig;
    irConfig.effect = SlotEffect::CabinetIR;
    irConfig.path = irFile.string();
    const uint64_t beforePrepare = AdaptiveIR::ConstructionCount();
    auto prepared = PrepareSlot(irConfig, 48000., 512);
    NAMStage stage;
    stage.Publish(std::move(prepared.dsp));
    stage.ResetStopped(48000., 512);
    assert(AdaptiveIR::ConstructionCount() - beforePrepare == 2);

    std::array<double, 16> l{}, r{};
    l[0] = 1.;
    r[0] = 1.;
    EQControls eq;
    eq.irLevel = 0.;
    stage.Process(l.data(), 16, true, 0., 0., {}, eq, {}, r.data());
    assert(std::abs(l[0]) > 0.1 && std::abs(r[0]) > 0.1);

    const uint64_t beforeDirtyReset = AdaptiveIR::ConstructionCount();
    stage.ResetStopped(48000., 512);
    assert(AdaptiveIR::ConstructionCount() - beforeDirtyReset == 2);
    l.fill(0.);
    r.fill(0.);
    stage.Process(l.data(), 16, true, 0., 0., {}, eq, {}, r.data());
    for (int i = 0; i < 16; ++i)
      assert(l[i] == 0. && r[i] == 0.);

    const uint64_t beforeRateChange = AdaptiveIR::ConstructionCount();
    stage.ResetStopped(96000., 256);
    assert(AdaptiveIR::ConstructionCount() - beforeRateChange == 2);
    const uint64_t beforeCleanReset = AdaptiveIR::ConstructionCount();
    stage.ResetStopped(96000., 256);
    assert(AdaptiveIR::ConstructionCount() == beforeCleanReset);
    std::filesystem::remove_all(dir);
  }

  // Compare the first audible block to the released warmup schedule. A slow
  // forget gate keeps the difference measurable after several prewarm calls.
  {
    constexpr double rate = 48000.;
    constexpr int block = 512;
    const auto dir = std::filesystem::current_path() / "nam-startup-fixture";
    std::filesystem::create_directories(dir);
    const auto modelFile = dir / "slow-lstm.nam";
    const nlohmann::json model{
      {"version", "0.7.0"}, {"architecture", "LSTM"}, {"sample_rate", rate},
      {"config", {{"num_layers", 1}, {"input_size", 1}, {"hidden_size", 1}}},
      {"weights", {0., 0., 0., 0., .2, 0., 0., 0., 0., 10., 0., 2., 0., 1., 1., 0.}}};
    std::ofstream(modelFile) << model.dump();

    auto reference = nam::get_dsp(modelFile);
    reference->ResetAndPrewarm(rate, 2048); // Released ResamplingNAM constructor.
    reference->ResetAndPrewarm(rate, block); // PrepareSlot.
    reference->ResetAndPrewarm(rate, block); // ResetStopped adopts the slot.

    SlotConfig config;
    config.path = modelFile.string();
    auto prepared = PrepareSlot(config, rate, block);
    auto* processor = prepared.dsp->model.get();
    NAMStage stage;
    stage.Publish(std::move(prepared.dsp));
    stage.ResetStopped(rate, block);

    std::array<NAM_SAMPLE, block> input{}, expected{}, actual{};
    input.fill(.2);
    auto* in = input.data();
    auto* out = expected.data();
    reference->process(&in, &out, block);
    out = actual.data();
    processor->process(&in, &out, block);
    assert(actual == expected);
    std::filesystem::remove_all(dir);
  }

  // Linear Reset replaces convolution history, so the fresh reset can be reused.
  {
    ResamplingNAM linear(std::make_unique<nam::Linear>(1, 1, 2, false, std::vector<float>{.5f, .5f}, 48000.), 48000., true);
    linear.Reset(48000., 512);
    allocations = deletions = 0;
    tracking = true;
    linear.Reset(48000., 512);
    tracking = false;
    assert(allocations == 0 && deletions == 0);

    NAM_SAMPLE input[]{.5, .5}, output[]{0., 0.};
    auto* in = input;
    auto* out = output;
    linear.process(&in, &out, 2);
    assert(output[0] == .25 && output[1] == .5);
    linear.Reset(48000., 512);
    linear.process(&in, &out, 2);
    assert(output[0] == .25 && output[1] == .5);
  }

  // 3. NAMStage::Prepared::IsLoaded() parity & gain memo bit-identity across 0 -> +6 -> -6 -> 0 dB.
  {
    const nlohmann::json unity{{"architecture", "Linear"},
                               {"config", {{"receptive_field", 1}, {"bias", false}}},
                               {"weights", {1.}},
                               {"sample_rate", 48000.}};
    {
      NAMStage::Prepared emptyNam;
      emptyNam.effect = SlotEffect::NAM;
      assert(emptyNam.IsLoaded() == emptyNam.Info().loaded && !emptyNam.IsLoaded());

      NAMStage::Prepared emptyIr;
      emptyIr.effect = SlotEffect::CabinetIR;
      assert(emptyIr.IsLoaded() == emptyIr.Info().loaded && !emptyIr.IsLoaded());

      NAMStage::Prepared loadedIr;
      loadedIr.effect = SlotEffect::CabinetIR;
      loadedIr.ir = std::make_unique<AdaptiveIR>(AdaptiveIR::IRData{{1.f}, 48000.}, 48000., 64);
      assert(loadedIr.IsLoaded() == loadedIr.Info().loaded && loadedIr.IsLoaded());

      NAMStage::Prepared boost;
      boost.effect = SlotEffect::Boost;
      assert(boost.IsLoaded() == boost.Info().loaded && boost.IsLoaded());

      auto loadedNam = prepare(unity, 48000.);
      assert(loadedNam->IsLoaded() == loadedNam->Info().loaded && loadedNam->IsLoaded());
    }

    NAMStage emptyStage;
    emptyStage.ResetStopped(48000., 64);
    std::array<double, 64> emptyBuf{};
    emptyBuf.fill(0.25);
    allocations = deletions = 0;
    tracking = true;
    emptyStage.Process(emptyBuf.data(), 64, true, 12., -12.);
    tracking = false;
    assert(allocations == 0 && deletions == 0);
    for (double s : emptyBuf)
      assert(s == 0.25);

    auto p = std::make_unique<NAMStage::Prepared>();
    p->effect = SlotEffect::NAM;
    p->model = std::make_unique<ResamplingNAM>(std::make_unique<nam::DSP>(1, 1, 48000.), 48000.);
    NAMStage stage;
    stage.Publish(std::move(p));
    stage.ResetStopped(48000., 64);
    SlotEQ eqRef, cutsRef;
    eqRef.Reset(48000.);
    eqRef.Process(nullptr, 0, false, {});
    cutsRef.Reset(48000.);
    cutsRef.Process(nullptr, 0, true, {});
    double refIn = 1., refOut = 1.;
    for (const double db : {0., 6., -6., 0.})
    {
      std::array<double, 64> block{}, refWet{};
      block.fill(0.5);
      allocations = deletions = 0;
      tracking = true;
      stage.Process(block.data(), 64, true, db, db);
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      const double inTarget = std::pow(10., db * .05);
      const double outTarget = std::pow(10., db * .05);
      const double inDelta = (inTarget - refIn) / 64.;
      const double outDelta = (outTarget - refOut) / 64.;
      for (int i = 0; i < 64; ++i)
      {
        refIn += inDelta;
        refWet[i] = 0.5 * refIn;
      }
      eqRef.Process(refWet.data(), 64, false, {});
      cutsRef.Process(refWet.data(), 64, true, {});
      for (int i = 0; i < 64; ++i)
      {
        refOut += outDelta;
        const double expected = refWet[i] * refOut;
        assert(block[i] == expected);
      }
    }
  }

  // 4. GridDCBlocker bit-identity vs recursive_linear_filter::HighPass across rates, blocks, and NaN.
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    GridDCBlocker blocker;
    allocations = deletions = 0;
    tracking = true;
    blocker.Prepare(rate, 2, 512);
    tracking = false;
    assert(allocations == 0 && deletions == 0);

    recursive_linear_filter::HighPass refFresh;
    refFresh.SetParams(recursive_linear_filter::HighPassParams(rate, 5.));
    std::array<double, 512> candL{}, candR{}, refInL{}, refInR{};
    const std::array<int, 6> blocks{1, 3, 31, 64, 127, 512};
    for (int b = 0; b < 18; ++b)
    {
      const int n = blocks[b % blocks.size()];
      for (int i = 0; i < n; ++i)
      {
        const double vL = 0.4 + 0.3 * std::sin((b * 512 + i) * 0.05);
        const double vR = -0.25 + 0.2 * std::cos((b * 512 + i) * 0.03);
        candL[i] = refInL[i] = vL;
        candR[i] = refInR[i] = vR;
      }
      if (b == 7)
        candL[0] = refInL[0] = std::numeric_limits<double>::quiet_NaN();
      double* candPtrs[]{candL.data(), candR.data()};
      double* refInPtrs[]{refInL.data(), refInR.data()};
      double** refOut = refFresh.Process(refInPtrs, 2, static_cast<size_t>(n));
      allocations = deletions = 0;
      tracking = true;
      double** candOut = blocker.Process(candPtrs, 2, static_cast<size_t>(n));
      tracking = false;
      assert(allocations == 0 && deletions == 0);
      for (int i = 0; i < n; ++i)
      {
        assert(candOut[0][i] == refOut[0][i]);
        assert(candOut[1][i] == refOut[1][i]);
      }
    }
  }
  std::cout << "PR4 hot path, NAM startup parity, AdaptiveIR reuse, and GridDCBlocker bit identity passed\n";
}
} // namespace

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
    {"stage-adoption-and-rate", TestStageAdoptionAndSampleRate},
    {"pr4-hotpath-prewarm", TestPR4HotPathAndPrewarm},
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
    {"noise-print-user-preset", TestSmartGateUserPreset},
    {"ai-gate", TestAIGate},
    {"pedal-distortion", TestPedalDistortion},
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
    {"transient-shaper", TestTransientShaper},
    {"harmonic-tremolo", TestHarmonicTremolo},
    {"multiband", TestMultiband},
    {"graphic-eq", TestGraphicEQ},
    {"signal-monitor", TestSignalMonitor},
    {"pitch-shift", TestPitchShift},
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
    {"granulator", TestGranulator},
    {"multi-head-stereo", TestMultiHeadStereo},
    {"outboard", TestOutboard},
    {"air-dynamics", TestAirDynamics},
    {"mix-law", TestMixLaw},
    {"delay-time-change", TestDelayTimeChange},
    {"expanded-delays", TestExpandedDelays},
    {"expanded-delay-preview", TestExpandedDelayPreview},
    {"digital-delay-voice", TestDigitalDelayVoice},
    {"chorus-voices", TestChorusVoices},
    {"spring-tank", TestSpringTank},
    {"trails", TestTrails},
    {"clicks", TestClicks},
    {"console-eq-calibration", TestConsoleEQCalibration},
    {"envelope-filter-controls", TestEnvelopeFilterControls},
    {"ring-carrier-lfo", TestRingCarrierLfo},
    {"volume-pedal", TestVolumePedal},
    {"dsp-safety", TestDSPSafety},
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
  run("stage-adoption-and-rate", TestStageAdoptionAndSampleRate);
  run("pr4-hotpath-prewarm", TestPR4HotPathAndPrewarm);
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
  run("noise-print-user-preset", TestSmartGateUserPreset);
  run("ai-gate", TestAIGate);
  run("pedal-distortion", TestPedalDistortion);
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
  run("transient-shaper", TestTransientShaper);
  run("harmonic-tremolo", TestHarmonicTremolo);
  run("multiband", TestMultiband);
  run("graphic-eq", TestGraphicEQ);
  run("signal-monitor", TestSignalMonitor);
  run("pitch-shift", TestPitchShift);
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
  run("granulator", TestGranulator);
  run("multi-head-stereo", TestMultiHeadStereo);
  run("outboard", TestOutboard);
  run("air-dynamics", TestAirDynamics);
  run("mix-law", TestMixLaw);
  run("delay-time-change", TestDelayTimeChange);
  run("expanded-delays", TestExpandedDelays);
  run("expanded-delay-preview", TestExpandedDelayPreview);
  run("digital-delay-voice", TestDigitalDelayVoice);
  run("chorus-voices", TestChorusVoices);
  run("spring-tank", TestSpringTank);
  run("trails", TestTrails);
  run("clicks", TestClicks);
  run("console-eq-calibration", TestConsoleEQCalibration);
  run("envelope-filter-controls", TestEnvelopeFilterControls);
  run("ring-carrier-lfo", TestRingCarrierLfo);
  run("volume-pedal", TestVolumePedal);
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
