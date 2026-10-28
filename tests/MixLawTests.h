#pragma once
#include "SlotOperations.h"
#include "ModulationMatrix.h"

void TestMixLaw()
{
  using E = SlotEffect;
  constexpr std::array effects{E::DigitalDelay, E::AnalogDelay, E::TapeDelay, E::MultiHeadDelay,
    E::ReverseDelay, E::DualDelay, E::DiffusedDelay, E::RoomReverb, E::PlateReverb, E::CloudReverb,
    E::HallReverb, E::SpringReverb, E::ShimmerReverb, E::AirPlate, E::AirHall, E::GalacticSpace, E::CosmicEcho};
  for (double rate : {48000., 96000.})
    for (auto effect : effects)
      for (bool stereo : {false, true})
        for (double mix : {0., 40., 100.})
        {
          const auto render = [&](bool constantDry, double output) {
            SlotConfig config; config.effect = effect;
            NAMStage stage; stage.Publish(PrepareSlot(config, rate, 127).dsp); stage.ResetStopped(rate, 127);
            assert(stage.Latency() == 0);
            FixedNAMGrid::Controls controls;
            const auto values = SlotDefaultValues(effect);
            grid_modulation::VisitSlotControls(controls, [&](int control, auto& field) {
              field = grid_modulation::FieldValue<std::remove_reference_t<decltype(field)>>(values[control]);
            });
            auto& c = controls.utility;
            c.constantDry = constantDry;
            c.delay[4] = c.reverb[4] = c.spring[3] = c.shimmer[0] = c.airPlate[4]
              = c.airHall[5] = c.space[5] = c.cosmic[6] = mix;
            c.delayOutput = c.spring[4] = c.airPlate[6] = c.airHall[6] = c.space[6] = output;
            std::array<std::vector<double>, 2> audio{std::vector<double>(8192), std::vector<double>(8192)};
            for (int i = 0; i < 8192; ++i)
            { audio[0][i] = .1 * std::sin(i * .031); audio[1][i] = .08 * std::cos(i * .047); }
            for (int pos = 0; pos < 8192; pos += 127)
              stage.Process(audio[0].data() + pos, std::min(127, 8192-pos), true, 0., 0., {}, {}, c,
                            stereo ? audio[1].data() + pos : nullptr);
            return audio;
          };
          // Only modules with a whole-signal Output trim apply this gain.
          const bool trimmed = effect == E::DigitalDelay || effect == E::AnalogDelay || effect == E::TapeDelay
            || effect == E::MultiHeadDelay || effect == E::ReverseDelay || effect == E::DualDelay
            || effect == E::DiffusedDelay || effect == E::SpringReverb || effect == E::AirPlate
            || effect == E::AirHall || effect == E::GalacticSpace;
          for (double output : {0., -6.})
          {
            const auto linear = render(false, output), constantDry = render(true, output);
            const double gain = trimmed ? std::pow(10., output / 20.) : 1.;
            for (int ch = 0; ch < (stereo ? 2 : 1); ++ch)
              for (int i = 4096; i < 8192; ++i)
              {
                const double dry = ch == 0 ? .1 * std::sin(i * .031) : .08 * std::cos(i * .047);
                const double expected = mix * .01 * dry * gain;
                if (std::abs(constantDry[ch][i] - linear[ch][i] - expected) >= 1.e-12)
                  std::cerr << "Mix law: " << SlotEffectName(effect) << " rate=" << rate << " mix=" << mix << '\n';
                assert(std::abs(constantDry[ch][i] - linear[ch][i] - expected) < 1.e-12);
                if (mix == 0. && output == 0.)
                { assert(linear[ch][i] == dry); assert(constantDry[ch][i] == dry); }
              }
          }
        }
  std::cout << "Mix law: all delay/reverb dry gains, trims, mono/stereo and zero latency passed\n";
}
