#pragma once
#include "EffectPresentation.h"
#include "TempoSync.h"
#include "dsp/reverb/CosmicReverb.h"
#include "dsp/reverb/ReverbCores.h"
#include <algorithm>
#include <cmath>
#include <complex>
#include <vector>

// Settled-control sketches, not measurements of live audio or hidden LFO phase.
namespace effect_preview
{
struct CosmicLines { std::array<double, 16> seconds{}; int count=0; };
inline CosmicLines Cosmic(double milliseconds, double warpPercent, int style, double rate)
{
  CosmicLines plan; style=std::clamp(style, 0, effect_modes::cosmicCount-1); plan.count=grid_cosmic::LineCount(style);
  for (int i=0; i<plan.count; ++i)
    plan.seconds[i]=grid_cosmic::BaseSamples(milliseconds, warpPercent*.01, style, i, rate)/rate;
  return plan;
}
inline double DelaySeconds(const UtilityControls& c)
{ return c.sync ? grid_tempo::Seconds(c.tempo, c.division) : std::clamp(c.delay[0], 5., 4000.) * .001; }
inline double LowPass(double hz, double cutoff, double rate, int poles = 1, double limit = .4)
{
  const double a = std::exp(-2. * 3.14159265358979323846 * std::min(cutoff, limit * rate) / rate);
  const double magnitude = std::abs((1. - a) / (1. - a * std::polar(1., -2. * 3.14159265358979323846 * hz / rate)));
  return std::pow(magnitude, poles);
}
inline double DistortionTone(double hz, const UtilityControls& c, double rate)
{
  const auto z = std::polar(1., -2. * 3.14159265358979323846 * hz / rate);
  const double low = std::exp(-2. * 3.14159265358979323846 * std::min(c.distortion[2], .4 * rate) / rate);
  const double dc = std::exp(-2. * 3.14159265358979323846 * 5. / rate);
  const auto wet = ((1. - z) / (1. - dc * z)) * ((1. - low) / (1. - low * z));
  const double mix = c.distortion[4] * .01;
  return std::abs(1. - mix + mix * wet) * std::pow(10., c.distortion[5] / 20.);
}
struct Echo
{
  double seconds = 0., left = 0., right = 0., treble = 0.;
};
struct Echoes
{
  std::array<Echo, 96> taps{};
  size_t count = 0;
  double horizon = 1., dry = 1.;
};
inline double TimeHorizon(double seconds)
{
  // Stable 1/2/5 windows let Time/Decay move marks instead of continuously
  // zooming the same picture to fill the panel after every small dial change.
  const double decade=std::pow(10.,std::floor(std::log10(std::max(.001,seconds))));
  for(double step : {1.,2.,5.,10.}) if(step*decade>=seconds) return step*decade;
  return 10.*decade;
}
// Window of Cosmic Echo's measured impulse: about four delay-line times, so several
// passes through the network show how they blur, on the same stable 1/2/5 steps.
inline double CosmicProbeSeconds(double timeMs)
{ return TimeHorizon(std::clamp(timeMs * .004, .25, 10.)); }
inline Echoes Delay(SlotEffect effect, const UtilityControls& c, double rate)
{
  Echoes result;
  const bool dual = effect == SlotEffect::DualDelay;
  const double time = DelaySeconds(c), ratio = dual ? std::clamp(c.dual[0], .25, 2.) : 1.;
  result.horizon = TimeHorizon(4. * time * std::max(1., ratio));
  const double gain = std::pow(10., c.delayOutput / 20.), mix = std::clamp(c.delay[4] * .01, 0., 1.);
  result.dry = (1. - mix) * gain;
  const double feedback = std::clamp(c.delay[1] * .01, 0., .82);
  const double age = effect == SlotEffect::AnalogDelay ? 1. - .6 * c.delay[3] * .01 : 1.;
  const bool wideCutoff=dual || effect==SlotEffect::MultiHeadDelay || effect==SlotEffect::DiffusedDelay;
  const double brightness = LowPass(5000., c.delay[2] * age, rate, effect == SlotEffect::AnalogDelay ? 2 : 1,wideCutoff ? .45 : .4);
  const double balance = std::clamp(c.dual[1] * .01, 0., 1.);
  const double left = dual ? std::min(1., 2. * (1. - balance)) : 1.;
  const double right = dual ? std::min(1., 2. * balance) : 1.;
  const auto add = [&](double seconds, double l, double r, int passes) {
    if (seconds <= result.horizon && result.count < result.taps.size())
      result.taps[result.count++] = {seconds, l, r, std::pow(brightness, passes)};
  };
  double amplitude = mix * gain, elapsed = 0.;
  for (int repeat = 1; repeat <= 12 && amplitude > 1.e-5; ++repeat)
  {
    if (c.pingPong)
    {
      const bool onRight = repeat % 2 == 0;
      elapsed += time * (onRight ? ratio : 1.);
      add(elapsed, onRight ? 0. : amplitude * left, onRight ? amplitude * right : 0., repeat);
    }
    else
    {
      if (effect == SlotEffect::MultiHeadDelay)
      {
        const auto& heads = effect_modes::TapeHeads(c.tapeHeads).gains;
        for (int head = 0; head < 4; ++head)
          if (heads[0][head] > 0. || heads[1][head] > 0.)
            add((repeat - 1 + (head + 1) * .25) * time, amplitude * heads[0][head], amplitude * heads[1][head], repeat);
      }
      else
      {
        add(repeat * time, amplitude * left, dual ? 0. : amplitude * right, repeat);
        if (dual) add(repeat * time * ratio, 0., amplitude * right, repeat);
      }
    }
    amplitude *= feedback;
  }
  return result;
}
inline double CapturePeriod(SlotEffect effect, double milliseconds, double fadeMilliseconds)
{
  const double window = milliseconds * .001;
  return effect == SlotEffect::Freeze ? window - std::min(window * .25, fadeMilliseconds * .001) : window;
}

inline std::complex<double> PoleResponse(double hz, double cutoff, double rate)
{
  const double a=std::exp(-2.*3.14159265358979323846*cutoff/rate);
  return (1.-a)/(1.-a*std::polar(1.,-2.*3.14159265358979323846*hz/rate));
}
inline double WahCenter(const UtilityControls& c, double rate)
{ return std::min(.35*rate,250.*std::exp2(c.wah[2]*.01)*std::pow(6.8,c.wah[0]*.01)); }
inline double WahMagnitude(double hz, const UtilityControls& c, double rate)
{
  // Bilinear state-variable bandpass, including broadband body, mix and trim.
  const double g=std::tan(3.14159265358979323846*WahCenter(c,rate)/rate),k=1./c.wah[1];
  const auto z=std::polar(1.,-2.*3.14159265358979323846*hz/rate);
  const auto s=(1.-z)/(1.+z);
  const auto band=g*s/(s*s+g*k*s+g*g);
  const double mix=c.wah[3]*.01;
  return std::abs(1.-mix+mix*(.35+1.65*k*band))*std::pow(10.,c.wah[4]*.05);
}
inline double ModulationMagnitude(SlotEffect effect, double hz, double phase, const UtilityControls& c, double rate)
{
  constexpr double pi=3.14159265358979323846;
  const double depth=c.modulation[1]*.01,character=c.modulation[3]*.01,mix=c.modulation[4]*.01;
  const auto z=std::polar(1.,-2.*pi*hz/rate);
  std::complex<double> stage;
  const bool flange=effect==SlotEffect::Flanger;
  if (flange)
  {
    const double wobble=(1.-.25*character)*std::sin(2.*pi*phase)+.25*character*std::sin(4.*pi*phase+.7);
    const double samples=rate*(c.extra.flangerDelay*.001+.0025*depth*wobble),whole=std::floor(samples),fraction=samples-whole;
    const auto delay=std::pow(z,whole)*((1.-fraction)+fraction*z);
    const auto tone=1.-character+character*PoleResponse(hz,12000.-9000.*character,rate);
    stage=delay*tone;
  }
  else
  {
    const double cutoff=std::min(.35*rate,650.*std::exp2(std::sin(2.*pi*phase)*depth*2.2+character));
    const double t=std::tan(pi*cutoff/rate),a=(t-1.)/(t+1.);
    stage=std::pow((a+z)/(1.+a*z),std::array<int,4>{4,6,8,12}[std::clamp(static_cast<int>(c.extra.phaserStages),0,3)]);
  }
  const double feedback=c.modulation[2]*.01*(flange ? 1.-2.*character : 1.);
  const auto wet=stage/(1.-feedback*(flange ? std::complex<double>(1.,0.) : z)*stage);
  return std::abs(1.-mix+mix*wet);
}
inline double RingGain(double phase, const UtilityControls& c, double rate)
{
  const double depth=c.ring[1]*.01,mix=c.ring[3]*.01;
  const auto tone=PoleResponse(c.ring[0],std::min(.45*rate,c.ring[2]),rate);
  return 1.-mix+mix*(1.-depth+depth*std::abs(tone)*std::sin(2.*3.14159265358979323846*phase+std::arg(tone)));
}
// Exact sample-and-hold/quantizer/post-filter on a quiet 220 Hz test tone.
// Only the UI/tests run this reference; it never touches a live processor.
// One cycle of a 220 Hz reference through the Bitcrusher's own steps, after one settling
// cycle: the quantized sample-and-hold staircase, and that staircase after High cut. Both
// are 100% wet; Mix and Output only blend and scale them.
struct CrusherCycle { std::vector<double> dry, held, wet; int start = 0; };
inline CrusherCycle CrusherReference(const UtilityControls& c, double rate)
{
  CrusherCycle result;
  const int period=std::max(8,static_cast<int>(std::lround(rate/220.)));
  const double steps=std::exp2(std::round(c.crusher[0])-1.);
  const double pole=std::exp(-2.*3.14159265358979323846*std::min(.45*rate,c.crusher[2])/rate);
  double phase=0.,held=0.,low=0.;
  result.start=period;
  for (int i=0;i<=2*period;++i)
  {
    const double dry=.8*std::sin(2.*3.14159265358979323846*220.*i/rate);
    phase+=std::min(c.crusher[1],rate)/rate;
    if (phase>=1.) { phase-=1.; held=std::round(dry*steps)/steps; }
    low=held+pole*(low-held);
    if (i>=period) { result.dry.push_back(dry); result.held.push_back(held); result.wet.push_back(low); }
  }
  return result;
}
inline double TailDB(double seconds, double decay)
{ return -60.*std::max(0.,seconds)/std::max(.001,decay); }

// Nominal decay time at one frequency. Room/Hall use the FDN's fixed shelf
// ratios (below 200 Hz, above Damping); Plate/Cloud apply their one-pole
// damping once per loop pass, so RT60(f) = 3T / (3T/decay - log10|LP(f)|).
inline double ReverbDecay(SlotEffect effect, double hz, double decay, double size, double damping, double rate)
{
  if (effect == SlotEffect::RoomReverb || effect == SlotEffect::HallReverb)
    return decay * (hz < 200. ? grid_reverb::DiffusionFDN::kLowDecayRatio
                    : hz > damping ? grid_reverb::DiffusionFDN::kHighDecayRatio : 1.);
  const double pass = effect == SlotEffect::PlateReverb ? grid_reverb::PlateTank::PassSeconds(size)
                                                         : grid_reverb::CloudTank::PassSeconds(size);
  // Same clamp as grid_reverb::Coefficient.
  const double loss = -std::log10(std::max(1.e-12, std::abs(PoleResponse(hz, std::clamp(damping, 1., .4 * rate), rate))));
  return 3. * pass / (3. * pass / std::max(.001, decay) + loss);
}

// Peak pitch deviation of a delay line swept by d(t) = excursion * w(rate * t):
// pitch ratio = 1 - d'(t). slopeFactor is the peak of w' / (2 pi), e.g. 2.05
// for sin + .15 sin(7x). Returned in cents for the larger (upward) swing.
inline double SweepCents(double excursionSeconds, double rate, double slopeFactor = 1.)
{
  const double slope = excursionSeconds * 2. * 3.14159265358979323846 * rate * slopeFactor;
  return 1200. * std::log2(1. + std::min(.99, slope));
}

// Uni-Vibe magnitude at one lamp value: four first-order allpasses; Chorus mode
// averages them with dry (notches), Vibrato mode is the allpass alone (flat).
inline double VibeMagnitude(double hz, double lamp, const UtilityControls& c, double rate)
{
  constexpr double pi = 3.14159265358979323846;
  constexpr double centers[]{180., 430., 1050., 2900.};
  const auto z = std::polar(1., -2. * pi * hz / rate);
  std::complex<double> wet(1., 0.);
  for (double center : centers)
  {
    const double f = std::min(.4 * rate, center * std::exp2(lamp * (1.2 + c.modulation[3] * .012)));
    const double t = std::tan(pi * f / rate), a = (t - 1.) / (t + 1.);
    wet *= (a + z) / (1. + a * z);
  }
  const double mode = c.vibeMode >= .5 ? 1. : 0., mix = c.modulation[4] * .01;
  const auto voiced = .5 * (1. + wet) + .5 * mode * (wet - 1.);
  return std::abs(1. - mix + mix * voiced);
}
} // namespace effect_preview
