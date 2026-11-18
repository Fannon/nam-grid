#pragma once

// Digital Delay Voice and Modulation (schema 32). Clean at zero Modulation is checked
// bit for bit against the previous build with verify-fx-processing.py.
namespace digital_delay_voice_test
{
using Audio = std::array<std::vector<double>, 2>;
inline Audio Render(double rate, int voice, double motion, double amplitude, double frequency, int switchTo = -1)
{
  SlotDelay left, right;
  left.Reset(rate);
  right.Reset(rate);
  UtilityControls c;
  c.delay = {100., 0., 12000., 0., 100.}; // One repeat at 100 ms, no ducking, fully wet.
  c.delayMotion = motion;
  c.delayOutput = 0.;
  c.expandedDelay[uDigitalDelayVoice - uSpaceHeads] = voice;
  const int frames = static_cast<int>(.25 * rate), burst = static_cast<int>(.05 * rate), block = 64;
  Audio audio{std::vector<double>(frames), std::vector<double>(frames)};
  for (int i = 0; i < burst; ++i)
    audio[0][i] = audio[1][i] = amplitude * std::sin(2. * grid_fx::pi * frequency * i / rate);
  const int before = allocations, destroyed = deletions;
  tracking = true;
  for (int first = 0; first < frames; first += block)
  {
    if (switchTo >= 0 && first >= static_cast<int>(.12 * rate))
      c.expandedDelay[uDigitalDelayVoice - uSpaceHeads] = switchTo;
    left.Process(audio[0].data() + first, std::min(block, frames - first), SlotEffect::DigitalDelay, c,
                 &right, audio[1].data() + first);
  }
  tracking = false;
  assert(allocations == before && deletions == destroyed);
  return audio;
}
inline double Peak(const std::vector<double>& signal, double rate, double start, double end)
{
  double peak = 0.;
  for (int i = static_cast<int>(start * rate); i < static_cast<int>(end * rate); ++i) peak = std::max(peak, std::abs(signal[i]));
  return peak;
}
}

void TestDigitalDelayVoice()
{
  using namespace digital_delay_voice_test;
  for (const double rate : {44100., 48000., 96000.})
  {
    // Lo-fi truncates toward zero: a repeat below its 8-bit step vanishes, while Clean keeps it.
    const auto quiet = Render(rate, 2, 0., .004, 440.), quietClean = Render(rate, 0, 0., .004, 440.);
    assert(Peak(quiet[0], rate, .1, .16) == 0. && Peak(quietClean[0], rate, .1, .16) > .003);
    // The converters band-limit each repeat: Lo-fi more than Rack.
    const double clean = Peak(Render(rate, 0, 0., .2, 7000.)[0], rate, .11, .14);
    const double rack = Peak(Render(rate, 1, 0., .2, 7000.)[0], rate, .11, .14);
    const double lofi = Peak(Render(rate, 2, 0., .2, 7000.)[0], rate, .11, .14);
    assert(clean > .15 && rack < .9 * clean && lofi < .5 * rack);
    // Modulation moves the two channels in opposite directions; at zero they stay identical.
    const auto still = Render(rate, 0, 0., .2, 1000.), moving = Render(rate, 0, 100., .2, 1000.);
    assert(still[0] == still[1]);
    double difference = 0.;
    for (size_t i = 0; i < moving[0].size(); ++i) difference = std::max(difference, std::abs(moving[0][i] - moving[1][i]));
    assert(difference > .01);
    // Switching voices while a repeat sounds fades the converter rather than stepping.
    for (const auto& [from, to] : {std::pair{0, 2}, std::pair{2, 0}, std::pair{1, 2}})
    {
      const auto switched = Render(rate, from, 0., .2, 440., to);
      for (const auto& channel : switched)
        for (const double sample : channel) assert(std::isfinite(sample) && std::abs(sample) < .25);
    }
  }
  std::cout << "Digital Delay voice: truncation, converter bandwidth, opposite modulation and voice changes passed\n";
}
