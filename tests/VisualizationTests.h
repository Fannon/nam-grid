#pragma once
#include "EffectPreview.h"
#include "EQSignalSpectrum.h"
#include "NAMStage.h"
#include "ImpulseProbe.h"
#include "LevelReadout.h"
#include "FrequencyReadout.h"
#include "ResponseFFT.h"

// Checks the parameter-interaction previews against the actual processors:
// Uni-Vibe notches, delay-sweep pitch, reverb band decay and the EQ analyzer.
void TestVisualizationInteractions()
{
  // Frequency charts use their own logarithmic or linear axis, and notes use A4=440 Hz.
  assert(std::abs(frequency_readout::Hertz(.5, 20., 20000.) - std::sqrt(20. * 20000.)) < 1.e-9);
  assert(frequency_readout::Hertz(.5, 0., 2000., false) == 1000.);
  assert(frequency_readout::Hertz(-1., 20., 20000.) == 20.);
  assert(frequency_readout::Hertz(2., 20., 20000.) == 20000.);
  assert(frequency_readout::Label(440.) == "440.0 Hz | A4");
  assert(frequency_readout::Label(698.456462866) == "698.5 Hz | F5");
  assert(frequency_readout::Label(261.6255653) == "261.6 Hz | C4");
  assert(frequency_readout::Label(0.) == "0 Hz");
  for (double rate : {44100., 48000., 96000.})
  {
    // Uni-Vibe at a frozen lamp (Depth 0): chorus mode notches, vibrato mode flat.
    for (double mode : {0., 1.})
      for (double mix : {50., 100.})
        for (double hz : {300., 1500., 6000.})
        {
          UtilityControls c; c.modulation = {.05, 0., 40., 40., mix}; c.vibeMode = mode;
          grid_roadmap::Vibe vibe; vibe.Reset(rate);
          double inPower = 0., outPower = 0.;
          for (int i = 0; i < 24000; ++i)
          {
            const double input = .001 * std::sin(2. * grid_fx::pi * hz * i / rate);
            double output = input; vibe.Process(&output, 1, c);
            if (i >= 12000) { inPower += input * input; outPower += output * output; }
          }
          assert(std::abs(std::sqrt(outPower / inPower) - effect_preview::VibeMagnitude(hz, 0., c, rate)) < .003);
        }

    // Vibrato's peak pitch follows 1 - d(delay)/dt; compare the shortest
    // measured period of a 1 kHz tone with SweepCents.
    {
      UtilityControls c; c.modulation = {2., 50., 0., 0., 100.};
      SlotModulation vibrato; vibrato.Reset(rate);
      double previous = 0., lastCrossing = -1., shortest = 1.e9;
      for (int i = 0; i < static_cast<int>(rate * 2.); ++i)
      {
        double output = .5 * std::sin(2. * grid_fx::pi * 1000. * i / rate);
        vibrato.Process(&output, 1, SlotEffect::Vibrato, c);
        if (i > rate * .1 && previous < 0. && output >= 0.)
        {
          const double crossing = i - output / (output - previous);
          if (lastCrossing > 0.) shortest = std::min(shortest, crossing - lastCrossing);
          lastCrossing = crossing;
        }
        previous = output;
      }
      const double measured = 1200. * std::log2(rate / shortest / 1000.);
      assert(std::abs(measured - effect_preview::SweepCents(.006 * .5, 2.)) < 4.);
    }

    // Room/Hall shelve fixed ratios; Plate/Cloud decay shortens with frequency
    // through the in-loop damping and is unchanged far below it.
    assert(std::abs(effect_preview::ReverbDecay(SlotEffect::RoomReverb, 100., 4., 1., 3000., rate) - 4. * .65) < 1.e-12);
    assert(std::abs(effect_preview::ReverbDecay(SlotEffect::HallReverb, 5000., 4., 1., 3000., rate) - 4. * .35) < 1.e-12);
    assert(effect_preview::ReverbDecay(SlotEffect::RoomReverb, 1000., 4., 1., 3000., rate) == 4.);
    for (auto effect : {SlotEffect::PlateReverb, SlotEffect::CloudReverb})
    {
      assert(std::abs(effect_preview::ReverbDecay(effect, 20., 4., 1., 12000., rate) - 4.) < .01);
      const double bright = effect_preview::ReverbDecay(effect, 5000., 4., 1., 8000., rate);
      const double dark = effect_preview::ReverbDecay(effect, 5000., 4., 1., 1500., rate);
      assert(dark < bright && bright < 4. && dark > 0.);
    }

    // The analyzer reads a sine's peak amplitude in its band, including high
    // bands that span many FFT bins.
    for (double hz : {1000., 9700.})
    {
      grid_eq_signal::Window window;
      for (size_t i = 0; i < window.samples.size(); ++i)
        window.samples[i] = static_cast<float>(.5 * std::sin(2. * grid_fx::pi * hz * i / rate));
      const auto bands = grid_eq_signal::Analyze(window.samples, rate);
      const double upper = std::min(20000., .49 * rate);
      const auto band = static_cast<size_t>(std::lround(std::log(hz / 20.) / std::log(upper / 20.) * (grid_eq_signal::bands - 1)));
      assert(std::abs(bands[band] - 20.f * std::log10(.5f)) < 1.5f);
      assert(bands[2] < -30.f);
    }
  }
}

