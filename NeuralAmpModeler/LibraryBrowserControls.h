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
  { GetUI()->CreateTextEntry(*this, mText, mRECT.GetHPadded(-12), GetStr()); }
  void OnMouseDown(float, float, const IMouseMod&) override { BeginEdit(); }
  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(grid_ui::surface, mRECT, 4.f);
    g.DrawRoundRect(grid_ui::border, mRECT, 4.f);
    grid_ui::FittedText(g, grid_ui::Text(14.f, GetStr()[0] ? grid_ui::text : grid_ui::muted),
                       GetStr()[0] ? GetStr() : "Search names, folders, maker, model...", mRECT.GetHPadded(-12));
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
    AddChildControl(new Button(IRECT(548, 26, 640, 58), [this](IControl*) {
      PLUG()->RescanCaptureFolder();
      if (!mContextRoot.empty())
      {
        const std::vector<std::string> roots{nam_library::PathText(mContextRoot)};
        const bool presets = mContextKind == nam_library::FileKind::RigPreset;
        mContextWorker.Request(presets ? std::vector<std::string>{} : roots,
                               presets ? roots : std::vector<std::string>{});
      }
    }, "Rescan"));
    mSave = new Button(IRECT(650, 26, 778, 58), [this](IControl*) { PLUG()->PromptRigPreset(true); }, "Save preset as...");
    AddChildControl(mSave);
    mFile = new Button(IRECT(788, 26, 882, 58), [this](IControl*) { ChooseFile(); }, "Open file...");
    AddChildControl(mFile);
    AddChildControl(new Button(IRECT(896, 26, 968, 58), [this](IControl*) { Hide(true); }, "Close"));
    for (int i = 0; i < 4; ++i)
    {
      mTabs[i] = new NAMBrowserTab(IRECT(32.f + i * 128, 76, 150.f + i * 128, 108), [this, i](IControl*) {
        SwitchKind(kTabs[i]);
      }, i == 0 ? "Modules" : i == 1 ? "NAM captures" : i == 2 ? "IR files" : "Presets");
      AddChildControl(mTabs[i]);
    }
    mSearch = new NAMBrowserSearchControl(IRECT(32, 124, 724, 160), [this](const std::string& text) {
      if (View().query != text) { View().selected.clear(); View().scroll = 0; }
      View().query = text; Rebuild();
    });
    AddChildControl(mSearch);
    AddChildControl(new Button(IRECT(734, 124, 798, 160), [this](IControl*) {
      FinishSearch(); View().query.clear(); mSearch->SetQuery(""); Rebuild();
    }, "Clear"));
    mSort = new Button(IRECT(808, 124, 968, 160), [this](IControl*) {
      mRelevance = !mRelevance; Rebuild();
    }, "Sort: relevance");
    AddChildControl(mSort);
    mGear = new Button(IRECT(732, 294, 968, 326), [this](IControl*) {
      mMenu.Clear();
      mMenu.AddItem(new IPopupMenu::Item("All gear", View().gear < 0 ? IPopupMenu::Item::kChecked : 0, 100));
      for (int i = 0; i <= static_cast<int>(nam_library::Gear::Spaces); ++i)
        mMenu.AddItem(new IPopupMenu::Item(nam_library::GearName(static_cast<nam_library::Gear>(i)),
                                          View().gear == i ? IPopupMenu::Item::kChecked : 0, 101 + i));
      GetUI()->CreatePopupMenu(*this, mMenu, mGear->GetRECT());
    }, "All gear");
    AddChildControl(mGear);
    mAdd = new Button(IRECT(32, 646, 166, 680), [this](IControl*) {
      const bool presets = mKind == nam_library::FileKind::RigPreset;
      WDL_String folder;
      GetUI()->PromptForDirectory(folder, [plugin = PLUG(), presets](const WDL_String&, const WDL_String& chosen) {
        if (!chosen.GetLength()) return;
        if (!presets) plugin->AddCaptureFolder(chosen.Get());
        else
        {
          auto roots = plugin->GetPresetFolders();
          if (roots.size() < 32) roots.emplace_back(chosen.Get());
          plugin->SetPresetFolders(std::move(roots), true);
        }
      });
    }, "+ Add folder");
    AddChildControl(mAdd);
    mRemove = new Button(IRECT(176, 646, 266, 680), [this](IControl*) {
      const auto root = std::find_if(mFolders.begin(), mFolders.end(), [this](const auto& folder) {
        return nam_library::SamePath(folder.path, View().folder) && folder.rootIndex >= 0 && folder.rootIndex < ConfiguredRoots().size();
      });
      if (root == mFolders.end()) return;
      const auto index = root->rootIndex;
      View().folder.clear();
      if (mKind != nam_library::FileKind::RigPreset) PLUG()->RemoveCaptureFolder(index);
      else
      {
        auto roots = PLUG()->GetPresetFolders();
        if (index < roots.size()) roots.erase(roots.begin() + index);
        PLUG()->SetPresetFolders(std::move(roots), true);
      }
      Rebuild();
    }, "Remove");
    mRemove->SetTooltip("Remove the selected library root from the library. Files stay on disk.");
    AddChildControl(mRemove);
    mLoad = new Button(IRECT(794, 646, 968, 680), [this](IControl*) { LoadSelected(); }, "Load");
    AddChildControl(mLoad);
  }

  void Open(nam_library::BrowserContext context, nam_library::FileKind kind, size_t slot)
  {
    mLastResultClick.clear();
    mContext = context;
    mContextKind = kind;
    mSlot = context == nam_library::BrowserContext::Library ? PLUG()->GetSelectedSlot() : slot;
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
        view.selected = nam_library::PathText(view.folder / SlotEffectName(effect == SlotEffect::TightGate ? SlotEffect::NaturalGate : effect));
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
        const auto root = std::vector<std::string>{nam_library::PathText(mContextRoot)};
        mContextWorker.Request(kind == nam_library::FileKind::RigPreset ? std::vector<std::string>{} : root,
                               kind == nam_library::FileKind::RigPreset ? root : std::vector<std::string>{});
      }
    }
    mFolderKind = nam_library::FileKind::SlotPreset; // Opening may add a temporary current folder.
    Hide(false); Rebuild(); RevealSelection();
  }
  void Hide(bool hide) override
  {
    if (hide) FinishSearch();
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
          mTabs[i]->SetRECT(IRECT(32.f + position * 128, 76, 150.f + position * 128, 108));
          ++position;
        }
      }
      mSave->Hide(mKind != nam_library::FileKind::RigPreset);
      mGear->Hide(mKind != nam_library::FileKind::NAM);
      mFile->Hide(mKind == nam_library::FileKind::Module);
      mAdd->Hide(mKind == nam_library::FileKind::Module);
      mRemove->Hide(mKind == nam_library::FileKind::Module);
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
      Disabled(mLoad, Selected() == nullptr);
      Disabled(mAdd, ConfiguredRoots().size() >= 32);
      const bool removable = std::any_of(mFolders.begin(), mFolders.end(), [this](const auto& folder) {
        return folder.rootIndex >= 0 && folder.rootIndex < ConfiguredRoots().size() && nam_library::SamePath(folder.path, View().folder);
      });
      Disabled(mRemove, !removable);
    }
    return IContainerBase::IsDirty();
  }
  void FocusCurrent() { Open(mContext, mKind, mSlot); }

  void Draw(IGraphics& g) override
  {
    using namespace grid_ui;
    g.FillRect(background, mRECT);
    g.DrawText(Text(23.f), mContext == nam_library::BrowserContext::Rig ? "Rig presets" : "Library browser", IRECT(32, 20, 530, 62));
    const auto target = mKind == nam_library::FileKind::RigPreset ? "Load a complete rig" : "Load into " + GridSlotName(mSlot);
    g.DrawText(Text(13.f, muted, EAlign::Far), target.c_str(), IRECT(554, 76, 968, 108));
    for (int i = 0; i < 4; ++i)
      if (mKind == kTabs[i])
      { const auto r = mTabs[i]->GetRECT(); g.FillRect(accent, IRECT(r.L, 111, r.R, 113)); }
    g.DrawText(Text(13.f, muted), "All words match. Use \"a phrase\", -exclude, maker:vox, model:ac30, path:clean.", IRECT(32, 166, 968, 188));
    g.FillRoundRect(surface.WithOpacity(.5f), IRECT(32, 200, 266, 632), 4);
    g.FillRoundRect(surface.WithOpacity(.5f), IRECT(282, 200, 716, 632), 4);
    g.FillRoundRect(surface.WithOpacity(.5f), IRECT(732, 338, 968, 632), 4);
    g.DrawText(Text(13.f, muted), mKind == nam_library::FileKind::Module ? "FX CATEGORIES" : mKind == nam_library::FileKind::RigPreset ? "PRESET FOLDERS" : "NAM / IR FOLDERS", IRECT(44, 200, 256, 228));
    const auto count = std::to_string(mResults.size()) + " " + nam_library::KindName(mKind) + " results";
    g.DrawText(Text(13.f, muted), count.c_str(), IRECT(294, 200, 704, 228));
    for (int row = 0; row < kFolderRows && View().folderScroll + row < mFolders.size(); ++row)
    {
      const auto& folder = mFolders[View().folderScroll + row];
      const float y = 232.f + row * 25;
      const bool selected = nam_library::SamePath(folder.path, View().folder);
      if (selected) g.FillRect(accent.WithOpacity(.14f), IRECT(34, y, 257, y + 25));
      FittedText(g, Text(14.f, selected ? accent : text), folder.label.c_str(),
                 IRECT(44.f + std::min(folder.depth, 7) * 12, y, 250, y + 25));
    }
    for (int row = 0; row < kRows && View().scroll + row < mResults.size(); ++row)
    {
      const auto& file = mLibrary->captures[mResults[View().scroll + row]];
      const float y = 232.f + row * 40;
      const bool selected = View().scroll + row == mSelectedRow;
      if (selected) g.FillRect(accent.WithOpacity(.14f), IRECT(284, y, 706, y + 39));
      FittedText(g, Text(15.f, selected ? accent : text), nam_library::PathText(file.path.stem()).c_str(), IRECT(294, y, 696, y + 22));
      FittedText(g, Text(13.f, muted), nam_library::PathText(file.relative.parent_path()).c_str(), IRECT(294, y + 21, 696, y + 38));
    }
    Scrollbar(g, 257, View().folderScroll, mFolders.size(), kFolderRows);
    Scrollbar(g, 707, View().scroll, mResults.size(), kRows);
    g.DrawText(Text(13.f, muted), "FILTER", IRECT(744, 200, 956, 228));
    FittedText(g, Text(13.f), mKind == nam_library::FileKind::Module ? "Built-in effects" : mKind == nam_library::FileKind::NAM ? "NAM metadata / gear" : mKind == nam_library::FileKind::IR ? "WAV impulse responses" : "nam-grid rigs (.ngrid)", IRECT(744, 234, 956, 261));
    if (mKind != nam_library::FileKind::NAM)
      g.DrawText(Text(13.f, muted), mKind == nam_library::FileKind::Module ? "Search effect names and sounds" : "Search by filename and folder", IRECT(744, 264, 956, 287));
    DrawDetails(g);
    if (mResults.empty())
    {
      const char* message = PLUG()->IsLibraryScanning() ? "Scanning folders..." : mRoots.empty() ? "Add a folder to this library." : "No matches. Clear search or choose All folders.";
      g.DrawMultiLineText(Text(14.f, muted), message, IRECT(314, 270, 684, 390));
    }
    if (mStatus.empty()) FittedText(g, Text(13.f, muted), PLUG()->GetCaptureStatus().c_str(), IRECT(282, 644, 780, 682));
    else g.DrawMultiLineText(Text(13.f, IColor(255, 238, 147, 122)), mStatus.c_str(), IRECT(282, 638, 780, 686));
    g.DrawText(Text(13.f, muted), "Select to inspect  /  Double-click or Enter to load  /  Up & Down to browse  /  Ctrl/Cmd+F to search  /  Esc to close", IRECT(32, 688, 968, 716));
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (y < 232 || y >= 632) return;
    if (x >= 257 && x <= 266) { mDrag = 1; DragScroll(y); return; }
    if (x >= 707 && x <= 716) { mDrag = 2; DragScroll(y); return; }
    if (x >= 32 && x < 257)
    {
      const int index = View().folderScroll + static_cast<int>((y - 232) / 25);
      if (index < mFolders.size()) { View().folder = mFolders[index].path; View().scroll = 0; Rebuild(); }
    }
    else if (x >= 282 && x < 707)
    {
      const int index = View().scroll + static_cast<int>((y - 232) / 40);
      if (index < mResults.size()) { mSelectedRow = index; View().selected = nam_library::PathText(mLibrary->captures[mResults[index]].path); mLastResultClick = View().selected; mStatus.clear(); SetDirty(false); }
    }
  }
  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    const auto previous = mLastResultClick;
    OnMouseDown(x, y, mod);
    const int row = View().scroll + static_cast<int>((y - 232) / 40);
    if (x >= 282 && x < 707 && y >= 232 && y < 632 && row < mResults.size()
        && !previous.empty() && previous == View().selected) LoadSelected();
  }
  void OnMouseDrag(float, float y, float, float, const IMouseMod&) override { if (mDrag) DragScroll(y); }
  void OnMouseUp(float, float, const IMouseMod&) override { mDrag = 0; }
  void OnMouseWheel(float x, float y, const IMouseMod&, float delta) override
  {
    if (y < 200 || y > 632) return;
    const bool folders = x < 282;
    auto& scroll = folders ? View().folderScroll : View().scroll;
    const int maximum = std::max(0, static_cast<int>(folders ? mFolders.size() : mResults.size()) - (folders ? kFolderRows : kRows));
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
      else index += (key.VK == kVK_UP || key.VK == kVK_PRIOR ? -1 : 1) * (key.VK == kVK_PRIOR || key.VK == kVK_NEXT ? kRows : 1);
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
  static constexpr int kRows = 10, kFolderRows = 16;
  struct State { std::string query, selected; std::filesystem::path folder; int gear = -1, scroll = 0, folderScroll = 0; };
  State& View() { return mViews[static_cast<int>(mKind)]; }
  std::vector<std::string> ConfiguredRoots()
  {
    return mKind == nam_library::FileKind::Module ? std::vector<std::string>{}
           : mKind == nam_library::FileKind::RigPreset ? PLUG()->GetPresetFolders() : PLUG()->GetCaptureFolders();
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
  bool ContextApplies() const
  {
    return mKind != nam_library::FileKind::Module
           && (mKind == nam_library::FileKind::RigPreset) == (mContextKind == nam_library::FileKind::RigPreset);
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
    for (int i = 0; i < 4; ++i) mTabs[i]->SetActive(kind == kTabs[i]);
    mSearch->SetQuery(View().query);
    mSave->Hide(kind != nam_library::FileKind::RigPreset);
    mGear->Hide(kind != nam_library::FileKind::NAM);
    mFile->Hide(kind == nam_library::FileKind::Module);
    mAdd->Hide(kind == nam_library::FileKind::Module);
    mRemove->Hide(kind == nam_library::FileKind::Module);
    mLoad->SetLabelStr(kind == nam_library::FileKind::RigPreset ? "Load preset" : ("Load into " + GridSlotName(mSlot)).c_str());
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
    View().scroll = std::clamp(View().scroll, 0, std::max(0, static_cast<int>(mResults.size()) - kRows));
    View().folderScroll = std::clamp(View().folderScroll, 0, std::max(0, static_cast<int>(mFolders.size()) - kFolderRows));
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
    if (index >= View().scroll + kRows) View().scroll = index - kRows + 1;
  }
  void LoadSelected()
  {
    FinishSearch();
    const auto* file = Selected();
    if (!file || !nam_library::Allows(mContext, file->kind)) return;
    // Copy before a rig recall can replace the snapshot and refresh controls.
    const auto path = file->path;
    if (file->kind == nam_library::FileKind::Module)
    {
      const auto effect = static_cast<SlotEffect>(file->module);
      const auto current = PLUG()->GetSlotEffect(mSlot);
      if (effect != (current == SlotEffect::TightGate ? SlotEffect::NaturalGate : current)) PLUG()->SetSlotEffect(mSlot, effect);
      Hide(true); return;
    }
    const auto error = file->kind == nam_library::FileKind::RigPreset ? PLUG()->LoadRigPreset(path)
                       : file->kind == nam_library::FileKind::IR ? PLUG()->LoadSlotIR(mSlot, nam_library::PathText(path).c_str())
                       : PLUG()->LoadSlot(mSlot, nam_library::PathText(path).c_str());
    mStatus = error.empty() ? "Loaded " + nam_library::PathText(path.stem()) : error;
    if (error.empty()) Hide(true);
    SetDirty(false);
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
        const auto error = ir ? PLUG()->LoadSlotIR(slot, chosen.Get()) : PLUG()->LoadSlot(slot, chosen.Get());
        mStatus = error.empty() ? "Loaded " + nam_library::PathText(std::filesystem::u8path(chosen.Get()).stem()) : error;
        if (error.empty()) Hide(true);
        SetDirty(false);
      });
  }
  void DrawDetails(IGraphics& g)
  {
    using namespace grid_ui;
    g.DrawText(Text(13.f, muted), "FILE DETAILS", IRECT(744, 342, 956, 370));
    const auto* file = Selected();
    if (!file) return;
    g.DrawMultiLineText(Text(16.f, accent), nam_library::PathText(file->path.stem()).c_str(), IRECT(744, 380, 956, 430));
    if (file->kind == nam_library::FileKind::Module)
    {
      g.DrawMultiLineText(Text(14.f, muted), file->name.c_str(), IRECT(744, 434, 956, 540));
      g.DrawMultiLineText(Text(13.f, muted), "Choose another effect to load its musical defaults. The current effect keeps your edits.", IRECT(744, 554, 956, 622));
      return;
    }
    const auto metadata = file->kind == nam_library::FileKind::NAM
      ? std::string(nam_library::GearName(file->gear)) + (file->inferred ? " (folder / filename hint)" : "")
        + (file->maker.empty() ? "" : "\n" + file->maker) + (file->model.empty() ? "" : " / " + file->model)
      : file->kind == nam_library::FileKind::IR ? "WAV impulse response" : "Complete nam-grid rig";
    g.DrawMultiLineText(Text(14.f, muted), metadata.c_str(), IRECT(744, 434, 956, 492));
    if (!file->name.empty()) FittedText(g, Text(13.f), file->name.c_str(), IRECT(744, 494, 956, 518));
    g.DrawMultiLineText(Text(13.f, muted), nam_library::PathText(file->path).c_str(), IRECT(744, 526, 956, 622));
  }
  static void Scrollbar(IGraphics& g, float x, int offset, int count, int visible)
  {
    if (count <= visible) return;
    g.FillRect(grid_ui::border.WithOpacity(.3f), IRECT(x, 232, x + 7, 632));
    const float height = std::max(24.f, 400.f * visible / count);
    const float y = 232.f + (400.f - height) * offset / (count - visible);
    g.FillRoundRect(grid_ui::muted.WithOpacity(.6f), IRECT(x, y, x + 7, y + height), 3);
  }
  void DragScroll(float y)
  {
    auto& offset = mDrag == 1 ? View().folderScroll : View().scroll;
    const int maximum = std::max(0, static_cast<int>(mDrag == 1 ? mFolders.size() : mResults.size()) - (mDrag == 1 ? kFolderRows : kRows));
    offset = static_cast<int>(std::clamp((y - 232.f) / 400.f, 0.f, 1.f) * maximum); SetDirty(false);
  }
  nam_library::BrowserContext mContext = nam_library::BrowserContext::Library;
  nam_library::FileKind mKind = nam_library::FileKind::NAM;
  nam_library::FileKind mContextKind = nam_library::FileKind::NAM;
  nam_library::FileKind mFolderKind = nam_library::FileKind::SlotPreset;
  static constexpr std::array<nam_library::FileKind, 4> kTabs{nam_library::FileKind::Module, nam_library::FileKind::NAM, nam_library::FileKind::IR, nam_library::FileKind::RigPreset};
  size_t mSlot = 0;
  std::array<State, 5> mViews;
  std::shared_ptr<const nam_library::Snapshot> mLibrary;
  std::shared_ptr<const nam_library::Snapshot> mModules, mCombined, mScanLibrary, mContextLibrary;
  nam_library::Worker mContextWorker;
  std::filesystem::path mContextRoot;
  bool mPendingSelection = false;
  std::vector<size_t> mResults;
  std::vector<nam_library::BrowserFolder> mFolders;
  std::vector<std::string> mRoots;
  std::string mStatus, mLastResultClick;
  bool mRelevance = true;
  int mDrag = 0;
  int mSelectedRow = -1;
  NAMBrowserSearchControl* mSearch = nullptr;
  grid_ui::Button *mLoad = nullptr, *mAdd = nullptr, *mRemove = nullptr, *mSort = nullptr, *mSave = nullptr, *mGear = nullptr, *mFile = nullptr;
  std::array<NAMBrowserTab*, 4> mTabs{};
  IPopupMenu mMenu;
};
