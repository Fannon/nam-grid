#pragma once
#include "SlotPedalDistortion.h"
#include "SpectralAnalysis.h"

namespace pedal_distortion_test
{
// A decaying, slightly inharmonic pluck: enough level for the voices to clip,
// with quiet tails that test the clean-up between notes.
inline std::vector<double> Pluck(double rate, double seconds, double hz = 110.)
{
  std::vector<double> x(static_cast<size_t>(rate * seconds));
  for (size_t i = 0; i < x.size(); ++i)
  {
    const double t = std::fmod(i / rate, .5);
    double v = 0.;
    for (int k = 1; k <= 6; ++k) v += std::sin(2. * spectral_test::pi * hz * k * 1.002 * t) / k;
    x[i] = .18 * std::exp(-4. * t) * v;
  }
  return x;
}
inline std::vector<double> Render(std::vector<double> x, double rate, const UtilityControls& c, int block = 64)
{
  SlotPedalDistortion slot;
  slot.Reset(rate);
  for (size_t pos = 0; pos < x.size(); pos += block)
    slot.Process(x.data() + pos, static_cast<int>(std::min<size_t>(block, x.size() - pos)), c);
  return x;
}
inline double RMS(const std::vector<double>& x, size_t from = 0)
{
  double sum = 0.;
  for (size_t i = from; i < x.size(); ++i) sum += x[i] * x[i];
  return std::sqrt(sum / std::max<size_t>(1, x.size() - from));
}
inline double DB(double v) { return 20. * std::log10(std::max(v, 1.e-30)); }
} // namespace pedal_distortion_test

