#pragma once
#include "LibraryBrowser.h"
#include "ITextEntryControl.h"
#include <cmath>

// Browser star: an outline when unrated, filled yellow, orange or red as it is cycled.
inline void DrawBrowserStar(IGraphics& g, float cx, float cy, float radius, int level)
{
  static const IColor colors[] = {grid_ui::muted, IColor(255, 246, 206, 74),
                                  IColor(255, 244, 148, 56), IColor(255, 232, 82, 74)};
  const int index = level >= 1 && level <= nam_library::kStarLevels ? level : 0;
  g.PathClear();
  for (int point = 0; point < 10; ++point)
  {
    const float angle = -1.5707963f + point * 0.62831853f; // Start at the top point.
    const float r = point % 2 == 0 ? radius : radius * .44f;
    const float x = cx + r * std::cos(angle), y = cy + r * std::sin(angle);
    if (point == 0) g.PathMoveTo(x, y);
    else g.PathLineTo(x, y);
  }
  g.PathClose();
  if (index) g.PathFill(colors[index]);
  else g.PathStroke(colors[0].WithOpacity(.7f), 1.2f);
}

class NAMBrowserTab : public grid_ui::Button
{
public:
  using grid_ui::Button::Button;
  void SetActive(bool active) { if (mActive != active) { mActive = active; SetDirty(false); } }
  bool IsActive() const { return mActive; }
  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(mActive ? grid_ui::accent.WithOpacity(.15f) : grid_ui::surface, mRECT, 3.f);
    g.DrawRoundRect(mActive ? grid_ui::accent : grid_ui::border, mRECT, 3.f);
    g.DrawText(grid_ui::Text(14.f, mActive ? grid_ui::accent : grid_ui::text, EAlign::Center), GetLabelStr(), mRECT);
  }
private:
  bool mActive = false;
};

// The results header's Starred toggle: a star and its label, lit like an active tab when on.
class NAMBrowserStarFilter : public NAMBrowserTab
{
public:
  using NAMBrowserTab::NAMBrowserTab;
  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(IsActive() ? grid_ui::accent.WithOpacity(.15f) : grid_ui::surface, mRECT, 3.f);
    g.DrawRoundRect(IsActive() ? grid_ui::accent : mMouseIsOver ? grid_ui::muted : grid_ui::border, mRECT, 3.f);
    DrawBrowserStar(g, mRECT.L + 15.f, mRECT.MH(), 6.5f, IsActive() ? 1 : 0);
    g.DrawText(grid_ui::Text(14.f, IsActive() ? grid_ui::accent : grid_ui::text, EAlign::Near), GetLabelStr(),
               IRECT(mRECT.L + 27.f, mRECT.T, mRECT.R - 4.f, mRECT.B));
  }
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
  void Focus()
  {
    mFocused = true;
    SetDirty(false);
    GetUI()->ReleaseMouseCapture();
#ifdef _WIN32
    // Bitwig on Windows can keep its own window in the foreground over a plugin window; SetFocus
    // alone then leaves the keyboard with the host, and typed letters trigger host shortcuts.
    // The click or shortcut that starts editing allows this process to raise its window first.
    const auto view = static_cast<HWND>(GetUI()->GetWindow());
    const auto root = GetAncestor(view, GA_ROOT);
    if (GetForegroundWindow() != root) SetForegroundWindow(root);
    SetFocus(view);
#endif
  }
  void Blur() { mFocused = false; SetDirty(false); }
  void BeginEdit()
  {
    Focus();
    // The editor stops short of the clear button, so it stays clickable while typing.
    GetUI()->CreateTextEntry(*this, mText, mRECT.GetHPadded(-12).GetVPadded(-3).GetReducedFromRight(24), GetStr());
  }
  bool StartTyping(const IKeyPress& key)
  {
    if (!mFocused || (key.C && key.VK != 'V')
        || (!key.C && static_cast<unsigned char>(key.utf8[0]) < 32)) return false;
    // Defer the modal text editor until typing begins: it consumes outside clicks,
    // so opening it immediately would swallow the first tab/result/button click.
    BeginEdit();
    GetUI()->GetTextEntryControl()->OnKeyDown(mRECT.MW(), mRECT.MH(), key);
    return true;
  }
  void OnMouseDown(float x, float, const IMouseMod&) override
  {
    if (mLive.empty() || x < ClearButton().L) { BeginEdit(); return; }
    auto* entry = GetUI()->GetTextEntryControl();
    if (entry && entry->EditInProgress() && GetUI()->GetControlInTextEntry() == this) entry->CommitEdit();
    SetQuery(""); mChanged(""); Focus();
  }
  void Draw(IGraphics& g) override
  {
    g.FillRoundRect(grid_ui::surface, mRECT, 4.f);
    auto* entry = GetUI()->GetTextEntryControl();
    const bool editing = entry && entry->EditInProgress() && GetUI()->GetControlInTextEntry() == this;
    g.DrawRoundRect(editing || mFocused ? grid_ui::accent : grid_ui::border, mRECT, 4.f);
    if (!mLive.empty())
    {
      const auto x = ClearButton().MW(), y = ClearButton().MH();
      g.DrawLine(grid_ui::muted, x - 5, y - 5, x + 5, y + 5, nullptr, 1.5f);
      g.DrawLine(grid_ui::muted, x - 5, y + 5, x + 5, y - 5, nullptr, 1.5f);
    }
    if (editing) return;
    grid_ui::FittedText(g, grid_ui::Text(14.f, GetStr()[0] ? grid_ui::text : grid_ui::muted),
                       GetStr()[0] ? GetStr() : "Search...", mRECT.GetHPadded(-12).GetReducedFromRight(24));
    if (mFocused && !GetStr()[0])
      g.DrawLine(grid_ui::text, mRECT.L + 12, mRECT.MH() - 7, mRECT.L + 12, mRECT.MH() + 7);
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
  IRECT ClearButton() const { return IRECT(mRECT.R - 34, mRECT.T, mRECT.R - 4, mRECT.B); }
  std::string mLive;
  bool mFocused = false;
  std::function<void(const std::string&)> mChanged;
};

