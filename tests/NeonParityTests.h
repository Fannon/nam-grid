#pragma once
#include "AIGate.h"
#include "NoisePrint.h"
#include <array>
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

namespace neon_parity_scalar
{
struct AIGateParams
{
  double reduction = 40., sensitivity = 0., mix = 100., attackProtection = 50.;
  double gateThreshold = -100., gateRelease = 40.;
};
struct AIGateResult
{
  std::vector<double> left, right;
  double reduction = 0., cleanPeak = 0.;
};
AIGateResult RunAIGate(double rate, const std::vector<double>& inL, const std::vector<double>* inR,
                       const std::vector<int>& blocks, const AIGateParams& p);

struct NoisePrintParams
{
  bool learn = false;
  double sensitivity = 1.5, reduction = 24., attack = 2., release = 120., mix = 100.;
  double gateThreshold = -100., gateRelease = 40., protectAttack = 100.;
  int trackingMode = 0;
};
struct NoisePrintStep
{
  NoisePrintParams params;
  std::vector<int> blocks;
  bool restoreInvalid = false;
};
struct NoisePrintResult
{
  std::vector<double> left, right;
  std::array<double, 96> profilePower{};
  bool profileValid = false;
  double reduction = 0., cleanPeak = 0., trackingScale = 1.;
};
NoisePrintResult RunNoisePrint(double rate, const std::vector<double>& inL, const std::vector<double>* inR,
                               const std::vector<NoisePrintStep>& steps);
} // namespace neon_parity_scalar

