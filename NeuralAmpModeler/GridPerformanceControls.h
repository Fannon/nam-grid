#pragma once

// Grid / Control: routing, the modulation matrix with its sources, and scenes.
// The header matches the main editor's; every page uses its 24 px side margins.
namespace grid_ui
{
inline constexpr float pageLeft = 24.f, pageRight = 976.f, pageTop = 72.f;

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
// like a tall Toggle: label on top, value box below, so it lines up with knobs.
class ParamMenu : public IControl
{
public:
  ParamMenu(const IRECT& r, int parameter, const char* label) : IControl(r, parameter), mLabel(label) {}
  void Draw(IGraphics& g) override
  {
    g.DrawText(Text(15.f, muted, EAlign::Center), mLabel.c_str(), mRECT.GetFromTop(22));
    const auto box = Box();
    WDL_String value; GetParam()->GetDisplay(value);
    g.FillRoundRect(surface, box, 3.f);
    g.DrawRoundRect(mMouseIsOver ? muted : border, box, 3.f);
    FittedText(g, Text(13.f, text, EAlign::Center), value.Get(), box.GetHPadded(-14.f));
    g.DrawLine(muted, box.R - 14, box.MH() - 2, box.R - 10, box.MH() + 2);
    g.DrawLine(muted, box.R - 10, box.MH() + 2, box.R - 6, box.MH() - 2);
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    if (!mod.L) return;
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
  IRECT Box() const { return IRECT(mRECT.L + 8.f, mRECT.T + 40.f, mRECT.R - 8.f, mRECT.T + 74.f); }
  std::string mLabel;
  IPopupMenu mMenu;
};

// Matrix row cells ------------------------------------------------------------

inline const char* ShortSource(int source)
{
  static constexpr const char* names[]{"—", "LFO", "Env", "Seq", "M1", "M2", "M3", "M4"};
  return names[std::clamp(source, 0, grid_performance::SourceCount - 1)];
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
    FittedText(g, Text(13.f, text), grid_performance::sourceNames[source], mRECT.GetReducedFromLeft(20.f).GetReducedFromRight(4.f));
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    if (!mod.L) return;
    mMenu.Clear();
    for (int source = 0; source < grid_performance::SourceCount; ++source)
      mMenu.AddItem(new IPopupMenu::Item(grid_performance::sourceNames[source],
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
                                       flag(uEQMode), flag(uVibeMode));
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
      if (RowActive(v, mRow) && (source == SourceLFO || source == SourceEnvelope || source == SourceSequencer))
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
  LFODisplay(const IRECT& r) : LiveSourceControl(r) { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override
  {
    using namespace grid_performance;
    PanelBackground(g, mRECT);
    const auto v = PLUG()->PerformanceValues();
    const int shape = static_cast<int>(v[LFOShape]);
    WDL_String caption;
    if (v[LFOSync] >= .5) caption.SetFormatted(64, "%s  ·  %s", lfoShapeNames[shape], grid_tempo::names[static_cast<int>(v[LFODivision])]);
    else caption.SetFormatted(64, "%s  ·  %.2f Hz", lfoShapeNames[shape], v[LFORate]);
    g.DrawText(Text(14.f, SourceColor(SourceLFO)), caption.Get(), IRECT(mRECT.L + 14, mRECT.T + 8, mRECT.R - 14, mRECT.T + 28));
    g.DrawText(Text(12.f, muted, EAlign::Far), "one cycle · bipolar", IRECT(mRECT.L + 14, mRECT.T + 8, mRECT.R - 14, mRECT.T + 28));
    const auto plot = IRECT(mRECT.L + 14, mRECT.T + 38, mRECT.R - 14, mRECT.B - 14);
    g.DrawLine(border, plot.L, plot.MH(), plot.R, plot.MH());
    const auto y = [&](double value) { return plot.MH() - static_cast<float>(value) * plot.H() * .45f; };
    // Random has no fixed curve: a held step per cycle, sketched with a fixed pattern.
    static constexpr double random[]{.55, -.35, .85, -.8, .15, -.5, .7, -.1};
    const auto wave = [&](double phase) { return shape == 5 ? random[std::min(7, static_cast<int>(phase * 8.))] : LFOWave(shape, phase); };
    const auto color = SourceColor(SourceLFO);
    float px = plot.L, py = y(wave(0.));
    for (int i = 1; i <= 240; ++i)
    {
      const double phase = std::min(i / 240., .99999);
      const float x = plot.L + plot.W() * i / 240.f, ny = y(wave(phase));
      g.DrawLine(color, px, py, x, ny, nullptr, 2.f);
      px = x; py = ny;
    }
    const auto monitor = PLUG()->ModulationMonitor();
    if (monitor.running && shape != 5)
    {
      const float x = plot.L + plot.W() * monitor.phase;
      g.DrawLine(color.WithOpacity(.35f), x, plot.T, x, plot.B);
      g.FillCircle(text, x, y(monitor.lfo), 4.f);
    }
    else if (monitor.running)
      g.FillCircle(text, plot.R - 6.f, y(monitor.lfo), 4.f);
  }
};
class EnvelopeDisplay : public LiveSourceControl
{
public:
  EnvelopeDisplay(const IRECT& r) : LiveSourceControl(r) { mIgnoreMouse = true; }
  bool IsDirty() override
  {
    if (!IsHidden())
    {
      const auto monitor = PLUG()->ModulationMonitor();
      mHistory[mWrite] = monitor.running ? monitor.envelope : 0.f;
      mWrite = (mWrite + 1) % mHistory.size();
    }
    return LiveSourceControl::IsDirty();
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_performance;
    PanelBackground(g, mRECT);
    const auto v = PLUG()->PerformanceValues();
    WDL_String caption;
    caption.SetFormatted(96, "Input envelope  ·  %.0f dB sensitivity  ·  %.1f / %.0f ms", v[EnvSensitivity], v[EnvAttack], v[EnvRelease]);
    const auto color = SourceColor(SourceEnvelope);
    g.DrawText(Text(14.f, color), caption.Get(), IRECT(mRECT.L + 14, mRECT.T + 8, mRECT.R - 14, mRECT.T + 28));
    const auto plot = IRECT(mRECT.L + 14, mRECT.T + 38, mRECT.R - 14, mRECT.B - 14);
    g.DrawLine(border, plot.L, plot.B, plot.R, plot.B);
    const size_t count = mHistory.size();
    for (size_t i = 0; i < count; ++i)
    {
      const float value = mHistory[(mWrite + i) % count];
      const float x = plot.L + plot.W() * i / count, w = plot.W() / count + .5f;
      g.FillRect(color.WithOpacity(.55f), IRECT(x, plot.B - plot.H() * value, x + w, plot.B));
    }
    if (!PLUG()->ModulationMonitor().running)
      g.DrawText(Text(13.f, muted, EAlign::Center), "Shown while a row uses a running source", plot);
  }

private:
  std::array<float, 180> mHistory{};
  size_t mWrite = 0;
};
// Eight step values as bars around the centre (50% = no change). Drag across to draw.
class StepEditor : public IControl
{
public:
  StepEditor(const IRECT& r)
  : IControl(r, {PerformanceParam(grid_performance::Step1), PerformanceParam(grid_performance::Step2),
                 PerformanceParam(grid_performance::Step3), PerformanceParam(grid_performance::Step4),
                 PerformanceParam(grid_performance::Step5), PerformanceParam(grid_performance::Step6),
                 PerformanceParam(grid_performance::Step7), PerformanceParam(grid_performance::Step8)})
  {
    SetTooltip("Draw the eight steps: above the centre line adds, below subtracts. Each step holds for one sequencer step.");
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_performance;
    PanelBackground(g, mRECT);
    const auto v = PLUG()->PerformanceValues();
    WDL_String caption;
    if (v[StepSync] >= .5) caption.SetFormatted(64, "Eight steps  ·  %s each", grid_tempo::names[static_cast<int>(v[StepDivision])]);
    else caption.SetFormatted(64, "Eight steps  ·  %.2f Hz", v[StepRate]);
    const auto color = SourceColor(SourceSequencer);
    g.DrawText(Text(14.f, color), caption.Get(), IRECT(mRECT.L + 14, mRECT.T + 8, mRECT.R - 14, mRECT.T + 28));
    const auto plot = Plot();
    g.DrawLine(border, plot.L, plot.MH(), plot.R, plot.MH());
    const auto monitor = PLUG()->ModulationMonitor();
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
    if (!IsHidden() && PLUG()->ModulationMonitor().running) SetDirty(false);
    return IControl::IsDirty();
  }

private:
  IRECT Plot() const { return IRECT(mRECT.L + 14, mRECT.T + 38, mRECT.R - 14, mRECT.B - 14); }
  void Set(float x, float y)
  {
    const auto plot = Plot();
    const int step = std::clamp(static_cast<int>((x - plot.L) / plot.W() * 8.f), 0, 7);
    SetValue(std::clamp((plot.B - y) / plot.H(), 0.f, 1.f), step);
    SetDirty(true, step);
  }
};
// Lows feed lane A and highs lane B: the complementary first-order split.
class CrossoverView : public IControl
{
public:
  CrossoverView(const IRECT& r)
  : IControl(r, {PerformanceParam(grid_performance::Crossover), PerformanceParam(grid_performance::CrossoverFrequency)})
  { mIgnoreMouse = true; }
  void Draw(IGraphics& g) override
  {
    using namespace grid_performance;
    PanelBackground(g, mRECT);
    const bool on = PLUG()->GetParam(PerformanceParam(Crossover))->Bool();
    const double frequency = PLUG()->GetParam(PerformanceParam(CrossoverFrequency))->Value();
    const auto plot = IRECT(mRECT.L + 14, mRECT.T + 34, mRECT.R - 14, mRECT.B - 26);
    g.DrawText(Text(14.f, on ? text : muted), on ? "Split: lows to A, highs to B" : "Split off: both lanes get the full signal",
               IRECT(mRECT.L + 14, mRECT.T + 8, mRECT.R - 14, mRECT.T + 28));
    const auto x = [&](double hz) { return plot.L + plot.W() * static_cast<float>(std::log(hz / 20.) / std::log(1000.)); };
    const auto y = [&](double gain) { return plot.B - plot.H() * static_cast<float>(gain); };
    for (double hz : {100., 1000., 10000.})
      g.DrawLine(border.WithOpacity(.5f), x(hz), plot.T, x(hz), plot.B);
    for (int lane = 0; lane < 2; ++lane)
    {
      const auto color = (lane ? cabColor : ampColor).WithOpacity(on ? 1.f : .35f);
      float px = plot.L, py = 0.f;
      for (int i = 0; i <= 160; ++i)
      {
        const double hz = 20. * std::pow(1000., i / 160.), ratio = hz / frequency;
        const double low = 1. / std::sqrt(1. + ratio * ratio), high = ratio / std::sqrt(1. + ratio * ratio);
        const double gain = on ? (lane ? high : low) : 1.;
        const float nx = x(hz), ny = y(gain * .9);
        if (i) g.DrawLine(color, px, py, nx, ny, nullptr, 2.f);
        px = nx; py = ny;
      }
    }
    g.DrawLine(muted.WithOpacity(on ? .9f : .4f), x(frequency), plot.T, x(frequency), plot.B);
    g.DrawText(Text(12.f, ampColor), "A: lows", IRECT(plot.L, plot.B + 6, plot.L + 80, plot.B + 22));
    g.DrawText(Text(12.f, cabColor, EAlign::Far), "B: highs", IRECT(plot.R - 80, plot.B + 6, plot.R, plot.B + 22));
    for (const auto& [hz, label] : {std::pair{100., "100"}, std::pair{1000., "1k"}, std::pair{10000., "10k"}})
      g.DrawText(Text(11.f, muted, EAlign::Center), label, IRECT(x(hz) - 20, plot.B + 6, x(hz) + 20, plot.B + 22));
  }
};
} // namespace grid_ui

class NAMGridPerformanceControl : public IContainerBase
{
public:
  explicit NAMGridPerformanceControl(const IRECT& r) : IContainerBase(r) {}
  void OnAttached() override
  {
    using namespace grid_ui; using namespace grid_performance;
    mTabs = new Tabs(IRECT(232, 14, 592, 42), {"Routing", "Modulation", "Scenes"}, [this](int page) { SetPage(page); });
    AddChildControl(mTabs);
    AddChildControl(new Button(IRECT(896, 14, pageRight, 42), [this](IControl*) { Hide(true); }, "Close"))
      ->SetTooltip("Close Grid / Control (Escape).");

    // Routing: one panel per lane, then timing and the frequency split.
    for (int lane = 0; lane < 2; ++lane)
    {
      const auto panel = LanePanel(lane);
      KnobAt(kRouting, panel.L + 16, panel.T + 64, lane ? LaneBGain : LaneAGain, 110.f)
        ->SetTooltip("Lane level after its four slots. Right-click to modulate it.");
      Add(kRouting, new Toggle(IRECT(panel.L + 150, panel.T + 64, panel.L + 260, panel.T + 164), PerformanceParam(lane ? LaneBMute : LaneAMute), "Mute"))
        ->SetTooltip("Silence this lane. Mute wins over solo.");
      Add(kRouting, new Toggle(IRECT(panel.L + 274, panel.T + 64, panel.L + 384, panel.T + 164), PerformanceParam(lane ? LaneBSolo : LaneASolo), "Solo"))
        ->SetTooltip("Hear only this lane, keeping its blend level. Soloing both lanes passes both.");
    }
    KnobAt(kRouting, 40, 352, LaneTiming, 110.f)
      ->SetTooltip("Positive values delay A, negative values delay B, up to 20 ms. Use it to audition capture timing; "
                   "it cannot correct every phase difference and never advances audio.");
    Add(kRouting, new Toggle(IRECT(176, 352, 296, 452), PerformanceParam(Crossover), "Frequency split"))
      ->SetTooltip("Send lows to lane A and highs to lane B. With the main A / B blend centred, empty lanes rebuild the original signal.");
    KnobAt(kRouting, 312, 352, CrossoverFrequency, 110.f)->SetTooltip("Split frequency of the first-order crossover.");
    Add(kRouting, new CrossoverView(IRECT(452, 344, 960, 512)));

    // Modulation: sources on the left, the matrix on the right.
    mSources = new Tabs(IRECT(pageLeft, pageTop, 452, pageTop + 26), {"LFO", "Envelope", "Sequencer"},
                        [this](int) { SetPage(mPage); }, {SourceColor(SourceLFO), SourceColor(SourceEnvelope), SourceColor(SourceSequencer)});
    Add(kModulation, mSources);
    const IRECT display(pageLeft, 106, 452, 316);
    AddSource(SourceLFO, new LFODisplay(display));
    AddSource(SourceEnvelope, new EnvelopeDisplay(display));
    AddSource(SourceSequencer, new StepEditor(display));
    const auto column = [](int i) { return pageLeft + 107.f * i; };
    AddSource(SourceLFO, new ParamMenu(IRECT(column(0), 328, column(1), 428), PerformanceParam(LFOShape), "Shape"));
    mRate[0] = AddSource(SourceLFO, Dial(column(1), 328, LFORate));
    mDivision[0] = AddSource(SourceLFO, new ParamMenu(IRECT(column(1), 328, column(2), 428), PerformanceParam(LFODivision), "Division"));
    AddSource(SourceLFO, new Toggle(IRECT(column(2) + 4, 328, column(3) - 4, 428), PerformanceParam(LFOSync), "Tempo sync"));
    AddSource(SourceEnvelope, Dial(column(0), 328, EnvSensitivity))
      ->SetTooltip("Gain before the follower: raise it until normal playing nearly fills the display.");
    AddSource(SourceEnvelope, Dial(column(1), 328, EnvAttack));
    AddSource(SourceEnvelope, Dial(column(2), 328, EnvRelease));
    mRate[1] = AddSource(SourceSequencer, Dial(column(0), 328, StepRate));
    mDivision[1] = AddSource(SourceSequencer, new ParamMenu(IRECT(column(0), 328, column(1), 428), PerformanceParam(StepDivision), "Division"));
    AddSource(SourceSequencer, new Toggle(IRECT(column(1) + 4, 328, column(2) - 4, 428), PerformanceParam(StepSync), "Tempo sync"));
    for (int i = 0; i < 4; ++i)
      KnobAt(kModulation, column(i), 484, macros[i], 107.f)
        ->SetTooltip("Macro: a manual or host-automated source. Use it in the matrix, or right-click any module knob.");
    for (int row = 0; row < rows; ++row)
    {
      const float top = RowTop(row);
      const auto cell = [&](float l, float r) { return IRECT(l, top, r, top + 32.f); };
      Add(kModulation, new SourceCell(cell(504, 594), PerformanceParam(RowParam(row, RowSource)), false));
      Add(kModulation, new DestinationCell(cell(600, 806), row));
      Add(kModulation, new DepthCell(cell(812, 912), row));
      Add(kModulation, new SourceCell(cell(918, 952), PerformanceParam(RowParam(row, RowVia)), true))
        ->SetTooltip("Via: another source scales this row's depth, from none (at its minimum) to all.");
      Add(kModulation, new ClearCell(cell(956, pageRight), row));
    }
    mConvert = new Button(IRECT(484, 514, 664, 542), [this](IControl*) { PLUG()->ConvertLegacyModulation(); }, "Convert to rows");
    Add(kModulation, mConvert);

    // Scenes: three cards.
    for (int scene = 0; scene < 3; ++scene)
    {
      const auto card = SceneCard(scene);
      mSceneLabels[scene] = new ITextControl(IRECT(card.L + 20, card.T + 48, card.R - 20, card.T + 70), "Empty", Text(14.f, muted, EAlign::Near));
      Add(kScenes, mSceneLabels[scene]);
      Add(kScenes, new Button(IRECT(card.L + 20, card.B - 52, card.L + 150, card.B - 20), [this, scene](IControl*) {
        PLUG()->RecallPerformanceScene(scene); SetDirty(false); }, "Recall"));
      Add(kScenes, new Button(IRECT(card.L + 164, card.B - 52, card.R - 20, card.B - 20), [this, scene](IControl*) {
        PLUG()->StorePerformanceScene(scene); SetDirty(false); }, "Store"))
        ->SetTooltip("Overwrite this scene with the current controls.");
    }
    mTabs->Select(kRouting);
  }
  void Draw(IGraphics& g) override
  {
    using namespace grid_ui; using namespace grid_performance;
    g.FillRect(background, mRECT);
    g.DrawText(Text(23.f), "Grid / Control", IRECT(pageLeft, 8, 220, 48));
    g.DrawLine(border, pageLeft, 56, pageRight, 56);
    const auto heading = [&](const char* label, float x, float y, float r = pageRight) {
      g.DrawText(Text(17.f, accent), label, IRECT(x, y, r, y + 24));
    };
    if (mPage == kRouting)
    {
      heading("Lanes", pageLeft, pageTop);
      for (int lane = 0; lane < 2; ++lane)
      {
        const auto panel = LanePanel(lane);
        PanelBackground(g, panel);
        g.DrawText(Text(17.f), lane ? "Lane B" : "Lane A", IRECT(panel.L + 16, panel.T + 10, panel.R - 16, panel.T + 32));
        std::string chain;
        for (int slot = 4 + lane * 4; slot < 8 + lane * 4; ++slot)
          if (PLUG()->IsSlotLoaded(slot) || PLUG()->GetSlotEffect(slot) != SlotEffect::NAM)
            chain += (chain.empty() ? "" : "  ›  ") + SlotID(slot) + " " + SlotModuleName(*PLUG(), slot);
        FittedText(g, Text(13.f, muted), chain.empty() ? "Empty: passes the Pre row's output" : chain,
                   IRECT(panel.L + 16, panel.T + 34, panel.R - 16, panel.T + 54));
      }
      heading("Timing and split", pageLeft, 296);
      PanelBackground(g, IRECT(pageLeft, 328, pageRight, 528));
      g.DrawText(Text(13.f, muted), "Blend A / B stays on the main page, right of the grid.", IRECT(pageLeft, 540, pageRight, 560));
    }
    else if (mPage == kModulation)
    {
      heading("Macros", pageLeft, 452, 452);
      heading("Matrix", 484, pageTop);
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
      if (mLegacy)
        g.DrawText(Text(13.f, muted), "Host automation turned on an older fixed assignment. Convert it to keep editing it here.",
                   IRECT(484, 484, pageRight, 508));
      g.DrawText(Text(13.f, muted), "Right-click any module knob to modulate it. Alt-drag a modulated knob to change its depth.",
                 IRECT(pageLeft, 676, pageRight, 700));
    }
    else
    {
      g.DrawText(Text(14.f, muted), "Store up to three performance snapshots, then recall them here or automate the same parameters in your DAW.",
                 IRECT(pageLeft, pageTop, pageRight, pageTop + 24));
      for (int scene = 0; scene < 3; ++scene)
      {
        const auto card = SceneCard(scene);
        PanelBackground(g, card);
        WDL_String title; title.SetFormatted(16, "Scene %d", scene + 1);
        g.DrawText(Text(19.f, PLUG()->HasPerformanceScene(scene) ? accent : text), title.Get(),
                   IRECT(card.L + 20, card.T + 16, card.R - 20, card.T + 44));
      }
      g.DrawText(Text(13.f, muted), "Scenes recall controls, bypass and modulation rows. Models, module types, quality and captured audio stay in place.",
                 IRECT(pageLeft, 288, pageRight, 312));
    }
  }
  bool IsDirty() override
  {
    if (!IsHidden())
    {
      for (int scene = 0; scene < 3; ++scene)
      {
        const char* label = PLUG()->HasPerformanceScene(scene) ? "Stored" : "Empty";
        if (mSceneLabels[scene] && std::strcmp(mSceneLabels[scene]->GetStr(), label) != 0)
        {
          mSceneLabels[scene]->SetStr(label);
          SetDirty(false); // The card title changes colour too.
        }
      }
      const bool legacy = PLUG()->HasLegacyModulation();
      const bool lfoSync = PLUG()->GetParam(PerformanceParam(grid_performance::LFOSync))->Bool();
      const bool stepSync = PLUG()->GetParam(PerformanceParam(grid_performance::StepSync))->Bool();
      if (legacy != mLegacy || lfoSync != mSync[0] || stepSync != mSync[1])
      {
        mLegacy = legacy; mSync = {lfoSync, stepSync};
        SetPage(mPage);
      }
    }
    return IContainerBase::IsDirty();
  }
  void Hide(bool hide) override
  {
    IContainerBase::Hide(hide);
    if (!hide) SetPage(mPage);
    if (GetUI()) GetUI()->SetAllControlsDirty();
  }
private:
  enum Page { kRouting = 0, kModulation, kScenes, kPages };
  static IRECT LanePanel(int lane) { return lane ? IRECT(512, 104, grid_ui::pageRight, 276) : IRECT(grid_ui::pageLeft, 104, 488, 276); }
  static IRECT SceneCard(int scene)
  {
    const float left = grid_ui::pageLeft + scene * 324.f;
    return IRECT(left, 108, left + 304.f, 268);
  }
  static float RowTop(int row) { return 126.f + 44.f * row; }
  IControl* Add(int page, IControl* control) { AddChildControl(control); mPages[page].push_back(control); return control; }
  // Source controls also belong to the Modulation page, shown for their source tab only.
  IControl* AddSource(int source, IControl* control)
  {
    Add(kModulation, control);
    mSourceOf[control] = source;
    return control;
  }
  IControl* Dial(float x, float y, int parameter)
  {
    return new grid_ui::Knob(IRECT(x, y, x + 107.f, y + 100.f), PerformanceParam(parameter), grid_performance::parameters[parameter].label);
  }
  IControl* KnobAt(int page, float x, float y, int parameter, float width)
  {
    return Add(page, new grid_ui::Knob(IRECT(x, y, x + width, y + 100.f), PerformanceParam(parameter), grid_performance::parameters[parameter].label));
  }
  void SetPage(int page)
  {
    mPage = page;
    const int source = mSources ? mSources->Selected() + grid_performance::SourceLFO : grid_performance::SourceLFO;
    for (int p = 0; p < kPages; ++p)
      for (auto* c : mPages[p])
      {
        bool hidden = IsHidden() || p != page;
        if (const auto it = mSourceOf.find(c); it != mSourceOf.end()) hidden = hidden || it->second != source;
        for (int i = 0; i < 2; ++i)
        {
          if (c == mRate[i]) hidden = hidden || mSync[i];
          if (c == mDivision[i]) hidden = hidden || !mSync[i];
        }
        if (c == mConvert) hidden = hidden || !mLegacy;
        c->Hide(hidden);
      }
    if (GetUI()) GetUI()->SetAllControlsDirty();
  }
  grid_ui::Tabs* mTabs = nullptr;
  grid_ui::Tabs* mSources = nullptr;
  std::array<std::vector<IControl*>, kPages> mPages;
  std::map<IControl*, int> mSourceOf;
  std::array<IControl*, 2> mRate{}, mDivision{};
  std::array<bool, 2> mSync{};
  IControl* mConvert = nullptr;
  std::array<ITextControl*, 3> mSceneLabels{};
  bool mLegacy = false;
  int mPage = kRouting;
};