class NAMLibraryBrowserControl : public IContainerBase
{
public:
  explicit NAMLibraryBrowserControl(const IRECT& r) : IContainerBase(r) {}
  void OnAttached() override
  {
    using namespace grid_ui;
    // The title names the target, so the tabs share its row.
    mTitle = new HintLabel(IRECT(24, 8, 240, 48), "Library browser", Text(24.f),
      "Select to inspect. Double-click or Enter to load. Up/Down and Page Up/Down browse; Ctrl/Cmd+F searches; Escape closes.", Icon::Browse);
    AddChildControl(mTitle);
    mDetailsTitle = new HintLabel(IRECT(744, kPanelTop, 876, kListTop), "DETAILS", Text(13.f, muted),
      "The selected file's metadata and full path.");
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
    // "..." marks the actions that open a file dialog. Save as is a rig action, top right;
    // Open file is the alternative to searching, so it sits beside the search field.
    mSave = new Button(IRECT(grid_layout::width - 224, 14, grid_layout::width - 112, 42), [this](IControl*) { PLUG()->PromptRigPreset(true); }, "Save as...", Icon::Save);
    mSave->SetTooltip("Save the current rig as a new preset file.");
    AddChildControl(mSave);
    mFile = new Button(IRECT(grid_layout::width - 136, 58, grid_layout::width - 24, 92), [this](IControl*) { ChooseFile(); }, "Open file...");
    mFile->SetTooltip("Choose a file outside the library folders.");
    AddChildControl(mFile);
    for (int i = 0; i < 4; ++i)
    {
      mTabs[i] = new NAMBrowserTab(TabBounds(i), [this, i](IControl*) {
        SwitchKind(kTabs[i]);
      }, i == 0 ? "Modules" : i == 1 ? "NAM captures" : i == 2 ? "IR files" : "Presets");
      AddChildControl(mTabs[i]);
    }
    mSearch = new NAMBrowserSearchControl(IRECT(24, 58, grid_layout::width - 24, 92), [this](const std::string& text) {
      if (View().query != text) { View().selected.clear(); View().scroll = 0; }
      View().query = text; Rebuild();
    });
    AddChildControl(mSearch);
    mSearch->SetTooltip("Match all words across names, paths and metadata. Use quoted phrases, -exclude, maker:vox, model:ac30, path:clean or gear:amp. Ctrl/Cmd+F focuses search; the cross clears it.");
    mSort = new Button(IRECT(584, kPanelTop + 2, 706, kListTop - 2), [this](IControl*) {
      mRelevance = !mRelevance; Rebuild();
    }, "Sort: relevance");
    mSort->SetTooltip("Relevance ranks exact and leading filename matches first; Name sorts alphabetically.");
    AddChildControl(mSort);
    mStarred = new NAMBrowserStarFilter(IRECT(478, kPanelTop + 2, 576, kListTop - 2), [this](IControl*) {
      View().starred = !View().starred; View().scroll = 0; Rebuild();
    }, "Starred");
    mStarred->SetTooltip("Show only starred results. Search and folders still apply; the browser opens showing everything.");
    AddChildControl(mStarred);
    mDelete = new Button(IRECT(), [this](IControl*) { DeleteSelectedPreset(); }, "Delete...");
    mDelete->SetTooltip("Delete the selected preset file from disk after confirmation. The current rig stays loaded.");
    AddChildControl(mDelete);
    // Source folders belong to the folder column. They are managed only in Settings,
    // one list per file type; "Manage folders" opens that page rather than a dialog.
    mFoldersButton = new Button(IRECT(), [this](IControl*) {
      Hide(true);
      GetUI()->GetControlWithTag(kCtrlTagSettingsBox)->Hide(false);
    }, "Manage folders");
    mFoldersButton->SetTooltip("Add or remove NAM, IR and preset folders in Settings.");
    AddChildControl(mFoldersButton);
    mRescan = new Button(IRECT(), [this](IControl*) {
      PLUG()->RescanCaptureFolder();
      if (!mContextRoot.empty()) RequestContextScan();
    }, "Rescan");
    mRescan->SetTooltip("Index changes made outside NAM Grid in every library folder.");
    AddChildControl(mRescan);
    // Dialog convention: the secondary action left of the primary one, bottom right.
    mCancel = new Button(IRECT(), [this](IControl*) { Hide(true); }, "Cancel");
    AddChildControl(mCancel);
    // The same action top right, where every full page has its Close.
    mTopCancel = new Button(IRECT(grid_layout::width - 104, 14, grid_layout::width - 24, 42), [this](IControl*) { Hide(true); }, "Cancel", Icon::Close);
    AddChildControl(mTopCancel);
    mApply = new Button(IRECT(), [this](IControl*) { LoadSelected(); }, "Apply");
    AddChildControl(mApply);
  }

