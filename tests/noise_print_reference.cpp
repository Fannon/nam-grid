// Manual numerical audit; scripts/verify-noise-print.py generates the reference
// from the committed, independently compiled pre-optimization implementation.
#include "NoisePrintReference.h"
#include "../NeuralAmpModeler/NoisePrint.h"
#include <bit>
#include <cassert>
#include <iomanip>
#include <iostream>
#include <limits>

int main()
{
  double maximumError = 0., maximumProfileError = 0., maximumMeterError = 0.;
  uint64_t samples = 0;
  uint64_t outputMismatches = 0, profileMismatches = 0, meterMismatches = 0;
  const auto different = [](double a, double b) { return std::bit_cast<uint64_t>(a) != std::bit_cast<uint64_t>(b); };
  for (double rate : {44100., 48000., 96000., 192000.})
    for (bool stereo : {false, true})
    {
      noise_print_reference::Processor before;
      noise_print::Processor after;
      before.Reset(rate);
      after.Reset(rate);
      noise_print_reference::Controls oldControls;
      noise_print::Controls newControls;
      std::array<double, 512> oldLeft{}, oldRight{}, newLeft{}, newRight{};
      uint32_t seed = 123;
      int block = 0, position = 0;
      constexpr double durations[]{.05, 1.1, .2, .06, .2, .35, .3, .05, .2, .2};
      constexpr int sizes[]{1, 31, 128, 17, 512, 7};
      for (int phase = 0; phase < 10; ++phase)
      {
        if (phase == 9)
        {
          before.Reset(rate * .5);
          after.Reset(rate * .5);
        }
        for (int done = 0, total = static_cast<int>(durations[phase] * rate); done < total;)
        {
          const int n = std::min(sizes[block % 6], total - done);
          const auto controls = [&](auto& c) {
            c.learn = phase == 1 || phase == 5;
            c.sensitivity = -3. + (block % 6);
            c.attack = .5 + (block % 4);
            c.release = 40. + (block % 5) * 50.;
            c.reduction = phase == 7 ? 0. : 24.;
            c.mix = phase == 3 ? 0. : phase == 8 ? (block % 3) * 50. : 100.;
            c.gateThreshold = phase == 6 ? -40. : -100.;
          };
          controls(oldControls);
          controls(newControls);
          for (int i = 0; i < n; ++i)
          {
            seed = seed * 1664525u + 1013904223u;
            const double noise = .006 * (double(seed) / 4294967296. * 2. - 1.);
            const double t = double(position + i) / rate;
            const double amplitude = phase == 6 && done < total / 2 ? .0001 : .12;
            const double note = oldControls.learn ? 0. : amplitude * std::sin(2764.601535159 * t);
            oldLeft[i] = newLeft[i] = noise + .006 * std::sin(376.9911184307752 * t) + note;
            oldRight[i] = newRight[i] = -.4 * noise + .5 * note;
          }
          if (phase == 8 && block % 23 == 0)
            oldLeft[0] = newLeft[0] = std::numeric_limits<double>::quiet_NaN();
          before.Process(oldLeft.data(), stereo ? oldRight.data() : nullptr, n, oldControls);
          after.Process(newLeft.data(), stereo ? newRight.data() : nullptr, n, newControls);
          for (int i = 0; i < n; ++i)
          {
            assert(std::isfinite(newLeft[i]) && std::isfinite(oldLeft[i]));
            maximumError = std::max(maximumError, std::abs(newLeft[i] - oldLeft[i]));
            outputMismatches += different(newLeft[i], oldLeft[i]);
            if (stereo)
            {
              assert(std::isfinite(newRight[i]) && std::isfinite(oldRight[i]));
              maximumError = std::max(maximumError, std::abs(newRight[i] - oldRight[i]));
              outputMismatches += different(newRight[i], oldRight[i]);
            }
          }
          maximumMeterError = std::max(maximumMeterError, std::abs(before.Reduction() - after.Reduction()));
          meterMismatches += different(before.Reduction(), after.Reduction());
          samples += n * (stereo ? 2 : 1);
          done += n;
          position += n;
          ++block;
        }
        const auto& a = before.GetProfile();
        const auto& b = after.GetProfile();
        assert(a.valid == b.valid);
        if (a.valid)
          for (int i = 0; i < noise_print::bands; ++i)
          {
            maximumProfileError =
              std::max(maximumProfileError, std::abs(a.power[i] - b.power[i]) / std::max(1.e-30, a.power[i]));
            profileMismatches += different(a.power[i], b.power[i]);
          }
      }
    }
  std::cout << std::setprecision(17) << samples << " mono/stereo samples, 44.1/48/96/192 kHz\n"
            << "Maximum absolute output difference: " << maximumError << "\n"
            << "Maximum relative profile-power difference: " << maximumProfileError << "\n"
            << "Maximum meter difference (dB): " << maximumMeterError << "\n";
  std::cout << "Bit mismatches (output/profile/meter): " << outputMismatches << "/" << profileMismatches << "/"
            << meterMismatches << "\n";
  assert(maximumError < 2.e-11 && maximumProfileError < 1.e-11 && maximumMeterError < 1.e-8);
#ifdef NAM_NOISE_PRINT_REQUIRE_EXACT
  assert(outputMismatches == 0 && profileMismatches == 0 && meterMismatches == 0);
#endif
}
