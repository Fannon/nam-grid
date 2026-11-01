#pragma once
#include "GridLayout.h"
#include "FrequencyReadout.h"

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
  }
  bool IsDirty() override
  {
    if (!IsHidden())
    {
      float x, y; GetUI()->GetMouseLocation(x, y);
      const bool hovered = mFrequency && mFrequencyBounds.Contains(x, y);
      if (hovered != mHovered || (hovered && x != mHoverX))
      { mHovered = hovered; mHoverX = x; SetDirty(false); }
    }
    return IContainerBase::IsDirty();
  }
  void Begin() { mDetail.clear(); mRegionCount = 0; mFrequency = false; }
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
void PlotCurve(IGraphics& g, const IRECT& p, Function f, double low, double high, IColor ink, float width = 1.5f)
{
  const auto y = [&](double v) { return p.B - p.H() * static_cast<float>(std::clamp((v-low)/(high-low), 0., 1.)); };
  double previous = f(0.);
  for (int i=1; i<=384; ++i)
  {
    const double current=f(i/384.);
    g.DrawLine(ink,p.L+p.W()*(i-1)/384.f,y(previous),p.L+p.W()*i/384.f,y(current),nullptr,width);
    previous=current;
  }
}
inline void PlotGrid(IGraphics& g, const IRECT& p)
{
  for (int i=0; i<=2; ++i)
    g.DrawLine(border.WithOpacity(.45f),p.L,p.T+p.H()*i/2.f,p.R,p.T+p.H()*i/2.f);
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
