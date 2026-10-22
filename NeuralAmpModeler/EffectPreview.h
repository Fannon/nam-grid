#pragma once
#include "EffectPresentation.h"
#include "TempoSync.h"
#include "dsp/reverb/CosmicReverb.h"
#include <algorithm>
#include <cmath>
#include <complex>

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
    const double samples=rate*(.003+.0025*depth*wobble),whole=std::floor(samples),fraction=samples-whole;
    const auto delay=std::pow(z,whole)*((1.-fraction)+fraction*z);
    const auto tone=1.-character+character*PoleResponse(hz,12000.-9000.*character,rate);
    stage=delay*tone;
  }
  else
  {
    const double cutoff=std::min(.35*rate,650.*std::exp2(std::sin(2.*pi*phase)*depth*2.2+character));
    const double t=std::tan(pi*cutoff/rate),a=(t-1.)/(t+1.);
    stage=std::pow((a+z)/(1.+a*z),6);
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
inline std::array<double, 385> CrusherReference(const UtilityControls& c, double rate)
{
  std::array<double,385> result{};
  const int count=std::max(384,static_cast<int>(std::ceil(rate*3./220.)));
  const double steps=std::exp2(std::round(c.crusher[0])-1.);
  const double pole=std::exp(-2.*3.14159265358979323846*std::min(.45*rate,c.crusher[2])/rate);
  const double mix=c.crusher[3]*.01,gain=std::pow(10.,c.crusher[4]*.05);
  double phase=0.,held=0.,low=0.; int next=0;
  for (int i=0;i<=count;++i)
  {
    const double dry=.8*std::sin(2.*3.14159265358979323846*220.*i/rate);
    phase+=std::min(c.crusher[1],rate)/rate;
    if (phase>=1.) { phase-=1.; held=std::round(dry*steps)/steps; }
    low=held+pole*(low-held);
    if (next<=384 && i>=static_cast<int>(std::lround(next*count/384.)))
      result[next++]=(dry+mix*(low-dry))*gain;
  }
  return result;
}
inline double TailDB(double seconds, double decay)
{ return -60.*std::max(0.,seconds)/std::max(.001,decay); }
} // namespace effect_preview
