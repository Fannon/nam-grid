#pragma once
#include "SlotOperations.h"
#include "ModulationMatrix.h"
#include <complex>

// Digital Delay Time change. A steady tone fills the whole line, so every tap reads the same
// frequency: a crossfade between taps keeps the tone, while a glide across the line shifts its
// pitch (Doppler). The measure is the share of energy more than 150 Hz from the tone in the
// 100 ms after a jump, under a Hann window (10 Hz bins).
namespace delay_time_change_test
{
constexpr double kTone = 1000., kBand = 150.;

class Rig
{
public:
  Rig(double rate, bool stereo, bool pingPong, double feedback, double time) : mRate(rate), mStereo(stereo)
  {
    SlotConfig config;
    config.effect = SlotEffect::DigitalDelay;
    mStage.Publish(PrepareSlot(config, rate, kBlock).dsp);
    mStage.ResetStopped(rate, kBlock);
    assert(mStage.Latency() == 0);
    const auto values = SlotDefaultValues(SlotEffect::DigitalDelay);
    grid_modulation::VisitSlotControls(mControls, [&](int control, auto& field) {
      field = grid_modulation::FieldValue<std::remove_reference_t<decltype(field)>>(values[control]);
    });
    auto& c = mControls.utility;
    c.delay = {time, feedback, 5000., 0., 100.}; // Ducking off: its envelope ripple adds sidebands.
    c.pingPong = pingPong;
  }
  UtilityControls& Controls() { return mControls.utility; }
  std::array<std::vector<double>, 2> Render(double seconds)
  {
    const int frames = static_cast<int>(seconds * mRate);
    std::array<std::vector<double>, 2> audio{std::vector<double>(frames), std::vector<double>(frames)};
    for (int i = 0; i < frames; ++i)
    {
      const double phase = 2. * grid_fx::pi * kTone * static_cast<double>(mPosition + i) / mRate;
      audio[0][i] = .1 * std::sin(phase);
      audio[1][i] = .1 * std::sin(phase + .7);
    }
    for (int pos = 0; pos < frames; pos += kBlock)
      mStage.Process(audio[0].data() + pos, std::min(kBlock, frames - pos), true, 0., 0., {}, {}, mControls.utility,
                     mStereo ? audio[1].data() + pos : nullptr);
    mPosition += frames;
    if (!mStereo) audio[1].clear();
    return audio;
  }

private:
  static constexpr int kBlock = 64;
  double mRate;
  bool mStereo;
  NAMStage mStage;
  FixedNAMGrid::Controls mControls;
  int64_t mPosition = 0;
};

// Share of windowed energy farther than kBand from kTone (Parseval over the full DFT).
inline double AwayFromTone(const std::vector<double>& x, double rate)
{
  const int n = static_cast<int>(x.size());
  std::vector<double> w(n);
  double total = 0.;
  for (int i = 0; i < n; ++i)
  {
    w[i] = x[i] * (.5 - .5 * std::cos(2. * grid_fx::pi * i / n));
    total += w[i] * w[i];
  }
  double band = 0.;
  const double bin = rate / n;
  for (int k = static_cast<int>(std::ceil((kTone - kBand) / bin)); k * bin <= kTone + kBand; ++k)
  {
    std::complex<double> sum;
    for (int i = 0; i < n; ++i) sum += w[i] * std::polar(1., -2. * grid_fx::pi * k * i / n);
    band += 2. * std::norm(sum) / n;
  }
  return std::max(0., 1. - band / total);
}
} // namespace delay_time_change_test

