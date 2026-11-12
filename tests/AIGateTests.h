#pragma once
#include "SlotOperations.h"
#include <cstring>
#include <vector>

// Noise Gate (AI): behaviour that must survive retraining, with wide margins.
namespace ai_gate_tests
{
// Idle single-coil style noise: hiss with mains hum and harmonics, about -52 dBFS.
inline double Noise(uint32_t& state, int sample, double rate)
{
  state = state * 1664525u + 1013904223u;
  const double t = 6.283185307179586 * sample / rate;
  return .002 * (double(state) / 4294967296. * 2. - 1.)
         + .0015 * std::sin(50. * t) + .001 * std::sin(150. * t + 1.) + .0005 * std::sin(250. * t + 2.);
}
// A plucked low A: decaying harmonics with a sharp start.
inline double Note(double seconds)
{
  if (seconds < 0.)
    return 0.;
  double value = 0.;
  for (int k = 1; k <= 8; ++k)
    value += std::sin(6.283185307179586 * 110. * k * seconds + k) * std::exp(-seconds * (1.5 + .6 * k)) / k;
  return .12 * value;
}
inline double Energy(const std::vector<double>& x, double from, double to, double rate)
{
  double sum = 0.;
  for (int i = static_cast<int>(from * rate); i < static_cast<int>(to * rate); ++i)
    sum += x[i] * x[i];
  return sum;
}
inline std::vector<double> Render(ai_gate::Processor& processor, std::vector<double> input, const ai_gate::Controls& c,
                                  std::vector<double>* right = nullptr)
{
  for (size_t done = 0; done < input.size(); done += 128)
  {
    const int n = static_cast<int>(std::min<size_t>(128, input.size() - done));
    allocations = deletions = 0;
    tracking = true;
    processor.Process(&input[done], right ? right->data() + done : nullptr, n, c);
    tracking = false;
    assert(!allocations && !deletions);
  }
  for (double x : input)
    assert(std::isfinite(x));
  return input;
}
} // namespace ai_gate_tests

inline void TestAIGateProtectionCost()
{
  using namespace ai_gate_tests;
  // A deliberately closed classifier isolates the protector from learned decisions.
  // It must preserve abrupt inputs, including unwanted noise bursts: that is the cost
  // the control documents, rather than a claim that it recognizes a guitar pick.
  std::array<float, 96 * 8> inputWeight{}, outputWeight{};
  std::array<float, 8> inputBias{};
  std::array<float, 3 * 8 * 8> recurrentWeight{};
  std::array<float, 24> recurrentBias{};
  std::array<float, 96> outputBias{};
  outputBias.fill(-30.f);
  const ai_gate::Model closed{8, 8, 1500, inputWeight.data(), inputBias.data(),
    recurrentWeight.data(), recurrentWeight.data(), recurrentBias.data(), recurrentBias.data(),
    outputWeight.data(), outputBias.data()};
  constexpr double rate = 48000.;
  std::vector<double> input(14400);
  uint32_t seed = 3;
  for (size_t i = 0; i < input.size(); ++i)
  {
    seed = seed * 1664525u + 1013904223u;
    input[i] = (double(seed) / 4294967296. * 2. - 1.) * (i >= 4800 && i < 5760 ? .04 : .0002);
  }
  ai_gate::Processor processor;
  ai_gate::Controls c;
  c.attackProtection = 0.;
  processor.Reset(rate, closed);
  const auto plain = Render(processor, input, c);
  c.attackProtection = 100.;
  processor.Reset(rate, closed);
  const auto protectedNoise = Render(processor, input, c);
  assert(Energy(protectedNoise, .1, .12, rate) > 4. * Energy(plain, .1, .12, rate));
  for (int i = 0; i < 4000; ++i) assert(plain[i] == protectedNoise[i]);
  // Controls moving while the host is stopped must not affect a subsequent reset.
  c.attackProtection = 0.;
  processor.Reset(rate, closed);
  assert(Render(processor, input, c) == plain);
}

