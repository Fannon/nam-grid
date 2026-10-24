#pragma once
#ifdef _WIN32
  #include <windows.h>
  #include "../NeuralAmpModeler/GridLayout.h"
  #include <cassert>
  #include <string>
  #include <iostream>
  #include <span>

// Hidden-host tests cannot use foreground keyboard navigation. Resolve a command
// from the plugin's actual generated Win32 menu, dismiss that menu, then queue the
// same WM_COMMAND consumed by iPlug2's native selection callback. No production
// test hook or global input injection is required.
namespace win_edit_menu
{
inline HWND editor = nullptr;
inline HMENU root = nullptr;
inline int mode = 0;
inline UINT command = UINT(-1);
inline bool delivered = false;
inline std::wstring label;
inline std::span<const char* const> choices;
inline int choice = 0, currentChoice = 0;
inline bool dismissChoice = false;
inline void StartDeadline()
{
  SetTimer(editor, 0x4e4755, 3000, [](HWND window, UINT, UINT_PTR timer, DWORD) {
    KillTimer(window, timer);
    std::cerr << "Native menu command not delivered (mode " << mode << ") ";
    if (mode == -1) std::wcerr << L"label=" << label;
    std::cerr << "\n";
    EndMenu();
  });
}
inline UINT FindNamedItem(HMENU menu)
{
  for (int i = 0; i < GetMenuItemCount(menu); ++i)
  {
    if (const auto sub = GetSubMenu(menu, i))
    {
      const auto found = FindNamedItem(sub);
      if (found != UINT(-1)) return found;
    }
    else
    {
      wchar_t text[512]{}; GetMenuStringW(menu, i, text, 512, MF_BYPOSITION);
      if (label == text) return GetMenuItemID(menu, i);
    }
  }
  return UINT(-1);
}
inline LRESULT CALLBACK Observe(int code, WPARAM w, LPARAM l)
{
  if (code >= 0)
  {
    const auto& message = *reinterpret_cast<CWPSTRUCT*>(l);
    if (message.hwnd == editor && message.message == WM_INITMENUPOPUP && !root)
    {
      const auto menu = reinterpret_cast<HMENU>(message.wParam);
      if (mode == -2)
      {
        assert(GetMenuItemCount(menu) == static_cast<int>(choices.size()));
        for (size_t i = 0; i < choices.size(); ++i)
        {
          char caption[512]{}; GetMenuStringA(menu, static_cast<UINT>(i), caption, 512, MF_BYPOSITION);
          assert(std::string(caption).starts_with(choices[i]));
          assert(((GetMenuState(menu, static_cast<UINT>(i), MF_BYPOSITION) & MF_CHECKED) != 0)
                 == (static_cast<int>(i) == currentChoice));
        }
        root = menu;
        if (!dismissChoice) command = GetMenuItemID(menu, choice);
      }
      else if (mode == -1)
      {
        command = FindNamedItem(menu);
        if (command != UINT(-1)) root = menu;
      }
      else if (auto destinations = GetSubMenu(menu, mode + 2))
      {
        root = menu;
        command = GetMenuItemID(destinations, 0);
      }
      if (root && (command != UINT(-1) || (mode == -2 && dismissChoice)))
      {
        SetTimer(editor, 0x4e4754, 20, [](HWND window, UINT, UINT_PTR timer, DWORD) {
          KillTimer(window, timer);
          EndMenu();
        });
      }
    }
    if (message.hwnd == editor && message.message == WM_UNINITMENUPOPUP
        && reinterpret_cast<HMENU>(message.wParam) == root)
    {
      if (mode == -2 && dismissChoice) delivered = true;
      else if (command != UINT(-1)) delivered = PostMessage(editor, WM_COMMAND, command, 0) != FALSE;
    }
  }
  return CallNextHookEx(nullptr, code, w, l);
}
inline void SelectChoice(HWND window, int x, int y, std::span<const char* const> options,
                         int selected, int current, bool dismiss = false)
{
  editor = window; root = nullptr; mode = -2; choices = options;
  choice = selected; currentChoice = current; dismissChoice = dismiss;
  command = UINT(-1); delivered = false;
  const auto hook = SetWindowsHookEx(WH_CALLWNDPROC, Observe, nullptr, GetCurrentThreadId());
  assert(hook);
  StartDeadline();
  SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(x, y));
  KillTimer(editor, 0x4e4755);
  UnhookWindowsHookEx(hook);
  SendMessage(editor, WM_LBUTTONUP, 0, MAKELPARAM(x, y));
  assert(delivered);
}
inline void SelectNamedItem(HWND window, int x, int y, const wchar_t* name, bool rightClick = false)
{
  editor = window; root = nullptr; mode = -1; label = name;
  command = UINT(-1); delivered = false;
  const auto hook = SetWindowsHookEx(WH_CALLWNDPROC, Observe, nullptr, GetCurrentThreadId());
  assert(hook);
  StartDeadline();
  SendMessage(editor, rightClick ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, rightClick ? MK_RBUTTON : MK_LBUTTON, MAKELPARAM(x, y));
  KillTimer(editor, 0x4e4755);
  UnhookWindowsHookEx(hook);
  SendMessage(editor, rightClick ? WM_RBUTTONUP : WM_LBUTTONUP, 0, MAKELPARAM(x, y));
  assert(delivered);
}
inline void SelectFirstDestination(HWND window, int source, int editMode)
{
  editor = window;
  root = nullptr;
  mode = editMode;
  command = UINT(-1);
  delivered = false;
  const auto hook = SetWindowsHookEx(WH_CALLWNDPROC, Observe, nullptr, GetCurrentThreadId());
  assert(hook);
  StartDeadline();
  SendMessage(editor, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(170 + 220 * source, grid_layout::cardTop + 56));
  KillTimer(editor, 0x4e4755);
  UnhookWindowsHookEx(hook);
  SendMessage(editor, WM_RBUTTONUP, 0, MAKELPARAM(170 + 220 * source, grid_layout::cardTop + 56));
  assert(delivered);
}
} // namespace win_edit_menu
#endif
