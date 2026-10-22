#include "SlotOutboard.h"
#include "SlotAirDynamics.h"
#include <chrono>
#include <iomanip>
#include <iostream>

// Deterministic 128-frame stereo workload, no NAM/host overhead. The gain figure
// is for this stimulus only; it is not a promise of program loudness matching.
int main()
{
  std::cout << "module,variant,rate,cpu_percent,gain_db,detector_gr_db\n";
  for (double rate : {44100., 48000., 88200., 96000., 192000.})
    for (auto effect : {SlotEffect::OutboardChannel, SlotEffect::ConsoleColour, SlotEffect::ConsoleEQ,
                        SlotEffect::ButterComp, SlotEffect::DigitalBlack, SlotEffect::Pressure})
      for (int desk = 0; desk < (effect == SlotEffect::ConsoleColour  ? 8
                                 : effect == SlotEffect::DigitalBlack ? 2
                                                                      : 1);
           ++desk)
      {
        SlotOutboard processor;
        SlotAirDynamics dynamics;
        if (IsAirDynamics(effect))
          dynamics.Reset(rate, effect);
        else
          processor.Reset(rate, effect);
        UtilityControls c;
        c.colourDesk = desk;
        c.blackMode = desk;
        // Exercise all bands for EQ timing; its insertion default is flat.
        if (effect == SlotEffect::ConsoleEQ)
          c.consoleEQ = {2., -1., 1., 4750., 300., 100., 0.};
        double fastest = 1.e10, energyIn = 0., energyOut = 0., reduction = 0.;
        std::array<double, 128> l{}, r{};
        for (int run = 0; run < 3; ++run)
        {
          energyIn = energyOut = reduction = 0.;
          const auto start = std::chrono::steady_clock::now();
          for (int block = 0; block < 1000; ++block)
          {
            for (int i = 0; i < 128; ++i)
            {
              const double t = (block * 128 + i) / rate;
              const double envelope = .15 + .85 * std::exp(-std::fmod(t, .25) * 18.);
              l[i] =
                envelope * (.22 * std::sin(2. * grid_fx::pi * 220. * t) + .08 * std::sin(2. * grid_fx::pi * 1760. * t));
              r[i] =
                envelope * (.18 * std::sin(2. * grid_fx::pi * 330. * t) + .06 * std::sin(2. * grid_fx::pi * 2640. * t));
              energyIn += l[i] * l[i] + r[i] * r[i];
            }
            if (IsAirDynamics(effect))
              dynamics.Process(l.data(), r.data(), 128, c);
            else
              processor.Process(l.data(), r.data(), 128, c);
            reduction = std::max(reduction, IsAirDynamics(effect) ? dynamics.Reduction() : processor.Reduction());
            for (int i = 0; i < 128; ++i)
              energyOut += l[i] * l[i] + r[i] * r[i];
          }
          const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
          if (run)
            fastest = std::min(fastest, seconds);
        }
        std::cout << SlotEffectName(effect) << ',' << desk << ',' << rate << ',' << std::fixed << std::setprecision(4)
                  << fastest / (128000. / rate) * 100. << ',' << 10. * std::log10(energyOut / energyIn) << ','
                  << reduction << '\n';
      }
}
