#pragma once
#ifdef _WIN32
  #include <windows.h>
  #include <cassert>

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
inline LRESULT CALLBACK Observe(int code, WPARAM w, LPARAM l)
{
  if (code >= 0)
  {
    const auto& message = *reinterpret_cast<CWPSTRUCT*>(l);
    if (message.hwnd == editor && message.message == WM_INITMENUPOPUP && !root)
    {
      const auto menu = reinterpret_cast<HMENU>(message.wParam);
      if (auto destinations = GetSubMenu(menu, mode))
      {
        root = menu;
        command = GetMenuItemID(destinations, 0);
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
inline void SelectFirstDestination(HWND window, int source, int editMode)
{
  editor = window;
  root = nullptr;
  mode = editMode;
  command = UINT(-1);
  delivered = false;
  const auto hook = SetWindowsHookEx(WH_CALLWNDPROC, Observe, nullptr, GetCurrentThreadId());
  assert(hook);
  SendMessage(editor, WM_RBUTTONDOWN, MK_RBUTTON, MAKELPARAM(170 + 220 * source, 160));
  UnhookWindowsHookEx(hook);
  SendMessage(editor, WM_RBUTTONUP, 0, MAKELPARAM(170 + 220 * source, 160));
  assert(delivered);
}
} // namespace win_edit_menu
#endif