// The measured Cosmic Echo view and the module level strip report what the audio does.
void TestMeasuredVisualizations()
{
  // Aligned lines (Warp 0) give a few distinct spikes. Warp spreads the lines into a
  // wash, and Density then fills it further; at Warp 0, Density changes little.
  const auto filled = [](double warp, double density) {
    ImpulseProbe::Request request;
    request.effect = SlotEffect::CosmicEcho;
    request.controls.cosmicStyle = static_cast<int>(effect_modes::CosmicStyle::Centaurus);
    request.controls.cosmic = {120., warp, 60., density, 12000., 0., 100.};
    const auto result = ImpulseProbe::Measure(request);
    assert(result.valid);
    const auto again = ImpulseProbe::Measure(request);
    assert(again.seconds == result.seconds && again.peak == result.peak); // Deterministic, so a cached result stays valid.
    const float loudest = *std::max_element(result.peak.begin(), result.peak.end());
    assert(loudest > 0.f);
    // Time bins within 20 dB of the loudest: how much of the window the response fills.
    return std::count_if(result.peak.begin(), result.peak.end(), [&](float peak) { return peak > .1f * loudest; });
  };
  // Compare the fraction of the displayed width filled above -20 dB.
  // The aligned echoes occupy little of the graph on either time scale.
  const auto sparse = filled(0., 0.);
  const auto spread = filled(50., 0.);
  assert(sparse < ImpulseProbe::bins/12 && filled(0., 100.) < ImpulseProbe::bins/12);
  assert(spread > 4*sparse);
  assert(filled(50., 100.) > spread + ImpulseProbe::bins/10);
  assert(ImpulseProbe::Key(SlotEffect::CosmicEcho, {1., 2.}) != ImpulseProbe::Key(SlotEffect::CosmicEcho, {2., 1.}));

  // At the same Time, a sustaining cathedral needs the long view while a
  // short decay leaves the attack visible at the default resolution.
  ImpulseProbe::Request cathedral;
  cathedral.effect = SlotEffect::CosmicEcho;
  cathedral.controls.cosmicStyle = static_cast<int>(effect_modes::CosmicStyle::Mooncourt);
  cathedral.controls.cosmic = {309., 86., 65.3, 67.5, 9000., 16.9, 100.};
  const auto longTail = ImpulseProbe::Measure(cathedral);
  assert(longTail.valid && longTail.seconds == 4.);
  const float peak = *std::max_element(longTail.peak.begin(), longTail.peak.end());
  assert(*std::max_element(longTail.peak.begin() + ImpulseProbe::bins/2, longTail.peak.end()) > .004f*peak);
  cathedral.controls.cosmic[2] = 5.;
  const auto shortTail = ImpulseProbe::Measure(cathedral);
  assert(shortTail.valid && shortTail.seconds == 2.);

  // A silent first two seconds can hide a single distant repeat.
  ImpulseProbe::Request echo;
  echo.effect = SlotEffect::CosmicEcho;
  echo.controls.cosmicStyle = static_cast<int>(effect_modes::CosmicStyle::Virgo);
  echo.controls.cosmic = {3000., 0., 0., 0., 9000., 0., 100.};
  const auto delayed = ImpulseProbe::Measure(echo);
  assert(delayed.valid && delayed.seconds == 4.);
  const auto loudest = std::max_element(delayed.peak.begin(), delayed.peak.end());
  assert(*loudest > 0.f && std::abs(std::distance(delayed.peak.begin(), loudest) - 180) <= 1);
  echo.controls.cosmic[0] = 6000.;
  const auto distant = ImpulseProbe::Measure(echo);
  assert(distant.valid && distant.seconds == 10.);
  assert(distant.peak[144] > 0.f || distant.peak[143] > 0.f);

  // Gain is the RMS change; a clipped sine has a lower crest factor than a clean one.
  const auto compare = [](double gain, double clip) {
    LevelComparison levels;
    for (int block = 0; block < 100 && !levels.valid; ++block)
    {
      double in = 0., out = 0., inPeak = 0., outPeak = 0.;
      for (int i = 0; i < 480; ++i)
      {
        const double x = .25 * std::sin((block * 480 + i) * .05), y = std::clamp(gain * x, -clip, clip);
        in += x * x; out += y * y; inPeak = std::max(inPeak, std::abs(x)); outPeak = std::max(outPeak, std::abs(y));
      }
      levels.Receive(in, out, inPeak, outPeak, 480, 48000.);
    }
    assert(levels.valid);
    return levels;
  };
  const auto louder = compare(2., 10.);
  assert(std::abs(louder.GainDB() - 20. * std::log10(2.)) < .01);
  assert(std::abs(louder.input.crestDB - 3.01) < .05 && std::abs(louder.CrestChangeDB()) < .05);
  const auto clipped = compare(4., .3);
  assert(clipped.CrestChangeDB() < -1.5);
  // Silence is not compared, and empty packets expire a reading.
  LevelComparison quiet;
  quiet.Receive(1.e-12, 1.e-12, 1.e-6, 1.e-6, 48000, 48000.);
  assert(!quiet.valid);
  auto stale = louder;
  for (int i = 0; i < 31; ++i) stale.Receive(0., 0., 0., 0., 0, 48000.);
  assert(!stale.valid);

  // AudioFFT response previews must match the exact brute-force DFT within 0.01 dB
  // across smooth EQ impulses and delayed/stereo cabinet IRs.
  {
    constexpr double rate = 48000.;
    constexpr double pi = grid_response_fft::kPi;
    // 1. MeasureLinearResponse vs exact DFT on a ConsoleEQ impulse.
    SlotOutboard strip;
    strip.Reset(rate, SlotEffect::ConsoleEQ);
    UtilityControls controls;
    controls.consoleEQ = {4.5, -3.5, 5.0, 90., 7500., 0., 100.};
    constexpr size_t length = 4096;
    constexpr double level = 1.e-3;
    std::vector<double> silence(length, 0.), impulse(length, 0.);
    strip.Process(silence.data(), nullptr, static_cast<int>(length), controls);
    impulse[0] = level;
    strip.Process(impulse.data(), nullptr, static_cast<int>(length), controls);
    std::array<float, grid_response_fft::kMeasuredBins> fast{};
    grid_response_fft::MeasureLinearResponse(impulse.data(), length, rate, fast, level);
    const double upper = std::min(20000., .49 * rate);
    double maxMeasuredError = 0.;
    for (size_t i = 0; i < fast.size(); ++i)
    {
      const double hz = 20. * std::pow(upper / 20., static_cast<double>(i) / (fast.size() - 1));
      const auto step = std::polar(1., -2. * pi * hz / rate);
      std::complex<double> phase(1., 0.), sum(0., 0.);
      for (size_t n = 0; n < length; ++n) { sum += impulse[n] * phase; phase *= step; }
      const double exact = 20. * std::log10(std::max(1.e-12, std::abs(sum) / level));
      maxMeasuredError = std::max(maxMeasuredError, std::abs(static_cast<double>(fast[i]) - exact));
    }
    assert(maxMeasuredError < 0.01);

    // 2. ComputeIRPreview (short stereo IR with delayed reflections) vs exact DFT.
    std::vector<float> irL(4096, 0.f), irR(4096, 0.f);
    irL[0] = 0.8f; irL[17] = -0.35f; irL[113] = 0.22f; irL[900] = -0.09f; irL[3500] = 0.03f;
    irR[0] = 0.7f; irR[29] = 0.41f; irR[211] = -0.18f; irR[1500] = 0.07f;
    const auto preview = grid_response_fft::ComputeIRPreview(irL, irR, rate, rate, false, AdaptiveIR::maximumSeconds);
    assert(preview.valid && !preview.longIR && std::abs(preview.seconds - 4096. / rate) < 1.e-9);
    std::array<double, grid_response_fft::kIRBins> exactIR{};
    double exactPeak = -1.e9;
    for (size_t i = 0; i < exactIR.size(); ++i)
    {
      const double hz = 20. * std::pow(preview.upper / 20., static_cast<double>(i) / (exactIR.size() - 1));
      const auto step = std::polar(1., -2. * pi * hz / rate);
      std::complex<double> phase(1., 0.), sumL(0., 0.), sumR(0., 0.);
      for (size_t n = 0; n < irL.size(); ++n)
      {
        sumL += static_cast<double>(irL[n]) * phase;
        sumR += static_cast<double>(irR[n]) * phase;
        phase *= step;
      }
      exactIR[i] = 20. * std::log10(std::max(1.e-12, std::max(std::abs(sumL), std::abs(sumR))));
      exactPeak = std::max(exactPeak, exactIR[i]);
    }
    double maxIRError = 0.;
    for (size_t i = 0; i < exactIR.size(); ++i)
      maxIRError = std::max(maxIRError, std::abs(static_cast<double>(preview.response[i]) - (exactIR[i] - exactPeak)));
    assert(maxIRError < 0.01);

    // 3. Long IR peak envelope matches bin peaks accurately.
    const auto longPreview = grid_response_fft::ComputeIRPreview(irL, irR, rate, rate, true, AdaptiveIR::maximumSeconds);
    assert(longPreview.valid && longPreview.longIR && std::abs(longPreview.response[0]) < 1.e-5f);
    assert(longPreview.response.back() < -40.f);
  }

  // ImpulseProbe: key 0 must not collide with initial state, superseded requests abort
  // early via generation counter, and A -> B -> A must never overwrite A with stale B.
  {
    ImpulseProbe probe;
    const auto waitFor = [&](uint64_t key) {
      const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
      while (std::chrono::steady_clock::now() < deadline)
      {
        const auto result = probe.Latest();
        if (result.valid && result.key == key) return result;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      return probe.Latest();
    };
    ImpulseProbe::Request reqA{SlotEffect::CosmicEcho, {}, 0};
    reqA.controls.cosmicStyle = static_cast<int>(effect_modes::CosmicStyle::Centaurus);
    reqA.controls.cosmic = {120., 50., 40., 30., 12000., 0., 100.};
    probe.Ask(reqA);
    const auto settledA = waitFor(0);
    assert(settledA.valid && settledA.key == 0);

    // Rapid A -> B -> A: B must be cancelled and must not replace A.
    ImpulseProbe::Request reqB = reqA;
    reqB.key = 42;
    reqB.controls.cosmic[2] = 0.85;
    probe.Ask(reqB);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    probe.Ask(reqA);
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    const auto afterRevert = probe.Latest();
    assert(afterRevert.valid && afterRevert.key == 0);

    // Rapid sweep B -> C -> D must settle on D.
    ImpulseProbe::Request reqC = reqB; reqC.key = 101; reqC.controls.cosmic[3] = 0.6;
    ImpulseProbe::Request reqD = reqB; reqD.key = 202; reqD.controls.cosmic[3] = 0.8;
    probe.Ask(reqB);
    probe.Ask(reqC);
    probe.Ask(reqD);
    const auto settledD = waitFor(202);
    assert(settledD.valid && settledD.key == 202);
  }
  std::cout << "Measured visualizations: Cosmic impulse blur and module level strip passed\n";
}
