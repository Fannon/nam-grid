#pragma once
#include "SlotTimeEffects.h"
#include "dsp/MinimumPhaseOversampling.h"
#include "dsp/airwindows/outboard/CStrip2.h"
#include "dsp/ConsoleBands.h"
#include "dsp/airwindows/outboard/ButterComp2.h"
#include "dsp/airwindows/outboard/BussColors4.h"
#include <memory>

inline bool IsOutboard(SlotEffect effect)
{ return effect >= SlotEffect::OutboardChannel && effect <= SlotEffect::ButterComp; }
inline bool IsOutboardCompressor(SlotEffect effect)
{ return effect == SlotEffect::OutboardChannel || effect == SlotEffect::ButterComp; }

// Each upstream core owns independent L/R histories. Owning the pair here
// avoids running its stereo math twice; it does not link the compressors.
class SlotOutboard
{
public:
  static int ColourFactorForRate(double rate) { return rate < 88200. ? 2 : 1; }
  // The explicit factor is an offline comparison hook for the aliasing tests,
  // not a saved control; production always uses ColourFactorForRate.
  void Reset(double rate, SlotEffect effect, int factor = 0)
  {
    mRate = std::max(1., rate);
    mEffect = effect;
    mColourFactor = factor == 1 || factor == 2 ? factor : ColourFactorForRate(rate);
    mColourOversampler.reset();
    if (effect == SlotEffect::ConsoleColour && mColourFactor == 2)
    {
      mColourOversampler = std::make_unique<ColourOversampler>();
      for (auto& filter : mColourOversampler->fir) filter.Reset();
      for (auto& filter : mColourOversampler->dry) filter.Reset();
    }
    mMixBypass.Reset(mRate);
    mControls.Reset(mRate);
    mShort.Reset(mRate); mInputControl.Reset(mRate);
    mStrip.reset();
    mButter.reset();
    mColour.reset();
    if (effect == SlotEffect::ConsoleEQ || effect == SlotEffect::OutboardChannel)
    {
      mStrip = std::make_unique<airwindows_outboard::CStrip2::Core>();
      mStrip->Reset(mRate);
      mBands.Reset();
    }
    if (effect == SlotEffect::ButterComp)
    {
      mButter = std::make_unique<airwindows_outboard::ButterComp2::Core>();
      mButter->Reset(mRate);
    }
    if (effect == SlotEffect::ConsoleColour)
    {
      mColour = std::make_unique<airwindows_outboard::BussColors4::Core>();
      mColour->Reset(mRate * mColourFactor);
    }
    mCounter = 0;
    mDesk = -1;
    mVoice = 1.;
    mMinimumGain = 1.;
    mControlSetValid = false;
    mLastSetDesk = -1;
  }
  void Process(double* left, double* right, int frames, const UtilityControls& c)
  {
    mInputControl.Target({c.extra.stripInput}, uStripInput);
    if (mStrip)
      mControls.Target(mEffect == SlotEffect::ConsoleEQ ? c.consoleEQ : c.strip,
                       mEffect == SlotEffect::ConsoleEQ ? uConsoleTreble : uStripLowCut);
    else
      mShort.Target(mButter ? c.butter : c.colour, mButter ? uButterCompress : uColourInput);
    mMixBypass.Target(c.colour[2] > 0.);
    const int desk = std::clamp(c.colourDesk, 0, 7);
    if (mDesk < 0)
      mDesk = desk;
    mMinimumGain = 1.;
    for (int i = 0; i < frames; ++i)
    {
      const auto& v = mStrip ? mControls.Tick() : mUnused;
      const auto& s = mStrip ? mUnusedShort : mShort.Tick();
      if (mColour)
      {
        // Fade to dry, clear the small FIR/sag histories, then restore the new
        // desk. No allocation or concurrent access to the live core.
        const double step = 1. / (.01 * mRate);
        mVoice = desk == mDesk ? std::min(1., mVoice + step) : std::max(0., mVoice - step);
        if (mVoice == 0. && mDesk != desk)
        {
          mColour->Reset(mRate * mColourFactor);
          mDesk = desk;
          mCounter = 0;
          mControlSetValid = false;
        }
      }
      if (mCounter == 0)
      {
        if (mStrip)
        {
          if (mEffect == SlotEffect::ConsoleEQ)
          {
            // CStrip2 keeps only its native cuts here; console_eq::Bands replaces its EQ.
            if (!mControlSetValid || v[5] != mLastStrip[5] || v[6] != mLastStrip[6])
              mStrip->Set({.5, .5, .5, .5, .5, v[5] * .01, v[6] * .01, 0., 0., 1. / 3.});
            if (!mControlSetValid || v[0] != mLastStrip[0] || v[1] != mLastStrip[1] || v[2] != mLastStrip[2]
                || v[3] != mLastStrip[3] || v[4] != mLastStrip[4])
              mBands.Set(mRate, v[0], v[1], v[2], v[3], v[4], 32);
          }
          else if (!mControlSetValid || v[0] != mLastStrip[0] || v[1] != mLastStrip[1] || v[2] != mLastStrip[2]
                   || v[3] != mLastStrip[3] || v[4] != mLastStrip[4])
            mStrip->Set({.5, .5, .5, .5, .5, v[1] * .01, v[0] * .01, v[2] * .01, v[3] * .01, (1. + v[4] * .01) / 3.});
          mLastStrip = v;
        }
        if (mButter && (!mControlSetValid || s[0] != mLastShort[0]))
        {
          mButter->Set({s[0] * .01, .5, 1.});
          mLastShort[0] = s[0];
        }
        if (mColour && (!mControlSetValid || mDesk != mLastSetDesk))
        {
          mColour->Set({mDesk / 7., .5, .5, 1.});
          mLastSetDesk = mDesk;
        }
        mControlSetValid = true;
      }
      mCounter = (mCounter + 1) % 32;
      const double dryL = left[i], dryR = right ? right[i] : dryL;
      const double inputGain = mStripInput(mInputControl.Tick()[0] * .05, [](double x) { return std::pow(10., x); });
      double wetL = dryL, wetR = dryR;
      if (mEffect == SlotEffect::OutboardChannel)
      {
        wetL *= inputGain;
        if (right) wetR *= inputGain;
      }
      double mix = 1., output = 1.;
      bool blended = false;
      if (mStrip)
      {
        const bool flat = mEffect == SlotEffect::ConsoleEQ && std::abs(v[0]) + std::abs(v[1]) + std::abs(v[2]) < 1.e-8
                          && v[5] >= 100. - 1.e-8 && v[6] < 1.e-8;
        if (right) mStrip->Tick(wetL, wetR);
        else mStrip->TickMono(wetL);
        if (mEffect == SlotEffect::ConsoleEQ)
        {
          mBands.Advance();
          wetL = mBands.Tick(wetL, 0);
          if (right) wetR = mBands.Tick(wetR, 1);
        }
        if (flat)
        {
          wetL = dryL;
          if (right) wetR = dryR;
        }
        if (mEffect == SlotEffect::OutboardChannel)
        {
          mix = v[6] * .01;
          output = mOutput(v[5] / 20., [](double x) { return std::pow(10., x); });
          Monitor(mStrip->Gain(0), right ? mStrip->Gain(1) : 1., mix);
        }
      }
      if (mButter)
      {
        // ButterComp2 works relative to full scale, like a mix bus; guitar sits far lower.
        // Drive the core by a fixed amount and undo it afterwards: the algorithm is unchanged,
        // only its operating level. Makeup, measured on a guitar DI and after a clean capture,
        // keeps typical notes near bypass loudness across the Compress range.
        const double makeup = mButterMakeup(s[0] * .01, [](double x) {
          return std::pow(10., (butterMakeupDB * std::pow(std::clamp(x, 0., 1.), 3.5) - butterDriveDB) / 20.);
        });
        wetL *= butterDrive;
        if (right)
        {
          wetR *= butterDrive;
          mButter->Tick(wetL, wetR);
          wetR *= makeup;
        }
        else
          mButter->TickMono(wetL);
        wetL *= makeup;
        mix = s[2] * .01;
        output = mOutput(s[1] / 20., [](double x) { return std::pow(10., x); });
        Monitor(mButter->Gain(0), right ? mButter->Gain(1) : 1., mix);
      }
      if (mColour)
      {
        const double input = mInput(s[0] / 20., [](double x) { return std::pow(10., x); });
        // Fixed 48 kHz plucked-tone RMS calibration; this preserves each desk's
        // spectral shape and is deliberately not program-dependent auto gain.
        constexpr std::array<double, 8> deskMatchDB{.8841, .6937, -1.3250, 1.0910, .5984, .7611, 2.7876, 1.1550};
        const double trim = mTrim((s[1] + deskMatchDB[mDesk]) / 20., [](double x) { return std::pow(10., x); });
        mix = s[2] * .01 * mVoice;
        wetL *= input;
        if (right) wetR *= input;
        if (mColourFactor == 2)
        {
          if (right) ProcessColour(wetL, wetR);
          else ProcessColourMono(wetL);
        }
        else
        {
          if (right) mColour->Tick(wetL, wetR);
          else mColour->TickMono(wetL);
        }
        wetL *= trim;
        if (right) wetR *= trim;
        if (mColourFactor == 2)
        {
          mColourOversampler->dry[0].Push(dryL);
          if (right) mColourOversampler->dry[1].Push(dryR);
          mMixBypass.Tick();
          double blendL = dryL + mix * (wetL - dryL), blendR = right ? dryR + mix * (wetR - dryR) : 0.;
          if (mix != 1. && !mMixBypass.IsDry())
          {
            const double alignedL = mColourOversampler->dry[0].Filtered();
            blendL = alignedL + mix * (wetL - alignedL);
            if (right)
            {
              const double alignedR = mColourOversampler->dry[1].Filtered();
              blendR = alignedR + mix * (wetR - alignedR);
            }
          }
          wetL = mMixBypass.Apply(dryL, blendL);
          if (right) wetR = mMixBypass.Apply(dryR, blendR);
          blended = true;
        }
      }
      // Output on compressor wrappers follows Mix. Console Colour's trims
      // affect only its wet path, guaranteeing exact dry at Mix zero.
      left[i] = (blended ? wetL : (mix == 0. ? dryL : dryL + mix * (wetL - dryL))) * output;
      if (right)
        right[i] = (blended ? wetR : (mix == 0. ? dryR : dryR + mix * (wetR - dryR))) * output;
    }
  }
  // Native detector attenuation, before automatic gain recovery, saturation
  // and user output. Never infer GR by dividing two nonlinear audio samples.
  double Reduction() const { return -20. * std::log10(std::max(1.e-12, mMinimumGain)); }

private:
  struct ColourOversampler
  {
    std::array<grid_oversampling::FIR<2, 32>, 2> fir;
    std::array<grid_oversampling::LinearReference<2>, 2> dry;
  };
  void ProcessColour(double& l, double& r)
  {
    constexpr int Factor = 2;
    auto upL = mColourOversampler->fir[0].Upsample(l), upR = mColourOversampler->fir[1].Upsample(r);
    for (int phase = 0; phase < Factor; ++phase) mColour->Tick(upL[phase], upR[phase]);
    l = mColourOversampler->fir[0].Downsample(upL); r = mColourOversampler->fir[1].Downsample(upR);
  }
  void ProcessColourMono(double& l)
  {
    constexpr int Factor = 2;
    auto upL = mColourOversampler->fir[0].Upsample(l);
    for (int phase = 0; phase < Factor; ++phase) mColour->TickMono(upL[phase]);
    l = mColourOversampler->fir[0].Downsample(upL);
  }
  std::unique_ptr<ColourOversampler> mColourOversampler;
  grid_oversampling::MixBypass mMixBypass;
  int mColourFactor = 2;
  void Monitor(double l, double r, double mix)
  {
    const double gain = 1. + mix * (std::min({1., l, r}) - 1.);
    // log is monotonic: keep the minimum gain and convert once per block.
    mMinimumGain = std::min(mMinimumGain, gain);
  }
  std::unique_ptr<airwindows_outboard::CStrip2::Core> mStrip;
  console_eq::Bands mBands;
  std::unique_ptr<airwindows_outboard::ButterComp2::Core> mButter;
  std::unique_ptr<airwindows_outboard::BussColors4::Core> mColour;
  grid_fx::SmoothControls<7> mControls;
  grid_fx::SmoothControls<3> mShort;
  grid_fx::SmoothControls<1> mInputControl;
  grid_fx::Memo mStripInput;
  grid_fx::Memo mInput, mTrim, mOutput, mButterMakeup;
  static constexpr double butterDriveDB = 7., butterMakeupDB = 4.8;
  static inline const double butterDrive = std::pow(10., butterDriveDB / 20.);
  std::array<double, 7> mUnused{}, mLastStrip{};
  std::array<double, 3> mUnusedShort{}, mLastShort{};
  SlotEffect mEffect = SlotEffect::OutboardChannel;
  double mRate = 48000., mMinimumGain = 1., mVoice = 1.;
  int mCounter = 0, mDesk = -1, mLastSetDesk = -1;
  bool mControlSetValid = false;
};
