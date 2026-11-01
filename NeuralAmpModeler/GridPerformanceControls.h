#pragma once

// Modulation page (sources, macros and the matrix), the routing panel below the
// grid, and the header scene buttons. Pages use the main editor's 24 px margins.
namespace grid_ui
{
inline void DrawFlowIcon(IGraphics& g, float x, float y, bool serial)
{
  const auto line = [&](float a, float b, float c, float d, IColor ink) {
    g.DrawLine(ink, x+a, y+b, x+c, y+d, nullptr, 1.5f);
  };
  if (serial)
  {
    line(-17,0,17,0,muted);
    g.FillRect(ampColor, IRECT(x-13,y-3,x-7,y+3));
    g.FillRect(cabColor, IRECT(x+7,y-3,x+13,y+3));
  }
  else
  {
    line(-19,0,-12,0,muted); line(-12,-5,-12,5,muted);
    line(-12,-5,12,-5,ampColor); line(-12,5,12,5,cabColor);
    line(12,-5,12,5,muted); line(12,0,19,0,muted);
    g.FillRect(ampColor, IRECT(x-3,y-7,x+3,y-3));
    g.FillRect(cabColor, IRECT(x-3,y+3,x+3,y+7));
  }
}
// Change topology explicitly inside Routing; the grid button only opens that page.
class FlowMode : public IControl
{
public:
  FlowMode(const IRECT& r) : IControl(r, PerformanceParam(grid_performance::LaneMode))
  { SetTooltip("Click to switch lanes: Parallel splits into A and B then blends; Sequential runs Pre > A > B > Post."); }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    if (mod.L) PLUG()->SetParameterFromUI(PerformanceParam(grid_performance::LaneMode), GetParam()->Int() ? 0. : 1.);
  }
  void Draw(IGraphics& g) override
  {
    const bool serial = GetParam()->Int() == grid_performance::Sequential;
    g.FillRoundRect(surface, mRECT, 3.f);
    g.DrawRoundRect(mMouseIsOver ? accent : border, mRECT, 3.f);
    const float x = mRECT.W() > 100.f ? mRECT.L + 24.f : mRECT.MW(), y = mRECT.MH();
    DrawFlowIcon(g, x, y, serial);
    if (mRECT.W() > 100.f)
      g.DrawText(Text(12.f, text, EAlign::Center), serial ? "Sequential" : "Parallel", IRECT(mRECT.L+46,mRECT.T,mRECT.R-3,mRECT.B));
  }
};

inline constexpr float pageLeft = 24.f, pageRight = grid_layout::width - 24.f, pageTop = 72.f;

// Segmented page or source selector; the selected segment is raised.
class Tabs : public IControl
{
public:
  Tabs(const IRECT& r, std::vector<std::string> labels, std::function<void(int)> select,
       std::vector<IColor> dots = {})
  : IControl(r), mLabels(std::move(labels)), mDots(std::move(dots)), mSelect(std::move(select))
  {
  }
  void Draw(IGraphics& g) override
  {
    for (int i = 0; i < static_cast<int>(mLabels.size()); ++i)
    {
      const auto cell = mRECT.SubRectHorizontal(static_cast<int>(mLabels.size()), i).GetHPadded(-2.f);
      const bool on = i == mSelected;
      g.FillRoundRect(on ? IColor(255, 49, 53, 59) : surface, cell, 3.f);
      g.DrawRoundRect(on ? muted : border, cell, 3.f);
      auto label = cell;
      if (i < static_cast<int>(mDots.size()))
      {
        IRECT measured; g.MeasureText(Text(14.f), mLabels[i].c_str(), measured);
        const float x = cell.MW() - measured.W() * .5f - 9.f;
        g.FillCircle(mDots[i].WithOpacity(on ? 1.f : .6f), x, cell.MH(), 3.f);
        label = label.GetHShifted(5.f);
      }
      g.DrawText(Text(14.f, on ? text : muted, EAlign::Center), mLabels[i].c_str(), label);
    }
  }
  void OnMouseDown(float x, float, const IMouseMod& mod) override
  {
    if (!mod.L) return;
    const int count = static_cast<int>(mLabels.size());
    Select(std::clamp(static_cast<int>((x - mRECT.L) / mRECT.W() * count), 0, count - 1));
  }
  void Select(int index)
  {
    mSelected = index;
    mSelect(index);
    SetDirty(false);
  }
  int Selected() const { return mSelected; }

private:
  std::vector<std::string> mLabels;
  std::vector<IColor> mDots;
  std::function<void(int)> mSelect;
  int mSelected = 0;
};

// An enum parameter as its current choice; a click lists every choice. Laid out
// with the module pane's label size and dial centre. Switches share the same box.
class ParamMenu : public IControl
{
public:
  ParamMenu(const IRECT& r, int parameter, const char* label, bool toggle = false)
  : IControl(r, parameter), mLabel(label), mToggle(toggle) {}
  void Draw(IGraphics& g) override
  {
    g.DrawText(Text(15.f, muted, EAlign::Center), mLabel.c_str(), mRECT.GetFromTop(14));
    const auto box = Box();
    WDL_String value; GetParam()->GetDisplay(value);
    const bool on = mToggle && GetParam()->Bool();
    g.FillRoundRect(on ? accent.WithOpacity(.15f) : surface, box, 3.f);
    g.DrawRoundRect(on ? accent : mMouseIsOver ? muted : border, box, 3.f);
    FittedText(g, Text(13.f, text, EAlign::Center), mToggle ? on ? "ON" : "OFF" : value.Get(), box.GetHPadded(-14.f));
    if (!mToggle)
    {
      g.DrawLine(muted, box.R - 14, box.MH() - 2, box.R - 10, box.MH() + 2);
      g.DrawLine(muted, box.R - 10, box.MH() + 2, box.R - 6, box.MH() - 2);
    }
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    if (!mod.L) return;
    if (mToggle) { PLUG()->SetParameterFromUI(GetParamIdx(), GetParam()->Bool() ? 0. : 1.); return; }
    mMenu.Clear();
    const auto* param = GetParam();
    for (int value = static_cast<int>(param->GetMin()); value <= static_cast<int>(param->GetMax()); ++value)
    {
      WDL_String label; param->GetDisplay(value, false, label);
      mMenu.AddItem(new IPopupMenu::Item(label.Get(), param->Int() == value ? IPopupMenu::Item::kChecked : 0, value));
    }
    GetUI()->CreatePopupMenu(*this, mMenu, Box());
  }
  void OnPopupMenuSelection(IPopupMenu* menu, int) override
  {
    if (const auto* item = menu ? menu->GetChosenItem() : nullptr)
      PLUG()->SetParameterFromUI(GetParamIdx(), item->GetTag());
  }

private:
  IRECT Box() const { return IRECT(mRECT.L + 8.f, mRECT.T + 24.f, mRECT.R - 8.f, mRECT.T + 56.f); }
  std::string mLabel;
  bool mToggle;
  IPopupMenu mMenu;
};