inline void TestAIGateFinalGate()
{
  using namespace ai_gate_tests;
  // Reduction 0 is exact dry, isolating the final gate shared with Smart Gate.
  constexpr double rate = 48000.;
  std::vector<double> input(static_cast<int>(1.2 * rate));
  uint32_t seed = 5;
  for (int i = 0; i < static_cast<int>(input.size()); ++i)
    input[i] = Noise(seed, i, rate) + Note(i / rate - .3) * (i < .7 * rate ? 1. : 0.);
  ai_gate::Processor processor;
  ai_gate::Controls c;
  c.reduction = 0.;
  processor.Reset(rate);
  assert(Render(processor, input, c) == input); // Off at -100 dB.
  c.gateThreshold = -30.; // Above the -52 dBFS noise, below the note.
  processor.Reset(rate);
  const auto gated = Render(processor, input, c);
  // Starts closed, passes the note unchanged once open, then closes after it.
  assert(Energy(gated, 0., .29, rate) < 1.e-6 * Energy(input, 0., .29, rate));
  assert(Energy(gated, .32, .6, rate) > .999 * Energy(input, .32, .6, rate));
  assert(Energy(gated, .9, 1.2, rate) < 1.e-6 * Energy(input, .9, 1.2, rate));
  assert(processor.Reduction() > 100.);
  // A longer Gate release fades more of what follows the note.
  c.gateRelease = 400.;
  processor.Reset(rate);
  const auto slow = Render(processor, input, c);
  assert(Energy(slow, .7, .9, rate) > 2. * Energy(gated, .7, .9, rate));

  // In a slot, AI Gate uses Smart Gate's Gate threshold and Gate release parameters.
  assert(UsesUtility(SlotEffect::AIGate, uNoiseGateThreshold) && UsesUtility(SlotEffect::AIGate, uNoiseGateRelease));
  assert(!UsesUtility(SlotEffect::AIGate, uNoiseRelease) && !UsesUtility(SlotEffect::AIGate, uNoiseLearn));
  SlotConfig config;
  config.effect = SlotEffect::AIGate;
  NAMStage stage;
  stage.Publish(PrepareSlot(config, rate, 128).dsp);
  stage.ResetStopped(rate, 128);
  UtilityControls utility;
  utility.aiGate.reduction = 0.;
  utility.noise.gateThreshold = -30.;
  std::array<double, 128> block;
  seed = 5;
  double energy = 0.;
  for (int done = 0; done < 4800; done += 128)
  {
    for (int i = 0; i < 128; ++i)
      block[i] = Noise(seed, done + i, rate);
    stage.Process(block.data(), 128, true, 0., 0., {}, {}, utility);
    for (double x : block)
      energy += x * x;
  }
  assert(energy == 0.);
}

