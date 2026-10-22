#pragma once
#include "EffectPreview.h"
#include "EQResponse.h"
#include "SlotDistortion.h"
#include "dsp/airwindows/AirwindowsTape.h"

namespace grid_ui
{
// The transfer curve is a stage preview; the second panel shows linear shaping
// or the stateful tape controls. Reuse each sampled endpoint on the next segment.
template <class Value>
bool DrawDriveSetup(IGraphics& g, SlotEffect effect, const IRECT& plot, IColor color,
                   Value value, double lowCut, double rate, double inputLevel)
{
  if (effect != SlotEffect::Distortion && effect != SlotEffect::Boost
      && effect != SlotEffect::TubeBoost && effect != SlotEffect::Tape) return false;
  const bool clean = effect == SlotEffect::Boost, tube = effect == SlotEffect::TubeBoost, tape = effect == SlotEffect::Tape;
  const int style = static_cast<int>(value(uDistStyle));
  const double drive = std::pow(10., value(uDistDrive) / 20.), bias = value(uDistBias) * .004;
  const IRECT shape(plot.L, plot.T, plot.MW() - 8, plot.B - 10), tonePlot(plot.MW() + 8, plot.T, plot.R, plot.B - 10);
  const double tubeDrive = std::pow(10., value(uTubeDrive) * .05);
  const double boostGain = std::pow(10., value(uBoostGain) * .05);
  const double tapeDrive = std::pow(10., value(uTapeDrive) * .05);
  const double tubeBias = value(uTubeBias) * .0035;
  const auto transfer = [&](double x) {
    if (clean) return x * boostGain;
    if (tube) return SlotTubeBoost::Transfer(x, tubeDrive, tubeBias);
    if (tape) return AirwindowsTape::Spiral(x * tapeDrive);
    return SlotDistortion::Transfer(x, style, drive, bias);
  };
  const auto xPos = [&](double x) { return shape.MW() + shape.W() * .5f * static_cast<float>(x); };
  const auto yPos = [&](double y) { return shape.MH() - shape.H() * .5f * static_cast<float>(y / (clean ? 8. : 2.)); };
  const char* caption = clean ? "Gain / EQ response" : tube ? "Tube shape / EQ + mix" : tape ? "Spiral stage / tape setup" : "Clip / high cut + mix";
  g.DrawText(Text(12.f, color), caption, IRECT(plot.L, 490, plot.R, 518));
  g.DrawLine(border, shape.L, shape.MH(), shape.R, shape.MH());
  g.DrawLine(border, shape.MW(), shape.T, shape.MW(), shape.B);
  g.DrawLine(border.WithOpacity(.5f), xPos(-1.), yPos(-1.), xPos(1.), yPos(1.));
  const double level = std::clamp(inputLevel, 0., 1.);
  const float activeLeft = xPos(-level), activeRight = xPos(level);
  g.FillRect(color.WithOpacity(.08f), IRECT(activeLeft, shape.T, activeRight, shape.B));
  // Sample densely enough to retain folds at maximum drive.
  constexpr int points = 512;
  double previousValue = transfer(-1.);
  for (int i = 1; i <= points; ++i)
  {
    const double a = -1. + 2. * (i - 1) / points, b = -1. + 2. * i / points;
    const bool active = level > 1.e-5 && b >= -level && a <= level;
    const double currentValue = transfer(b);
    g.DrawLine(active ? color : color.WithOpacity(.5f), xPos(a), yPos(previousValue), xPos(b), yPos(currentValue),
               nullptr, active ? 2.5f : 1.5f);
    previousValue = currentValue;
  }
  if (level > 1.e-5)
    for (double input : {-level, level})
    {
      g.DrawLine(muted.WithOpacity(.6f), xPos(input), shape.B, xPos(input), yPos(transfer(input)));
      g.FillCircle(text, xPos(input), yPos(transfer(input)), 2.5f);
    }
  if (tape)
  {
    g.DrawText(Text(12.f, muted), "Head bump", IRECT(tonePlot.L, tonePlot.T, tonePlot.R, tonePlot.T + 22));
    const IRECT amount(tonePlot.L, tonePlot.T + 34, tonePlot.R, tonePlot.T + 46);
    g.FillRect(border.WithOpacity(.35f), amount);
    g.FillRect(color, IRECT(amount.L, amount.T, amount.L + amount.W() * value(uTapeBump) * .01, amount.B));
    WDL_String settings; settings.SetFormatted(80, "%.0f%% bump | %.0f%% wet", value(uTapeBump), value(uTapeMix));
    g.DrawText(Text(11.f, muted), settings.Get(), IRECT(tonePlot.L, tonePlot.T + 52, tonePlot.R, tonePlot.T + 76));
    settings.SetFormatted(64, "Output %+.1f dB", value(uTapeOutput));
    g.DrawText(Text(11.f, muted), settings.Get(), IRECT(tonePlot.L, tonePlot.T + 78, tonePlot.R, tonePlot.T + 100));
    g.DrawText(Text(11.f, muted), "Spiral stage; dynamic tape response", IRECT(plot.L, plot.B - 8, plot.R, plot.B + 8));
  }
  else
  {
    UtilityControls controls;
    for (int i = 0; i < 6; ++i) controls.distortion[i] = value(uDistStyle + i);
    const grid_eq_response::Boost boostEQ(rate, clean ? value(uBoostGain) : 0., value(uBoostTilt), lowCut);
    const double tubeDC = std::exp(-2. * grid_fx::pi * 5. / rate);
    const double tubeMix = value(uTubeMix) * .01, tubeOutput = std::pow(10., value(uTubeOutput) * .05);
    const double upper = std::min(20000., .45 * rate);
    const auto toneY = [&](double hz) {
      double magnitude;
      if (clean) magnitude = std::abs(boostEQ.At(hz));
      else if (tube)
      {
        const auto z = std::polar(1., -2. * grid_fx::pi * hz / rate);
        const auto dc = tubeDC * (1. - z) / (1. - tubeDC * z);
        magnitude = std::abs(1. - tubeMix + tubeMix * tubeDrive * boostEQ.At(hz) * dc) * tubeOutput;
      }
      else magnitude = effect_preview::DistortionTone(hz, controls, rate);
      const double db = 20. * std::log10(std::max(1.e-9, magnitude));
      return tonePlot.B - tonePlot.H() * static_cast<float>((std::clamp(db, -36., 36.) + 36.) / 72.);
    };
    const float unity = tonePlot.B - tonePlot.H() * .5f;
    g.DrawLine(border, tonePlot.L, unity, tonePlot.R, unity);
    float previousY = toneY(20.);
    for (int i = 1; i <= 80; ++i)
    {
      const double hz = 20. * std::pow(upper / 20., i / 80.);
      const float currentY = toneY(hz);
      g.DrawLine(color, tonePlot.L + tonePlot.W() * (i - 1) / 80.f, previousY,
                        tonePlot.L + tonePlot.W() * i / 80.f, currentY);
      previousY = currentY;
    }
    g.DrawText(Text(11.f, muted), clean ? "Gain (+/-8)" : "Shape", IRECT(shape.L, plot.B - 9, shape.R, plot.B));
    g.DrawText(Text(11.f, muted, EAlign::Far), tube ? "Small-signal EQ" : "20 Hz - 20k", IRECT(tonePlot.L, plot.B - 9, tonePlot.R, plot.B));
  }
  return true;
}
} // namespace grid_ui