// A saved-state choice in one header row: shows "Label: value" and opens a menu of its
// values. Used for the split's slope, which is never host automation (see StateOnly).
class PerformanceChoice : public IControl
{
public:
  PerformanceChoice(const IRECT& r, int parameter, const char* label) : IControl(r, parameter), mLabel(label) {}
  void Draw(IGraphics& g) override
  {
    WDL_String value;
    GetParam()->GetDisplay(GetValue(), true, value);
    g.FillRoundRect(surface, mRECT, 3.f);
    g.DrawRoundRect(mMouseIsOver ? muted : border, mRECT, 3.f);
    FittedText(g, Text(13.f, text, EAlign::Center), mLabel + ": " + value.Get(), mRECT.GetHPadded(-8.f));
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    if (!mod.L) return;
    if (GetParam()->GetMax() - GetParam()->GetMin() == 1.)
    {
      PLUG()->SetParameterFromUI(GetParamIdx(), GetParam()->Int() == GetParam()->GetMin() ? GetParam()->GetMax() : GetParam()->GetMin());
      return;
    }
    mMenu.Clear();
    const auto* param = GetParam();
    for (int value = static_cast<int>(param->GetMin()); value <= static_cast<int>(param->GetMax()); ++value)
    {
      WDL_String item;
      param->GetDisplay(value, false, item);
      mMenu.AddItem(new IPopupMenu::Item(item.Get(), param->Int() == value ? IPopupMenu::Item::kChecked : 0, value));
    }
    GetUI()->CreatePopupMenu(*this, mMenu, mRECT);
  }
  void OnPopupMenuSelection(IPopupMenu* menu, int) override
  {
    if (const auto* item = menu ? menu->GetChosenItem() : nullptr)
      PLUG()->SetParameterFromUI(GetParamIdx(), item->GetTag());
    SetDirty(false);
  }

private:
  std::string mLabel;
  IPopupMenu mMenu;
};

// Matrix row cells ------------------------------------------------------------

inline const char* ShortSource(int source)
{
  return grid_performance::sourceShortNames[std::clamp(source, 0, grid_performance::SourceCount - 1)];
}
// A row's Source or Via: a colored dot and the source name.
class SourceCell : public IControl
{
public:
  SourceCell(const IRECT& r, int parameter, bool compact) : IControl(r, parameter), mCompact(compact) {}
  void Draw(IGraphics& g) override
  {
    const int source = GetParam()->Int();
    g.FillRoundRect(surface, mRECT, 3.f);
    g.DrawRoundRect(mMouseIsOver ? muted : border, mRECT, 3.f);
    if (source == grid_performance::SourceNone)
    {
      g.DrawText(Text(13.f, muted, EAlign::Center), mCompact ? "—" : "Source", mRECT);
      return;
    }
    const auto color = SourceColor(source);
    if (mCompact)
    {
      g.DrawText(Text(13.f, color, EAlign::Center), ShortSource(source), mRECT);
      return;
    }
    g.FillCircle(color, mRECT.L + 11.f, mRECT.MH(), 3.5f);
    FittedText(g, Text(13.f, text), SourceName(*PLUG(), source), mRECT.GetReducedFromLeft(20.f).GetReducedFromRight(4.f));
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    if (!mod.L) return;
    mMenu.Clear();
    for (int source : grid_performance::sourceMenuOrder)
      mMenu.AddItem(new IPopupMenu::Item(SourceName(*PLUG(), source).c_str(),
                                         GetParam()->Int() == source ? IPopupMenu::Item::kChecked : 0, source));
    GetUI()->CreatePopupMenu(*this, mMenu, mRECT);
  }
  void OnPopupMenuSelection(IPopupMenu* menu, int) override
  {
    if (const auto* item = menu ? menu->GetChosenItem() : nullptr)
      PLUG()->SetParameterFromUI(GetParamIdx(), item->GetTag());
  }

private:
  bool mCompact;
  IPopupMenu mMenu;
};

inline std::string SlotID(int slot)
{
  auto name = GridSlotName(slot);
  name.erase(std::remove(name.begin(), name.end(), ' '), name.end());
  return name;
}
inline std::string SlotModuleName(const PLUG_CLASS_NAME& plug, int slot)
{
  const auto path = plug.GetSlotPath(slot);
  const auto effect = plug.GetSlotEffect(slot);
  if (!path.empty()) return nam_library::PathText(std::filesystem::u8path(path).stem());
  return effect == SlotEffect::NAM ? "Empty" : SlotEffectName(effect);
}
// The module's own knob labels, as the slot editor shows them now.
inline std::vector<grid_modulation::Destination> SlotDestinations(const PLUG_CLASS_NAME& plug, int slot)
{
  const auto flag = [&](int p) { return plug.GetParam(SlotUtilityParam(slot, p))->Bool(); };
  return grid_modulation::Destinations(plug.GetSlotEffect(slot), !plug.GetSlotPath(slot).empty(), flag(uTempoSync),
                                       flag(uEQMode), flag(uVibeMode), flag(uParamLowMode));
}
// Destination: "A2 › Mix" with the module name, or the routing control. The
// nested menu lists each slot's module and its modulatable knobs.
class DestinationCell : public IControl
{
public:
  DestinationCell(const IRECT& r, int row)
  : IControl(r, {PerformanceParam(grid_performance::RowParam(row, grid_performance::RowSlot)),
                 PerformanceParam(grid_performance::RowParam(row, grid_performance::RowDestination)),
                 PerformanceParam(grid_performance::RowParam(row, grid_performance::RowSource))})
  , mRow(row)
  {
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_performance;
    g.FillRoundRect(surface, mRECT, 3.f);
    g.DrawRoundRect(mMouseIsOver ? muted : border, mRECT, 3.f);
    const int slot = Value(RowSlot), control = Value(RowDestination);
    const bool unused = Value(RowSource) == SourceNone;
    const auto inner = mRECT.GetHPadded(-8.f);
    if (unused && slot == 0 && control == 0)
    {
      g.DrawText(Text(13.f, muted), "Destination", inner);
      return;
    }
    std::string primary, secondary;
    bool known = true;
    if (slot == gridSlot)
    {
      primary = std::string("Grid › ") + (control >= 0 && control < GridDestinations ? gridDestinationNames[control] : "?");
      known = control >= 0 && control < GridDestinations;
    }
    else
    {
      const auto destinations = SlotDestinations(*PLUG(), slot);
      const auto found = std::find_if(destinations.begin(), destinations.end(), [&](const auto& d) { return d.control == control; });
      known = found != destinations.end();
      primary = SlotID(slot) + " › " + (known ? found->label : grid_modulation::ControlName(control));
      secondary = known ? SlotModuleName(*PLUG(), slot) : "not on this module";
    }
    const auto style = Text(13.f, unused || !known ? muted : text);
    IRECT measured; g.MeasureText(style, primary.c_str(), measured);
    FittedText(g, style, primary, inner);
    if (!secondary.empty() && measured.W() + 24.f < inner.W())
      FittedText(g, Text(12.f, muted, EAlign::Far), secondary, inner.GetReducedFromLeft(measured.W() + 10.f));
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    using namespace grid_performance;
    if (!mod.L) return;
    mMenu.Clear();
    const int slot = Value(RowSlot), control = Value(RowDestination);
    for (int s = 0; s < gridSlot; ++s)
    {
      const auto label = GridSlotName(s) + "  ·  " + SlotModuleName(*PLUG(), s);
      const auto destinations = SlotDestinations(*PLUG(), s);
      if (destinations.empty())
      {
        mMenu.AddItem(new IPopupMenu::Item(label.c_str(), IPopupMenu::Item::kDisabled));
        continue;
      }
      auto* knobs = new IPopupMenu;
      for (const auto& d : destinations)
        knobs->AddItem(new IPopupMenu::Item(d.label, s == slot && d.control == control ? IPopupMenu::Item::kChecked : 0,
                                            s * 1024 + d.control));
      mMenu.AddItem(label.c_str(), knobs);
    }
    mMenu.AddSeparator();
    auto* grid = new IPopupMenu;
    for (int d = 0; d < GridDestinations; ++d)
      grid->AddItem(new IPopupMenu::Item(gridDestinationNames[d], slot == gridSlot && control == d ? IPopupMenu::Item::kChecked : 0,
                                         gridSlot * 1024 + d));
    mMenu.AddItem("Grid routing", grid);
    GetUI()->CreatePopupMenu(*this, mMenu, mRECT);
  }
  void OnPopupMenuSelection(IPopupMenu* menu, int) override
  {
    using namespace grid_performance;
    const auto* item = menu ? menu->GetChosenItem() : nullptr;
    if (!item || item->GetTag() < 0) return;
    const auto param = [&](int field) { return PerformanceParam(RowParam(mRow, field)); };
    // Pause an active row while both halves of its destination change.
    const int source = Value(RowSource);
    if (source != SourceNone) PLUG()->SetParameterFromUI(param(RowSource), SourceNone);
    PLUG()->SetParameterFromUI(param(RowSlot), item->GetTag() / 1024);
    PLUG()->SetParameterFromUI(param(RowDestination), item->GetTag() % 1024);
    if (source != SourceNone) PLUG()->SetParameterFromUI(param(RowSource), source);
  }

private:
  int Value(int field)
  { return GetDelegate()->GetParam(PerformanceParam(grid_performance::RowParam(mRow, field)))->Int(); }
  int mRow;
  IPopupMenu mMenu;
};
// Bipolar depth: drag sideways (or up/down), Shift for fine steps, double-click for 0.
// A thin tick shows the row's live amount while its source runs.
class DepthCell : public IControl
{
public:
  DepthCell(const IRECT& r, int row) : IControl(r, PerformanceParam(grid_performance::RowParam(row, grid_performance::RowDepth))), mRow(row)
  {
    SetTooltip("Depth as a share of the destination knob's range. Drag sideways or up/down; Shift for fine steps; double-click for 0.");
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_performance;
    const auto v = PLUG()->PerformanceValues();
    const int source = static_cast<int>(v[RowParam(mRow, RowSource)]);
    const double depth = GetParam()->Value() * .01;
    const auto color = SourceColor(source);
    g.FillRoundRect(surface, mRECT, 3.f);
    g.DrawRoundRect(mMouseIsOver ? muted : border, mRECT, 3.f);
    // Value above a slim bipolar bar; the bar fills from its centre.
    const IRECT track(mRECT.L + 8.f, mRECT.B - 9.f, mRECT.R - 8.f, mRECT.B - 6.f);
    const float centre = track.MW(), x = centre + static_cast<float>(depth) * track.W() * .5f;
    g.FillRect(border, track);
    if (depth != 0.)
      g.FillRect(color.WithOpacity(source == SourceNone ? .4f : 1.f), IRECT(std::min(centre, x), track.T, std::max(centre, x), track.B));
    g.FillRect(muted, IRECT(centre - .5f, track.T - 2.f, centre + .5f, track.B + 2.f));
    double live = 0., via = 0.;
    const auto monitor = PLUG()->ModulationMonitor();
    if (RowActive(v, mRow) && grid_modulation::LiveValue(source, v, monitor, live)
        && grid_modulation::LiveValue(static_cast<int>(v[RowParam(mRow, RowVia)]), v, monitor, via))
    {
      const float liveX = centre + static_cast<float>(std::clamp(grid_modulation::RowAmount(v, mRow, live, via), -1., 1.)) * track.W() * .5f;
      g.FillCircle(text, liveX, track.MH(), 2.5f);
    }
    WDL_String value; value.SetFormatted(16, depth == 0. ? "0%%" : "%+.0f%%", depth * 100.);
    g.DrawText(Text(13.f, source == SourceNone ? muted : text, EAlign::Center), value.Get(),
               IRECT(mRECT.L, mRECT.T + 1.f, mRECT.R, track.T - 1.f));
  }
  void OnMouseDrag(float, float, float dX, float dY, const IMouseMod& mod) override
  {
    const double change = (dX - dY) / mRECT.W() * (mod.S ? .1 : 1.);
    SetValue(std::clamp(GetValue() + change, 0., 1.));
    SetDirty(true);
  }
  void OnMouseDblClick(float, float, const IMouseMod&) override
  {
    SetValue(GetParam()->ToNormalized(0.));
    SetDirty(true);
  }
  bool IsDirty() override
  {
    using namespace grid_performance;
    if (!IsHidden() && PLUG()->ModulationMonitor().running)
    {
      const auto v = PLUG()->PerformanceValues();
      const int source = static_cast<int>(v[RowParam(mRow, RowSource)]);
      if (RowActive(v, mRow) && DynamicSource(source))
        SetDirty(false);
    }
    return IControl::IsDirty();
  }

private:
  int mRow;
};
class ClearCell : public IControl
{
public:
  ClearCell(const IRECT& r, int row) : IControl(r), mRow(row) { SetTooltip("Clear this row"); }
  void Draw(IGraphics& g) override
  {
    const float x = mRECT.MW(), y = mRECT.MH();
    const auto color = mMouseIsOver ? text : muted;
    g.DrawLine(color, x - 4.f, y - 4.f, x + 4.f, y + 4.f, nullptr, 1.5f);
    g.DrawLine(color, x - 4.f, y + 4.f, x + 4.f, y - 4.f, nullptr, 1.5f);
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    if (mod.L) PLUG()->ClearModulationRow(mRow);
  }

private:
  int mRow;
};