inline void TestNeonParity()
{
  auto buildBlocks = [](size_t total) {
    constexpr std::array<int, 3> pattern{1, 37, 128};
    std::vector<int> blocks;
    size_t pos = 0, idx = 0;
    while (pos < total)
    {
      const int n = static_cast<int>(std::min<size_t>(pattern[idx++ % pattern.size()], total - pos));
      blocks.push_back(n);
      pos += n;
    }
    return blocks;
  };

  for (double rate : {44100., 48000., 96000.})
  {
    const size_t nSamples = static_cast<size_t>(rate * 0.25);
    std::vector<double> inL(nSamples), inR(nSamples);
    for (size_t i = 0; i < nSamples; ++i)
    {
      const double t = static_cast<double>(i) / rate;
      const double env = t < 0.05 ? 0.002 : t < 0.15 ? 0.25 : 0.01;
      inL[i] = env * (std::sin(2. * 3.14159265358979323846 * 220. * t)
                      + 0.3 * std::sin(2. * 3.14159265358979323846 * 1760. * t))
               + 0.0005 * std::sin(2. * 3.14159265358979323846 * 4000. * t);
      inR[i] = env * 0.8 * std::cos(2. * 3.14159265358979323846 * 330. * t)
               - 0.0005 * std::cos(2. * 3.14159265358979323846 * 3500. * t);
    }
    const auto blocks = buildBlocks(nSamples);

    // 1. AIGate SIMD vs scalar parity (mono and stereo)
    for (bool stereo : {false, true})
    {
      neon_parity_scalar::AIGateParams sp{40., 25., 85., 60., -45., 40.};
      const auto ref = neon_parity_scalar::RunAIGate(rate, inL, stereo ? &inR : nullptr, blocks, sp);

      ai_gate::Processor simd;
      simd.Reset(rate);
      ai_gate::Controls c{sp.reduction, sp.sensitivity, sp.mix, sp.attackProtection, sp.gateThreshold, sp.gateRelease};
      std::vector<double> outL = inL, outR = inR;
      size_t pos = 0;
      for (int n : blocks)
      {
        allocations = deletions = 0; tracking = true;
        simd.Process(outL.data() + pos, stereo ? outR.data() + pos : nullptr, n, c);
        tracking = false;
        assert(allocations == 0 && deletions == 0);
        pos += n;
      }
      double maxDiff = 0.;
      for (size_t i = 0; i < nSamples; ++i)
      {
        maxDiff = std::max(maxDiff, std::abs(outL[i] - ref.left[i]));
        if (stereo) maxDiff = std::max(maxDiff, std::abs(outR[i] - ref.right[i]));
      }
      assert(maxDiff < 1.e-6);
      assert(std::abs(simd.Reduction() - ref.reduction) < 1.e-4);
      assert(std::abs(simd.CleanPeak() - ref.cleanPeak) < 1.e-6);
    }

    // 2. NoisePrint SIMD vs scalar parity: untrained idle (active final gate),
    //    Learn (>= 0.3s), Fixed + Follow (inverseScale), and Restore(invalid).
    const size_t idleN = static_cast<size_t>(rate * 0.05);
    const size_t learnN = static_cast<size_t>(rate * 0.35);
    const size_t fixedN = static_cast<size_t>(rate * 0.15);
    const size_t followN = static_cast<size_t>(rate * 0.35);
    const size_t postRestoreN = static_cast<size_t>(rate * 0.05);
    const size_t totalNP = idleN + learnN + fixedN + followN + postRestoreN;
    std::vector<double> npInL(totalNP), npInR(totalNP);
    for (size_t i = 0; i < totalNP; ++i)
    {
      const double t = static_cast<double>(i) / rate;
      const double noise = 0.008 * std::sin(2. * 3.14159265358979323846 * 1200. * t)
                         + 0.004 * std::sin(2. * 3.14159265358979323846 * 3700. * t);
      const double tone = (i >= idleN + learnN && i < idleN + learnN + fixedN + followN)
        ? 0.25 * std::sin(2. * 3.14159265358979323846 * 220. * t) : 0.;
      const double loudNoise = (i >= idleN + learnN + fixedN && i < idleN + learnN + fixedN + followN)
        ? 0.016 * std::sin(2. * 3.14159265358979323846 * 1200. * t)
          + 0.008 * std::sin(2. * 3.14159265358979323846 * 3700. * t) : 0.;
      npInL[i] = noise + tone + loudNoise;
      npInR[i] = 0.7 * noise - 0.8 * tone + 0.7 * loudNoise;
    }

    for (bool stereo : {false, true})
    {
      neon_parity_scalar::NoisePrintParams pIdle{false, 1.5, 24., 2., 120., 100., -40., 40., 100., 0};
      neon_parity_scalar::NoisePrintParams pLearn{true, 1.5, 24., 2., 120., 100., -40., 40., 100., 0};
      neon_parity_scalar::NoisePrintParams pFixed{false, 1.5, 24., 2., 120., 100., -40., 40., 100., 0};
      neon_parity_scalar::NoisePrintParams pFollow{false, 1.5, 24., 2., 120., 100., -40., 40., 100., 1};
      std::vector<neon_parity_scalar::NoisePrintStep> steps{
        {pIdle, buildBlocks(idleN), false},
        {pLearn, buildBlocks(learnN), false},
        {pFixed, buildBlocks(fixedN), false},
        {pFollow, buildBlocks(followN), false},
        {pIdle, buildBlocks(postRestoreN), true},
      };
      const auto ref = neon_parity_scalar::RunNoisePrint(rate, npInL, stereo ? &npInR : nullptr, steps);
      assert(!ref.profileValid); // After Restore(invalid)

      noise_print::Processor simd;
      simd.Reset(rate);
      std::vector<double> outL = npInL, outR = npInR;
      size_t pos = 0;
      bool sawValidProfile = false;
      for (size_t s = 0; s < steps.size(); ++s)
      {
        const auto& step = steps[s];
        if (step.restoreInvalid)
          simd.Restore(noise_print::Profile{});
        noise_print::Controls c{
          step.params.learn, step.params.sensitivity, step.params.reduction, step.params.attack, step.params.release,
          step.params.mix, step.params.gateThreshold, step.params.gateRelease, step.params.protectAttack,
          step.params.trackingMode};
        for (int n : step.blocks)
        {
          allocations = deletions = 0; tracking = true;
          simd.Process(outL.data() + pos, stereo ? outR.data() + pos : nullptr, n, c);
          tracking = false;
          assert(allocations == 0 && deletions == 0);
          pos += n;
        }
        if (s == 2) sawValidProfile = simd.GetProfile().valid;
      }
      assert(sawValidProfile);
      assert(!simd.GetProfile().valid);
      double maxDiff = 0.;
      for (size_t i = 0; i < totalNP; ++i)
      {
        maxDiff = std::max(maxDiff, std::abs(outL[i] - ref.left[i]));
        if (stereo) maxDiff = std::max(maxDiff, std::abs(outR[i] - ref.right[i]));
      }
      assert(maxDiff < 1.e-10);
      assert(std::abs(simd.Reduction() - ref.reduction) < 1.e-8);
      assert(std::abs(simd.CleanPeak() - ref.cleanPeak) < 1.e-10);
      assert(std::abs(simd.TrackingScale() - ref.trackingScale) < 1.e-10);
    }
  }
  std::cout << "NEON parity: AIGate & NoisePrint SIMD vs scalar parity, untrained idle, Follow inverseScale, Restore(invalid), and zero audio allocations passed\n";
}