void TestDelayTimeChange()
{
  using namespace delay_time_change_test;
  double worstCrossfade = 0., leastGlide = 1.;
  for (const double rate : {44100., 48000.})
    for (const auto& [stereo, pingPong] : {std::pair{false, false}, std::pair{true, false}, std::pair{true, true}})
      for (const double feedback : {0., 35.})
        for (const auto& [from, to] : {std::pair{4000., 5.}, std::pair{5., 4000.}, std::pair{350., 1200.}})
          for (const bool crossfade : {false, true})
          {
            Rig rig(rate, stereo, pingPong, feedback, from);
            rig.Controls().extra.delayTimeChange = crossfade ? 1. : 0.;
            rig.Render(4.3); // Fill the whole 4 s line with the tone.
            rig.Controls().delay[0] = to;
            const auto audio = rig.Render(.1);
            for (int ch = 0; ch < (stereo ? 2 : 1); ++ch)
            {
              // Ping-pong without feedback leaves the right tank silent.
              if (*std::max_element(audio[ch].begin(), audio[ch].end()) < 1.e-3) continue;
              const double away = AwayFromTone(audio[ch], rate);
              if (crossfade) worstCrossfade = std::max(worstCrossfade, away);
              else leastGlide = std::min(leastGlide, away);
              if (crossfade && away > 1.e-6)
                std::cerr << "Time change: " << from << " -> " << to << " ms at " << rate << " Hz, feedback " << feedback
                          << ", channel " << ch << ": " << away << " of the energy away from the tone\n";
            }
          }
  // Glide sweeps pitch (the released behavior); Crossfade keeps the tone.
  assert(leastGlide > .5);
  assert(worstCrossfade < 1.e-6);

  // A second jump during the fade holds the incoming tap and fades again; no third tap.
  for (const bool stereo : {false, true})
  {
    Rig rig(48000., stereo, stereo, 35., 4000.);
    rig.Controls().extra.delayTimeChange = 1.;
    rig.Render(4.3);
    rig.Controls().delay[0] = 5.;
    auto audio = rig.Render(.01);
    rig.Controls().delay[0] = 2000.;
    const auto rest = rig.Render(.09);
    audio[0].insert(audio[0].end(), rest[0].begin(), rest[0].end());
    assert(AwayFromTone(audio[0], 48000.) < 1.e-6);
  }

  // Retargeting must eventually reach the latest request, not just keep a stable pitch.
  {
    Rig rig(48000., true, false, 0., 350.);
    rig.Controls().extra.delayTimeChange = 1.;
    rig.Render(4.3);
    rig.Controls().delay[0] = 5.37;
    rig.Render(.01);
    rig.Controls().delay[0] = 700.25;
    rig.Render(.19);
    const auto actual = rig.Render(.01);
    Rig reference(48000., true, false, 0., 700.25);
    reference.Render(4.5);
    const auto expected = reference.Render(.01);
    for (int ch = 0; ch < 2; ++ch)
      for (int i = 0; i < actual[ch].size(); ++i) assert(std::abs(actual[ch][i] - expected[ch][i]) < 1.e-10);
  }

  // Fractional taps have different tone phases: rapid retargets must blend those too.
  // A smooth amplitude envelope has small sidebands, but no Doppler sweep.
  for (const bool pingPong : {false, true})
  {
    Rig rig(48000., true, pingPong, 35., 350.);
    rig.Controls().extra.delayTimeChange = 1.;
    rig.Render(4.3);
    std::array<std::vector<double>, 2> audio;
    for (const double time : {5.37, 1200.13, 2000.71, 330.29, 4000.})
    {
      rig.Controls().delay[0] = time;
      const auto part = rig.Render(.01);
      for (int ch = 0; ch < 2; ++ch) audio[ch].insert(audio[ch].end(), part[ch].begin(), part[ch].end());
    }
    const auto rest = rig.Render(.15);
    for (int ch = 0; ch < 2; ++ch)
    {
      audio[ch].insert(audio[ch].end(), rest[ch].begin(), rest[ch].end());
      assert(AwayFromTone(audio[ch], 48000.) < 1.e-5);
    }
  }

  // Tempo and division jumps use the same crossfade; synced times can exceed 4000 ms.
  {
    Rig rig(48000., true, false, 35., 400.);
    auto& c = rig.Controls();
    c.extra.delayTimeChange = 1.;
    c.sync = true; c.tempo = 120.; c.division = 9;
    rig.Render(8.3);
    c.tempo = 30.; // Whole note: 2 s -> 8 s.
    for (const auto& channel : rig.Render(.1)) assert(AwayFromTone(channel, 48000.) < 1.e-6);
    c.division = 0; // 8 s -> 250 ms.
    for (const auto& channel : rig.Render(.1)) assert(AwayFromTone(channel, 48000.) < 1.e-6);
  }

  // Steps up to 1 ms per update still glide, bit-identical to Glide (modulation keeps its Doppler).
  {
    std::array<std::vector<double>, 2> outputs;
    for (const bool crossfade : {false, true})
    {
      Rig rig(48000., true, false, 35., 350.);
      rig.Controls().extra.delayTimeChange = crossfade ? 1. : 0.;
      rig.Render(.5);
      std::vector<double> out;
      for (int step = 0; step < 200; ++step)
      {
        rig.Controls().delay[0] = 350. + .9 * step;
        const auto audio = rig.Render(64. / 48000.);
        out.insert(out.end(), audio[0].begin(), audio[0].end());
        out.insert(out.end(), audio[1].begin(), audio[1].end());
      }
      outputs[crossfade] = out;
    }
    assert(outputs[0] == outputs[1]);
  }
  std::cout << "Delay time change: crossfade keeps the tone (worst " << worstCrossfade << " away), glide sweeps it (least "
            << leastGlide << "); retarget and sub-threshold glide passed\n";
}
