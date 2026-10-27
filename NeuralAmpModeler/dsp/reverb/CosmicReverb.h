#pragma once
#include "ReverbPrimitives.h"
#include "../../EffectModes.h"
#include <memory>

// C++ adaptation of David Braun's MIT re.valhallaSupermassive reconstruction,
// faustlibraries fadc1f7da4a903de25554e538c7a454a2f130299, reverbs.lib.
// Design references: Sean Costello / Valhalla DSP. This is not Valhalla's DSP.
// Original modes: Gemini, Sagittarius, Virgo, Centaurus, Great Annihilator, Lyra.
// Aurora Hall is an original NAM Grid eight-loop diffused FDN; see COSMIC.md.
// See COSMIC.md and LICENSE.txt for attribution and adaptation differences.
namespace grid_cosmic
{
inline double Ratio(double warp)
{ warp = std::clamp(warp, 0., 1.); return 1. - warp*warp*warp/std::sqrt(8.); }
using Style = effect_modes::CosmicStyle;
inline bool IsFourLine(int style) { return style == static_cast<int>(Style::Virgo) || style == static_cast<int>(Style::Lyra); }
inline int LineCount(int style) { return IsFourLine(style) ? 4 : 16; }
inline int Exponent(int style, int line) { return IsFourLine(style) ? 4*line : line; }
inline double BaseSamples(double milliseconds, double warp, int style, int line, double rate)
{
  if (style == static_cast<int>(Style::AuroraHall))
  {
    // A little unequal spacing remains at Warp 0, allowing Density to work
    // without first spreading identical delays. Seconds scale with host rate.
    constexpr double spacing[]{0., .101, .213, .327, .443, .557, .669, .781};
    constexpr double diffuser[]{.019, .029, .037, .043, .053, .061, .071, .083};
    const double ratio = line < 8 ? 1.-(.15+.85*std::clamp(warp,0.,1.))*spacing[line] : diffuser[line-8];
    return milliseconds*.001*rate*ratio + 33.*rate/48000.;
  }
  return milliseconds*.001*rate*std::pow(Ratio(warp), Exponent(style, line)) + 33. + (style == static_cast<int>(Style::GreatAnnihilator) && line%4 == 0 ? 32. : 0.);
}

// Moving first-order Thiran read, preserving loop magnitude. The filled count
// allows O(1) logical clearing with no audio-thread allocation or large memset.
class Line
{
public:
  void Reset(double rate)
  {
    mCapacity=static_cast<size_t>(std::ceil(8.05*rate))+64;
    // Filled-count guards make unwritten samples inaccessible. Avoid touching
    // eight seconds of storage on every reset; physical pages fill as needed.
    mData.reset(new double[mCapacity]); Clear();
    mMaximum = static_cast<double>(mCapacity-3);
  }
  void Clear() { mIndex = mFilled = 0; mPrevious = 0.; }
  double Tick(double input, double delay)
  {
    // Same values as std::clamp and unsigned conversions; signed/value forms are cheaper on MSVC x64.
    delay = delay < 2. ? 2. : mMaximum < delay ? mMaximum : delay;
    const auto integer = static_cast<int64_t>(delay-.49999);
    const auto whole = static_cast<size_t>(integer);
    const double fraction = delay-static_cast<double>(integer), a = (1.-fraction)/(1.+fraction);
    const auto tap = [&](size_t offset) {
      if (offset > mFilled) return 0.;
      return mData[mIndex >= offset ? mIndex-offset : mCapacity-(offset-mIndex)];
    };
    const double out = grid_fx::Flush(a*tap(whole)+tap(whole+1)-a*mPrevious);
    mData[mIndex] = grid_fx::Flush(input);
    if (++mIndex == mCapacity) mIndex = 0;
    mFilled = std::min(mFilled+1, mCapacity); mPrevious = out;
    return out;
  }
private:
  std::unique_ptr<double[]> mData;
  size_t mIndex = 0, mFilled = 0, mCapacity = 0;
  double mPrevious = 0., mMaximum = 0.;
};

class Core
{
public:
  using Stereo = grid_reverb::Stereo;
  using Quad = std::array<double,4>;
  void Reset(double rate)
  {
    mRate = rate;
    for (auto& line : mLines) line.Reset(rate);
    mLfoB = std::tan(grid_fx::pi*.25/rate); mLfoA = (mLfoB-1.)/(mLfoB+1.); mLfoB /= 1.+mLfoB;
    mPhaseStep = static_cast<uint32_t>(.3*1073741824./rate+.5);
    mHighPass = std::exp(-2.*grid_fx::pi*80./rate);
    Clear();
  }
  void Clear()
  {
    for (auto& line : mLines) line.Clear();
    mFeedback = {}; mAPOutput = {}; mAPInput = {}; mDryPrevious = {};
    mFilterInput = {}; mFilterHigh = {}; mFilterLow = {};
    mLfoPrevious = {}; mLfoValue = {}; mPhase = 0;
    mFinishDelay = {}; mFinishIndex = 0;
  }
  void Configure(const std::array<double, 7>& values, int style, bool draining)
  {
    mStyle = std::clamp(style, 0, effect_modes::cosmicCount-1);
    style = mStyle;
    const double feedback = draining ? 0. : values[2]*.01;
    mGain = std::min(.9995, std::sqrt(feedback));
    mInjection = std::sqrt(std::min(1., .2/std::max(.000001, mGain)));
    mGlobalGain = feedback;
    mAllpass = .91*feedback; mFeedthrough = std::pow(mAllpass, style == static_cast<int>(Style::Centaurus) ? 4. : 8.);
    const double angle = grid_fx::pi*values[3]*.0025;
    mCos = std::cos(angle); mSin = std::sin(angle);
    mDensity = std::clamp(values[3]*.01, 0., 1.);
    mLowPass = std::exp(-2.*grid_fx::pi*std::min(.45*mRate, values[4])/mRate);
    mMotion = (500./48000.)*mRate*std::pow(values[5]*.01, 2.);
    for (int i = 0; i < LineCount(style); ++i)
    {
      mBase[i] = BaseSamples(values[0], values[1]*.01, style, i, mRate)-1.;
      constexpr int quadrant[]{0,2,1,3,1,3,0,2,2,0,3,1,3,1,2,0};
      const double phase = style == static_cast<int>(Style::Gemini) ? 2.*grid_fx::pi*i/16.
                         : style == static_cast<int>(Style::Sagittarius) ? grid_fx::pi*(i/2)/4.+grid_fx::pi*(i%2)
                         : IsFourLine(style) ? grid_fx::pi*i/2. : grid_fx::pi*quadrant[i]/2.;
      mModCos[i] = std::cos(phase); mModSin[i] = std::sin(phase);
    }
  }
  Stereo Tick(Stereo input)
  {
    mPhase = (mPhase+mPhaseStep)&0x3fffffffu;
    const double angle = 2.*grid_fx::pi*mPhase/1073741824.;
    const Stereo oscillator{std::sin(angle), std::cos(angle)};
    for (int ch = 0; ch < 2; ++ch)
    {
      mLfoValue[ch] = mLfoB*(oscillator[ch]+mLfoPrevious[ch])-mLfoA*mLfoValue[ch];
      mLfoPrevious[ch] = oscillator[ch];
    }
    Stereo out{};
    if (mStyle == 0)
    {
      auto scattered = mFeedback;
      // R(theta) tensor product: exactly the butterfly used by the model.
      for (int bit = 1; bit < 16; bit *= 2)
        for (int i = 0; i < 16; ++i)
          if ((i&bit) == 0)
          {
            const double a=scattered[i], b=scattered[i+bit];
            scattered[i]=mCos*a-mSin*b; scattered[i+bit]=mSin*a+mCos*b;
          }
      for (int i = 0; i < 16; ++i)
      {
        mFeedback[i] = mLines[i].Tick(mGain*scattered[i]+mInjection*mDryPrevious[i%2], Delay(i));
        out[i%2] += mFeedback[i]/std::sqrt(8.);
      }
    }
    else if (mStyle == 1)
    {
      out = input;
      for (int stage = 0; stage < 8; ++stage)
      {
        Stereo delayed{};
        for (int ch = 0; ch < 2; ++ch)
          delayed[ch] = mLines[stage*2+ch].Tick(mAllpass*mAPOutput[stage][ch]+mAPInput[stage][ch], Delay(stage*2+ch));
        const Stereo rotated{mCos*delayed[0]-mSin*delayed[1], mSin*delayed[0]+mCos*delayed[1]};
        for (int ch=0; ch<2; ++ch) mAPInput[stage][ch]=out[ch];
        for (int ch = 0; ch < 2; ++ch) out[ch] = rotated[ch]-mAllpass*out[ch];
        for (int ch=0; ch<2; ++ch) mAPOutput[stage][ch]=out[ch];
      }
      for (int ch = 0; ch < 2; ++ch)
      {
        const double value = out[ch]-mFeedthrough*input[ch];
        out[ch] = mFinishDelay[ch][mFinishIndex]; mFinishDelay[ch][mFinishIndex] = value;
      }
      mFinishIndex = (mFinishIndex+1)%32;
    }
    else if (mStyle == static_cast<int>(Style::Centaurus))
    {
      Quad x{input[0],input[1],input[0],input[1]};
      for (int stage=0; stage<4; ++stage) x = MatrixAllpass(x,stage);
      for (int ch=0; ch<2; ++ch)
      {
        const double value=x[ch]-mFeedthrough*input[ch];
        out[ch]=mFinishDelay[ch][mFinishIndex]; mFinishDelay[ch][mFinishIndex]=value;
      }
      mFinishIndex=(mFinishIndex+1)%32;
    }
    else if (mStyle == static_cast<int>(Style::GreatAnnihilator))
    {
      Quad x{};
      for (int ch=0; ch<4; ++ch)
        x[ch]=mLines[4*ch].Tick(mGlobalGain*mFeedback[ch]+mDryPrevious[ch%2],Delay(4*ch));
      x=Scatter(x);
      for (int stage=1; stage<4; ++stage) x=MatrixAllpass(x,stage);
      for (int ch=0; ch<4; ++ch) mFeedback[ch]=x[ch];
      out={x[0],x[1]};
    }
    else if (mStyle == static_cast<int>(Style::Lyra))
    {
      const auto scattered=Scatter({mFeedback[0],mFeedback[1],mFeedback[2],mFeedback[3]});
      for (int ch=0; ch<4; ++ch)
        mFeedback[ch]=mLines[ch].Tick(mGain*scattered[ch]+mInjection*mDryPrevious[ch%2],Delay(ch));
      out={(mFeedback[0]+mFeedback[2])/std::sqrt(2.),(mFeedback[1]+mFeedback[3])/std::sqrt(2.)};
    }
    else if (mStyle == static_cast<int>(Style::AuroraHall))
    {
      // Products of Givens rotations are orthogonal at every Density. At zero
      // each loop repeats independently; at full density all eight exchange
      // energy. No linear blend of feedback matrices or nonlinear saturation.
      std::array<double,8> scattered{};
      for (int i=0; i<8; ++i) scattered[i]=mFeedback[i];
      for (int bit=1; bit<8; bit*=2)
        for (int i=0; i<8; ++i) if ((i&bit)==0)
        {
          const double a=scattered[i], b=scattered[i+bit];
          scattered[i]=mCos*a-mSin*b; scattered[i+bit]=mSin*a+mCos*b;
        }
      const double diffusion=.65*mDensity;
      for (int i=0; i<8; ++i)
      {
        // Unit-norm stereo injection/output projections. Each loop has a
        // short allpass. At Density 0 it is a plain delay; increasing the
        // coefficient adds reflections without a lossy dry/diffused blend.
        const double sign=(i&2) ? -.5 : .5;
        const double value=mLines[i].Tick(mGain*scattered[i]+sign*mDryPrevious[i%2],Delay(i));
        const double delayed=mLines[8+i].Tick(diffusion*mAPOutput[i][0]+mAPInput[i][0],Delay(8+i));
        const double diffused=delayed-diffusion*value;
        mAPInput[i][0]=value; mAPOutput[i][0]=diffused;
        // Normalize the DC blocker's Nyquist gain to unity, so damping never
        // adds loop energy. Cuts uses one independent state per main loop.
        mFeedback[i]=Cuts(diffused,i)*(.5*(1.+mHighPass));
        out[i%2]+=sign*mFeedback[i];
      }
    }
    else // Virgo: cuts inside feedback, output after the matrix.
    {
      Quad x{};
      for (int i=0; i<4; ++i)
        x[i]=Cuts(mLines[i].Tick(mGain*mFeedback[i]+mInjection*mDryPrevious[i%2],Delay(i)),i);
      x=Scatter(x);
      for (int i=0; i<4; ++i) mFeedback[i]=x[i];
      out={x[0],x[1]};
    }
    mDryPrevious = input;
    if (mStyle != static_cast<int>(Style::Virgo) && mStyle != static_cast<int>(Style::AuroraHall)) for (int ch = 0; ch < 2; ++ch) out[ch] = Cuts(out[ch], ch);
    return out;
  }
private:
  Quad Scatter(const Quad& x) const
  {
    const double cc=mCos*mCos, ss=mSin*mSin, cs=mCos*mSin;
    return {cc*x[0]-cs*x[1]-cs*x[2]+ss*x[3], cs*x[0]+cc*x[1]-ss*x[2]-cs*x[3],
            cs*x[0]-ss*x[1]+cc*x[2]-cs*x[3], ss*x[0]+cs*x[1]+cs*x[2]+cc*x[3]};
  }
  Quad MatrixAllpass(const Quad& input, int stage)
  {
    Quad delayed{};
    for (int ch=0; ch<4; ++ch)
    {
      const int line=4*ch+stage;
      delayed[ch]=mLines[line].Tick(mAllpass*mAPOutput[stage][ch]+mAPInput[stage][ch],Delay(line));
    }
    auto output=Scatter(delayed);
    mAPInput[stage]=input;
    for (int ch=0; ch<4; ++ch) output[ch]-=mAllpass*input[ch];
    mAPOutput[stage]=output;
    return output;
  }
  double Delay(int line) const
  {
    const double motion=mStyle == static_cast<int>(Style::AuroraHall) ? std::min(mMotion,.01*mBase[line]) : mMotion;
    return std::clamp(mBase[line]+motion*(mLfoValue[0]*mModCos[line]+mLfoValue[1]*mModSin[line]), 2., 8.04*mRate+33.);
  }
  double Cuts(double input, int channel)
  {
    const double high = input-mFilterInput[channel]+mHighPass*mFilterHigh[channel];
    mFilterInput[channel] = input; mFilterHigh[channel] = grid_fx::Flush(high);
    return mFilterLow[channel] = grid_fx::Flush((1.-mLowPass)*high+mLowPass*mFilterLow[channel]);
  }
  std::array<Line, 16> mLines;
  std::array<double, 16> mFeedback{}, mBase{}, mModCos{}, mModSin{};
  std::array<Quad, 8> mAPOutput{}, mAPInput{};
  std::array<std::array<double, 32>, 2> mFinishDelay{};
  Stereo mDryPrevious{}, mLfoPrevious{}, mLfoValue{};
  std::array<double, 8> mFilterInput{}, mFilterHigh{}, mFilterLow{};
  double mRate=48000., mGlobalGain=0., mGain=0., mInjection=1., mAllpass=0., mFeedthrough=0., mCos=1., mSin=0.;
  double mLowPass=0., mHighPass=0., mMotion=0., mLfoA=0., mLfoB=0., mDensity=0.;
  uint32_t mPhase=0, mPhaseStep=0;
  int mStyle=0, mFinishIndex=0;
};
} // namespace grid_cosmic
