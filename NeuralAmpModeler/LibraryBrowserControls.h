#pragma once
#include "LibraryBrowser.h"
#include "ITextEntryControl.h"

class NAMBrowserTab : public grid_ui::Button
{
public:
  using grid_ui::Button::Button;
  void SetActive(bool active) { if (mActive != active) { mActive = active; SetDirty(false); } }
  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(mActive ? grid_ui::accent.WithOpacity(.15f) : grid_ui::surface, mRECT, 3.f);
    g.DrawRoundRect(mActive ? grid_ui::accent : grid_ui::border, mRECT, 3.f);
    g.DrawText(grid_ui::Text(14.f, mActive ? grid_ui::accent : grid_ui::text, EAlign::Center), GetLabelStr(), mRECT);
  }
private:
  bool mActive = false;
};

// Use iPlug's existing editor for Unicode entry, selection, clipboard and undo.
// Read its public text view while editing so filtering does not wait for Enter.
class NAMBrowserSearchControl : public IEditableTextControl
{
public:
  NAMBrowserSearchControl(const IRECT& r, std::function<void(const std::string&)> changed)
  : IEditableTextControl(r, "", grid_ui::Text(14.f), grid_ui::surface), mChanged(std::move(changed))
  {
    SetTextEntryLength(512);
    mText.mTextEntryBGColor = grid_ui::surface;
    mText.mTextEntryFGColor = grid_ui::text;
  }
  void BeginEdit()
  {
    GetUI()->ReleaseMouseCapture();
#ifdef _WIN32
    SetFocus(static_cast<HWND>(GetUI()->GetWindow()));
#endif
    GetUI()->CreateTextEntry(*this, mText, mRECT.GetHPadded(-12).GetVPadded(-3), GetStr());
  }
  void OnMouseDown(float, float, const IMouseMod&) override { BeginEdit(); }
  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(grid_ui::surface, mRECT, 4.f);
    auto* entry = GetUI()->GetTextEntryControl();
    const bool editing = entry && entry->EditInProgress() && GetUI()->GetControlInTextEntry() == this;
    g.DrawRoundRect(editing ? grid_ui::accent : grid_ui::border, mRECT, 4.f);
    if (editing) return;
    grid_ui::FittedText(g, grid_ui::Text(14.f, GetStr()[0] ? grid_ui::text : grid_ui::muted),
                       GetStr()[0] ? GetStr() : "Search...", mRECT.GetHPadded(-12));
  }
  bool IsDirty() override
  {
    auto* entry = GetUI()->GetTextEntryControl();
    std::string value = GetStr();
    if (entry && entry->EditInProgress() && GetUI()->GetControlInTextEntry() == this)
    {
      std::u16string text;
      const int length = std::min(512, ITextEntryControl::GetLength(entry));
      for (int i = 0; i < length; ++i) text += ITextEntryControl::GetChar(entry, i);
      if (!text.empty() && text.back() >= 0xd800 && text.back() <= 0xdbff) text.pop_back();
      value = UTF16ToUTF8String(text);
    }
    value = nam_library::BoundedQuery(std::move(value));
    if (value != mLive) { mLive = value; mChanged(value); }
    return IEditableTextControl::IsDirty();
  }
  void SetQuery(const std::string& value)
  { mLive = value; SetStr(value.c_str()); SetDirty(false); }
  void OnTextEntryCompletion(const char* text, int) override
  { SetQuery(nam_library::BoundedQuery(text)); mChanged(mLive); }
private:
  std::string mLive;
  std::function<void(const std::string&)> mChanged;
};