  void Open(nam_library::BrowserContext context, nam_library::FileKind kind, size_t slot)
  {
    Hide(true);
    mPreviewSelection.clear();
    PLUG()->ReloadLibraryStars(); // Another instance or a synced copy may have changed favorites.json.
    mContext = context;
    mContextKind = kind;
    mSlot = context == nam_library::BrowserContext::Library ? PLUG()->GetSelectedSlot() : slot;
    mPreview = context != nam_library::BrowserContext::Rig && kind != nam_library::FileKind::RigPreset;
    mTitle->SetTooltip(mPreview
      ? "Select to audition; adjust parameters below. Double-click or Enter keeps the module and edits. Close or Escape restores the original. Up/Down browses; click a star to mark a module, capture or IR, and Starred to list only those; Ctrl/Cmd+F searches."
      : "Select to inspect. Double-click or Enter to load. Up/Down and Page Up/Down browse; click a star to mark a preset, and Starred to list only those; Ctrl/Cmd+F searches; Escape closes.");
    SetTargetAndDrawRECTs(IRECT(0, 0, grid_layout::width, mPreview ? grid_layout::editorTop : grid_layout::height));
    if (mPreview) mStatus = PLUG()->BeginSlotPreview(mSlot);
    mContextWorker.Request(std::vector<std::string>{});
    mContextLibrary.reset(); mContextRoot.clear(); mPendingSelection = false; mScanLibrary.reset();
    SwitchKind(kind);
    auto& view = View();
    view.selected.clear();
    view.query.clear(); view.folder.clear(); view.scroll = view.folderScroll = 0; view.starred = false;
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
    mSearch->Focus();
  }
  void Hide(bool hide) override
  {
    if (hide) { FinishSearch(); if (mSearch) mSearch->Blur(); PLUG()->EndSlotPreview(false); mPreview = false; }
    IContainerBase::Hide(hide);
    // This composite page and the live text editor share a single redraw region.
    if (GetUI()) GetUI()->SetStrictDrawing(!hide);
    if (!hide) Layout();
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
      Disabled(mDelete, !Selected() || Selected()->kind != nam_library::FileKind::RigPreset);
      Disabled(mApply, !Selected());
    }
    return IContainerBase::IsDirty();
  }
  void FocusCurrent() { Open(mContext, mKind, mSlot); }

  void Draw(IGraphics& g) override
  {
    using namespace grid_ui;
    g.FillRect(background, mRECT);
    for (int i = 0; i < 4; ++i)
      if (mKind == kTabs[i] && !mTabs[i]->IsHidden())
      { const auto r = mTabs[i]->GetRECT(); g.FillRect(accent, IRECT(r.L, r.B + 3, r.R, r.B + 5)); }
    g.FillRoundRect(surface.WithOpacity(.5f), IRECT(24, kPanelTop, 266, ListBottom()), 4);
    g.FillRoundRect(surface.WithOpacity(.5f), IRECT(282, kPanelTop, 716, ListBottom()), 4);
    g.FillRoundRect(surface.WithOpacity(.5f), IRECT(732, kPanelTop, grid_layout::width - 24, ListBottom()), 4);
    const char* folders = mKind == nam_library::FileKind::Module ? "FX CATEGORIES"
                        : mKind == nam_library::FileKind::RigPreset ? "PRESET FOLDERS"
                        : mKind == nam_library::FileKind::IR ? "IR FOLDERS" : "NAM FOLDERS";
    g.DrawText(Text(13.f, muted), folders, IRECT(36, kPanelTop, 256, kListTop));
    const auto count = std::to_string(mResults.size()) + (mResults.size() == 1 ? " RESULT" : " RESULTS");
    g.DrawText(Text(13.f, muted), PLUG()->IsLibraryScanning() ? "SCANNING..." : count.c_str(), IRECT(294, kPanelTop, 470, kListTop));
    if (HasSources()) g.DrawLine(border.WithOpacity(.6f), 36, FolderBottom() + 2, 254, FolderBottom() + 2);
    for (int row = 0; row < FolderRows() && View().folderScroll + row < mFolders.size(); ++row)
    {
      const auto& folder = mFolders[View().folderScroll + row];
      const float y = kListTop + row * kRowHeight;
      const bool selected = nam_library::SamePath(folder.path, View().folder);
      if (selected) g.FillRect(accent.WithOpacity(.14f), IRECT(26, y, 257, y + kRowHeight));
      FittedText(g, Text(14.f, selected ? accent : text), folder.label.c_str(),
                 IRECT(36.f + std::min(folder.depth, 7) * 12, y, 250, y + kRowHeight));
    }
    for (int row = 0; row < Rows() && View().scroll + row < mResults.size(); ++row)
    {
      const auto& file = mLibrary->captures[mResults[View().scroll + row]];
      const float y = kListTop + row * kRowHeight;
      const bool selected = View().scroll + row == mSelectedRow;
      if (selected) g.FillRect(accent.WithOpacity(.14f), IRECT(284, y, 706, y + kRowHeight));
      const bool module = file.kind == nam_library::FileKind::Module;
      // Stars mark results for this user only, never for a rig or host. Every row keeps the
      // star column so names align, even a file the scan has not hashed yet.
      const auto key = nam_library::StarKey(file, mFavoritesFolder);
      if (!key.empty()) DrawBrowserStar(g, kStarCenter, y + kRowHeight * .5f, 8.f, PLUG()->GetLibraryStar(key));
      FittedText(g, Text(14.f, selected ? accent : text), nam_library::PathText(file.path.stem()).c_str(), IRECT(318.f, y, module ? 586 : 696, y + kRowHeight));
      if (module) FittedText(g, Text(12.f, muted, EAlign::Far), nam_library::PathText(file.relative.parent_path()).c_str(), IRECT(594, y, 696, y + kRowHeight));
    }
    Scrollbar(g, 257, FolderBottom(), View().folderScroll, mFolders.size(), FolderRows());
    Scrollbar(g, 707, ListBottom(), View().scroll, mResults.size(), Rows());
    DrawDetails(g);
    if (mResults.empty())
    {
      const char* message = PLUG()->IsLibraryScanning() ? "Scanning..."
        : View().starred ? "No starred matches.\nClick the star beside a result to mark it."
        : !mRoots.empty() ? "No matches"
        : mKind == nam_library::FileKind::RigPreset ? "No preset folders yet.\nAdd one with Manage folders."
        : mKind == nam_library::FileKind::IR ? "No IR folders yet.\nAdd one with Manage folders."
        : "No NAM folders yet.\nAdd one with Manage folders.";
      DrawClippedText(g, Text(14.f, muted), message, IRECT(314, kListTop + 60, 684, kListTop + 180));
    }
    // Errors and load results sit just above the action they concern.
    if (!mStatus.empty())
      DrawClippedText(g, Text(12.f, IColor(255, 238, 147, 122)).WithVAlign(EVAlign::Bottom), mStatus.c_str(),
                          IRECT(744, FooterTop() - 44, 964, FooterTop() - 6));
  }

  void OnMouseDown(float x, float y, const IMouseMod&) override
  {
    if (y < kListTop || y >= ListBottom()) return;
    const bool folderColumn = x >= 24 && x <= 266 && y < FolderBottom();
    if (folderColumn && x >= 257) { mDrag = 1; DragScroll(y); return; }
    if (x >= 707 && x <= 716) { mDrag = 2; DragScroll(y); return; }
    if (folderColumn)
    {
      const int index = View().folderScroll + static_cast<int>((y - kListTop) / kRowHeight);
      if (index < mFolders.size()) { View().folder = mFolders[index].path; View().scroll = 0; Rebuild(); }
    }
    else if (x >= 282 && x < 707)
    {
      const int index = View().scroll + static_cast<int>((y - kListTop) / kRowHeight);
      if (index < mResults.size())
      {
        const auto& file = mLibrary->captures[mResults[index]];
        // The star is its own target: marking a row keeps the selection and the audition.
        if (x >= kStarLeft && x < kStarRight) { CycleStar(file); return; }
        mSelectedRow = index; View().selected = nam_library::PathText(file.path); mStatus.clear(); SetDirty(false);
      }
    }
  }
  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    // iPlug2 reports a fast second click as a double-click instead of a mouse-down, so a star
    // cycles once more here; it must never also load the row.
    if (y >= kListTop && y < ListBottom() && x >= kStarLeft && x < kStarRight) { OnMouseDown(x, y, mod); return; }
    OnMouseDown(x, y, mod);
    const int row = View().scroll + static_cast<int>((y - kListTop) / kRowHeight);
    if (x >= 282 && x < 707 && y >= kListTop && y < ListBottom() && row < mResults.size()) LoadSelected();
  }
  void OnMouseDrag(float, float y, float, float, const IMouseMod&) override { if (mDrag) DragScroll(y); }
  void OnMouseUp(float, float, const IMouseMod&) override { mDrag = 0; }
  void OnMouseWheel(float x, float y, const IMouseMod&, float delta) override
  {
    if (y < kListTop || y >= ListBottom()) return;
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
    else mSearch->StartTyping(key);
    return true; // Modal page: never pass keys to controls beneath it.
  }

