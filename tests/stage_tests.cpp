#include "A2Fixture.h"
#include <array>
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
#include "DynamicsTests.h"
#include "SlotFeaturesTests.h"
#include "TimeEffectsTests.h"
#include "TunerTempoUtilityTests.h"
#include "RoadmapEffectsTests.h"
#include "HQEffectsTests.h"
#include "StereoGuitarEffectsTests.h"
#include "DSPReviewTests.h"
#include "DistortionTests.h"
#include "NoisePrintTests.h"

int main(int argc, char** argv)
try
{
  assert(argc >= 2 && argc <= 6);
  nam::activations::Activation::enable_fast_tanh();
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
  lstm.Process(lstmInput.data(), 512, true, 0, 0, false, 0, 0, 1);
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
  drive.Process(data.data(), 512, true, 0, 0, false, 0, 0, 1);
  amp.Process(data.data(), 512, true, 0, 0, false, 0, 0, 1);
  assert(std::abs(data[500] - 1.) < 1e-6); // 3*(2*.05+.1)+.4; reversed order yields 1.2.
  // Gain changes and settled bypass preserve independent stage controls.
  for (int b = 0; b < 5; ++b)
  {
    data.fill(.05);
    drive.Process(data.data(), 512, false, 0, 0, false, 0, 0, 1);
  }
  assert(std::abs(data[500] - .05) < 1e-9);
  for (int b = 0; b < 5; ++b)
  {
    data.fill(.05);
    drive.Process(data.data(), 512, true, 6.020599913, -6.020599913, false, 0, 0, 1);
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
      one.Process(data.data(), n, true, 0, 0, false, 0, 0, 1);
      two.Process(data.data(), n, true, 0, 0, false, 0, 0, 1);
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
    bypass.Process(data.data(), 512, false, 0, 0, false, 0, 0, 1);
    for (int i = 0; i < 512; ++i)
      assert(data[i] == (i == bypass.Latency() ? 1. : 0.));
    // A host may advertise an actual maximum of one frame, not just vary blocks below 512.
    bypass.ResetStopped(rate, 1);
    data[0] = .05;
    bypass.Process(data.data(), 1, true, 0, 0, false, 0, 0, 1);
    assert(std::isfinite(data[0]));
  }
  TestFixedGrid();
  TestDynamics();
  TestSlotFeatures();
  TestTimeEffects();
  TestTunerTempoUtility();
  TestRoadmapEffects();
  TestHQEffects();
  TestStereoGuitarEffects();
  TestDSPReviewFixes();
  TestDistortionAndMonoMeters();
  TestCaptureSeams();
  TestNoisePrint();
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