class NAMLibraryBrowserControl : public IContainerBase
{
public:
  explicit NAMLibraryBrowserControl(const IRECT& r) : IContainerBase(r) {}
  void OnAttached() override
  {
    using namespace grid_ui;
    mTitle = new HintLabel(IRECT(24, 8, 530, 48), "Library browser", Text(28.f),
      "Select to inspect. Double-click or Enter to load. Up/Down and Page Up/Down browse; Ctrl/Cmd+F searches; Escape closes.");
    AddChildControl(mTitle);
    mDetailsTitle = new HintLabel(IRECT(744, 164, 964, 192), "FILE DETAILS", Text(13.f, muted),
      "The selected file's metadata and full path. NAM files can also be filtered by gear type.");
    AddChildControl(mDetailsTitle);
    auto modules = std::make_shared<nam_library::Snapshot>();
    for (const auto& group : nam_library::kModuleGroups)
      for (const auto effect : group.effects)
      {
        nam_library::Capture item;
        item.path = std::filesystem::u8path(group.name) / SlotEffectName(effect);
        item.relative = item.path;
        item.kind = nam_library::FileKind::Module;
        item.module = static_cast<int>(effect);
        item.name = effect_presentation::Sound(effect);
        // The editor's EQ caption describes its plot; the browser describes the sound.
        if (effect == SlotEffect::ToneEQ) item.name = "Three-band guitar EQ. Shape bass, mids and treble after a capture or elsewhere in the grid.";
        if (effect == SlotEffect::CutEQ) item.name = "Remove low-end rumble and high-end fizz with low and high cuts.";
        if (effect == SlotEffect::ParametricEQ) item.name = "Boost or cut a focused band, or switch to a notch to tame a resonance.";
        if (effect == SlotEffect::ConsoleEQ) item.name = "Nonlinear band density with native low and high cuts.";
        item.model = grid_ui::Tone(effect, nam_library::Gear::Unknown).label;
        modules->captures.push_back(std::move(item));
      }
    mModules = modules;
    AddChildControl(new Button(IRECT(548, 14, 640, 42), [this](IControl*) {
      PLUG()->RescanCaptureFolder();
      if (!mContextRoot.empty()) RequestContextScan();
    }, "Rescan"));
    mSave = new Button(IRECT(650, 14, 778, 42), [this](IControl*) { PLUG()->PromptRigPreset(true); }, "Save preset as...");
    AddChildControl(mSave);
    mFile = new Button(IRECT(788, 14, 882, 42), [this](IControl*) { ChooseFile(); }, "Open file...");
    AddChildControl(mFile);
    AddChildControl(new Button(IRECT(896, 14, 976, 42), [this](IControl*) { Hide(true); }, "Close"));
    for (int i = 0; i < 4; ++i)
    {
      mTabs[i] = new NAMBrowserTab(IRECT(24.f + i * 128, 60, 142.f + i * 128, 88), [this, i](IControl*) {
        SwitchKind(kTabs[i]);
      }, i == 0 ? "Modules" : i == 1 ? "NAM captures" : i == 2 ? "IR files" : "Presets");
      AddChildControl(mTabs[i]);
    }
    mSearch = new NAMBrowserSearchControl(IRECT(24, 104, 724, 140), [this](const std::string& text) {
      if (View().query != text) { View().selected.clear(); View().scroll = 0; }
      View().query = text; Rebuild();
    });
    AddChildControl(mSearch);
    mSearch->SetTooltip("Match all words across names, paths and metadata. Use quoted phrases, -exclude, maker:vox, model:ac30, path:clean or gear:amp. Ctrl/Cmd+F focuses search.");
    AddChildControl(new Button(IRECT(734, 104, 798, 140), [this](IControl*) {
      FinishSearch(); View().query.clear(); mSearch->SetQuery(""); Rebuild();
    }, "Clear"));
    mSort = new Button(IRECT(808, 104, 976, 140), [this](IControl*) {
      mRelevance = !mRelevance; Rebuild();
    }, "Sort: relevance");
    AddChildControl(mSort);
    mGear = new Button(IRECT(744, 260, 964, 288), [this](IControl*) {
      mMenu.Clear();
      mMenu.AddItem(new IPopupMenu::Item("All gear", View().gear < 0 ? IPopupMenu::Item::kChecked : 0, 100));
      for (int i = 0; i <= static_cast<int>(nam_library::Gear::Spaces); ++i)
        mMenu.AddItem(new IPopupMenu::Item(nam_library::GearName(static_cast<nam_library::Gear>(i)),
                                          View().gear == i ? IPopupMenu::Item::kChecked : 0, 101 + i));
      GetUI()->CreatePopupMenu(*this, mMenu, mGear->GetRECT());
    }, "All gear");
    AddChildControl(mGear);
    mAdd = new Button(IRECT(548, 60, 656, 88), [this](IControl*) {
      WDL_String folder;
      GetUI()->PromptForDirectory(folder, [plugin = PLUG(), kind = mKind](const WDL_String&, const WDL_String& chosen) {
        if (chosen.GetLength()) plugin->AddLibraryFolder(kind, chosen.Get());
      });
    }, "+ Add folder");
    AddChildControl(mAdd);
    mRemove = new Button(IRECT(664, 60, 782, 88), [this](IControl*) {
      const auto root = std::find_if(mFolders.begin(), mFolders.end(), [this](const auto& folder) {
        return nam_library::SamePath(folder.path, View().folder) && folder.rootIndex >= 0 && folder.rootIndex < ConfiguredRoots().size();
      });
      if (root == mFolders.end()) return;
      const auto index = root->rootIndex;
      View().folder.clear();
      PLUG()->RemoveLibraryFolder(mKind, index);
      Rebuild();
    }, "Remove folder");
    mRemove->SetTooltip("Remove the selected source folder from the library. Its files stay on disk.");
    AddChildControl(mRemove);
    mDelete = new Button(IRECT(856, 60, 976, 88), [this](IControl*) { DeleteSelectedPreset(); }, "Delete preset");
    mDelete->SetTooltip("Delete the selected preset file from disk after confirmation. The current rig stays loaded.");
    AddChildControl(mDelete);
    mAdd->SetTooltip("Add a source folder and index its files and subfolders in the background.");
    mGear->SetTooltip("Filter NAM captures by metadata or gear inferred from their filenames and folders.");
  }