inline void TestAIGate()
{
  using namespace ai_gate_tests;
  TestAIGateProtectionCost();
  TestAIGateFinalGate();
  // The built-in weights are finite; unsupported models never reach the inference loops.
  {
    namespace m = ai_gate::model;
    const auto finite = [](const auto& values) { return std::all_of(std::begin(values), std::end(values), [](float x) { return std::isfinite(x); }); };
    assert(finite(m::inputWeight) && finite(m::inputBias) && finite(m::gruInput) && finite(m::gruHidden)
           && finite(m::gruInputBias) && finite(m::gruHiddenBias) && finite(m::outputWeight) && finite(m::outputBias));
    auto model = ai_gate::DefaultModel();
    assert(ai_gate::Supported(model));
    for (auto [inner, hidden, tickRate] : {std::array{72, 64, 1500}, std::array{63, 64, 1500}, std::array{64, 60, 1500},
                                           std::array{0, 64, 1500}, std::array{64, 64, 0}, std::array{64, 64, -1}})
    {
      auto broken = model;
      broken.inner = inner, broken.hidden = hidden, broken.tickRate = tickRate;
      assert(!ai_gate::Supported(broken));
      ai_gate::Processor fallback, reference;
      fallback.Reset(48000., broken);
      reference.Reset(48000.);
      std::array<double, 256> a{}, b{};
      uint32_t seed = 3;
      for (int i = 0; i < 256; ++i)
        a[i] = b[i] = Noise(seed, i, 48000.) + (i > 100 ? .1 : 0.);
      fallback.Process(a.data(), nullptr, 256, {});
      reference.Process(b.data(), nullptr, 256, {});
      assert(a == b);
    }
    auto missing = model;
    missing.gruHidden = nullptr;
    assert(!ai_gate::Supported(missing));
  }
  for (double rate : {44100., 48000., 96000., 192000.})
  {
    const int frames = static_cast<int>(3. * rate);
    std::vector<double> noise(frames), note(frames), mixed(frames);
    uint32_t seed = 7;
    for (int i = 0; i < frames; ++i)
    {
      noise[i] = Noise(seed, i, rate);
      note[i] = Note(i / rate - 1.5); // 1.5 s of idle noise, then a pick.
      mixed[i] = noise[i] + note[i];
    }
    ai_gate::Processor processor;
    ai_gate::Controls c;
    const auto reductionDB = [&](const std::vector<double>& out, const std::vector<double>& in, double from, double to) {
      return 10. * std::log10(Energy(in, from, to, rate) / std::max(1.e-30, Energy(out, from, to, rate)));
    };

    // Idle noise closes the gate without any Learn step; the pick comes through.
    processor.Reset(rate);
    const auto gated = Render(processor, mixed, c);
    assert(reductionDB(gated, mixed, .5, 1.45) > 15.);
    assert(std::abs(reductionDB(gated, mixed, 1.5, 1.7)) < 1.);  // Note body within 1 dB.
    assert(reductionDB(gated, mixed, 1.5, 1.505) < 3.);          // First 5 ms of the attack.
    assert(processor.Reduction() >= 0.);
    // Protection offers a continuous range without changing neural decisions.
    auto plainControls = c, fullControls = c;
    plainControls.attackProtection = 0.; fullControls.attackProtection = 100.;
    processor.Reset(rate); const auto plain = Render(processor, mixed, plainControls);
    processor.Reset(rate); const auto full = Render(processor, mixed, fullControls);
    const double plainAttack = Energy(plain, 1.5, 1.505, rate);
    const double middleAttack = Energy(gated, 1.5, 1.505, rate);
    const double fullAttack = Energy(full, 1.5, 1.505, rate);
    assert(fullAttack > 1.5 * plainAttack && middleAttack > plainAttack && middleAttack < fullAttack);
    // Host block size must not alter envelopes, recurrent history or gain ramps.
    processor.Reset(rate);
    auto divided = mixed;
    for (size_t done = 0; done < divided.size();)
    {
      const int n = static_cast<int>(std::min<size_t>(1 + done % 137, divided.size() - done));
      processor.Process(divided.data() + done, nullptr, n, c);
      done += n;
    }
    assert(divided == gated);
    // No startup burst: the first 20 ms after a reset are already reduced.
    assert(reductionDB(gated, mixed, 0., .02) > 15.);

    // Reduction limits the attenuation; 0 dB is exactly dry.
    processor.Reset(rate);
    c.reduction = 12.;
    const auto limited = Render(processor, mixed, c);
    const double limitedDB = reductionDB(limited, mixed, .5, 1.45);
    assert(limitedDB > 6. && limitedDB < 12.5);
    processor.Reset(rate);
    c.reduction = 0.;
    assert(Render(processor, mixed, c) == mixed);
    // Automating Reduction to 0 settles on exactly dry, without a step in the output.
    processor.Reset(rate);
    c.reduction = 40.;
    Render(processor, std::vector<double>(mixed.begin(), mixed.begin() + frames / 2), c);
    c.reduction = 0.;
    std::vector<double> tone(static_cast<size_t>(rate / 2));
    for (size_t i = 0; i < tone.size(); ++i)
      tone[i] = .001 * std::sin(6.283185307179586 * 200. * i / rate); // Quiet enough to be gated.
    const auto opened = Render(processor, tone, c);
    for (size_t i = 1; i < opened.size(); ++i)
      assert(std::abs((opened[i] - opened[i - 1]) - (tone[i] - tone[i - 1])) < 1.e-4); // A one-sample gain jump would reach 1e-3.
    for (size_t i = static_cast<size_t>(.1 * rate); i < opened.size(); ++i)
      assert(opened[i] == tone[i]);
    c.reduction = 40.;

    // Sensitivity trades cleaning against sustain, monotonically on idle noise.
    double previous = 0.;
    for (double sensitivity : {-100., 0., 100.})
    {
      processor.Reset(rate);
      c.sensitivity = sensitivity;
      const double db = reductionDB(Render(processor, mixed, c), mixed, .5, 1.45);
      assert(db >= previous - .5);
      previous = db;
    }
    c.sensitivity = 0.;

    // Mix 0 becomes exactly dry once its 5 ms ramp has finished.
    processor.Reset(rate);
    c.mix = 0.;
    const auto dry = Render(processor, mixed, c);
    for (int i = static_cast<int>(.01 * rate); i < frames; ++i)
      assert(dry[i] == mixed[i]);
    c.mix = 100.;

    // A reset does not remember earlier Mix automation.
    processor.Reset(rate);
    assert(Render(processor, mixed, c) == gated);

    // Digital silence stays silent; a reset reproduces the same output.
    processor.Reset(rate);
    for (double x : Render(processor, std::vector<double>(static_cast<size_t>(rate / 4)), c))
      assert(x == 0.);
    processor.Reset(rate);
    assert(Render(processor, mixed, c) == gated);

    // Linked stereo: one set of gains, so anti-phase input stays anti-phase and
    // equal channels match the mono result's decisions.
    processor.Reset(rate);
    std::vector<double> right(frames);
    for (int i = 0; i < frames; ++i)
      right[i] = -mixed[i];
    const auto left = Render(processor, mixed, c, &right);
    for (int i = 0; i < frames; ++i)
      assert(left[i] == -right[i]);
    assert(std::abs(reductionDB(left, mixed, .5, 1.45) - reductionDB(gated, mixed, .5, 1.45)) < 1.e-6);

    // In a slot: no reported latency, and an open gate passes the current sample.
    SlotConfig config;
    config.effect = SlotEffect::AIGate;
    NAMStage stage;
    stage.Publish(PrepareSlot(config, rate, 128).dsp);
    stage.ResetStopped(rate, 128);
    assert(stage.Latency() == 0);
    UtilityControls utility;
    utility.aiGate.reduction = 0.;
    std::array<double, 128> block{};
    block[0] = .2;
    stage.Process(block.data(), 128, true, 0., 0., {}, {}, utility);
    assert(block[0] == .2);
  }
}
