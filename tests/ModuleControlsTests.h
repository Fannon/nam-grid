#pragma once
#include "SlotOperations.h"
#include "ModulationMatrix.h"
#include "ModuleCatalog.h"

namespace module_controls_test
{
inline double Energy(const std::vector<double>& audio)
{
  double sum = 0.;
  for (size_t i = audio.size() / 2; i < audio.size(); ++i) sum += audio[i] * audio[i];
  return sum;
}
inline std::vector<double> Compress(double rate, double hz, double ratio, double detector)
{
  SlotDynamics compressor; compressor.Reset(rate);
  DynamicsControls c; c.compThreshold = -24.; c.compMakeup = 0.; c.smoothRatio = ratio; c.detectorCut = detector;
  std::vector<double> audio(static_cast<size_t>(rate));
  for (size_t i = 0; i < audio.size(); ++i) audio[i] = .6 * std::sin(2. * grid_fx::pi * hz * i / rate);
  compressor.Process(audio.data(), static_cast<int>(audio.size()), SlotEffect::SmoothComp, c);
  return audio;
}
}

void TestModuleControls()
{
  using namespace module_controls_test;
  for (double rate : {44100., 48000., 96000.})
  {
    // A detector cut changes compression, rather than filtering audible bass.
    const auto unity = Compress(rate, 80., 1., 0.);
    const auto unityCut = Compress(rate, 80., 1., 500.);
    assert(unity == unityCut);
    assert(Energy(Compress(rate, 80., 8., 0.)) < Energy(Compress(rate, 80., 2., 0.)));
    assert(Energy(Compress(rate, 80., 8., 500.)) > 2. * Energy(Compress(rate, 80., 8., 0.)));

    // Juno width affects wet stereo only. Width zero centers a wet signal,
    // while Mix zero still returns unequal stereo inputs exactly.
    for (double mix : {0., 100.})
    {
      grid_hq::JunoChorus juno; juno.Reset(rate);
      UtilityControls c; c.juno[1] = mix; c.extra.junoWidth = 0.;
      std::vector<double> l(8192), r(8192);
      for (size_t i = 0; i < l.size(); ++i) { l[i] = .2 * std::sin(i * .071); r[i] = -.3 * std::cos(i * .043); }
      const auto dryL = l, dryR = r;
      juno.Process(l.data(), r.data(), static_cast<int>(l.size()), c);
      for (size_t i = 0; i < l.size(); ++i)
        assert(mix == 0. ? l[i] == dryL[i] && r[i] == dryR[i] : l[i] == r[i]);
    }
    // Hidden controls from related modules must not leak into another voice.
    {
      UtilityControls aControls, bControls;
      bControls.extra.boostMidGain = 12.; bControls.extra.boostMidFrequency = 2000.;
      SlotTubeBoost a, b; a.Reset(rate); b.Reset(rate);
      std::array<double, 512> input{}, actual{}, expected{};
      for (size_t i = 0; i < input.size(); ++i) input[i] = actual[i] = expected[i] = .2 * std::sin(i * .071);
      a.Process(actual.data(), input.data(), 512, aControls, {});
      b.Process(expected.data(), input.data(), 512, bControls, {});
      assert(actual == expected);
      SlotModulation va, vb; va.Reset(rate); vb.Reset(rate);
      actual = expected = input; bControls.extra.modSpread = 100.;
      va.Process(actual.data(), 512, SlotEffect::Vibrato, aControls, true);
      vb.Process(expected.data(), 512, SlotEffect::Vibrato, bControls, true);
      assert(actual == expected);
    }
    // Mono input remains centered at zero spread and becomes stereo with Spread.
    for (double spread : {0., 100.})
    {
      grid_roadmap::Vibe a, b; a.Reset(rate); b.Reset(rate);
      UtilityControls c; c.extra.vibeSpread = spread;
      std::vector<double> l(8192), r(8192);
      for (size_t i = 0; i < l.size(); ++i) l[i] = r[i] = .2 * std::sin(i * .071);
      a.Process(l.data(), static_cast<int>(l.size()), c, false);
      b.Process(r.data(), static_cast<int>(r.size()), c, true);
      double difference = 0.;
      for (size_t i = 0; i < l.size(); ++i) difference += std::abs(l[i] - r[i]);
      assert(spread == 0. ? difference == 0. : difference > 1.);
    }
    // LoFi Drive creates harmonics, and its dry path remains unchanged at Mix zero.
    UtilityControls clean; clean.lofi = {30., 0., 0., 0., 0., 100., 0.};
    auto driven = clean; driven.extra.lofiDrive = 18.;
    const auto plain = lofi_test::Render(rate, clean, 300.);
    const auto saturated = lofi_test::Render(rate, driven, 300.);
    const auto harmonic = [&](const auto& audio, double hz) {
      double real = 0., imag = 0.;
      for (size_t i = audio.size() / 2; i < audio.size(); ++i)
      {
        const double phase = 2. * grid_fx::pi * hz * i / rate;
        real += audio[i] * std::cos(phase); imag += audio[i] * std::sin(phase);
      }
      return real * real + imag * imag;
    };
    assert(harmonic(saturated, 900.) > 100. * harmonic(plain, 900.));
    driven.lofi[5] = 0.;
    SlotLoFiTape tape; tape.Reset(rate);
    std::array<double, 512> dry{}, audio{};
    for (size_t i = 0; i < dry.size(); ++i) dry[i] = audio[i] = .7 * std::sin(i * .17);
    tape.Process(audio.data(), nullptr, static_cast<int>(audio.size()), driven);
    assert(audio == dry);

    // Signed channel offset delays only the requested channel; mode changes
    // preserve the signal and do not masquerade as polarity or pan controls.
    for (int mode = 0; mode < 4; ++mode)
    {
      StereoUtility utility; utility.Reset(rate); UtilityControls c; c.extra.utilityMode = mode;
      std::array<double, 16> l{}, r{}; l[0] = .25; r[0] = -.5;
      utility.Process(l.data(), r.data(), 16, c);
      assert(l[0] == (mode == 1 || mode == 3 ? -.5 : .25));
      assert(r[0] == (mode == 1 || mode == 2 ? .25 : -.5));
    }
    for (double sign : {-1., 1.})
    {
      StereoUtility utility; utility.Reset(rate); UtilityControls c; c.extra.utilitySkew = sign * 2.5 * 1000. / rate;
      std::array<double, 16> l{}, r{}; l[0] = r[0] = 1.;
      utility.Process(l.data(), r.data(), 16, c);
      const auto& delayed = sign > 0. ? r : l;
      const auto& original = sign > 0. ? l : r;
      assert(original[0] == 1. && delayed[0] == 0.);
      assert(std::abs(delayed[2] - .5) < 1.e-10 && std::abs(delayed[3] - .5) < 1.e-10);
    }

    // Every affected module, all new controls at both extremes, variable blocks,
    // active automation and bypass: finite, partition-independent and no audio allocation.
    for (const auto& group : nam_library::kModuleGroups)
      for (auto effect : group.effects)
      {
        bool affected = false;
        for (int p = uToneMidFrequency; p < kUtilityControls; ++p) affected |= UsesUtility(effect, p);
        if (!affected) continue;
        SlotConfig config; config.effect = effect;
        NAMStage a, b;
        a.Publish(PrepareSlot(config, rate, 512).dsp); b.Publish(PrepareSlot(config, rate, 512).dsp);
        a.ResetStopped(rate, 512); b.ResetStopped(rate, 512);
        FixedNAMGrid::Controls controls;
        const auto defaults = SlotDefaultValues(effect);
        grid_modulation::VisitSlotControls(controls, [&](int p, auto& field) {
          field = grid_modulation::FieldValue<std::remove_reference_t<decltype(field)>>(defaults[p]);
        });
        for (int step = 0; step < 4; ++step)
        {
          for (int p = uToneMidFrequency; p < kUtilityControls; ++p)
            if (UsesUtility(effect, p)) grid_modulation::WriteControl(controls, parameter_layout::standardControls + p,
              step % 2 ? kEffectParameters[p].maximum : kEffectParameters[p].minimum);
          std::array<double, 4096> l{}, r{}, expectedL{}, expectedR{};
          for (size_t i = 0; i < l.size(); ++i) { l[i] = expectedL[i] = .2 * std::sin((i + step * 4096) * .071); r[i] = expectedR[i] = -.7 * l[i]; }
          const auto process = [&](NAMStage& stage, double* left, double* right, int count) {
            stage.Process(left, count, step != 3, 0., 0., controls.dynamics, controls.eq, controls.utility, right);
          };
          for (size_t i = 0; i < l.size(); i += 512) process(a, expectedL.data() + i, expectedR.data() + i, 512);
          for (size_t i = 0; i < l.size();)
          {
            const int count = std::min<int>(l.size() - i, std::array<int, 5>{1, 3, 127, 64, 512}[i % 5]);
            allocations = deletions = 0; tracking = true;
            Eigen::internal::set_is_malloc_allowed(false);
            process(b, l.data() + i, r.data() + i, count);
            Eigen::internal::set_is_malloc_allowed(true); tracking = false;
            assert(allocations == 0 && deletions == 0); i += count;
          }
          for (size_t i = 0; i < l.size(); ++i)
          {
            assert(std::isfinite(l[i]) && std::isfinite(r[i]));
            if (std::abs(l[i] - expectedL[i]) >= 1.e-8 || std::abs(r[i] - expectedR[i]) >= 1.e-8)
              std::cerr << "New-control partition mismatch: " << SlotEffectName(effect) << " step=" << step << " sample=" << i << '\n';
            assert(std::abs(l[i] - expectedL[i]) < 1.e-8 && std::abs(r[i] - expectedR[i]) < 1.e-8);
          }
        }
      }
  }
}