// Source displays ----------------------------------------------------------

inline void PanelBackground(IGraphics& g, const IRECT& r)
{
  g.FillRoundRect(surface, r, 4.f);
  g.DrawRoundRect(border, r, 4.f);
}
// Live sources redraw only while audio runs and the source is shown.
class LiveSourceControl : public IControl
{
public:
  using IControl::IControl;
  bool IsDirty() override
  {
    if (!IsHidden() && (PLUG()->ModulationMonitor().running || mWasRunning)) SetDirty(false);
    mWasRunning = PLUG()->ModulationMonitor().running;
    return IControl::IsDirty();
  }

private:
  bool mWasRunning = false;
};
class LFODisplay : public LiveSourceControl
{
public:
  LFODisplay(const IRECT& r, int instance = 0) : LiveSourceControl(r), mInstance(instance) { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override
  {
    using namespace grid_performance;
    PanelBackground(g, mRECT);
    const auto v = PLUG()->PerformanceValues();
    const int shape = static_cast<int>(v[SourceParam(LFOShape, mInstance)]);
    WDL_String caption;
    if (v[SourceParam(LFOSync, mInstance)] >= .5) caption.SetFormatted(64, "%s  ·  %s", lfoShapeNames[shape], grid_tempo::names[static_cast<int>(v[SourceParam(LFODivision, mInstance)])]);
    else caption.SetFormatted(64, "%s  ·  %.2f Hz", lfoShapeNames[shape], v[SourceParam(LFORate, mInstance)]);
    g.DrawText(Text(14.f, SourceColor(mInstance ? SourceLFO2 : SourceLFO)), caption.Get(), IRECT(mRECT.L + 14, mRECT.T + 8, mRECT.R - 14, mRECT.T + 28));
    std::string mode = v[SourceParam(LFOOneShot, mInstance)] >= .5 ? "one shot" : "one cycle";
    if (v[SourceParam(LFORetrigger, mInstance)] > 0.) mode += std::string("  ·  ") + retriggerNames[static_cast<int>(v[SourceParam(LFORetrigger, mInstance)])];
    g.DrawText(Text(12.f, muted, EAlign::Far), mode.c_str(), IRECT(mRECT.L + 14, mRECT.T + 8, mRECT.R - 14, mRECT.T + 28));
    const auto plot = IRECT(mRECT.L + 14, mRECT.T + 38, mRECT.R - 14, mRECT.B - 14);
    g.DrawLine(border, plot.L, plot.MH(), plot.R, plot.MH());
    const auto y = [&](double value) { return plot.MH() - static_cast<float>(value) * plot.H() * .45f; };
    // Random has no fixed curve: a held step per cycle, sketched with a fixed pattern.
    static constexpr double random[]{.55, -.35, .85, -.8, .15, -.5, .7, -.1};
    const auto wave = [&](double phase) { return shape == 5 ? random[std::min(7, static_cast<int>(phase * 8.))] : LFOWave(shape, phase); };
    const auto color = SourceColor(mInstance ? SourceLFO2 : SourceLFO);
    float px = plot.L, py = y(wave(0.));
    for (int i = 1; i <= 240; ++i)
    {
      const double phase = std::min(i / 240., .99999);
      const float x = plot.L + plot.W() * i / 240.f, ny = y(wave(phase));
      g.DrawLine(color, px, py, x, ny, nullptr, 2.f);
      px = x; py = ny;
    }
    const auto monitor = PLUG()->ModulationMonitor().Instance(mInstance);
    if (monitor.running && shape != 5)
    {
      const float x = plot.L + plot.W() * monitor.phase;
      g.DrawLine(color.WithOpacity(.35f), x, plot.T, x, plot.B);
      g.FillCircle(text, x, y(monitor.lfo), 4.f);
    }
    else if (monitor.running)
      g.FillCircle(text, plot.R - 6.f, y(monitor.lfo), 4.f);
  }
private:
  int mInstance;
};
class EnvelopeDisplay : public LiveSourceControl
{
public:
  EnvelopeDisplay(const IRECT& r, int instance = 0) : LiveSourceControl(r), mInstance(instance) { mIgnoreMouse = true; }
  bool IsDirty() override
  {
    if (!IsHidden())
    {
      const auto monitor = PLUG()->ModulationMonitor().Instance(mInstance);
      mLevel[mWrite] = monitor.running ? monitor.input : 0.f;
      mOutput[mWrite] = monitor.running ? monitor.envelope : 0.f;
      mWrite = (mWrite + 1) % mOutput.size();
    }
    return LiveSourceControl::IsDirty();
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_performance;
    PanelBackground(g, mRECT);
    const auto v = PLUG()->PerformanceValues();
    WDL_String listen; PLUG()->GetParam(PerformanceParam(SourceParam(Env1Input, mInstance)))->GetDisplay(listen);
    WDL_String caption; caption.SetFormatted(96, "Envelope  ·  %s  ·  %.1f / %.0f ms", listen.Get(), v[SourceParam(EnvAttack, mInstance)], v[SourceParam(EnvRelease, mInstance)]);
    const auto color = SourceColor(mInstance ? SourceEnv2 : SourceEnvelope);
    g.DrawText(Text(14.f, color), caption.Get(), IRECT(mRECT.L + 14, mRECT.T + 8, mRECT.R - 14, mRECT.T + 28));
    g.DrawText(Text(12.f, muted, EAlign::Far), "grey: level  ·  colour: output", IRECT(mRECT.L + 14, mRECT.T + 8, mRECT.R - 14, mRECT.T + 28));
    const auto plot = IRECT(mRECT.L + 14, mRECT.T + 38, mRECT.R - 14, mRECT.B - 14);
    g.DrawLine(border, plot.L, plot.B, plot.R, plot.B);
    // Level on a -60..0 dBFS scale, with the threshold; output on a 0..1 scale.
    const auto levelY = [&](double level) {
      const double db = 20. * std::log10(std::max(level, 1.e-6));
      return plot.B - plot.H() * static_cast<float>(std::clamp((db + 60.) / 60., 0., 1.));
    };
    const size_t count = mOutput.size();
    const float w = plot.W() / count;
    for (size_t i = 0; i < count; ++i)
    {
      const size_t at = (mWrite + i) % count;
      const float x = plot.L + w * i;
      g.FillRect(color.WithOpacity(.55f), IRECT(x, plot.B - plot.H() * mOutput[at], x + w + .5f, plot.B));
      if (i) g.DrawLine(muted.WithOpacity(.8f), x - w, levelY(mLevel[(at + count - 1) % count]), x, levelY(mLevel[at]));
    }
    if (v[SourceParam(EnvThreshold, mInstance)] > kThresholdOff)
    {
      const float y = levelY(std::pow(10., v[SourceParam(EnvThreshold, mInstance)] * .05));
      for (float x = plot.L; x < plot.R; x += 8.f) g.DrawLine(text.WithOpacity(.7f), x, y, std::min(x + 4.f, plot.R), y);
      WDL_String label; label.SetFormatted(32, "threshold %.0f dB", v[SourceParam(EnvThreshold, mInstance)]);
      g.DrawText(Text(11.f, text, EAlign::Far), label.Get(), IRECT(plot.L, y - 14.f, plot.R, y - 2.f));
    }
    if (!PLUG()->ModulationMonitor().Instance(mInstance).running)
      g.DrawText(Text(13.f, muted, EAlign::Center), "Shown while a row uses a running source", plot);
  }

private:
  int mInstance;
  std::array<float, 180> mLevel{}, mOutput{};
  size_t mWrite = 0;
};
// Eight step values as bars around the centre (50% = no change). Drag across to draw.
class StepEditor : public IControl
{
public:
  StepEditor(const IRECT& r, int instance = 0)
  : IControl(r, {PerformanceParam(grid_performance::SourceParam(grid_performance::Step1, instance)), PerformanceParam(grid_performance::SourceParam(grid_performance::Step2, instance)),
                 PerformanceParam(grid_performance::SourceParam(grid_performance::Step3, instance)), PerformanceParam(grid_performance::SourceParam(grid_performance::Step4, instance)),
                 PerformanceParam(grid_performance::SourceParam(grid_performance::Step5, instance)), PerformanceParam(grid_performance::SourceParam(grid_performance::Step6, instance)),
                 PerformanceParam(grid_performance::SourceParam(grid_performance::Step7, instance)), PerformanceParam(grid_performance::SourceParam(grid_performance::Step8, instance))}), mInstance(instance)
  {
    SetTooltip("Draw the eight steps: above the centre line adds, below subtracts. Each step holds for one sequencer step.");
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_performance;
    PanelBackground(g, mRECT);
    const auto v = PLUG()->PerformanceValues();
    WDL_String caption;
    if (v[SourceParam(StepSync, mInstance)] >= .5) caption.SetFormatted(64, "Eight steps  ·  %s each", grid_tempo::names[static_cast<int>(v[SourceParam(StepDivision, mInstance)])]);
    else caption.SetFormatted(64, "Eight steps  ·  %.2f Hz", v[SourceParam(StepRate, mInstance)]);
    const auto color = SourceColor(mInstance ? SourceSeq2 : SourceSequencer);
    g.DrawText(Text(14.f, color), caption.Get(), IRECT(mRECT.L + 14, mRECT.T + 8, mRECT.R - 14, mRECT.T + 28));
    const auto plot = Plot();
    g.DrawLine(border, plot.L, plot.MH(), plot.R, plot.MH());
    const auto monitor = PLUG()->ModulationMonitor().Instance(mInstance);
    for (int i = 0; i < 8; ++i)
    {
      const auto cell = plot.SubRectHorizontal(8, i).GetHPadded(-4.f);
      const float y = cell.B - cell.H() * static_cast<float>(GetValue(i));
      const bool current = monitor.running && monitor.step == i;
      g.FillRect(color.WithOpacity(current ? .9f : .5f), IRECT(cell.L, std::min(y, cell.MH()), cell.R, std::max(y, cell.MH())));
      g.DrawLine(current ? text : color, cell.L, y, cell.R, y, nullptr, 2.f);
    }
  }
  void OnMouseDown(float x, float y, const IMouseMod& mod) override { if (mod.L) Set(x, y); }
  void OnMouseDrag(float x, float y, float, float, const IMouseMod&) override { Set(x, y); }
  bool IsDirty() override
  {
    if (!IsHidden() && PLUG()->ModulationMonitor().Instance(mInstance).running) SetDirty(false);
    return IControl::IsDirty();
  }

private:
  int mInstance;
  IRECT Plot() const { return IRECT(mRECT.L + 14, mRECT.T + 38, mRECT.R - 14, mRECT.B - 14); }
  void Set(float x, float y)
  {
    const auto plot = Plot();
    const int step = std::clamp(static_cast<int>((x - plot.L) / plot.W() * 8.f), 0, 7);
    SetValue(std::clamp((plot.B - y) / plot.H(), 0.f, 1.f), step);
    SetDirty(true, step);
  }
};
// Routing always shows the signal path, with a compact crossover below it when enabled.
class CrossoverView : public IControl
{
public:
  CrossoverView(const IRECT& r)
  : IControl(r, {PerformanceParam(grid_performance::Crossover), PerformanceParam(grid_performance::CrossoverFrequency),
                 PerformanceParam(grid_performance::CrossoverSlope), PerformanceParam(grid_performance::LaneMode), kParallelBlend,
                 PerformanceParam(grid_performance::LaneAGain), PerformanceParam(grid_performance::LaneBGain),
                 PerformanceParam(grid_performance::LaneABalance), PerformanceParam(grid_performance::LaneBBalance),
                 PerformanceParam(grid_performance::LaneAMute), PerformanceParam(grid_performance::LaneBMute),
                 PerformanceParam(grid_performance::LaneASolo), PerformanceParam(grid_performance::LaneBSolo)})
  { SetTooltip("M mutes a lane. S solos and unmutes it, clearing the other solo. Solo keeps the A/B blend level. M and S on the same lane are exclusive."); }