  void Open(nam_library::BrowserContext context, nam_library::FileKind kind, size_t slot)
  {
    Hide(true);
    mPreviewSelection.clear();
    mContext = context;
    mContextKind = kind;
    mSlot = context == nam_library::BrowserContext::Library ? PLUG()->GetSelectedSlot() : slot;
    mPreview = context != nam_library::BrowserContext::Rig && kind != nam_library::FileKind::RigPreset;
    mTitle->SetStr(context == nam_library::BrowserContext::Rig ? "Rig presets" : "Library browser");
    mTitle->SetTooltip(mPreview
      ? "Select to audition; adjust parameters below. Double-click or Enter keeps the module and edits. Close or Escape restores the original. Up/Down browses; Ctrl/Cmd+F searches."
      : "Select to inspect. Double-click or Enter to load. Up/Down and Page Up/Down browse; Ctrl/Cmd+F searches; Escape closes.");
    SetTargetAndDrawRECTs(IRECT(0, 0, grid_layout::width, mPreview ? grid_layout::editorTop : grid_layout::height));
    if (mPreview) mStatus = PLUG()->BeginSlotPreview(mSlot);
    mContextWorker.Request(std::vector<std::string>{});
    mContextLibrary.reset(); mContextRoot.clear(); mPendingSelection = false; mScanLibrary.reset();
    SwitchKind(kind);
    auto& view = View();
    view.selected.clear();
    view.query.clear(); view.gear = -1; view.folder.clear(); view.scroll = view.folderScroll = 0;
    mSearch->SetQuery("");
    if (kind == nam_library::FileKind::Module)
    {
      const auto effect = PLUG()->GetSlotEffect(mSlot);
      if (const auto* group = nam_library::ModuleGroupFor(effect))
      {
        view.folder = std::filesystem::u8path(group->name);
        view.selected = nam_library::PathText(view.folder / SlotEffectName(effect));
      }
    }
    else if (kind != nam_library::FileKind::RigPreset) view.selected = PLUG()->GetSlotPath(mSlot);
    else
    {
      const auto files = PLUG()->GetRigPresetFiles();
      for (const auto& file : files)
        if (nam_library::PathText(file.stem()) == PLUG()->GetRigPresetName()) view.selected = nam_library::PathText(file);
    }
    if (kind != nam_library::FileKind::Module && !view.selected.empty())
    {
      view.folder = std::filesystem::u8path(view.selected).parent_path();
      const auto roots = Roots();
      const auto library = PLUG()->GetCaptureLibrary();
      const bool indexed = std::any_of(library->captures.begin(), library->captures.end(), [&](const auto& file) {
        return nam_library::SamePath(file.path, std::filesystem::u8path(view.selected)) && file.kind == kind;
      });
      if (!indexed || std::none_of(roots.begin(), roots.end(), [&](const auto& root) { return nam_library::Within(view.folder, std::filesystem::u8path(root)); }))
      {
        mContextRoot = view.folder;
        mPendingSelection = true;
        RequestContextScan();
      }
    }
    mFolderKind = nam_library::FileKind::SlotPreset; // Opening may add a temporary current folder.
    Hide(false); Rebuild(); RevealSelection();
  }
  void Hide(bool hide) override
  {
    if (hide) { FinishSearch(); PLUG()->EndSlotPreview(false); mPreview = false; }
    IContainerBase::Hide(hide);
    // This composite page and the live text editor share a single redraw region.
    if (GetUI()) GetUI()->SetStrictDrawing(!hide);
    if (!hide)
    {
      int position = 0;
      for (int i = 0; i < 4; ++i)
      {
        const bool allowed = nam_library::Allows(mContext, kTabs[i]);
        mTabs[i]->Hide(!allowed);
        if (allowed)
        {
          mTabs[i]->SetTargetAndDrawRECTs(IRECT(24.f + position * 128, 60, 142.f + position * 128, 88));
          ++position;
        }
      }
      mSave->Hide(mKind != nam_library::FileKind::RigPreset);
      mGear->Hide(mKind != nam_library::FileKind::NAM);
      mFile->Hide(mKind == nam_library::FileKind::Module);
      mAdd->Hide(mKind == nam_library::FileKind::Module);
      mRemove->Hide(mKind == nam_library::FileKind::Module);
      mDelete->Hide(mKind != nam_library::FileKind::RigPreset);
    }
    if (GetUI()) GetUI()->SetAllControlsDirty();
  }
  bool IsDirty() override
  {
    if (!IsHidden())
    {
      // Rows/details are drawn by this composite parent, while the live editor
      // updates later in the control stack. Keep the whole modal page coherent.
      SetDirty(false);
      if (auto result = mContextWorker.TakeResult()) { mContextLibrary = result; mPendingSelection = false; mScanLibrary.reset(); }
      const auto snapshot = PLUG()->GetCaptureLibrary();
      const auto roots = Roots();
      if (snapshot != mScanLibrary || roots != mRoots) Rebuild();
      if (mPreview) PreviewSelected();
      if (const auto* selected = Selected())
        mDetailsTitle->SetTooltip((selected->kind == nam_library::FileKind::Module
          ? selected->name + " New FX start with musical defaults; the current FX keeps its edits."
          : nam_library::PathText(selected->path)).c_str());
      Disabled(mAdd, ConfiguredRoots().size() >= 32);
      const bool removable = std::any_of(mFolders.begin(), mFolders.end(), [this](const auto& folder) {
        return folder.rootIndex >= 0 && folder.rootIndex < ConfiguredRoots().size() && nam_library::SamePath(folder.path, View().folder);
      });
      Disabled(mRemove, !removable);
      Disabled(mDelete, !Selected() || Selected()->kind != nam_library::FileKind::RigPreset);
    }
    return IContainerBase::IsDirty();
  }
  void FocusCurrent() { Open(mContext, mKind, mSlot); }

