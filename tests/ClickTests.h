#pragma once
#include "SlotOperations.h"
#include "ModulationMatrix.h"
#include "ModuleCatalog.h"
#include "EffectPresentation.h"
#include <cstdio>
#include <string>

// Clicks and pops. A quiet 110 Hz sine has almost no energy above 6 kHz, so a
// discontinuity shows up as a broadband spike after a steep high-pass. Every
// catalog module runs through the real slot path while it is bypassed, re-enabled,
// replaced, and every visible control jumps to its ends and back (unsmoothed host
// automation), stepping through every mode. The spike right after an event may not
// exceed twice that of the slot's own 5 ms bypass fade, or three times the module's settled
// high-band level before and after the event, so bright modules are judged against
// themselves. NAM captures and IRs need files and are covered by their own tests.
namespace click_test
{
using Controls = FixedNAMGrid::Controls;
constexpr double kInput = .1, kTone = 110.;

inline Controls FromValues(const SlotValues& values)
{
  Controls c;
  grid_modulation::VisitSlotControls(c, [&](int control, auto& field) {
    field = grid_modulation::FieldValue<std::remove_reference_t<decltype(field)>>(values[control]);
  });
  c.utility.tempo = 120.;
  return c;
}

// Fourth-order Butterworth high-pass at 6 kHz (two RBJ sections at Q 0.54/1.31).
class Detector
{
public:
  explicit Detector(double rate)
  {
    mA.SetParams(recursive_linear_filter::BiquadParams(rate, 6000., .5411961, 0.));
    mB.SetParams(recursive_linear_filter::BiquadParams(rate, 6000., 1.3065630, 0.));
  }
  double Tick(double x) { return mB.Tick(mA.Tick(x)); }

private:
  ScalarFilter<CutCoefficients<true>> mA, mB;
};

class Rig
{
public:
  Rig(SlotEffect effect, double rate, int block) : mRate(rate), mBlock(block), mDetector{Detector(rate), Detector(rate)}
  {
    mConfig.effect = effect;
    mConfig.values = SlotDefaultValues(effect);
    mStage.Publish(PrepareSlot(mConfig, rate, block).dsp);
    mStage.ResetStopped(rate, block);
    mControls = FromValues(mConfig.values);
  }
  // Renders `seconds` of stereo tone; returns the high-passed peak from `from` seconds on.
  double Render(double seconds, double from = 0.)
  {
    const auto frames = static_cast<size_t>(seconds * mRate), first = static_cast<size_t>(from * mRate);
    std::vector<double> left(mBlock), right(mBlock);
    double peak = 0.;
    for (size_t done = 0; done < frames;)
    {
      const int n = static_cast<int>(std::min<size_t>(mBlock, frames - done));
      for (int i = 0; i < n; ++i)
      {
        const double phase = 2. * grid_fx::pi * kTone * static_cast<double>(mPosition + i) / mRate;
        left[i] = kInput * std::sin(phase);
        right[i] = kInput * std::sin(phase + .3);
      }
      mStage.Process(left.data(), n, mControls.enabled, mControls.inputDB, mControls.trimDB, mControls.dynamics,
                     mControls.eq, mControls.utility, right.data());
      for (int i = 0; i < n; ++i)
      {
        const double l = std::abs(mDetector[0].Tick(left[i])), r = std::abs(mDetector[1].Tick(right[i]));
        if (done + i >= first) peak = std::max({peak, l, r});
      }
      mPosition += n;
      done += n;
    }
    return peak;
  }
  void Set(int control, double value)
  {
    mConfig.values[control] = value;
    mControls = FromValues(mConfig.values);
    mControls.utility.trails = mTrails;
  }
  double Get(int control) const { return mConfig.values[control]; }
  void SetMixLaw(bool constantDry) { mControls.utility.constantDry = constantDry; }
  void SetTrails(bool trails) { mControls.utility.trails = mTrails = trails; }
  void Replace()
  {
    mStage.Publish(PrepareSlot(mConfig, mRate, mBlock).dsp);
    mStage.Collect();
  }

private:
  double mRate;
  int mBlock;
  SlotConfig mConfig;
  NAMStage mStage;
  Controls mControls;
  std::array<Detector, 2> mDetector;
  int64_t mPosition = 0;
  bool mTrails = false;
};

// The values an event visits: discrete controls step through every choice,
// continuous controls jump to both ends.
inline std::vector<double> Targets(int control)
{
  using namespace parameter_layout;
  // Shared slot controls the modules show (gate, compressor, Tone/Cut EQ, cuts, IR level).
  constexpr std::array<std::pair<double, double>, standardControls> standard{
    std::pair{-24., 24.}, {-24., 24.}, {0., 1.}, {-90., -10.}, {20., 500.}, {0., 80.}, {-48., 0.}, {40., 600.},
    {0., 18.}, {-12., 12.}, {-12., 12.}, {-12., 12.}, {0., 300.}, {2000., 20000.}, {-6., 6.}, {-40., 0.}, {0., 1.}};
  if (control < standardControls) return {standard[control].first, standard[control].second};
  const int p = control - standardControls;
  const auto& d = kEffectParameters[p];
  const bool discrete = !EffectChoices(p).empty() || IsAlgorithmMode(p) || (d.step >= 1. && d.maximum - d.minimum <= 16.);
  if (!discrete) return {d.minimum, d.maximum};
  std::vector<double> values;
  for (double v = d.minimum; v <= d.maximum; v += 1.) values.push_back(v);
  return values;
}

inline std::vector<int> EventControls(SlotEffect effect)
{
  using namespace parameter_layout;
  std::vector<int> controls;
  for (const auto& destination : grid_modulation::Destinations(effect, true, false, false, false))
    if (destination.control < standardControls) controls.push_back(destination.control);
  for (int p = 0; p < kUtilityControls; ++p)
    if (effect_presentation::Visible(effect, p) && UsesUtility(effect, p) && p != uNoiseLearn)
      controls.push_back(standardControls + p);
  return controls;
}

// Events that change the sound abruptly by design, or known deferred cases. Each
// names its reason; anything else must stay within the click limit.
inline const char* Exemption(SlotEffect effect, int control)
{
  using namespace parameter_layout;
  using E = SlotEffect;
  if (control < standardControls) return nullptr;
  switch (control - standardControls)
  {
    // Delay-length controls glide (tape-style Doppler); a host jump across their range
    // is a fast pitch chirp rather than a step.
    case uDelayTime: case uDualRatio: case uShiftDelay: case uReverbSize: case uAirHallPosition: case uSpaceSize:
    case uCosmicTime: case uCosmicWarp:
      return "delay-length glide";
    case uCrushBits: return "quantization is the effect";
    // Airwindows coefficients update every 32 samples (upstream: once per host block);
    // a full-range jump steps them audibly. Interpolating needs changes inside the
    // vendored DSP, so these two remain known limitations.
    case uStripSaturate: return effect == E::OutboardChannel ? "Airwindows 32-sample coefficient updates" : nullptr;
    case uConsoleMid: case uConsoleHighCut:
      return effect == E::ConsoleEQ ? "Airwindows 32-sample coefficient updates" : nullptr;
  }
  return nullptr;
}

// The accepted standard: the slot's own bypass, a 5 ms linear fade of the full test
// tone starting at its peak, the steepest point for that fade.
inline double FadeReference(double rate)
{
  Detector detector(rate);
  const int fade = static_cast<int>(.005 * rate), peakAt = static_cast<int>(rate / (4. * kTone));
  double peak = 0.;
  for (int i = 0; i < static_cast<int>(rate * .05); ++i)
  {
    const double gain = std::clamp(1. - static_cast<double>(i - peakAt) / fade, 0., 1.);
    peak = std::max(peak, std::abs(detector.Tick(gain * kInput * std::sin(2. * grid_fx::pi * kTone * i / rate))));
  }
  return peak;
}

inline std::string EventName(int control, double value)
{
  char text[160];
  std::snprintf(text, sizeof text, "%s -> %g", grid_modulation::ControlName(control), value);
  return text;
}
} // namespace click_test