  void Draw(IGraphics& g) override
  {
    using namespace grid_performance;
    VisualizationBackground(g, mRECT);
    const bool serial = Value(LaneMode) >= .5;
    const bool split = !serial && Value(Crossover) >= .5;
    if (serial != mSerial) { mPeaks = {}; mSerial = serial; }
    DrawFlow(g, serial, split);
    if (split) DrawSplit(g);
    DrawMeters(g, serial);
  }
  bool IsHit(float x, float y) const override
  {
    if (Value(grid_performance::LaneMode) >= .5) return false;
    for (int lane = 0; lane < 2; ++lane)
      for (bool solo : {false, true})
        if (LaneButton(lane, solo).Contains(x, y)) return true;
    return false;
  }
  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    using namespace grid_performance;
    if (!mod.L || Value(LaneMode) >= .5) return;
    for (int lane = 0; lane < 2; ++lane)
      for (bool solo : {false, true})
        if (LaneButton(lane, solo).Contains(x, y))
        {
          const int muteParam = lane ? LaneBMute : LaneAMute;
          const int soloParam = lane ? LaneBSolo : LaneASolo;
          const int target = solo ? soloParam : muteParam;
          const bool enable = Value(target) < .5;
          if (enable)
          {
            PLUG()->SetParameterFromUI(PerformanceParam(solo ? muteParam : soloParam), 0.);
            if (solo) PLUG()->SetParameterFromUI(PerformanceParam(lane ? LaneASolo : LaneBSolo), 0.);
          }
          PLUG()->SetParameterFromUI(PerformanceParam(target), enable ? 1. : 0.);
          return;
        }
  }
  void OnMsgFromDelegate(int, int size, const void* data) override
  {
    if (!data || size != sizeof(iplug::ISenderData<1, FixedNAMGrid::LanePeaks>)) return;
    const auto& peaks = static_cast<const iplug::ISenderData<1, FixedNAMGrid::LanePeaks>*>(data)->vals[0];
    for (int lane = 0; lane < 2; ++lane)
      for (int ch = 0; ch < 2; ++ch) mPeaks[lane][ch] = std::max(mPeaks[lane][ch], peaks[lane][ch]);
    SetDirty(false);
  }
  bool IsDirty() override
  {
    const double now = ModuleLevels::Now(), dt = std::max(0., now - mTime); mTime = now;
    const float decay = static_cast<float>(std::exp(-dt / .35));
    for (auto& lane : mPeaks)
      for (auto& peak : lane)
        if (peak > 1.e-6f) { peak *= decay; SetDirty(false); } else peak = 0.f;
    if (!IsHidden())
    {
      float x, y; GetUI()->GetMouseLocation(x, y);
      const bool hovered = Value(grid_performance::LaneMode) < .5
        && Value(grid_performance::Crossover) >= .5 && SplitPlot().Contains(x, y);
      if (hovered != mHovered || (hovered && x != mHoverX))
      { mHovered = hovered; mHoverX = x; SetDirty(false); }
    }
    return IControl::IsDirty();
  }
private:
  FixedNAMGrid::LanePeaks mPeaks{};
  double mTime = ModuleLevels::Now();
  bool mHovered = false, mSerial = false;
  float mHoverX = -1.f;
  double Value(int p) const
  { return static_cast<PLUG_CLASS_NAME*>(const_cast<CrossoverView*>(this)->GetDelegate())->GetParam(PerformanceParam(p))->Value(); }
  float LaneY(int lane) const { return mRECT.T + (lane ? 88.f : 46.f); }
  IRECT LaneNode(int lane) const { return IRECT(mRECT.MW()-112, LaneY(lane)-13, mRECT.MW()+112, LaneY(lane)+13); }
  IRECT LaneButton(int lane, bool solo) const
  {
    const auto node = LaneNode(lane);
    return IRECT(node.R-(solo?24.f:47.f), node.T+4, node.R-(solo?5.f:28.f), node.B-4);
  }
  IRECT SplitPlot() const { return IRECT(mRECT.L+14, mRECT.T+122, mRECT.R-14, mRECT.B-54); }
  void DrawMeters(IGraphics& g, bool serial)
  {
    for (int lane = 0; lane < 2; ++lane)
    {
      const float x = mRECT.L+14+lane*(mRECT.W()*.5f), y = mRECT.B-20;
      const auto color = lane ? cabColor : ampColor;
      WDL_String caption;
      const float peak = std::max(mPeaks[lane][0], mPeaks[lane][1]);
      const char* name = lane ? (serial ? "After B" : "B") : (serial ? "After A" : "A");
      if (peak > 1.e-6f) caption.SetFormatted(64, "%s  %.1f dBFS", name, 20.f*std::log10(peak));
      else caption.SetFormatted(64, "%s  -inf dBFS", name);
      g.DrawText(Text(11.f, muted), caption.Get(), IRECT(x, y-17, x+mRECT.W()*.5f-24, y-3));
      for (int ch = 0; ch < 2; ++ch)
      {
        const IRECT bar(x+12, y+ch*8, x+mRECT.W()*.5f-28, y+ch*8+5);
        g.DrawText(Text(9.f, muted), ch ? "R" : "L", IRECT(x, y+ch*8-3, x+10, y+ch*8+8));
        g.FillRect(surface, bar);
        const float db = 20.f*std::log10(std::max(1.e-6f, mPeaks[lane][ch]));
        g.FillRect(db >= 0.f ? COLOR_RED : color, IRECT(bar.L, bar.T, bar.L+bar.W()*std::clamp((db+60.f)/60.f, 0.f, 1.f), bar.B));
      }
    }
  }
  void DrawFlow(IGraphics& g, bool serial, bool split)
  {
    using namespace grid_performance;
    const float l = mRECT.L+18, r = mRECT.R-18, y = mRECT.T+67;
    g.DrawText(Text(14.f, text), serial ? "Sequential: Pre > A > B > Post" : "Parallel: lane contributions to Post",
               IRECT(l, mRECT.T+6, r, mRECT.T+26));
    const auto box = [&](const IRECT& b, const char* label, IColor color) {
      g.FillRoundRect(surface, b, 4.f); g.DrawRoundRect(color, b, 4.f);
      g.DrawText(Text(12.f, color, EAlign::Center), label, b);
    };
    if (serial)
    {
      const float step = (r-l)/4.f;
      g.DrawLine(border, l+20, y, r-20, y, nullptr, 2.f);
      const char* labels[]{"Pre", "A", "B", "Post"};
      for (int i = 0; i < 4; ++i)
        box(IRECT(l+i*step, y-17, l+(i+1)*step-12, y+17), labels[i], i == 1 ? ampColor : i == 2 ? cabColor : muted);
      g.DrawText(Text(12.f, muted, EAlign::Center), "Meters: after each lane, before Post", IRECT(l, y+35, r, y+57));
      return;
    }
    const float fork = l+54, merge = r-54;
    g.DrawLine(border, l+42, y, fork, y); g.DrawLine(border, fork, LaneY(0), fork, LaneY(1));
    g.DrawLine(border, merge, LaneY(0), merge, LaneY(1)); g.DrawLine(border, merge, y, r-42, y);
    box(IRECT(l, y-13, l+42, y+13), "Pre", muted);
    box(IRECT(r-42, y-13, r, y+13), "Post", muted);
    const double blend = PLUG()->GetParam(kParallelBlend)->Value();
    for (int lane = 0; lane < 2; ++lane)
    {
      const auto color = lane ? cabColor : ampColor;
      const auto node = LaneNode(lane);
      g.DrawLine(color, fork, LaneY(lane), merge, LaneY(lane));
      const double balance = Value(lane ? LaneBBalance : LaneABalance);
      const double gain = Value(lane ? LaneBGain : LaneAGain);
      const bool mute = Value(lane ? LaneBMute : LaneAMute) >= .5;
      const bool solo = Value(lane ? LaneBSolo : LaneASolo) >= .5;
      const bool silent = mute || (Value(lane ? LaneASolo : LaneBSolo) >= .5 && !solo);
      g.FillRoundRect(surface, node, 4.f); g.DrawRoundRect(silent ? muted : color, node, 4.f);
      WDL_String label;
      label.SetFormatted(80, "%c %.0f%% %+.1f dB %s", lane ? 'B' : 'A', lane ? blend : 100.-blend,
                         gain, silent ? "Muted" : balance == 0. ? "Centre" : balance < 0. ? "Left" : "Right");
      g.DrawText(Text(11.f, silent ? muted : color, EAlign::Center), label.Get(), IRECT(node.L+4, node.T, node.R-50, node.B));
      for (bool soloButton : {false, true})
      {
        const bool active = soloButton ? solo : mute;
        const auto button = LaneButton(lane, soloButton);
        g.FillRoundRect(active ? color : editorBackground, button, 2.f);
        g.DrawRoundRect(active ? color : border, button, 2.f);
        g.DrawText(Text(11.f, active ? background : muted, EAlign::Center), soloButton ? "S" : "M", button);
      }
    }
    if (!split)
      g.DrawText(Text(11.f, muted, EAlign::Center), "Empty lanes pass the signal from Pre.", IRECT(l, mRECT.T+108, r, mRECT.T+129));
  }
  void DrawSplit(IGraphics& g)
  {
    using namespace grid_performance;
    const double frequency = Value(CrossoverFrequency);
    const bool steep = Value(CrossoverSlope) >= .5;
    const auto plot = SplitPlot();
    g.DrawText(Text(11.f, muted), "Split: low A / high B", IRECT(plot.L, plot.T-18, plot.R, plot.T-3));
    g.DrawText(Text(11.f, muted, EAlign::Far), steep ? "24 dB/oct" : "6 dB/oct", IRECT(plot.L, plot.T-18, plot.R, plot.T-3));
    const auto x = [&](double hz) { return plot.L+plot.W()*static_cast<float>(std::log(hz/20.)/std::log(1000.)); };
    for (double hz : {100., 1000., 10000.}) g.DrawLine(border.WithOpacity(.5f), x(hz), plot.T, x(hz), plot.B);
    for (int lane = 0; lane < 2; ++lane)
    {
      float px = plot.L, py = 0.f;
      for (int i = 0; i <= 160; ++i)
      {
        const double hz = 20.*std::pow(1000., i/160.), ratio = hz/frequency;
        const double low = steep ? 1./(1.+std::pow(ratio, 4.)) : 1./std::sqrt(1.+ratio*ratio);
        const double high = steep ? std::pow(ratio, 4.)/(1.+std::pow(ratio, 4.)) : ratio/std::sqrt(1.+ratio*ratio);
        const float nx = x(hz), ny = plot.B-plot.H()*.9f*static_cast<float>(lane ? high : low);
        if (i) g.DrawLine(lane ? cabColor : ampColor, px, py, nx, ny, nullptr, 1.5f);
        px = nx; py = ny;
      }
    }
    g.DrawLine(muted, x(frequency), plot.T, x(frequency), plot.B);
    for (const auto& [hz, label] : {std::pair{100., "100"}, std::pair{1000., "1k"}, std::pair{10000., "10k"}})
      g.DrawText(Text(10.f, muted, EAlign::Center), label, IRECT(x(hz)-20, plot.B+1, x(hz)+20, plot.B+13));
    DrawFrequencyCursor(g, plot, 20., 20000.);
  }
};