  void Draw(IGraphics& g) override
  {
    using namespace grid_ui;
    g.FillRect(background, mRECT);
    const auto target = mKind == nam_library::FileKind::RigPreset ? "" : GridSlotName(mSlot);
    g.DrawText(Text(13.f, muted, EAlign::Far), target.c_str(), IRECT(788, 60, 976, 88));
    for (int i = 0; i < 4; ++i)
      if (mKind == kTabs[i])
      { const auto r = mTabs[i]->GetRECT(); g.FillRect(accent, IRECT(r.L, 91, r.R, 93)); }
    g.FillRoundRect(surface.WithOpacity(.5f), IRECT(24, 164, 266, ListBottom()), 4);
    g.FillRoundRect(surface.WithOpacity(.5f), IRECT(282, 164, 716, ListBottom()), 4);
    g.FillRoundRect(surface.WithOpacity(.5f), IRECT(732, 164, 976, ListBottom()), 4);
    g.DrawText(Text(13.f, muted), mKind == nam_library::FileKind::Module ? "FX CATEGORIES" : mKind == nam_library::FileKind::RigPreset ? "PRESET FOLDERS" : "NAM / IR FOLDERS", IRECT(36, 164, 256, 192));
    for (int row = 0; row < FolderRows() && View().folderScroll + row < mFolders.size(); ++row)
    {
      const auto& folder = mFolders[View().folderScroll + row];
      const float y = kFolderTop + row * kRowHeight;
      const bool selected = nam_library::SamePath(folder.path, View().folder);
      if (selected) g.FillRect(accent.WithOpacity(.14f), IRECT(26, y, 257, y + kRowHeight));
      FittedText(g, Text(14.f, selected ? accent : text), folder.label.c_str(),
                 IRECT(36.f + std::min(folder.depth, 7) * 12, y, 250, y + kRowHeight));
    }
    for (int row = 0; row < Rows() && View().scroll + row < mResults.size(); ++row)
    {
      const auto& file = mLibrary->captures[mResults[View().scroll + row]];
      const float y = kResultsTop + row * kRowHeight;
      const bool selected = View().scroll + row == mSelectedRow;
      if (selected) g.FillRect(accent.WithOpacity(.14f), IRECT(284, y, 706, y + kRowHeight));
      const bool module = file.kind == nam_library::FileKind::Module;
      FittedText(g, Text(14.f, selected ? accent : text), nam_library::PathText(file.path.stem()).c_str(), IRECT(294, y, module ? 586 : 696, y + kRowHeight));
      if (module) FittedText(g, Text(12.f, muted, EAlign::Far), nam_library::PathText(file.relative.parent_path()).c_str(), IRECT(594, y, 696, y + kRowHeight));
    }
    Scrollbar(g, 257, View().folderScroll, mFolders.size(), FolderRows());
    Scrollbar(g, 707, View().scroll, mResults.size(), Rows());
    DrawDetails(g);
    if (mResults.empty())
    {
      const char* message = PLUG()->IsLibraryScanning() ? "Scanning..." : mRoots.empty() ? "No folders" : "No matches";
      g.DrawMultiLineText(Text(14.f, muted), message, IRECT(314, 270, 684, 390));
    }
    if (!mStatus.empty()) FittedText(g, Text(12.f, IColor(255, 238, 147, 122)), mStatus.c_str(), IRECT(24, 142, 976, 162));
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (y < kResultsTop || y >= ListBottom()) return;
    if (x >= 257 && x <= 266) { mDrag = 1; DragScroll(y); return; }
    if (x >= 707 && x <= 716) { mDrag = 2; DragScroll(y); return; }
    if (x >= 24 && x < 257 && y >= kFolderTop)
    {
      const int index = View().folderScroll + static_cast<int>((y - kFolderTop) / kRowHeight);
      if (index < mFolders.size()) { View().folder = mFolders[index].path; View().scroll = 0; Rebuild(); }
    }
    else if (x >= 282 && x < 707)
    {
      const int index = View().scroll + static_cast<int>((y - kResultsTop) / kRowHeight);
      if (index < mResults.size()) { mSelectedRow = index; View().selected = nam_library::PathText(mLibrary->captures[mResults[index]].path); mStatus.clear(); SetDirty(false); }
    }
  }
  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    OnMouseDown(x, y, mod);
    const int row = View().scroll + static_cast<int>((y - kResultsTop) / kRowHeight);
    if (x >= 282 && x < 707 && y >= kResultsTop && y < ListBottom() && row < mResults.size()) LoadSelected();
  }
  void OnMouseDrag(float, float y, float, float, const IMouseMod&) override { if (mDrag) DragScroll(y); }
  void OnMouseUp(float, float, const IMouseMod&) override { mDrag = 0; }
  void OnMouseWheel(float x, float y, const IMouseMod&, float delta) override
  {
    if (y < kResultsTop || y >= ListBottom()) return;
    const bool folders = x < 282;
    auto& scroll = folders ? View().folderScroll : View().scroll;
    const int maximum = std::max(0, static_cast<int>(folders ? mFolders.size() : mResults.size()) - (folders ? FolderRows() : Rows()));
    scroll = std::clamp(scroll + (delta < 0 ? 3 : -3), 0, maximum); SetDirty(false);
  }
  bool OnKeyDown(float, float, const IKeyPress& key) override { return HandleKey(key); }
  bool HandleKey(const IKeyPress& key)
  {
    if (key.VK == kVK_ESCAPE) Hide(true);
    else if (key.C && key.VK == 'F') mSearch->BeginEdit();
    else if (key.VK == kVK_RETURN) LoadSelected();
    else if (key.VK == kVK_UP || key.VK == kVK_DOWN || key.VK == kVK_PRIOR || key.VK == kVK_NEXT || key.VK == kVK_HOME || key.VK == kVK_END)
    {
      int index = SelectedRow();
      if (key.VK == kVK_HOME) index = 0;
      else if (key.VK == kVK_END) index = static_cast<int>(mResults.size()) - 1;
      else index += (key.VK == kVK_UP || key.VK == kVK_PRIOR ? -1 : 1) * (key.VK == kVK_PRIOR || key.VK == kVK_NEXT ? Rows() : 1);
      if (!mResults.empty())
      {
        mSelectedRow = std::clamp(index, 0, static_cast<int>(mResults.size()) - 1);
        View().selected = nam_library::PathText(mLibrary->captures[mResults[mSelectedRow]].path);
      }
      RevealSelection(); SetDirty(false);
    }
    return true; // Modal page: never pass keys to controls beneath it.
  }
  void OnPopupMenuSelection(IPopupMenu* menu, int) override
  {
    const auto* item = menu ? menu->GetChosenItem() : nullptr;
    if (item && item->GetTag() >= 100) { View().gear = item->GetTag() - 101; Rebuild(); }
  }

