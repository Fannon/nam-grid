#pragma once
#include "SlotOperations.h"
#include "ModulationMatrix.h"

// Rig-wide Trails: a bypassed delay or reverb keeps ringing over the dry signal, but
// nothing played after its 5 ms input fade reaches the tail.
namespace trails_test
{
constexpr int kBlock = 128;

struct Render
{
  std::array<std::vector<double>, 2> input, output;
};

// Plays `played` until `bypassAt`, then `after`, with the slot bypassed from `bypassAt`.
// Returns the input and the slot output for both channels.
template <typename Played, typename After>
Render Play(SlotEffect effect, double rate, bool trails, int frames, int bypassAt, int switchAt, Played&& played,
            After&& after, bool stereo = true, int enableAt = -1)
{
  SlotConfig config; config.effect = effect;
  NAMStage stage; stage.Publish(PrepareSlot(config, rate, kBlock).dsp); stage.ResetStopped(rate, kBlock);
  FixedNAMGrid::Controls controls;
  const auto values = SlotDefaultValues(effect);
  grid_modulation::VisitSlotControls(controls, [&](int control, auto& field) {
    field = grid_modulation::FieldValue<std::remove_reference_t<decltype(field)>>(values[control]);
  });
  auto& c = controls.utility;
  c.tempo = 120.;
  c.trails = trails;
  // Half wet, so every module's tail is clearly above the dry signal.
  c.delay[4] = c.reverb[4] = c.spring[3] = c.shimmer[0] = c.airPlate[4] = c.airHall[5] = c.space[5] = c.cosmic[6] = 50.;
  Render r;
  for (int ch = 0; ch < 2; ++ch)
  {
    r.input[ch].resize(frames);
    for (int i = 0; i < frames; ++i) r.input[ch][i] = i < switchAt ? played(i, ch) : after(i, ch);
  }
  r.output = r.input;
  for (int pos = 0; pos < frames; pos += kBlock)
  {
    const bool enabled = pos < bypassAt || (enableAt >= 0 && pos >= enableAt);
    stage.Process(r.output[0].data() + pos, std::min(kBlock, frames - pos), enabled, 0., 0., {}, {}, c,
                  stereo ? r.output[1].data() + pos : nullptr);
  }
  return r;
}

inline double Phrase(int i, int ch, double rate)
{
  const double t = i / rate;
  return (ch ? .08 : .1) * std::sin(2. * grid_fx::pi * 330. * t) * (.6 + .4 * std::sin(2. * grid_fx::pi * 3. * t));
}
} // namespace trails_test

void TestTrails()
{
  using namespace trails_test;
  using E = SlotEffect;
  constexpr std::array effects{E::DigitalDelay, E::AnalogDelay, E::TapeDelay, E::MultiHeadDelay,
    E::ReverseDelay, E::DualDelay, E::DiffusedDelay, E::RoomReverb, E::PlateReverb, E::CloudReverb,
    E::HallReverb, E::SpringReverb, E::ShimmerReverb, E::AirPlate, E::AirHall, E::GalacticSpace, E::CosmicEcho};
  int listed = 0;
  for (int e = 0; e <= static_cast<int>(E::Last); ++e)
    listed += IsTrailEffect(static_cast<E>(e));
  assert(listed == static_cast<int>(effects.size()));
  const double rate = 48000.;
  // One second fills the default effects; another covers many post-fade blocks.
  const int frames = static_cast<int>(rate * 2.), bypassAt = 1500 * kBlock / 4; // 1 s, on a block boundary
  // The input fade and the slot crossfade take 5 ms; after that the input changes.
  const int switchAt = bypassAt + 4 * kBlock;
  for (const auto effect : effects)
    for (const bool stereo : {false, true})
    {
      assert(IsTrailEffect(effect));
      const auto phrase = [&](int i, int ch) { return Phrase(i, ch, rate); };
      const auto silence = [](int, int) { return 0.; };
      const auto loud = [&](int i, int ch) { return 3. * Phrase(i + 777, 1 - ch, rate); };
      const auto quiet = Play(effect, rate, true, frames, bypassAt, switchAt, phrase, silence, stereo);
      const auto played = Play(effect, rate, true, frames, bypassAt, switchAt, phrase, loud, stereo);
      const auto off = Play(effect, rate, false, frames, bypassAt, switchAt, phrase, loud, stereo);
      double tail = 0., leak = 0.;
      for (int ch = 0; ch < (stereo ? 2 : 1); ++ch)
        for (int i = switchAt; i < frames; ++i)
        {
          // Without Trails, bypass leaves the dry signal exactly.
          assert(off.output[ch][i] == off.input[ch][i]);
          const double ringing = quiet.output[ch][i] - quiet.input[ch][i];
          tail = std::max(tail, std::abs(ringing));
          leak = std::max(leak, std::abs(played.output[ch][i] - played.input[ch][i] - ringing));
        }
      if (tail < 1.e-3 || leak > 1.e-12)
        std::cerr << "Trails: " << SlotEffectName(effect) << " stereo=" << stereo << " tail=" << tail
                  << " leak=" << leak << '\n';
      assert(tail >= 1.e-3); // The tail rings on after bypass (-40 dB below the phrase or louder).
      assert(leak <= 1.e-12); // Playing after the input fade changes nothing but the dry signal.
    }

  // While the slot plays, and for modules without a tail, Trails changes nothing.
  for (const auto effect : {E::DigitalDelay, E::HallReverb, E::ShimmerReverb, E::Chorus, E::Freeze, E::Stutter})
  {
    const auto phrase = [&](int i, int ch) { return Phrase(i, ch, rate); };
    const bool tails = IsTrailEffect(effect);
    const int bypass = tails ? frames : bypassAt;
    const auto on = Play(effect, rate, true, frames, bypass, frames, phrase, phrase);
    const auto off = Play(effect, rate, false, frames, bypass, frames, phrase, phrase);
    for (int ch = 0; ch < 2; ++ch)
      for (int i = 0; i < frames; ++i) assert(on.output[ch][i] == off.output[ch][i]);
  }

  // A decayed tail stops: the delay drains with its feedback muted and suspends, and
  // re-enabling later replays no old repeat. Default Digital Delay: 350 ms, 35% feedback.
  {
    const int longFrames = static_cast<int>(rate * 30.), enableAt = static_cast<int>(rate * 28.) / kBlock * kBlock;
    const auto phrase = [&](int i, int ch) { return Phrase(i, ch, rate); };
    const auto silence = [](int, int) { return 0.; };
    const auto r = Play(E::DigitalDelay, rate, true, longFrames, bypassAt, switchAt, phrase, silence, true, enableAt);
    double early = 0.;
    for (int i = switchAt; i < static_cast<int>(rate * 2.); ++i) early = std::max(early, std::abs(r.output[0][i]));
    assert(early > 1.e-2);
    // The trail ends 9 s after its last audible repeat; the drain then takes another 9 s.
    for (int ch = 0; ch < 2; ++ch)
      for (int i = static_cast<int>(rate * 26.); i < longFrames; ++i)
        assert(i < enableAt ? r.output[ch][i] == 0. : std::abs(r.output[ch][i]) < 1.e-9);
  }
  std::cout << "Trails: " << effects.size() << " delays/reverbs ring out after bypass without new input, mono and stereo\n";
}