private:
  // Panels share one header row; lists start below it. Footer buttons pin to the panel bottom,
  // which moves with the page height (audition above the editor, or full-height presets).
  static constexpr float kRowHeight = 25.f, kPanelTop = 104.f, kListTop = 132.f;
  // The star sits in the row's left margin, clear of the name text.
  static constexpr float kStarCenter = 302.f, kStarLeft = 288.f, kStarRight = 316.f;
  int Rows() const { return static_cast<int>((ListBottom() - kListTop) / kRowHeight); }
  int FolderRows() const { return static_cast<int>((FolderBottom() - kListTop) / kRowHeight); }
  float ListBottom() const { return mRECT.B - 12.f; }
  float FooterTop() const { return ListBottom() - 40.f; }
  bool HasSources() const { return mKind != nam_library::FileKind::Module; }
  float FolderBottom() const { return HasSources() ? FooterTop() - 6.f : ListBottom(); }
  static IRECT TabBounds(int position) { return IRECT(256.f + position * 124, 14, 372.f + position * 124, 42); }
  void Layout()
  {
    const bool preset = mKind == nam_library::FileKind::RigPreset;
    if (preset) mTitle->SetStr("Rig presets");
    else
    {
      auto slot = GridSlotName(mSlot);
      slot.erase(std::remove(slot.begin(), slot.end(), ' '), slot.end()); // Matches the tile ID.
      mTitle->SetStr(("Browse for " + slot).c_str());
    }
    int allowed = 0;
    for (int i = 0; i < 4; ++i) allowed += nam_library::Allows(mContext, kTabs[i]);
    int position = 0;
    for (int i = 0; i < 4; ++i)
    {
      // A single tab is not a choice: the title already says what is listed.
      const bool shown = allowed > 1 && nam_library::Allows(mContext, kTabs[i]);
      mTabs[i]->Hide(!shown);
      if (shown) mTabs[i]->SetTargetAndDrawRECTs(TabBounds(position++));
    }
    mSave->Hide(!preset);
    mFile->Hide(!HasSources());
    mDelete->Hide(!preset);
    mFoldersButton->Hide(!HasSources());
    mRescan->Hide(!HasSources());
    const float top = FooterTop() + 6.f, bottom = ListBottom() - 6.f;
    mFoldersButton->SetTargetAndDrawRECTs(IRECT(32, top, 170, bottom));
    mRescan->SetTargetAndDrawRECTs(IRECT(178, top, 258, bottom));
    mSearch->SetTargetAndDrawRECTs(IRECT(24, 58, HasSources() ? grid_layout::width - 144 : grid_layout::width - 24, 92));
    // Presets add Delete to the footer actions, left of Close and Load.
    mDelete->SetTargetAndDrawRECTs(IRECT(740, top, 810, bottom));
    mCancel->SetTargetAndDrawRECTs(preset ? IRECT(818, top, grid_layout::width - 112, bottom) : IRECT(740, top, grid_layout::width - 152, bottom));
    mApply->SetTargetAndDrawRECTs(preset ? IRECT(grid_layout::width - 104, top, grid_layout::width - 32, bottom) : IRECT(grid_layout::width - 144, top, grid_layout::width - 32, bottom));
  }
  struct State { std::string query, selected; std::filesystem::path folder; int scroll = 0, folderScroll = 0; bool starred = false; };
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
      ? "Select to audition; adjust parameters below. Double-click or Enter keeps the module and edits. Close or Escape restores the original. Up/Down browses; click a star to mark a module, capture or IR, and Starred to list only those; Ctrl/Cmd+F searches."
      : "Select to inspect. Double-click or Enter to load. Up/Down and Page Up/Down browse; click a star to mark a preset, and Starred to list only those; Ctrl/Cmd+F searches; Escape closes.");
    for (int i = 0; i < 4; ++i) mTabs[i]->SetActive(kind == kTabs[i]);
    mSearch->SetQuery(View().query);
    if (!IsHidden()) { Layout(); GetUI()->SetAllControlsDirty(); }
    Rebuild();
    if (!IsHidden()) mSearch->Focus();
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
    mResults = nam_library::Browse(*mLibrary, mKind, View().query, View().folder, -1, mRelevance);
    // A star cleared under the filter keeps its row until the next rebuild, so it can be cycled on.
    mFavoritesFolder = PLUG()->GetFavoritesFolder();
    if (View().starred)
      mResults = nam_library::KeepStarred(*mLibrary, std::move(mResults), PLUG()->GetLibraryStars(), mFavoritesFolder);
    mStarred->SetActive(View().starred);
    mSelectedRow = -1;
    const auto selected = std::filesystem::u8path(View().selected);
    for (int i = 0; i < mResults.size(); ++i)
      if (nam_library::SamePath(mLibrary->captures[mResults[i]].path, selected)) { mSelectedRow = i; break; }
    if (mSelectedRow < 0 && !mResults.empty() && !mPendingSelection)
    { mSelectedRow = 0; View().selected = nam_library::PathText(mLibrary->captures[mResults.front()].path); }
    View().scroll = std::clamp(View().scroll, 0, std::max(0, static_cast<int>(mResults.size()) - Rows()));
    View().folderScroll = std::clamp(View().folderScroll, 0, std::max(0, static_cast<int>(mFolders.size()) - FolderRows()));
    mSort->SetLabelStr(mRelevance ? "Sort: relevance" : "Sort: name");
    const bool preset = mKind == nam_library::FileKind::RigPreset;
    mApply->SetLabelStr(preset ? "Load" : "Apply");
    mApply->SetTooltip(preset ? "Load the selected rig preset (Enter or double-click)."
                              : "Keep the auditioned selection in this slot (Enter or double-click). Cancel restores the original.");
    for (auto* cancel : {mCancel, mTopCancel})
    {
      cancel->SetLabelStr(mPreview ? "Cancel" : "Close");
      cancel->SetTooltip(mPreview ? "Restore the original module and parameters, then close (Escape)." : "Close the browser (Escape).");
    }
    RevealSelection();
    SetDirty(false);
  }
  int SelectedRow() const { return mSelectedRow; }
  const nam_library::Capture* Selected()
  { const int index = SelectedRow(); return index < 0 ? nullptr : &mLibrary->captures[mResults[index]]; }
  void CycleStar(const nam_library::Capture& file)
  {
    const auto key = nam_library::StarKey(file, mFavoritesFolder);
    if (key.empty()) return;
    // A module's path ends in its name; files keep theirs. Either way the name is only a hint.
    if (!PLUG()->CycleLibraryStar(key, nam_library::PathText(file.path.filename())))
      mStatus = "Unable to save favorites.json in your preset folder (damaged or read-only?); this star lasts until NAM Grid closes.";
    SetDirty(false);
  }
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
  static void DrawClippedText(IGraphics& g, const IText& text, const char* content, const IRECT& bounds)
  {
    // NanoVG wraps to the width but does not enforce the requested height.
    // Restore the control's original drawing clip after each text block.
    g.PathClipRegion(bounds);
    g.DrawMultiLineText(text, content, bounds);
    g.PathClipRegion();
  }
  void DrawDetails(IGraphics& g)
  {
    using namespace grid_ui;
    const auto* file = Selected();
    if (!file) return;
    // Content flows from the header down; the actions stay pinned to the footer.
    const float bottom = FooterTop() - (mStatus.empty() ? 6.f : 48.f);
    if (file->kind == nam_library::FileKind::Module)
    {
      FittedText(g, Text(16.f, accent), nam_library::PathText(file->path.stem()).c_str(), IRECT(744, kListTop, 964, kListTop + 26));
      const auto body = file->name + "\n\n" + ModuleParameters(static_cast<SlotEffect>(file->module));
      DrawClippedText(g, Text(14.f, muted).WithVAlign(EVAlign::Top), body.c_str(), IRECT(744, kListTop + 34, 964, bottom));
      return;
    }
    // Multiline drawing does not clip to its height. Bound the title to two
    // lines so long capture filenames cannot run into the metadata below.
    TwoLineText(g, Text(16.f, accent), nam_library::PathText(file->path.stem()), IRECT(744, kListTop + 4, 964, kListTop + 46));
    const auto metadata = file->kind == nam_library::FileKind::NAM
      ? std::string(nam_library::GearName(file->gear))
        + (file->maker.empty() ? "" : "\n" + file->maker) + (file->model.empty() ? "" : " / " + file->model)
      : file->kind == nam_library::FileKind::IR ? "WAV impulse response" : "Complete NAM Grid rig";
    const auto body = metadata + (file->name.empty() ? "" : "\n" + file->name) + "\n\n" + nam_library::PathText(file->path);
    DrawClippedText(g, Text(13.f, muted).WithVAlign(EVAlign::Top), body.c_str(), IRECT(744, kListTop + 52, 964, bottom));
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
  void Scrollbar(IGraphics& g, float x, float bottom, int offset, int count, int visible)
  {
    if (count <= visible) return;
    g.FillRect(grid_ui::border.WithOpacity(.3f), IRECT(x, kListTop, x + 7, bottom));
    const float height = std::max(24.f, (bottom - kListTop) * visible / count);
    const float y = kListTop + ((bottom - kListTop) - height) * offset / (count - visible);
    g.FillRoundRect(grid_ui::muted.WithOpacity(.6f), IRECT(x, y, x + 7, y + height), 3);
  }
  void DragScroll(float y)
  {
    auto& offset = mDrag == 1 ? View().folderScroll : View().scroll;
    const int maximum = std::max(0, static_cast<int>(mDrag == 1 ? mFolders.size() : mResults.size()) - (mDrag == 1 ? FolderRows() : Rows()));
    const float bottom = mDrag == 1 ? FolderBottom() : ListBottom();
    offset = static_cast<int>(std::clamp((y - kListTop) / (bottom - kListTop), 0.f, 1.f) * maximum); SetDirty(false);
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
  NAMBrowserStarFilter* mStarred = nullptr;
  std::filesystem::path mFavoritesFolder; // Rig preset star keys are relative to it.
  grid_ui::Button *mFoldersButton = nullptr, *mRescan = nullptr, *mDelete = nullptr, *mSort = nullptr, *mSave = nullptr,
                  *mCancel = nullptr, *mTopCancel = nullptr, *mApply = nullptr, *mFile = nullptr;
  std::array<NAMBrowserTab*, 4> mTabs{};
};
