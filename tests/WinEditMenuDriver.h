#pragma once
#ifdef _WIN32
  #include <windows.h>
  #include "../NeuralAmpModeler/GridLayout.h"
  #include <cassert>
  #include <string>
  #include <iostream>

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
inline void StartDeadline()
{
  SetTimer(editor, 0x4e4755, 3000, [](HWND window, UINT, UINT_PTR timer, DWORD) {
    KillTimer(window, timer);
    std::cerr << "Native menu command not delivered (mode " << mode << ")\n";
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
      if (mode == -1)
      {
        command = FindNamedItem(menu);
        if (command != UINT(-1)) root = menu;
      }
      else if (auto destinations = GetSubMenu(menu, mode + 2))
      {
        root = menu;
        command = GetMenuItemID(destinations, 0);
      }
      if (root && command != UINT(-1))
      {
        SetTimer(editor, 0x4e4754, 20, [](HWND window, UINT, UINT_PTR timer, DWORD) {
          KillTimer(window, timer);
          EndMenu();
        });
      }
    }
    if (message.hwnd == editor && message.message == WM_UNINITMENUPOPUP
        && reinterpret_cast<HMENU>(message.wParam) == root && command != UINT(-1))
    {
      delivered = PostMessage(editor, WM_COMMAND, command, 0) != FALSE;
    }
  }
  return CallNextHookEx(nullptr, code, w, l);
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
