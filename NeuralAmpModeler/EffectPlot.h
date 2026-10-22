#pragma once

namespace grid_ui
{
// Every FX preview reserves the same title, explanation, chart and footer rows.
// Muted = reference/dry, module colour = processed, amber = second channel/GR.
inline void PlotTitle(IGraphics& g, const IRECT& p, IColor ink, const char* title)
{ g.DrawText(Text(14.f, ink), title, IRECT(p.L, 490, p.R, 513)); }
inline void PlotDetail(IGraphics& g, const IRECT& p, const char* detail)
{ g.DrawText(Text(13.f, muted), detail, IRECT(p.L, 515, p.R, 537)); }
inline void PlotFooter(IGraphics& g, const IRECT& p, const char* left, const char* right = "")
{
  const float split = *right ? p.R - 92.f : p.R;
  g.DrawText(Text(13.f, muted), left, IRECT(p.L, 671, split, 692));
  g.DrawText(Text(13.f, muted, EAlign::Far), right, IRECT(split, 671, p.R, 692));
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
