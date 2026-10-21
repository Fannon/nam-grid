#pragma once

class NAMRigPresetControl : public grid_ui::Button
{
public:
  explicit NAMRigPresetControl(const IRECT& r)
  : grid_ui::Button(r, [](IControl*) {}, "Untitled") {}
  void Draw(IGraphics& g) override
  {
    grid_ui::Button::Draw(g);
    g.DrawLine(grid_ui::muted, mRECT.R - 19, mRECT.MH() - 2, mRECT.R - 15, mRECT.MH() + 2);
    g.DrawLine(grid_ui::muted, mRECT.R - 15, mRECT.MH() + 2, mRECT.R - 11, mRECT.MH() - 2);
  }
  void OnMouseDown(float, float, const IMouseMod&) override
  {
    auto* plug = static_cast<NeuralAmpModeler*>(GetDelegate());
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