// Header scene buttons: click recalls a stored scene; right-click stores or clears.
class SceneButton : public IControl
{
public:
  SceneButton(const IRECT& r, int scene) : IControl(r), mScene(scene)
  {
    WDL_String tip;
    tip.SetFormatted(160, "Scene %d: click to recall. Right-click to store the current settings here or clear it. "
                          "Scenes keep the loaded models and modules.", scene + 1);
    SetTooltip(tip.Get());
  }
  void Draw(IGraphics& g) override
  {
    const bool stored = PLUG()->HasPerformanceScene(mScene), active = PLUG()->GetActiveScene() == mScene;
    g.FillRoundRect(active ? accent : stored ? accent.WithOpacity(.15f) : surface, mRECT, 3.f);
    g.DrawRoundRect(stored ? accent.WithOpacity(mMouseIsOver ? 1.f : .7f) : mMouseIsOver ? muted : border, mRECT, 3.f);
    const char* label[]{"1", "2", "3"};
    g.DrawText(Text(14.f, active ? background : stored ? accent : muted, EAlign::Center), label[mScene], mRECT);
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    if (mod.R)
    {
      mMenu.Clear();
      WDL_String store, clear;
      store.SetFormatted(32, "Store in scene %d", mScene + 1);
      clear.SetFormatted(32, "Clear scene %d", mScene + 1);
      mMenu.AddItem(new IPopupMenu::Item(store.Get(), 0, 1));
      mMenu.AddItem(new IPopupMenu::Item(clear.Get(), PLUG()->HasPerformanceScene(mScene) ? 0 : IPopupMenu::Item::kDisabled, 2));
      GetUI()->CreatePopupMenu(*this, mMenu, mRECT);
    }
    else if (mod.L)
      PLUG()->RecallPerformanceScene(mScene);
    SetDirty(false);
  }
  void OnPopupMenuSelection(IPopupMenu* menu, int) override
  {
    const auto* item = menu ? menu->GetChosenItem() : nullptr;
    if (!item) return;
    if (item->GetTag() == 1) PLUG()->StorePerformanceScene(mScene);
    if (item->GetTag() == 2) PLUG()->ClearPerformanceScene(mScene);
    SetDirty(false);
  }
  bool IsDirty() override
  {
    // Recall, store or a project load may change the state outside this button.
    const int state = (PLUG()->HasPerformanceScene(mScene) ? 1 : 0) | (PLUG()->GetActiveScene() == mScene ? 2 : 0);
    if (state != mState) { mState = state; SetDirty(false); }
    return IControl::IsDirty();
  }

private:
  int mScene, mState = -1;
  IPopupMenu mMenu;
};
// First click opens Routing; further clicks change mode while keeping it visible.
class RoutingButton : public IControl
{
public:
  RoutingButton(const IRECT& r) : IControl(r, PerformanceParam(grid_performance::LaneMode))
  { SetTooltip("Open Routing; click again to switch Sequential / Parallel while keeping it open. Amber = A; blue = B. Close or Escape returns to the module."); }
  void Draw(IGraphics& g) override
  {
    const bool open = PLUG()->IsRoutingOpen();
    g.FillRoundRect(open ? IColor(255, 49, 53, 59) : surface, mRECT, 3.f);
    g.DrawRoundRect(open || mMouseIsOver ? muted : border, mRECT, 3.f);
    const bool serial = PLUG()->GetParam(PerformanceParam(grid_performance::LaneMode))->Int() == grid_performance::Sequential;
    DrawFlowIcon(g, mRECT.MW(), mRECT.T + 14.f, serial);
    g.DrawText(Text(10.f, open ? text : muted, EAlign::Center), serial ? "Sequential" : "Parallel", mRECT.GetFromBottom(20.f));
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    if (!mod.L) return;
    if (!PLUG()->IsRoutingOpen()) PLUG()->SetRoutingOpen(true);
    else PLUG()->SetParameterFromUI(PerformanceParam(grid_performance::LaneMode), GetParam()->Int() ? 0. : 1.);
  }
  bool IsDirty() override
  {
    if (PLUG()->IsRoutingOpen() != mOpen) { mOpen = !mOpen; SetDirty(false); }
    return IControl::IsDirty();
  }

private:
  bool mOpen = false;
};
} // namespace grid_ui

