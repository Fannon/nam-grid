#pragma once
#include "EffectPreview.h"
#include "ModuleCatalog.h"
#include "SlotDefaults.h"
#include <cstring>
#include <string_view>

void TestEffectPresentation()
{
  for (int p = 0; p < kUtilityControls; ++p)
  {
    const auto choices = EffectChoices(p);
    for (size_t i = 0; i < choices.size(); ++i)
    {
      assert(std::strlen(choices[i]) > 0);
      for (size_t j = 0; j < i; ++j) assert(std::strcmp(choices[i], choices[j]) != 0);
      if (p == uDistStyle)
        assert(std::strlen(effect_presentation::Help(SlotEffect::Distortion, p, static_cast<int>(i))) > 12);
    }
  }
  // Check every offered effect, including conditional modes, against actual
  // editor positions, including per-effect overrides for shared parameters.
  for (const auto& group : nam_library::kModuleGroups)
    for (auto effect : group.effects)
      for (bool sync : {false, true})
        for (bool alternate : {false, true})
        {
          assert(std::strlen(effect_presentation::Sound(effect)) > 12);
          const auto defaults = SlotDefaultValues(effect);
          for (int p = 0; p < kUtilityControls; ++p)
            assert(defaults[17+p] >= kEffectParameters[p].minimum && defaults[17+p] <= kEffectParameters[p].maximum);
          std::array<bool, 7> occupied{};
          if (effect == SlotEffect::NoiseGate || effect == SlotEffect::SmoothComp || effect == SlotEffect::PunchComp
              || effect == SlotEffect::ToneEQ || effect == SlotEffect::CutEQ)
            occupied[0] = occupied[1] = occupied[2] = true;
          if (effect == SlotEffect::ParametricEQ) occupied[4] = occupied[5] = true;
          if (effect == SlotEffect::Boost || effect == SlotEffect::TubeBoost) occupied[2] = true;
          for (int p = 0; p < kUtilityControls; ++p)
          {
            if (!effect_presentation::Visible(effect, p, sync, alternate, alternate)) continue;
            assert(std::strlen(effect_presentation::Help(effect, p)) > 12);
            assert(std::strlen(effect_presentation::Label(effect, p)) > 0);
            if (effect_presentation::HeaderControl(p)) continue;
            const int position = effect_presentation::Position(effect, p);
            if (position < 0 || position >= 7 || occupied[position])
              std::cerr << "Control overlap: effect=" << static_cast<int>(effect) << " parameter="
                        << kEffectParameters[p].name << " position=" << position
                        << " sync=" << sync << " alternate=" << alternate << '\n';
            assert(position >= 0 && position < 7 && !occupied[position]);
            occupied[position] = true;
            // Output below the first control, Mix last before Enabled.
            const std::string_view name = kEffectParameters[p].name;
            const auto endsWith = [&](std::string_view suffix) {
              return name.size() > suffix.size() && name.substr(name.size() - suffix.size()) == suffix;
            };
            if (endsWith(" Output")) assert(position == effect_presentation::kOutputPosition);
            if (endsWith(" Mix")) assert(position == effect_presentation::kMixPosition);
          }
          // The top row fills from the left without gaps (a mode may hide one control).
          if (alternate) continue;
          for (int position = 1; position < 4; ++position)
            if (occupied[position] && !occupied[position - 1])
              std::cerr << "Top-row gap: effect=" << static_cast<int>(effect) << " position=" << position - 1 << '\n';
          for (int position = 1; position < 4; ++position) assert(!occupied[position] || occupied[position - 1]);
        }
  // Mode selectors and saved options live in the header, never in the knob grid.
  for (int p = 0; p < kUtilityControls; ++p)
    if (IsAlgorithmMode(p)) assert(effect_presentation::HeaderControl(p));
  assert(effect_presentation::Position(SlotEffect::ConsoleColour, uColourInput) == 0);
  assert(effect_presentation::Position(SlotEffect::ConsoleColour, uColourMix) == effect_presentation::kMixPosition);
  assert(effect_presentation::Position(SlotEffect::Chorus, uModCharacter) == 2);
  assert(effect_presentation::Position(SlotEffect::Phaser, uModCharacter) == 3);
  assert(!effect_presentation::Visible(SlotEffect::ReverseDelay, uDelayPingPong));
  assert(effect_presentation::Visible(SlotEffect::DualDelay, uDelayPingPong));
  assert(!effect_presentation::Visible(SlotEffect::UniVibe, uModMix, false, false, true));
  assert(!effect_presentation::Visible(SlotEffect::ParametricEQ, uGain, false, true));
  assert(std::strcmp(effect_presentation::Label(SlotEffect::DigitalDelay, uDelayCharacter), "Ducking") == 0);
  assert(std::strcmp(effect_presentation::Label(SlotEffect::TapeDelay, uDelayCharacter), "Saturation") == 0);
  UtilityControls c; c.delay = {400., 50., 5000., 0., 100.}; c.dual = {.75, 50.};
  const auto repeats = effect_preview::Delay(SlotEffect::DualDelay, c, 48000.);
  assert(repeats.count >= 4);
  assert(std::abs(repeats.taps[0].seconds - .4) < 1.e-12 && repeats.taps[0].left == 1.);
  assert(std::abs(repeats.taps[1].seconds - .3) < 1.e-12 && repeats.taps[1].right == 1.);
  c.pingPong = true;
  const auto ping = effect_preview::Delay(SlotEffect::DualDelay, c, 48000.);
  assert(std::abs(ping.taps[0].seconds - .4) < 1.e-12 && ping.taps[0].right == 0.);
  assert(std::abs(ping.taps[1].seconds - .7) < 1.e-12 && ping.taps[1].left == 0. && ping.taps[1].right == .5);
  c.delay[2] = 800.;
  const auto dark = effect_preview::Delay(SlotEffect::DualDelay, c, 48000.);
  assert(dark.taps[0].treble < ping.taps[0].treble && dark.taps[1].treble < dark.taps[0].treble);
  c.sync = true; c.tempo = 120.; c.division = 5;
  assert(std::abs(effect_preview::DelaySeconds(c) - .5) < 1.e-12);
  assert(std::abs(effect_preview::CapturePeriod(SlotEffect::Freeze, 500., 20.) - .48) < 1.e-12);
  assert(std::abs(effect_preview::CapturePeriod(SlotEffect::Stutter, 125., 5.) - .125) < 1.e-12);
}
