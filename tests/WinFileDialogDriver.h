#pragma once
#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <cassert>
#include <fstream>
#include <vector>

// Feed the actual common file dialog on this test thread. No foreground keys,
// desktop-wide input injection, or production test interface is involved.
namespace win_file_dialog
{
// Points the plugin's settings and default folders at a private, empty home before the plugin
// loads, so tests never read or write the user's real configuration, even if they abort.
struct IsolatedUserHome
{
  std::filesystem::path home, file;
  explicit IsolatedUserHome(const std::string& name)
  : home(std::filesystem::temp_directory_path() / (name + "-" + std::to_string(GetCurrentProcessId())))
  , file(home / "config" / "settings.json")
  {
    std::error_code error;
    std::filesystem::remove_all(home, error);
    std::filesystem::create_directories(home);
    assert(SetEnvironmentVariableW(L"NAM_GRID_HOME", home.c_str()));
  }
  ~IsolatedUserHome()
  {
    std::error_code error;
    std::filesystem::remove_all(home, error);
  }
};
inline std::wstring selected;
inline bool delivered = false, cancel = false, directory = false;
inline unsigned attempts = 0;
inline HWND FilenameEdit(HWND dialog)
{
  HWND result = nullptr;
  EnumChildWindows(dialog, [](HWND child, LPARAM data) -> BOOL {
    wchar_t type[32]{}; GetClassNameW(child, type, 32);
    const int id = GetDlgCtrlID(child);
    // Filename edit IDs vary between Open, Save and Windows versions. Ignore
    // the address-bar edit and use the dialog's remaining edit control.
    if (lstrcmpW(type, L"Edit") == 0 && id != 0xa205)
    { *reinterpret_cast<HWND*>(data) = child; return FALSE; }
    return TRUE;
  }, reinterpret_cast<LPARAM>(&result));
  return result;
}
inline LRESULT CALLBACK Observe(int code, WPARAM w, LPARAM l)
{
  if (code == HCBT_ACTIVATE)
  {
    const auto dialog = reinterpret_cast<HWND>(w);
    wchar_t type[32]{};
    GetClassNameW(dialog, type, 32);
    if (std::wstring(type) == L"#32770" && GetDlgItem(dialog, IDOK))
    {
      SetWindowPos(dialog, nullptr, -32000, -32000, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      wchar_t title[128]{}; GetWindowTextW(dialog, title, 128);
      if (std::wstring(title) == L"Rig preset")
        EnumChildWindows(dialog, [](HWND child, LPARAM) -> BOOL {
          wchar_t kind[32]{}, text[2048]{};
          GetClassNameW(child, kind, 32); GetWindowTextW(child, text, 2048);
          if (lstrcmpW(kind, L"Static") == 0 && text[0]) std::wcerr << L"Preset error: " << text << L"\n";
          return TRUE;
        }, 0);
      attempts = 0;
      SetTimer(dialog, 0x4e4750, 100, [](HWND window, UINT, UINT_PTR timer, DWORD) {
        const auto edit = FilenameEdit(window);
        if (cancel || ++attempts >= 50)
        { KillTimer(window, timer); PostMessageW(window, WM_COMMAND, IDCANCEL, 0); }
        else if (directory)
        {
          KillTimer(window, timer);
          SendMessageW(window, BFFM_SETSELECTIONW, TRUE, reinterpret_cast<LPARAM>(selected.c_str()));
          delivered = true;
          PostMessageW(GetDlgItem(window, IDOK), BM_CLICK, 0, 0);
        }
        else if (edit)
        {
          KillTimer(window, timer);
          delivered = SendMessageW(edit, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(selected.c_str())) != 0;
          SendMessageW(GetParent(edit), WM_COMMAND, MAKEWPARAM(GetDlgCtrlID(edit), EN_CHANGE), reinterpret_cast<LPARAM>(edit));
          PostMessageW(GetDlgItem(window, IDOK), BM_CLICK, 0, 0);
        }
        else
          return; // Allow the dialog to finish creating its filename control.
      });
    }
  }
  return CallNextHookEx(nullptr, code, w, l);
}
template <class Click>
void Choose(const std::filesystem::path& path, Click click, bool dismiss = false, bool folder = false)
{
  auto nativePath = path;
  selected = nativePath.make_preferred().wstring(); delivered = false; cancel = dismiss; directory = folder;
  const auto hook = SetWindowsHookExW(WH_CBT, Observe, nullptr, GetCurrentThreadId());
  assert(hook);
  click();
  UnhookWindowsHookEx(hook);
  assert(dismiss || delivered);
}

// Answer the next native Yes/No message box raised by click().
inline bool answerYes = false, answered = false;
inline LRESULT CALLBACK Answer(int code, WPARAM w, LPARAM l)
{
  if (code == HCBT_ACTIVATE)
  {
    const auto box = reinterpret_cast<HWND>(w);
    wchar_t type[32]{};
    GetClassNameW(box, type, 32);
    if (std::wstring(type) == L"#32770" && GetDlgItem(box, IDYES))
    {
      SetWindowPos(box, nullptr, -32000, -32000, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
      answered = true;
      PostMessageW(box, WM_COMMAND, answerYes ? IDYES : IDNO, 0);
    }
  }
  return CallNextHookEx(nullptr, code, w, l);
}
template <class Click>
void Confirm(bool yes, Click click)
{
  answerYes = yes; answered = false;
  const auto hook = SetWindowsHookExW(WH_CBT, Answer, nullptr, GetCurrentThreadId());
  assert(hook);
  click();
  UnhookWindowsHookEx(hook);
  assert(answered);
}
}
#endif