// Optional performance controls on the main page. These share the modulation page's
// parameters, so host automation and edits in either view always stay in sync.
class NAMGridMacroControl : public IContainerBase
{
public:
  explicit NAMGridMacroControl(const IRECT& r) : IContainerBase(r) {}
  void OnAttached() override
  {
    using namespace grid_ui; using namespace grid_performance;
    for (int i = 0; i < 4; ++i)
    {
      auto* knob = new MacroKnob(IRECT(4, 178 + i * 72, grid_layout::sidebarRight - 4, 246 + i * 72), i, true);
      AddChildControl(knob);
      AddChildControl(new MacroLabel(knob->NameBounds(), i, true));
    }
  }
  void Draw(IGraphics& g) override
  {
    const auto panel = IRECT(-12, mRECT.T, mRECT.R, mRECT.B);
    g.FillRoundRect(grid_ui::editorBackground, panel, grid_layout::cardCornerRadius);
    g.DrawRoundRect(grid_ui::border, panel, grid_layout::cardCornerRadius);
  }
};

// Routing replaces the module editor below the grid, laid out like one:
// two rows of four controls with a visualization panel on the right.
class NAMGridRoutingControl : public IContainerBase
{
public:
  explicit NAMGridRoutingControl(const IRECT& r) : IContainerBase(r) {}
  void OnAttached() override
  {
    using namespace grid_ui; using namespace grid_performance;
    const auto knob = [&](int position, int parameter, const char* tip) {
      AddChildControl(new Knob(ControlBounds(position), PerformanceParam(parameter), parameters[parameter].label, true))->SetTooltip(tip);
    };
    knob(0, LaneAGain, "Lane A level after its four slots. Right-click to modulate it.");
    knob(1, LaneABalance, "Lane A left/right balance: -100% left, 0% centre, +100% right. Centre preserves both channels; turning attenuates the opposite channel.");
    knob(4, LaneBGain, "Lane B level after its four slots. Right-click to modulate it.");
    knob(5, LaneBBalance, "Lane B left/right balance: -100% left, 0% centre, +100% right. Centre preserves both channels; turning attenuates the opposite channel.");
    knob(2, CrossoverFrequency, "Frequency dividing the low A branch and high B branch when Frequency split is on.");
    knob(3, LaneTiming, "Positive values delay A, negative values delay B, up to 20 ms. Use it to audition capture "
                        "timing; it cannot correct every phase difference and never advances audio.");
    AddChildControl(new Toggle(IRECT(grid_layout::parameterLeft, grid_layout::descriptionTop, grid_layout::parameterLeft + 176, grid_layout::descriptionTop + 24), PerformanceParam(Crossover), "Frequency split"))
      ->SetTooltip("Send lows to lane A and highs to lane B. With A / B centred, empty lanes rebuild the original signal.");
    AddChildControl(new PerformanceChoice(IRECT(grid_layout::parameterRight - 292, grid_layout::moduleHeaderTop,
                                                grid_layout::parameterRight - 128, grid_layout::moduleHeaderBottom),
                                         PerformanceParam(CrossoverSlope), "Slope"))
      ->SetTooltip("Gentle is the released 6 dB/octave split. Steep is a 24 dB/octave Linkwitz-Riley split: the "
                   "driven lane gets far less low end, at the cost of an allpass phase shift around the split.");
    AddChildControl(new FlowMode(IRECT(grid_layout::parameterLeft + 100, grid_layout::moduleHeaderTop,
                                      grid_layout::parameterRight - 304, grid_layout::moduleHeaderBottom)));
    AddChildControl(new Button(IRECT(grid_layout::parameterRight - 80, grid_layout::moduleHeaderTop, grid_layout::parameterRight, grid_layout::moduleHeaderBottom), [](IControl* caller) {
      static_cast<PLUG_CLASS_NAME*>(caller->GetDelegate())->SetRoutingOpen(false); }, "Close", Icon::Close))
      ->SetTooltip("Back to the selected module (Escape).");
    AddChildControl(new CrossoverView(IRECT(grid_layout::visualizationLeft, grid_layout::visualizationTop, grid_layout::visualizationRight, grid_layout::visualizationBottom)), kCtrlTagRoutingView);
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_ui;
    DrawEditorBackground(g, mRECT);
    const bool serial = static_cast<PLUG_CLASS_NAME*>(GetDelegate())->GetParam(PerformanceParam(grid_performance::LaneMode))->Int() == grid_performance::Sequential;
    g.DrawText(Text(22.f, accent), "Routing", IRECT(grid_layout::parameterLeft, grid_layout::moduleHeaderTop, 110, grid_layout::moduleHeaderBottom));
    if (serial)
    {
      g.DrawText(Text(16.f, text), "Pre > A > B > Post",
                 IRECT(grid_layout::parameterLeft+12, grid_layout::knobTop+22, grid_layout::parameterRight, grid_layout::knobTop+48));
      g.DrawText(Text(13.f, muted), "One chain. Switch to Parallel for lane mixing and balance.",
                 IRECT(grid_layout::parameterLeft+12, grid_layout::knobTop+54, grid_layout::parameterRight, grid_layout::knobTop+80));
    }
  }
};