void TestClicks()
{
  using namespace click_test;
  constexpr int block = 37; // Odd: exercises partial control quanta and smoothing schedules.
  constexpr double window = .06, settle = .24, settledFrom = .14;
  int events = 0, exempt = 0, findings = 0;
  for (const double rate : {48000., 96000.})
  {
    // Control smoothing is time based, so the 5 ms fade's 48 kHz spike sets the limit at every rate.
    const double limit = 2. * FadeReference(48000.);
    for (const auto& group : nam_library::kModuleGroups)
      for (const auto effect : group.effects)
      {
        Rig rig(effect, rate, block);
        rig.Render(1.); // Fill delays and reverbs, settle detectors and smoothing.
        double settled = rig.Render(.3);
        const auto check = [&](const std::string& event, const char* exemption, auto&& apply) {
          apply();
          const double spike = rig.Render(window);
          const double after = rig.Render(settle, settledFrom);
          ++events;
          if (spike > std::max(3. * std::max(settled, after), limit))
          {
            if (exemption) ++exempt;
            else
            {
              ++findings;
              std::printf("Click: %s at %g Hz, %s: %.2e (limit %.2e, settled %.2e)\n", SlotEffectName(effect), rate,
                          event.c_str(), spike, limit, std::max(settled, after));
              std::fflush(stdout);
            }
          }
          settled = after;
        };
        check("constant dry", nullptr, [&] { rig.SetMixLaw(true); });
        check("linear mix law", nullptr, [&] { rig.SetMixLaw(false); });
        check("bypass", nullptr, [&] { rig.Set(parameter_layout::kSlotEnabled, 0.); });
        check("enable", nullptr, [&] { rig.Set(parameter_layout::kSlotEnabled, 1.); });
        // Quick re-enable while a delay is still draining replays its pending repeats.
        check("bypass again", nullptr, [&] { rig.Set(parameter_layout::kSlotEnabled, 0.); });
        check("enable while draining", nullptr, [&] { rig.Set(parameter_layout::kSlotEnabled, 1.); });
        check("replace", nullptr, [&] { rig.Replace(); });
        for (const int control : EventControls(effect))
        {
          const double start = rig.Get(control);
          const char* exemption = Exemption(effect, control);
          for (const double target : Targets(control))
            if (target != start) check(EventName(control, target), exemption, [&] { rig.Set(control, target); });
          check(EventName(control, start) + " (start)", exemption, [&] { rig.Set(control, start); });
        }
        if (effect == SlotEffect::DigitalDelay)
        {
          // Crossfade switches taps instead of gliding, so Time and tempo jumps get no exemption.
          const auto set = [&](int p, double value) {
            check(EventName(parameter_layout::standardControls + p, value) + " (crossfade)", nullptr,
                  [&] { rig.Set(parameter_layout::standardControls + p, value); });
          };
          set(uDelayTimeChange, 1.);
          const double time = rig.Get(parameter_layout::standardControls + uDelayTime);
          for (const double target : Targets(parameter_layout::standardControls + uDelayTime)) set(uDelayTime, target);
          set(uDelayTime, time);
          set(uTempoSync, 1.);
          for (const double division : Targets(parameter_layout::standardControls + uTempoDivision)) set(uTempoDivision, division);
          set(uTempoSync, 0.);
          set(uDelayTimeChange, 0.);
        }
        if (effect == SlotEffect::JunoChorus)
          for (int from = 0; from < 4; ++from)
            for (int to = 0; to < 4; ++to)
              if (from != to)
              {
                rig.Set(parameter_layout::standardControls + uJunoMode, from);
                settled = rig.Render(.6);
                check("Juno mode " + std::to_string(from) + " to " + std::to_string(to), nullptr,
                      [&] { rig.Set(parameter_layout::standardControls + uJunoMode, to); });
              }
        if (IsTrailEffect(effect))
        {
          // After the control sweep, so the events above keep their history. A ringing tail
          // continues under new playing, fades out when Trails is turned off, and leaves
          // with the replacement fade; draining tails never return.
          using parameter_layout::kSlotEnabled;
          check("trails on", nullptr, [&] { rig.SetTrails(true); });
          check("bypass with trails", nullptr, [&] { rig.Set(kSlotEnabled, 0.); });
          check("enable while trailing", nullptr, [&] { rig.Set(kSlotEnabled, 1.); });
          check("bypass with trails again", nullptr, [&] { rig.Set(kSlotEnabled, 0.); });
          check("trails off while trailing", nullptr, [&] { rig.SetTrails(false); });
          check("trails on while draining", nullptr, [&] { rig.SetTrails(true); });
          check("enable after trails", nullptr, [&] { rig.Set(kSlotEnabled, 1.); });
          check("bypass before replace", nullptr, [&] { rig.Set(kSlotEnabled, 0.); });
          check("replace while trailing", nullptr, [&] { rig.Replace(); });
          check("enable replaced", nullptr, [&] { rig.Set(kSlotEnabled, 1.); });
          check("trails off", nullptr, [&] { rig.SetTrails(false); });
        }
      }
  }
  std::cout << "Clicks: " << events << " bypass/replace/control/mode events at 48/96 kHz, " << findings
            << " above the limit (" << exempt << " exempt by design)" << std::endl;
  assert(events > 2000 && findings == 0);
}
