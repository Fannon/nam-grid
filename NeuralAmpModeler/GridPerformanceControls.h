#pragma once

class NAMGridPerformanceControl : public IContainerBase
{
public:
  explicit NAMGridPerformanceControl(const IRECT& r) : IContainerBase(r) {}
  void OnAttached() override
  {
    using namespace grid_ui; using namespace grid_performance;
    AddChildControl(new Button(IRECT(862, 28, 968, 60), [this](IControl*) { Hide(true); }, "Close"));
    for (int page = 0; page < 3; ++page)
      AddChildControl(new Button(IRECT(32.f + page * 180, 88, 200.f + page * 180, 120),
                                [this, page](IControl*) { SetPage(page); },
                                page == 0 ? "Routing" : page == 1 ? "Modulation" : "Scenes"));
    KnobAt(0, 40, 190, LaneAGain); ToggleAt(0, 170, 205, LaneAMute); ToggleAt(0, 170, 253, LaneASolo);
    KnobAt(0, 355, 190, LaneBGain); ToggleAt(0, 485, 205, LaneBMute); ToggleAt(0, 485, 253, LaneBSolo);
    KnobAt(0, 730, 190, LaneTiming)->SetTooltip("Positive delays lane A; negative delays lane B. Intentional timing, up to 20 ms. No audio is advanced.");
    ToggleAt(0, 40, 365, Crossover);
    KnobAt(0, 245, 335, CrossoverFrequency);
    Add(0, new Knob(IRECT(425, 335, 540, 440), kParallelBlend, "Blend A / B"));
    KnobAt(0, 620, 335, Macro1); KnobAt(0, 780, 335, Macro2);
    Add(0, new ITextControl(IRECT(40, 485, 955, 540),
      "Frequency split sends lows to A and highs to B. Center Blend reconstructs unity with empty lanes.", Text(14.f, muted)));
    Add(0, new ITextControl(IRECT(40, 545, 955, 600),
      "Solo preserves the lane's blend level. Fine timing helps audition captures; it cannot correct every phase difference.", Text(14.f, muted)));
    Add(0, new ITextControl(IRECT(40, 615, 955, 660),
      "Macros are assigned on the Modulation page and can also be automated by the host.", Text(14.f, muted)));

    const auto pair = [&](float x, float y, int first, int second) { KnobAt(1, x, y, first); KnobAt(1, x + 135, y, second); };
    for (int column = 0; column < 3; ++column)
      Add(1, new ITextControl(IRECT(40.f + column * 325, 143, 315.f + column * 325, 174),
        column == 0 ? "LFO" : column == 1 ? "Input envelope" : "Eight-step sequencer", Text(17.f, accent)));
    pair(40, 170, LFOSlot, LFOTarget); pair(40, 270, LFORate, LFODepth);
    ToggleAt(1, 40, 390, LFOSync); KnobAt(1, 175, 370, LFODivision);
    pair(365, 170, EnvSlot, EnvTarget); pair(365, 270, EnvSensitivity, EnvDepth);
    pair(365, 370, EnvAttack, EnvRelease);
    pair(690, 170, StepSlot, StepTarget); pair(690, 270, StepRate, StepDepth);
    ToggleAt(1, 690, 390, StepSync); KnobAt(1, 825, 370, StepDivision);
    Add(1, new ITextControl(IRECT(40, 480, 955, 500), "Macro assignments: target slot / parameter / depth", Text(14.f, muted)));
    KnobAt(1, 40, 500, Macro1Slot); KnobAt(1, 175, 500, Macro1Target); KnobAt(1, 310, 500, Macro1Depth);
    KnobAt(1, 555, 500, Macro2Slot); KnobAt(1, 690, 500, Macro2Target); KnobAt(1, 825, 500, Macro2Depth);
    for (int i = 0; i < 8; ++i) KnobAt(1, 40.f + 117 * i, 610, Step1 + i, 95.f);

    Add(2, new ITextControl(IRECT(40, 150, 955, 210),
      "Store three performance settings, then recall them here or automate the same parameters in your DAW.", Text(14.f, muted)));
    Add(2, new ITextControl(IRECT(40, 210, 955, 270),
      "Scenes recall controls and bypass. Models, effect types, quality settings and captured audio stay in place.", Text(14.f, muted)));
    for (int scene = 0; scene < 3; ++scene)
    {
      const float x = 55.f + scene * 315;
      mSceneLabels[scene] = new ITextControl(IRECT(x, 325, x + 265, 365), "Empty scene", Text(19.f, accent));
      Add(2, mSceneLabels[scene]);
      Add(2, new Button(IRECT(x, 400, x + 115, 438), [this, scene](IControl*) { PLUG()->StorePerformanceScene(scene); SetDirty(false); }, "Store"));
      Add(2, new Button(IRECT(x + 130, 400, x + 255, 438), [this, scene](IControl*) { PLUG()->RecallPerformanceScene(scene); SetDirty(false); }, "Recall"));
    }
    SetPage(0);
  }
  void Draw(IGraphics& g) override
  {
    g.FillRect(grid_ui::background, mRECT);
    g.DrawText(grid_ui::Text(23.f), "Grid / Control", IRECT(32, 24, 650, 64));
  }
  bool IsDirty() override
  {
    if (!IsHidden()) for (int scene = 0; scene < 3; ++scene)
    {
      const std::string label = "Scene " + std::to_string(scene + 1) + (PLUG()->HasPerformanceScene(scene) ? " / stored" : " / empty");
      if (mSceneLabels[scene]) mSceneLabels[scene]->SetStr(label.c_str());
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
  void Add(int page, IControl* control) { AddChildControl(control); mPages[page].push_back(control); }
  IControl* KnobAt(int page, float x, float y, int parameter, float width = 115.f)
  {
    const auto& p = grid_performance::parameters[parameter];
    auto* control = new grid_ui::Knob(IRECT(x, y, x + width, y + 100), PerformanceParam(parameter), p.label);
    Add(page, control); return control;
  }
  void ToggleAt(int page, float x, float y, int parameter)
  { Add(page, new grid_ui::Toggle(IRECT(x, y, x + 150, y + 32), PerformanceParam(parameter), grid_performance::parameters[parameter].label)); }
  void SetPage(int page)
  {
    mPage = page;
    for (int p = 0; p < 3; ++p) for (auto* c : mPages[p]) c->Hide(p != page);
    if (GetUI()) GetUI()->SetAllControlsDirty();
  }
  std::array<std::vector<IControl*>, 3> mPages;
  std::array<ITextControl*, 3> mSceneLabels{};
  int mPage = 0;
};