private:
  static constexpr float kRowHeight = 25.f, kResultsTop = 164.f, kFolderTop = 196.f;
  int Rows() const { return static_cast<int>((ListBottom() - kResultsTop) / kRowHeight); }
  int FolderRows() const { return static_cast<int>((ListBottom() - kFolderTop) / kRowHeight); }
  float ListBottom() const { return mRECT.B - 12.f; }
  struct State { std::string query, selected; std::filesystem::path folder; int gear = -1, scroll = 0, folderScroll = 0; };
  State& View() { return mViews[static_cast<int>(mKind)]; }
  std::vector<std::string> ConfiguredRoots()
  {
    return PLUG()->GetLibraryFolders(mKind);
  }
  std::vector<std::string> Roots()
  {
    auto roots = ConfiguredRoots();
    if (ContextApplies() && !mContextRoot.empty())
    {
      if (std::none_of(roots.begin(), roots.end(), [this](const auto& root) { return nam_library::Within(mContextRoot, std::filesystem::u8path(root)); }))
        roots.push_back(nam_library::PathText(mContextRoot));
    }
    return roots;
  }
  bool ContextApplies() const { return mKind != nam_library::FileKind::Module && mKind == mContextKind; }
  // Index the current file's folder temporarily, for its own kind only.
  void RequestContextScan()
  {
    const std::vector<std::string> root{nam_library::PathText(mContextRoot)}, none;
    mContextWorker.Request(mContextKind == nam_library::FileKind::NAM ? root : none,
                           mContextKind == nam_library::FileKind::IR ? root : none,
                           mContextKind == nam_library::FileKind::RigPreset ? root : none);
  }
  static void Disabled(IControl* control, bool value)
  { if (control->IsDisabled() != value) control->SetDisabled(value); }
  void FinishSearch()
  {
    if (!mSearch || !GetUI()) return;
    auto* entry = GetUI()->GetTextEntryControl();
    if (entry && entry->EditInProgress() && GetUI()->GetControlInTextEntry() == mSearch) entry->CommitEdit();
  }
  void SwitchKind(nam_library::FileKind kind)
  {
    if (!nam_library::Allows(mContext, kind)) return;
    FinishSearch(); mKind = kind; mStatus.clear();
    const bool preview = mContext != nam_library::BrowserContext::Rig && kind != nam_library::FileKind::RigPreset;
    if (preview != mPreview)
    {
      PLUG()->EndSlotPreview(false);
      mPreview = preview;
      mPreviewSelection.clear();
      if (mPreview) mStatus = PLUG()->BeginSlotPreview(mSlot);
      SetTargetAndDrawRECTs(IRECT(0, 0, grid_layout::width, mPreview ? grid_layout::editorTop : grid_layout::height));
    }
    mTitle->SetTooltip(mPreview
      ? "Select to audition; adjust parameters below. Double-click or Enter keeps the module and edits. Close or Escape restores the original. Up/Down browses; Ctrl/Cmd+F searches."
      : "Select to inspect. Double-click or Enter to load. Up/Down and Page Up/Down browse; Ctrl/Cmd+F searches; Escape closes.");
    for (int i = 0; i < 4; ++i) mTabs[i]->SetActive(kind == kTabs[i]);
    mSearch->SetQuery(View().query);
    mSave->Hide(kind != nam_library::FileKind::RigPreset);
    mGear->Hide(kind != nam_library::FileKind::NAM);
    mFile->Hide(kind == nam_library::FileKind::Module);
    mAdd->Hide(kind == nam_library::FileKind::Module);
    mRemove->Hide(kind == nam_library::FileKind::Module);
    mDelete->Hide(kind != nam_library::FileKind::RigPreset);
    Rebuild();
  }
  void Rebuild()
  {
    const auto snapshot = PLUG()->GetCaptureLibrary();
    const auto roots = Roots();
    const bool refreshFolders = snapshot != mScanLibrary || roots != mRoots || mFolderKind != mKind;
    if (snapshot != mScanLibrary)
    {
      mScanLibrary = snapshot;
      mCombined = snapshot;
      if (mContextLibrary && !mContextLibrary->captures.empty())
      {
        auto combined = std::make_shared<nam_library::Snapshot>(*snapshot);
        std::set<std::filesystem::path> paths;
        for (const auto& file : combined->captures) paths.insert(file.path);
        for (const auto& file : mContextLibrary->captures)
          if (paths.insert(file.path).second) combined->captures.push_back(file);
        mCombined = combined;
      }
    }
    mLibrary = mKind == nam_library::FileKind::Module ? mModules : mCombined;
    mRoots = roots;
    if (refreshFolders)
    {
      mFolderKind = mKind;
      if (mKind == nam_library::FileKind::Module)
      {
        mFolders = {{{}, "All categories"}};
        for (const auto& group : nam_library::kModuleGroups)
          mFolders.push_back({std::filesystem::u8path(group.name), group.name});
      }
      else mFolders = nam_library::BrowserFolders(*mLibrary, mKind, mRoots);
      if (ContextApplies() && !mContextRoot.empty()
          && std::none_of(mFolders.begin(), mFolders.end(), [this](const auto& f) { return nam_library::SamePath(f.path, mContextRoot); }))
        mFolders.push_back({mContextRoot, "Current folder"});
    }
    if (std::none_of(mFolders.begin(), mFolders.end(), [this](const auto& f) { return nam_library::SamePath(f.path, View().folder); })) View().folder.clear();
    mResults = nam_library::Browse(*mLibrary, mKind, View().query, View().folder, View().gear, mRelevance);
    mSelectedRow = -1;
    const auto selected = std::filesystem::u8path(View().selected);
    for (int i = 0; i < mResults.size(); ++i)
      if (nam_library::SamePath(mLibrary->captures[mResults[i]].path, selected)) { mSelectedRow = i; break; }
    if (mSelectedRow < 0 && !mResults.empty() && !mPendingSelection)
    { mSelectedRow = 0; View().selected = nam_library::PathText(mLibrary->captures[mResults.front()].path); }
    View().scroll = std::clamp(View().scroll, 0, std::max(0, static_cast<int>(mResults.size()) - Rows()));
    View().folderScroll = std::clamp(View().folderScroll, 0, std::max(0, static_cast<int>(mFolders.size()) - FolderRows()));
    mSort->SetLabelStr(mRelevance ? "Sort: relevance" : "Sort: name");
    mGear->SetLabelStr(View().gear < 0 ? "All gear" : nam_library::GearName(static_cast<nam_library::Gear>(View().gear)));
    RevealSelection();
    SetDirty(false);
  }
  int SelectedRow() const { return mSelectedRow; }
  const nam_library::Capture* Selected()
  { const int index = SelectedRow(); return index < 0 ? nullptr : &mLibrary->captures[mResults[index]]; }
  void RevealSelection()
  {
    const int index = SelectedRow();
    if (index < 0) return;
    if (index < View().scroll) View().scroll = index;
    if (index >= View().scroll + Rows()) View().scroll = index - Rows() + 1;
  }
  void LoadSelected()
  {
    FinishSearch();
    const auto* file = Selected();
    if (!file || !nam_library::Allows(mContext, file->kind)) return;
    if (mPreview)
    {
      PreviewSelected(true);
      if (!mStatus.empty()) return;
      PLUG()->EndSlotPreview(true);
      Hide(true);
      return;
    }
    // Copy before a rig recall can replace the snapshot and refresh controls.
    const auto path = file->path;
    if (file->kind == nam_library::FileKind::Module)
    {
      const auto effect = static_cast<SlotEffect>(file->module);
      const auto current = PLUG()->GetSlotEffect(mSlot);
      if (effect != current) PLUG()->SetSlotEffect(mSlot, effect);
      Hide(true); return;
    }
    const auto error = file->kind == nam_library::FileKind::RigPreset ? PLUG()->LoadRigPreset(path)
                       : file->kind == nam_library::FileKind::IR ? PLUG()->LoadSlotIR(mSlot, nam_library::PathText(path).c_str())
                       : PLUG()->LoadSlot(mSlot, nam_library::PathText(path).c_str());
    mStatus = error.empty() ? "Loaded " + nam_library::PathText(path.stem()) : error;
    if (error.empty()) Hide(true);
    SetDirty(false);
  }
  void DeleteSelectedPreset()
  {
    FinishSearch();
    const auto* file = Selected();
    if (!file || file->kind != nam_library::FileKind::RigPreset) return;
    const auto path = file->path;
    if (!PLUG()->ConfirmDeleteRigPreset(path)) return;
    // Keep the selection in place: the following preset, or the previous one at the end.
    const int row = SelectedRow();
    const int neighbour = row + 1 < static_cast<int>(mResults.size()) ? row + 1 : row - 1;
    const auto next = neighbour >= 0 ? nam_library::PathText(mLibrary->captures[mResults[neighbour]].path) : std::string{};
    const auto error = PLUG()->DeleteRigPreset(path);
    mStatus = error.empty() ? "Deleted " + nam_library::PathText(path.stem()) : error;
    if (error.empty())
    {
      View().selected = next;
      if (mContextLibrary)
      {
        auto context = std::make_shared<nam_library::Snapshot>(*mContextLibrary);
        context->captures.erase(std::remove_if(context->captures.begin(), context->captures.end(),
          [&](const auto& entry) { return nam_library::SamePath(entry.path, path); }), context->captures.end());
        mContextLibrary = context;
      }
      mScanLibrary.reset();
    }
    Rebuild();
  }
  void PreviewSelected(bool force = false)
  {
    const auto* file = Selected();
    if (!file) return;
    const auto key = std::to_string(static_cast<int>(file->kind)) + ":" + nam_library::PathText(file->path);
    if (!force && key == mPreviewSelection) return;
    mPreviewSelection = key;
    const auto effect = file->kind == nam_library::FileKind::Module ? static_cast<SlotEffect>(file->module)
      : file->kind == nam_library::FileKind::IR ? SlotEffect::CabinetIR : SlotEffect::NAM;
    mStatus = PLUG()->PreviewSlot(effect, file->kind == nam_library::FileKind::Module ? "" : nam_library::PathText(file->path));
  }
  void ChooseFile()
  {
    FinishSearch();
    if (mKind == nam_library::FileKind::RigPreset) { PLUG()->PromptRigPreset(false); return; }
    WDL_String file, directory;
    const bool ir = mKind == nam_library::FileKind::IR;
    GetUI()->PromptForFile(file, directory, EFileAction::Open, ir ? "wav" : "nam",
      [this, ir, slot = mSlot](const WDL_String& chosen, const WDL_String&) {
        if (!chosen.GetLength()) return;
        const auto error = mPreview ? PLUG()->PreviewSlot(ir ? SlotEffect::CabinetIR : SlotEffect::NAM, chosen.Get())
          : ir ? PLUG()->LoadSlotIR(slot, chosen.Get()) : PLUG()->LoadSlot(slot, chosen.Get());
        mStatus = error.empty() ? "Loaded " + nam_library::PathText(std::filesystem::u8path(chosen.Get()).stem()) : error;
        if (error.empty()) { PLUG()->EndSlotPreview(true); Hide(true); }
        SetDirty(false);
      });
  }
  void DrawDetails(IGraphics& g)
  {
    using namespace grid_ui;
    const auto* file = Selected();
    if (!file) return;
    if (file->kind == nam_library::FileKind::Module)
    {
      FittedText(g, Text(16.f, accent), nam_library::PathText(file->path.stem()).c_str(), IRECT(744, 188, 964, 214));
      const auto body = file->name + "\n\n" + ModuleParameters(static_cast<SlotEffect>(file->module));
      g.DrawMultiLineText(Text(14.f, muted).WithVAlign(EVAlign::Top), body.c_str(), IRECT(744, 220, 964, ListBottom() - 8));
      return;
    }
    g.DrawMultiLineText(Text(16.f, accent).WithVAlign(EVAlign::Top), nam_library::PathText(file->path.stem()).c_str(), IRECT(744, 194, 964, 254));
    const auto metadata = file->kind == nam_library::FileKind::NAM
      ? std::string(nam_library::GearName(file->gear))
        + (file->maker.empty() ? "" : "\n" + file->maker) + (file->model.empty() ? "" : " / " + file->model)
      : file->kind == nam_library::FileKind::IR ? "WAV impulse response" : "Complete NAM Grid rig";
    const auto body = metadata + (file->name.empty() ? "" : "\n" + file->name) + "\n\n" + nam_library::PathText(file->path);
    g.DrawMultiLineText(Text(13.f, muted).WithVAlign(EVAlign::Top), body.c_str(),
      IRECT(744, file->kind == nam_library::FileKind::NAM ? 300 : 264, 964, ListBottom() - 8));
  }
  static std::string ModuleParameters(SlotEffect effect)
  {
    switch (effect)
    {
      case SlotEffect::NoiseGate: return "Threshold, Release, Reduction";
      case SlotEffect::SmoothComp: case SlotEffect::PunchComp: return "Threshold, Release, Makeup";
      case SlotEffect::ToneEQ: return "Bass, Mid, Treble";
      case SlotEffect::CutEQ: return "Low cut, High cut, Presence";
      default: break;
    }
    std::string result;
    for (int p = 0; p < kUtilityControls; ++p)
      if (effect_presentation::Visible(effect, p))
      {
        if (!result.empty()) result += ", ";
        result += effect_presentation::Label(effect, p);
      }
    return result;
  }
  void Scrollbar(IGraphics& g, float x, int offset, int count, int visible)
  {
    if (count <= visible) return;
    const float top = x < 282 ? kFolderTop : kResultsTop;
    g.FillRect(grid_ui::border.WithOpacity(.3f), IRECT(x, top, x + 7, ListBottom()));
    const float height = std::max(24.f, (ListBottom() - top) * visible / count);
    const float y = top + ((ListBottom() - top) - height) * offset / (count - visible);
    g.FillRoundRect(grid_ui::muted.WithOpacity(.6f), IRECT(x, y, x + 7, y + height), 3);
  }
  void DragScroll(float y)
  {
    auto& offset = mDrag == 1 ? View().folderScroll : View().scroll;
    const int maximum = std::max(0, static_cast<int>(mDrag == 1 ? mFolders.size() : mResults.size()) - (mDrag == 1 ? FolderRows() : Rows()));
    const float top = mDrag == 1 ? kFolderTop : kResultsTop;
    offset = static_cast<int>(std::clamp((y - top) / (ListBottom() - top), 0.f, 1.f) * maximum); SetDirty(false);
  }
  nam_library::BrowserContext mContext = nam_library::BrowserContext::Library;
  nam_library::FileKind mKind = nam_library::FileKind::NAM;
  nam_library::FileKind mContextKind = nam_library::FileKind::NAM;
  nam_library::FileKind mFolderKind = nam_library::FileKind::SlotPreset;
  static constexpr std::array<nam_library::FileKind, 4> kTabs{nam_library::FileKind::Module, nam_library::FileKind::NAM, nam_library::FileKind::IR, nam_library::FileKind::RigPreset};
  size_t mSlot = 0;
  bool mPreview = false;
  std::string mPreviewSelection;
  std::array<State, 5> mViews;
  std::shared_ptr<const nam_library::Snapshot> mLibrary;
  std::shared_ptr<const nam_library::Snapshot> mModules, mCombined, mScanLibrary, mContextLibrary;
  nam_library::Worker mContextWorker;
  std::filesystem::path mContextRoot;
  bool mPendingSelection = false;
  std::vector<size_t> mResults;
  std::vector<nam_library::BrowserFolder> mFolders;
  std::vector<std::string> mRoots;
  std::string mStatus;
  bool mRelevance = true;
  int mDrag = 0;
  int mSelectedRow = -1;
  NAMBrowserSearchControl* mSearch = nullptr;
  grid_ui::HintLabel* mTitle = nullptr;
  grid_ui::HintLabel* mDetailsTitle = nullptr;
  grid_ui::Button *mAdd = nullptr, *mRemove = nullptr, *mDelete = nullptr, *mSort = nullptr, *mSave = nullptr, *mGear = nullptr, *mFile = nullptr;
  std::array<NAMBrowserTab*, 4> mTabs{};
  IPopupMenu mMenu;
};
