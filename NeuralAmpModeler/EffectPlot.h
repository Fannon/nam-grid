#pragma once
#include "GridLayout.h"
#include "FrequencyReadout.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

namespace grid_ui
{
inline void DrawFrequencyCursor(IGraphics& g, const IRECT& bounds, double low, double high, bool logarithmic = true)
{
  float x, y; g.GetMouseLocation(x, y);
  if (!bounds.Contains(x, y)) return;
  const double hz = frequency_readout::Hertz((x - bounds.L) / bounds.W(), low, high, logarithmic);
  const auto label = frequency_readout::Label(hz);
  g.DrawLine(accent.WithOpacity(.85f), x, bounds.T, x, bounds.B, nullptr, 1.f);
  const float width = std::min(148.f, bounds.W());
  const float left = std::clamp(x + 8.f, bounds.L, bounds.R - width);
  const IRECT box(left, bounds.T + 4.f, left + width, bounds.T + 25.f);
  g.FillRoundRect(surface, box, 3.f); g.DrawRoundRect(accent, box, 3.f);
  g.DrawText(Text(12.f, text, EAlign::Center), label.c_str(), box);
}
// Seconds at evenly spaced pixel columns of a time axis, NaN where no time maps.
inline constexpr int kTimeColumns = 65;
using TimeColumns = std::array<double, kTimeColumns>;
inline void DrawTimeCursor(IGraphics& g, const IRECT& bounds, const TimeColumns& seconds)
{
  float x, y; g.GetMouseLocation(x, y);
  if (!bounds.Contains(x, y)) return;
  const double column = std::clamp((x - bounds.L) / bounds.W(), 0.f, 1.f) * (kTimeColumns - 1);
  const int left = std::min(static_cast<int>(column), kTimeColumns - 2);
  const double t = seconds[left] + (column - left) * (seconds[left + 1] - seconds[left]);
  if (!std::isfinite(t) || t < 0.) return;
  WDL_String label;
  if (t < .01) label.SetFormatted(24, "%.1f ms", t * 1000.);
  else if (t < 1.) label.SetFormatted(24, "%.0f ms", t * 1000.);
  else label.SetFormatted(24, "%.2f s", t);
  g.DrawLine(accent.WithOpacity(.85f), x, bounds.T, x, bounds.B, nullptr, 1.f);
  const float width = std::min(76.f, bounds.W());
  const float boxLeft = std::clamp(x + 8.f, bounds.L, bounds.R - width);
  const IRECT box(boxLeft, bounds.T + 4.f, boxLeft + width, bounds.T + 25.f);
  g.FillRoundRect(surface, box, 3.f); g.DrawRoundRect(accent, box, 3.f);
  g.DrawText(Text(12.f, text, EAlign::Center), label.Get(), box);
}
// A time mark's label, centred on its line, small with a dark backing so curves drawn
// later never hide it.
inline void DrawTimeLabel(IGraphics& g, const IRECT& box, const char* label)
{
  const float half = .5f * std::min(box.W(), 6.f + 5.6f * static_cast<float>(std::strlen(label)));
  g.FillRoundRect(surface.WithOpacity(.85f), IRECT(box.MW() - half, box.T, box.MW() + half, box.B), 2.f);
  g.DrawText(Text(10.f, muted, EAlign::Center), label, box);
}
// Every FX preview reserves the same title, chart and footer rows.
// Muted = reference/dry, module colour = processed, amber = second channel/GR.
class PlotHelp : public IContainerBase
{
public:
  explicit PlotHelp(const IRECT& bounds) : IContainerBase(bounds) { SetIgnoreMouse(false); }
  void OnAttached() override
  {
    for (auto& region : mRegions)
    {
      region.control = new RegionControl();
      AddChildControl(region.control);
      region.control->Hide(true);
    }
  }
  void Draw(IGraphics& g) override
  {
    if (mFrequency) DrawFrequencyCursor(g, mFrequencyBounds, mLow, mHigh, mLogarithmic);
    for (int i = 0; i < mLabelCount; ++i) DrawTimeLabel(g, mLabels[i].box, mLabels[i].text);
    for (int i = 0; i < mTimeCount; ++i) DrawTimeCursor(g, mTimes[i].bounds, mTimes[i].seconds);
  }
  bool IsDirty() override
  {
    if (!IsHidden())
    {
      float x, y; GetUI()->GetMouseLocation(x, y);
      bool hovered = mFrequency && mFrequencyBounds.Contains(x, y);
      for (int i = 0; i < mTimeCount; ++i) hovered |= mTimes[i].bounds.Contains(x, y);
      if (hovered != mHovered || (hovered && x != mHoverX))
      { mHovered = hovered; mHoverX = x; SetDirty(false); }
    }
    return IContainerBase::IsDirty();
  }
  void Begin() { mDetail.clear(); mRegionCount = 0; mFrequency = false; mTimeCount = 0; mLabelCount = 0; }
  // Time-mark labels, drawn above the chart content.
  void TimeLabel(const IRECT& box, const char* text)
  {
    if (mLabelCount >= static_cast<int>(mLabels.size())) return;
    auto& label = mLabels[mLabelCount++];
    label.box = box;
    std::snprintf(label.text, sizeof(label.text), "%s", text);
  }
  // Time axes for the hover readout; Cosmic Echo has two.
  void Time(const IRECT& bounds, const TimeColumns& seconds)
  { if (mTimeCount < static_cast<int>(mTimes.size())) mTimes[mTimeCount++] = {bounds, seconds}; }
  void Frequency(const IRECT& bounds, double low, double high, bool logarithmic = true)
  { mFrequencyBounds = bounds; mLow = low; mHigh = high; mLogarithmic = logarithmic; mFrequency = true; }
  void Detail(const char* text) { mDetail = text; }
  void RegionDetail(const IRECT& bounds, const char* text)
  {
    if (mRegionCount >= mRegions.size()) return;
    auto& region = mRegions[mRegionCount++];
    region.bounds = bounds;
    region.text = text;
  }
  void End(IGraphics& g)
  {
    bool changed = mDetail != GetTooltip();
    if (changed) SetTooltip(mDetail.c_str());
    for (size_t i = 0; i < mRegions.size(); ++i)
    {
      auto& region = mRegions[i];
      const bool hidden = IsHidden() || i >= mRegionCount;
      if (region.control->IsHidden() != hidden) { region.control->Hide(hidden); changed = true; }
      if (hidden) continue;
      if (region.control->GetRECT() != region.bounds)
      {
        region.control->SetTargetAndDrawRECTs(region.bounds);
        changed = true;
      }
      if (region.text != region.control->GetTooltip())
      {
        region.control->SetTooltip(region.text.c_str());
        changed = true;
      }
    }
    if (changed) g.UpdateTooltips();
  }
private:
  class RegionControl : public IControl
  {
  public:
    RegionControl() : IControl(IRECT()) { SetIgnoreMouse(false); }
    void Draw(IGraphics&) override {}
  };
  struct Region { RegionControl* control = nullptr; IRECT bounds; std::string text; };
  std::array<Region, 4> mRegions{};
  size_t mRegionCount = 0;
  std::string mDetail;
  IRECT mFrequencyBounds;
  double mLow = 20., mHigh = 20000.;
  bool mFrequency = false, mLogarithmic = true, mHovered = false;
  struct TimeAxis { IRECT bounds; TimeColumns seconds{}; };
  std::array<TimeAxis, 2> mTimes{};
  int mTimeCount = 0;
  struct Label { IRECT box; char text[16]{}; };
  std::array<Label, 14> mLabels{};
  int mLabelCount = 0;
  float mHoverX = -1.f;
};
inline PlotHelp* CurrentPlotHelp(IGraphics& g)
{
  auto* plug = static_cast<PLUG_CLASS_NAME*>(g.GetDelegate());
  const int tag = plug->IsSignalMonitorOpen() ? kCtrlTagSignalMonitorPlotHelp
                                            : kCtrlTagPlotHelpBase + static_cast<int>(plug->GetSelectedSlot());
  return static_cast<PlotHelp*>(g.GetControlWithTag(tag));
}
inline void PlotFrequency(IGraphics& g, const IRECT& bounds, double low, double high, bool logarithmic = true)
{ if (auto* help = CurrentPlotHelp(g)) help->Frequency(bounds, low, high, logarithmic); }
inline void PlotTitle(IGraphics& g, const IRECT& p, IColor ink, const char* title)
{ g.DrawText(Text(14.f, ink), title, IRECT(p.L, grid_layout::plotTitleTop, p.R, grid_layout::plotTitleBottom)); }
inline void PlotDetail(IGraphics& g, const IRECT&, const char* detail)
{
  if (auto* help = CurrentPlotHelp(g)) help->Detail(detail);
}
inline void PlotRegionDetail(IGraphics& g, const IRECT& bounds, const char* detail)
{
  if (auto* help = CurrentPlotHelp(g)) help->RegionDetail(bounds, detail);
}
inline void PlotFooter(IGraphics& g, const IRECT& p, const char* left, const char* right = "")
{
  const float split = *right ? p.R - 92.f : p.R;
  g.DrawText(Text(13.f, muted), left, IRECT(p.L, grid_layout::plotFooterTop, split, grid_layout::plotFooterBottom));
  g.DrawText(Text(13.f, muted, EAlign::Far), right, IRECT(split, grid_layout::plotFooterTop, p.R, grid_layout::plotFooterBottom));
}
template<class Function>
void PlotCurve(IGraphics& g, const IRECT& p, Function f, double low, double high, IColor ink, float width = 1.5f, int segments = 384)
{
  const auto y = [&](double v) { return p.B - p.H() * static_cast<float>(std::clamp((v-low)/(high-low), 0., 1.)); };
  g.PathClear();
  g.PathMoveTo(p.L, y(f(0.)));
  for (int i = 1; i <= segments; ++i)
  {
    const double x = static_cast<double>(i) / segments;
    g.PathLineTo(p.L + p.W() * static_cast<float>(x), y(f(x)));
  }
  // One stroke avoids antialiased caps/seams between adjacent short segments.
  g.PathStroke(IPattern(ink), width);
}
inline void PlotGrid(IGraphics& g, const IRECT& p)
{
  for (int i=0; i<=2; ++i)
    g.DrawLine(border.WithOpacity(.45f),p.L,p.T+p.H()*i/2.f,p.R,p.T+p.H()*i/2.f);
}
// Inverts a monotonic seconds-to-pixel map at evenly spaced columns, by bisection.
template<class X>
TimeColumns InvertTime(const IRECT& p, X& x)
{
  TimeColumns seconds{};
  const double origin = x(0.), direction = x(1.e-3) >= origin ? 1. : -1.;
  for (int i = 0; i < kTimeColumns; ++i)
  {
    const double want = direction * (p.L + p.W() * i / (kTimeColumns - 1.) - origin);
    if (want < 0.) { seconds[i] = std::numeric_limits<double>::quiet_NaN(); continue; }
    double low = 0., high = 1.e-3;
    while (direction * (x(high) - origin) < want && high < 1.e4) high *= 2.;
    for (int step = 0; step < 32; ++step)
    {
      const double middle = .5 * (low + high);
      (direction * (x(middle) - origin) < want ? low : high) = middle;
    }
    seconds[i] = .5 * (low + high);
  }
  return seconds;
}
// Faint dotted reference lines at 100, 200 and 400 ms and 1, 2, 4 and 8 s on a time axis,
// labelled at the bottom. `x` maps seconds to a pixel, linearly or not, in either direction:
// histories count back from now at the right edge. A mark is drawn only inside the plot and
// at least 44 px from time zero and from the previous mark, so labels never crowd. Labels
// straddle the bottom axis, `drop` pixels below the chart, clear of the footer text; pass 0
// where other text sits directly under the chart. The axis also gets the hover time readout.
// Returns how many marks were drawn; with none, the plot still needs its time span in words.
inline constexpr float kTimeLabelWidth = 38.f;
// The marks that fit, in order: visit(seconds, pixel). Returns how many there are.
template<class X, class Visit>
int ForEachTimeMark(const IRECT& p, X& x, Visit&& visit)
{
  constexpr double marks[]{.1, .2, .4, 1., 2., 4., 8.};
  constexpr float spacing = 44.f;
  float previous = static_cast<float>(x(0.));
  int count = 0;
  for (const double seconds : marks)
  {
    const float at = static_cast<float>(x(seconds));
    // Centred labels need half their width inside the chart.
    if (at > p.R - .5f * kTimeLabelWidth || at < p.L + .5f * kTimeLabelWidth) break;
    if (std::abs(at - previous) < spacing) continue;
    visit(seconds, at);
    previous = at;
    ++count;
  }
  return count;
}
// How many marks PlotTimeMarks would draw, for layouts that depend on them.
template<class X>
int TimeMarkCount(const IRECT& p, X&& x) { return ForEachTimeMark(p, x, [](double, float) {}); }
template<class X>
int PlotTimeMarks(IGraphics& g, const IRECT& p, X&& x, bool cursor = true, float drop = 5.f)
{
  auto* help = cursor ? CurrentPlotHelp(g) : nullptr;
  if (help) help->Time(p, InvertTime(p, x));
  constexpr float label = kTimeLabelWidth;
  return ForEachTimeMark(p, x, [&](double seconds, float at) {
    g.DrawDottedLine(muted.WithOpacity(.35f), at, p.T, at, p.B);
    WDL_String text;
    if (seconds < 1.) text.SetFormatted(16, "%.0f ms", seconds * 1000.);
    else text.SetFormatted(16, "%.0f s", seconds);
    const float top = p.B - 12.f + drop;
    const IRECT box(at - .5f * label, top, at + .5f * label, top + 12.f);
    if (help) help->TimeLabel(box, text.Get());
    else DrawTimeLabel(g, box, text.Get());
  });
}
inline void PlotFrequencyAxis(IGraphics& g, const IRECT& p, double upper, const char* scale)
{
  PlotFrequency(g, p, 20., upper);
  PlotGrid(g,p);
  for (double hz : {100.,1000.,10000.})
    if (hz<upper)
    {
      const float x=p.L+p.W()*static_cast<float>(std::log(hz/20.)/std::log(upper/20.));
      g.DrawLine(border.WithOpacity(.35f),x,p.T,x,p.B);
    }
  WDL_String label; label.SetFormatted(48,"20 Hz - %.1fk",upper/1000.);
  PlotFooter(g,p,label.Get(),scale);
}
} // namespace grid_ui
