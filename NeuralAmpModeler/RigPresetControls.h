#pragma once

class NAMRigPresetControl : public grid_ui::Button
{
public:
  explicit NAMRigPresetControl(const IRECT& r)
  : grid_ui::Button(r, [](IControl*) {}, "Untitled")
  { SetTooltip("Browse rig presets. Right-click for quick load / save actions."); }
  void Draw(IGraphics& g) override
  {
    DrawBox(g, mRECT.GetHPadded(-36.f)); // Centred, clear of the browse icon.
    grid_ui::DrawIcon(g, grid_ui::Icon::Browse, mRECT.R - 19, mRECT.MH(), grid_ui::muted);
  }
  void OnMouseDown(float, float, const IMouseMod& mod) override
  {
    auto* plug = static_cast<NeuralAmpModeler*>(GetDelegate());
    if (!mod.R)
    {
      plug->OpenLibraryBrowser(nam_library::BrowserContext::Rig, nam_library::FileKind::RigPreset);
      return;
    }
    mFiles = plug->GetRigPresetFiles();
    mMenu.Clear();
    mMenu.AddItem(new IPopupMenu::Item("Load preset file...", 0, 1));
    mMenu.AddItem(new IPopupMenu::Item("Save preset as...", 0, 2));
    mMenu.AddSeparator();
    if (mFiles.empty())
      mMenu.AddItem("Load or save a preset to browse its folder", -1, IPopupMenu::Item::kDisabled);
    for (size_t i = 0; i < mFiles.size(); ++i)
    {
      const auto name = nam_library::PathText(mFiles[i].stem());
      mMenu.AddItem(new IPopupMenu::Item(name.c_str(), name == plug->GetRigPresetName() ? IPopupMenu::Item::kChecked : 0,
                                       static_cast<int>(100 + i)));
    }
    GetUI()->CreatePopupMenu(*this, mMenu, mRECT);
  }
  void OnPopupMenuSelection(IPopupMenu* menu, int) override
  {
    const auto* item = menu ? menu->GetChosenItem() : nullptr;
    if (!item) return;
    auto* plug = static_cast<NeuralAmpModeler*>(GetDelegate());
    const int tag = item->GetTag();
    if (tag == 1 || tag == 2) plug->PromptRigPreset(tag == 2);
    else if (tag >= 100 && tag - 100 < mFiles.size())
    {
      const auto error = plug->LoadRigPreset(mFiles[tag - 100]);
      if (!error.empty()) GetUI()->ShowMessageBox(error.c_str(), "Rig preset", EMsgBoxType::kMB_OK);
    }
  }
private:
  IPopupMenu mMenu;
  std::vector<std::filesystem::path> mFiles;
};

// Click is Save as, the safe default. Shift- or Alt-click saves over the loaded
// preset file directly; right-click offers both. Without a file, Save falls back to Save as.
class NAMRigSaveControl : public grid_ui::Button
{
public:
  explicit NAMRigSaveControl(const IRECT& r)
  : grid_ui::Button(r, [this](IControl*) {
      auto* plug = static_cast<NeuralAmpModeler*>(GetDelegate());
      if (mOverwrite) plug->QuickSaveRigPreset();
      else plug->PromptRigPreset(true);
    }, "Save", grid_ui::Icon::Save, true)
  {}
  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (!mod.R)
    {
      mOverwrite = mod.S || mod.A;
      grid_ui::Button::OnMouseDown(x, y, mod);
      return;
    }
    const auto file = static_cast<NeuralAmpModeler*>(GetDelegate())->GetRigPresetFile();
    const auto save = file.empty() ? std::string("Save") : "Save over " + nam_library::PathText(file.filename());
    mMenu.Clear();
    mMenu.AddItem(new IPopupMenu::Item(save.c_str(), file.empty() ? IPopupMenu::Item::kDisabled : 0, 1));
    mMenu.AddItem(new IPopupMenu::Item("Save as...", 0, 2));
    GetUI()->CreatePopupMenu(*this, mMenu, mRECT);
  }
  void OnPopupMenuSelection(IPopupMenu* menu, int) override
  {
    const auto* item = menu ? menu->GetChosenItem() : nullptr;
    if (!item) return;
    auto* plug = static_cast<NeuralAmpModeler*>(GetDelegate());
    if (item->GetTag() == 1) plug->QuickSaveRigPreset();
    else if (item->GetTag() == 2) plug->PromptRigPreset(true);
  }
private:
  IPopupMenu mMenu;
  bool mOverwrite = false;
};