// Modulation: sources and macros beside the matrix, in one full page.
class NAMGridPerformanceControl : public IContainerBase
{
public:
  explicit NAMGridPerformanceControl(const IRECT& r) : IContainerBase(r) {}
  void OnAttached() override
  {
    using namespace grid_ui; using namespace grid_performance;
    AddChildControl(new Button(IRECT(pageRight - 80, 14, pageRight, 42), [this](IControl*) { Hide(true); }, "Close", Icon::Close))
      ->SetTooltip("Close Modulation (Escape).");
    mSources = new Tabs(IRECT(pageLeft, pageTop, 452, pageTop + 26), {"LFO 1", "LFO 2", "Env 1", "Env 2", "Seq 1", "Seq 2"},
                        [this](int) { Show(); }, {SourceColor(SourceLFO1), SourceColor(SourceLFO2), SourceColor(SourceEnv1), SourceColor(SourceEnv2),
                                               SourceColor(SourceSeq1), SourceColor(SourceSeq2)});
    Add(mSources);
    const IRECT display(pageLeft, 106, 452, 300);
    for (int instance = 0; instance < sourceInstances; ++instance)
    {
      AddSource(instance ? SourceLFO2 : SourceLFO1, new LFODisplay(display, instance));
      AddSource(instance ? SourceEnv2 : SourceEnv1, new EnvelopeDisplay(display, instance));
      AddSource(instance ? SourceSeq2 : SourceSeq1, new StepEditor(display, instance));
    }
    const auto cell = [](int column, int row) {
      const float x = pageLeft + 107.f * column, y = 310.f + grid_layout::knobRowPitch * row;
      return IRECT(x, y, x + 107.f, y + grid_layout::knobBottom - grid_layout::knobTop);
    };
    const auto tip = [](IControl* control, const char* text) {
      if (text) control->SetTooltip(text);
      return control;
    };
    for (int instance = 0; instance < sourceInstances; ++instance)
    {
      const auto dial = [&](int source, int column, int row, int parameter, const char* help = nullptr) {
        parameter = SourceParam(parameter, instance);
        source = instance ? SourceLFO2 + source - SourceLFO1 : source;
        return tip(AddSource(source, new Knob(cell(column, row), PerformanceParam(parameter), parameters[parameter].label, true)), help);
      };
      const auto menu = [&](int source, int column, int row, int parameter, const char* help) {
        parameter = SourceParam(parameter, instance);
        source = instance ? SourceLFO2 + source - SourceLFO1 : source;
        return tip(AddSource(source, new ParamMenu(cell(column, row), PerformanceParam(parameter), parameters[parameter].label)), help);
      };
      const auto toggle = [&](int source, int column, int row, int parameter, const char* help) {
        parameter = SourceParam(parameter, instance);
        source = instance ? SourceLFO2 + source - SourceLFO1 : source;
        return tip(AddSource(source, new ParamMenu(cell(column, row), PerformanceParam(parameter), parameters[parameter].label, true)), help);
      };
      menu(SourceLFO, 0, 0, LFOShape, "Random holds a new value for each cycle.");
      mRate[instance * 2] = dial(SourceLFO, 1, 0, LFORate);
      mDivision[instance * 2] = menu(SourceLFO, 1, 0, LFODivision, "Cycle length in host tempo.");
      toggle(SourceLFO, 2, 0, LFOSync, "Follow the host tempo.");
      menu(SourceLFO, 3, 0, LFORetrigger, "Restart the cycle on each played note (Audio onset, heard where the matching Env listens) "
                                          "or on each MIDI note-on sent to the plugin. Song position follows the host's playback "
                                          "position, so cycles line up with the bars on every playback; stopped, it runs free.");
      toggle(SourceLFO, 0, 1, LFOOneShot,
             "Run one cycle per retrigger, then hold its end: Saw down becomes a decay envelope for each note.");
      menu(SourceEnvelope, 0, 0, EnvInput, "Where the follower listens: the grid input, or after any slot (e.g. after the amp).");
      dial(SourceEnvelope, 1, 0, EnvSensitivity, "Gain above the threshold: raise it until normal playing nearly fills the output.");
      dial(SourceEnvelope, 2, 0, EnvThreshold, "Level the follower ignores, e.g. pick noise or quiet playing. Off at the minimum.");
      dial(SourceEnvelope, 3, 0, EnvCurve, "Bends the response: positive reacts sooner, negative only to louder playing.");
      dial(SourceEnvelope, 0, 1, EnvAttack);
      dial(SourceEnvelope, 1, 1, EnvRelease, "How fast the output falls after you stop playing: the ducking recovery time.");
      mRate[instance * 2 + 1] = dial(SourceSequencer, 0, 0, StepRate);
      mDivision[instance * 2 + 1] = menu(SourceSequencer, 0, 0, StepDivision, "Step length in host tempo.");
      toggle(SourceSequencer, 1, 0, StepSync, "Follow the host tempo.");
      menu(SourceSequencer, 2, 0, StepRetrigger, "Restart at step 1 on each played note or MIDI note-on. Song position "
                                                 "follows the host's playback position, so step 1 lands on the bars.");
    }
    for (int i = 0; i < 4; ++i)
    {
      auto* knob = new MacroKnob(IRECT(484.f + 123.f * i, 512, 607.f + 123.f * i, 590), i, true);
      Add(knob);
      Add(new MacroLabel(knob->NameBounds(), i, true));
    }
    for (int row = 0; row < rows; ++row)
    {
      const float top = RowTop(row);
      const auto rowCell = [&](float l, float r) { return IRECT(l, top, r, top + 32.f); };
      Add(new SourceCell(rowCell(504, 594), PerformanceParam(RowParam(row, RowSource)), false));
      Add(new DestinationCell(rowCell(600, 806), row));
      Add(new DepthCell(rowCell(812, 912), row));
      Add(new SourceCell(rowCell(918, 952), PerformanceParam(RowParam(row, RowVia)), true))
        ->SetTooltip("Via: another source scales this row's depth, from none (at its minimum) to all.");
      Add(new ClearCell(rowCell(956, pageRight), row));
    }
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_ui; using namespace grid_performance;
    g.FillRect(background, mRECT);
    IconLabel(g, Icon::Modulation, "Modulation", IRECT(pageLeft, 8, 300, 48), Text(23.f));
    g.DrawLine(border, pageLeft, 56, pageRight, 56);
    g.DrawText(Text(17.f, accent), "Matrix", IRECT(484, pageTop, pageRight, pageTop + 24));
    g.DrawText(Text(13.f, muted, EAlign::Far), "Rows on one knob add, within its range", IRECT(484, pageTop, pageRight, pageTop + 24));
    const auto columnLabel = [&](const char* label, float l, float r, EAlign align = EAlign::Near) {
      g.DrawText(Text(12.f, muted, align), label, IRECT(l, 104, r, 120));
    };
    columnLabel("Source", 504, 594);
    columnLabel("Destination", 600, 806);
    columnLabel("Depth", 812, 912, EAlign::Center);
    columnLabel("Via", 918, 952, EAlign::Center);
    for (int row = 0; row < rows; ++row)
    {
      WDL_String index; index.SetFormatted(8, "%d", row + 1);
      g.DrawText(Text(13.f, muted, EAlign::Center), index.Get(), IRECT(484, RowTop(row), 500, RowTop(row) + 32));
    }
    g.DrawText(Text(17.f, accent), "Macros", IRECT(484, 484, pageRight, 508));
    g.DrawText(Text(13.f, muted), "Right-click any module knob to modulate it. Alt-drag a modulated knob to change its depth.",
               IRECT(pageLeft, grid_layout::visualizationBottom - 40, pageRight, grid_layout::visualizationBottom - 16));
  }
  bool IsDirty() override
  {
    if (!IsHidden())
    {
      bool changed = false;
      for (int instance = 0; instance < grid_performance::sourceInstances; ++instance)
        for (int clock = 0; clock < 2; ++clock)
        {
          const int index = instance * 2 + clock;
          const int parameter = grid_performance::SourceParam(clock ? grid_performance::Seq1Sync : grid_performance::LFO1Sync, instance);
          const bool sync = PLUG()->GetParam(PerformanceParam(parameter))->Bool();
          changed = changed || sync != mSync[index];
          mSync[index] = sync;
        }
      if (changed) Show();
    }
    return IContainerBase::IsDirty();
  }
  void Hide(bool hide) override
  {
    IContainerBase::Hide(hide);
    if (!hide) Show();
    if (GetUI()) GetUI()->SetAllControlsDirty();
  }
private:
  static float RowTop(int row) { return 126.f + 44.f * row; }
  IControl* Add(IControl* control) { AddChildControl(control); mControls.push_back(control); return control; }
  // Source controls are shown for their source tab only.
  IControl* AddSource(int source, IControl* control)
  {
    Add(control);
    mSourceOf[control] = source;
    return control;
  }
  // Child visibility for the selected source and its sync mode.
  void Show()
  {
    const int source = mSources ? grid_performance::sourceTabs[mSources->Selected()] : grid_performance::SourceLFO;
    for (auto* c : mControls)
    {
      bool hidden = IsHidden();
      if (const auto it = mSourceOf.find(c); it != mSourceOf.end()) hidden = hidden || it->second != source;
      for (int i = 0; i < mRate.size(); ++i)
      {
        if (c == mRate[i]) hidden = hidden || mSync[i];
        if (c == mDivision[i]) hidden = hidden || !mSync[i];
      }
      c->Hide(hidden);
    }
    if (GetUI()) GetUI()->SetAllControlsDirty();
  }
  grid_ui::Tabs* mSources = nullptr;
  std::vector<IControl*> mControls;
  std::map<IControl*, int> mSourceOf;
  std::array<IControl*, grid_performance::sourceInstances * 2> mRate{}, mDivision{};
  std::array<bool, grid_performance::sourceInstances * 2> mSync{};
};