void TestPedalDistortion()
{
  using namespace pedal_distortion_test;
  namespace pd = grid_pedal_distortion;
  static_assert(SlotPedalDistortion::latency == 0, "Built-in effects add no reported latency");
  static_assert(pd::styleCount == effect_modes::pedalDistortionCount);
  for (double rate : {44100., 48000., 96000.})
  {
    const auto input = Pluck(rate, 1.);
    const double inputRMS = RMS(input);
    std::array<std::vector<double>, pd::styleCount> outputs;
    double lowest = 1.e9, highest = -1.e9;
    for (int style = 0; style < pd::styleCount; ++style)
    {
      UtilityControls c;
      c.pedalDistortion[0] = style;
      outputs[style] = Render(input, rate, c);
      for (double v : outputs[style]) assert(std::isfinite(v) && std::abs(v) < 4.);
      // Silence stays silent from a reset, including through the biased stages.
      for (double v : Render(std::vector<double>(2048, 0.), rate, c)) assert(std::abs(v) < 1.e-15);
      // Level matching: Gain 50% with a flat EQ stays near the input loudness.
      const double change = DB(RMS(outputs[style]) / inputRMS);
      assert(change > -8. && change < 6.);
      lowest = std::min(lowest, change); highest = std::max(highest, change);
      for (int other = 0; other < style; ++other)
      {
        double difference = 0.;
        for (size_t i = 0; i < input.size(); ++i) difference += std::abs(outputs[style][i] - outputs[other][i]);
        assert(difference / input.size() > 1.e-3); // Each style is distinct.
      }
      // Mix 0% is exact dry audio scaled by Level, with no added delay.
      c.pedalDistortion[8] = 0.;
      c.pedalDistortion[7] = -6.;
      const auto dry = Render(input, rate, c);
      const double level = std::pow(10., -6. * .05);
      for (size_t i = 0; i < input.size(); ++i) assert(dry[i] == input[i] * level);
      // More Gain compresses harder: the crest factor falls.
      const auto crest = [&](double gain) {
        UtilityControls g; g.pedalDistortion[0] = style; g.pedalDistortion[1] = gain;
        const auto y = Render(input, rate, g);
        double peak = 0.; for (double v : y) peak = std::max(peak, std::abs(v));
        return peak / RMS(y);
      };
      assert(crest(90.) < crest(10.));
    }
    assert(highest - lowest < 5.); // Styles are level-matched to each other.
  }
  // Tight moves the pre-clip bass cut: positive removes low fundamentals before clipping,
  // so the 82 Hz tone loses more of its fundamental than at negative settings.
  const double rate = 48000.;
  for (int style = 0; style < pd::styleCount; ++style)
  {
    const int count = 16384, bin = static_cast<int>(std::round(82. * count / rate)) | 1;
    std::vector<double> tone(count * 3);
    for (size_t i = 0; i < tone.size(); ++i) tone[i] = .1 * std::sin(2. * spectral_test::pi * bin * i / count);
    const auto fundamental = [&](double tight) {
      UtilityControls c; c.pedalDistortion[0] = style; c.pedalDistortion[2] = tight;
      const auto y = Render(tone, rate, c);
      return spectral_test::ToneAmplitude(std::span<const double>(y).last(count), bin);
    };
    assert(fundamental(100.) < fundamental(-100.));
  }
  // A settled tone through one style, and one partial of it relative to another.
  constexpr int toneCount = 16384;
  const auto settledTone = [&](int style, double gain, int bin, double level) {
    UtilityControls c; c.pedalDistortion[0] = style; c.pedalDistortion[1] = gain;
    std::vector<double> tone(toneCount * 3);
    for (size_t i = 0; i < tone.size(); ++i) tone[i] = level * std::sin(2. * spectral_test::pi * bin * i / toneCount);
    const auto y = Render(tone, rate, c);
    return std::vector<double>(y.end() - toneCount, y.end());
  };
  const auto partial = [&](const std::vector<double>& y, int bin) { return spectral_test::ToneAmplitude(y, bin); };
  // Even harmonics stay in loud notes. The level-following bias keeps the duty cycle
  // uneven where a fixed bias alone clips into a symmetric square wave; the voices
  // without it stay odd.
  {
    constexpr int bin = 75; // 220 Hz
    const auto second = [&](pd::Style style) {
      const auto y = settledTone(static_cast<int>(style), 70., bin, .25);
      return DB(partial(y, 2 * bin) / partial(y, bin));
    };
    assert(second(pd::Style::ClassicDist) > -24.);
    assert(second(pd::Style::RawDist) > -20.);
    assert(second(pd::Style::BritishCrunch) > -30. && second(pd::Style::BritishCrunch) < second(pd::Style::RawDist));
    assert(second(pd::Style::HotLead) < -30.);
    assert(second(pd::Style::MetalStack) < -80.);
  }
  // Raw Dist's bass corner follows Gain: a low setting stays full-range and a
  // high one thins the lows, measured at small signal.
  {
    const auto bassShare = [&](double gain) {
      const int raw = static_cast<int>(pd::Style::RawDist);
      return DB(partial(settledTone(raw, gain, 28, 1.e-6), 28) / partial(settledTone(raw, gain, 341, 1.e-6), 341)); // 82 Hz and 1 kHz
    };
    assert(bassShare(0.) > -3.);
    assert(bassShare(50.) < bassShare(0.) - 3. && bassShare(50.) > bassShare(100.) + 3.);
    assert(bassShare(100.) < -12.);
  }
  // The shared EQ is flat at zero, reaches its settings at the band centres and
  // the editor response matches the processed small-signal filters.
  for (double hz : {40., 100., 800., 3000., 12000.})
    assert(std::abs(std::abs(SlotPedalDistortion::EQResponse(hz, rate, 0., 0., 800., 0.)) - 1.) < 1.e-12);
  assert(std::abs(DB(std::abs(SlotPedalDistortion::EQResponse(1200., rate, 0., 9., 1200., 0.))) - 9.) < .01);
  assert(std::abs(DB(std::abs(SlotPedalDistortion::EQResponse(20., rate, -12., 0., 800., 0.))) + 12.) < .5);
  assert(std::abs(DB(std::abs(SlotPedalDistortion::EQResponse(18000., rate, 0., 0., 800., 12.))) - 12.) < .5);
  {
    // Biquad responses agree with their impulse responses (independent DFT check).
    pd::Biquad b;
    b.Peak(1000., .9, 6., rate);
    std::vector<double> impulse(8192);
    for (size_t i = 0; i < impulse.size(); ++i) impulse[i] = b.Tick(i == 0 ? 1. : 0.);
    for (double hz : {250., 1000., 4000.})
    {
      std::complex<double> sum{};
      for (size_t i = 0; i < impulse.size(); ++i) sum += impulse[i] * std::polar(1., -2. * spectral_test::pi * hz * i / rate);
      assert(std::abs(std::abs(sum) - std::abs(b.Response(hz, rate))) < 1.e-9);
    }
  }
  // Style changes crossfade: switching styles in the middle of a note makes no step.
  {
    const auto input = Pluck(rate, .6);
    UtilityControls a; a.pedalDistortion[0] = 1.;
    UtilityControls b; b.pedalDistortion[0] = 4.;
    SlotPedalDistortion slot; slot.Reset(rate);
    auto y = input;
    const int half = static_cast<int>(y.size() / 2);
    for (int pos = 0; pos < half; pos += 64) slot.Process(y.data() + pos, std::min(64, half - pos), a);
    double jump = 0.;
    for (int pos = half; pos < static_cast<int>(y.size()); pos += 64)
      slot.Process(y.data() + pos, std::min(64, static_cast<int>(y.size()) - pos), b);
    for (int i = half - 32; i < half + 32; ++i) jump = std::max(jump, std::abs(y[i + 1] - y[i]));
    assert(jump < .5);
  }
  std::cout << "Pedal Distortion: " << pd::styleCount
            << " styles level-matched, exact dry Mix, silence, Gain compression, even harmonics, Tight, EQ and crossfades at 44.1-96 kHz passed\n";
}
