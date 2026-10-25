// Native Windows editor checks for both shipped formats. Uses the actual monitor
// DPI; --scale fails if the requested display scale is not active (no fake zoom).
#include "ClapTestHost.h"
#include "IsolatedUserHome.h"
#include "WinEditorCapture.h"
#include "pluginterfaces/base/ipluginbase.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/gui/iplugviewcontentscalesupport.h"
#include "pluginterfaces/gui/iplugview.h"
#include <commctrl.h>
#include <functional>
#include <thread>

struct EditorHost : Host
{
  HWND parent = nullptr;
  EditorHost()
  {
    host.get_extension = [](const clap_host* h, const char* id) -> const void* {
      static const clap_host_gui gui{
        [](const clap_host*) {},
        [](const clap_host* h, uint32_t width, uint32_t height) {
          auto* self = static_cast<EditorHost*>(static_cast<Host*>(h->host_data));
          return SetWindowPos(self->parent, nullptr, 0, 0, width, height, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER) != 0;
        },
        [](const clap_host*) { return true; }, [](const clap_host*) { return true; },
        [](const clap_host*, bool) {}};
      return std::strcmp(id, CLAP_EXT_GUI) == 0 ? &gui : Host::extension(h, id);
    };
  }
};

// The view reports DPI changes through this host-owned frame. Keep its lifetime
// outside the view, which retains it until setFrame(nullptr).
struct EditorFrame : Steinberg::IPlugFrame
{
  HWND parent = nullptr;
  Steinberg::uint32 refs = 1;
  Steinberg::tresult PLUGIN_API queryInterface(const Steinberg::TUID id, void** object) override
  {
    using namespace Steinberg;
    if (!std::memcmp(id, INLINE_UID_OF(FUnknown), sizeof(TUID))
        || !std::memcmp(id, INLINE_UID_OF(IPlugFrame), sizeof(TUID)))
    { *object = static_cast<IPlugFrame*>(this); addRef(); return kResultOk; }
    *object = nullptr; return kNoInterface;
  }
  Steinberg::uint32 PLUGIN_API addRef() override { return ++refs; }
  Steinberg::uint32 PLUGIN_API release() override { return --refs; }
  Steinberg::tresult PLUGIN_API resizeView(Steinberg::IPlugView* view, Steinberg::ViewRect* size) override
  {
    assert(SetWindowPos(parent, nullptr, 0, 0, size->getWidth(), size->getHeight(), SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER));
    // This plugin has a fixed logical layout; onSize can reject user resizing.
    view->onSize(size);
    return Steinberg::kResultOk;
  }
};

static void PumpFor(int ms)
{
  const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
  do { pump(); std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
  while (std::chrono::steady_clock::now() < until);
}

static HWND TooltipFor(HWND editor)
{
  HWND tooltip = nullptr;
  std::pair<HWND, HWND*> match{editor, &tooltip};
  EnumThreadWindows(GetCurrentThreadId(), [](HWND window, LPARAM data) -> BOOL {
    const auto& match = *reinterpret_cast<std::pair<HWND, HWND*>*>(data);
    wchar_t kind[64]{}; GetClassNameW(window, kind, 64);
    if (lstrcmpW(kind, TOOLTIPS_CLASSW)) return TRUE;
    TOOLINFOW info{}; info.cbSize = TTTOOLINFOW_V2_SIZE;
    info.hwnd = match.first; info.uId = reinterpret_cast<UINT_PTR>(match.first);
    if (!SendMessageW(window, TTM_GETTOOLINFOW, 0, reinterpret_cast<LPARAM>(&info))) return TRUE;
    *match.second = window;
    return FALSE;
  }, reinterpret_cast<LPARAM>(&match));
  assert(tooltip);
  return tooltip;
}

static void CheckEditor(HWND parent, double scale, const std::function<double()>& inputDB,
                        const std::filesystem::path& screenshot)
{
  auto editor = GetWindow(parent, GW_CHILD); assert(editor);
  PumpFor(200);
  RECT client{}; assert(GetClientRect(editor, &client));
  assert(std::abs(client.right - 1020 * scale) <= 1);
  assert(std::abs(client.bottom - 720 * scale) <= 1);
  const auto xy = [&](double x, double y) { return MAKELPARAM(std::lround(x * scale), std::lround(y * scale)); };
  const auto clickValue = [&] {
    SendMessage(editor, WM_LBUTTONDOWN, MK_LBUTTON, xy(37, 128));
    SendMessage(editor, WM_LBUTTONUP, 0, xy(37, 128));
    PumpFor(40);
  };
  const auto key = [&](int vk, BYTE modifiers = 0) {
    BYTE previous[256]{}, neutral[256]{};
    assert(GetKeyboardState(previous));
    neutral[VK_SHIFT] = modifiers & 1 ? 0x80 : 0;
    neutral[VK_CONTROL] = modifiers & 2 ? 0x80 : 0;
    neutral[VK_MENU] = modifiers & 4 ? 0x80 : 0;
    assert(SetKeyboardState(neutral));
    SendMessage(editor, WM_KEYDOWN, vk, static_cast<LPARAM>(MapVirtualKey(vk, MAPVK_VK_TO_VSC)) << 16);
    SendMessage(editor, WM_KEYUP, vk, 0);
    assert(SetKeyboardState(previous));
    PumpFor(20);
  };
  clickValue();
  for (const wchar_t c : std::wstring(L"-3.5"))
  {
    const auto mapped = VkKeyScanW(c); assert(mapped != -1);
    key(LOBYTE(mapped), HIBYTE(mapped));
  }
  key(VK_RETURN);
  assert(std::abs(inputDB() + 3.5) < .01);
  clickValue(); key('9'); key(VK_ESCAPE);
  assert(std::abs(inputDB() + 3.5) < .01); // Escape cancels the edit.

  InvalidateRect(editor, nullptr, FALSE); UpdateWindow(editor);
  SendMessage(editor, WM_MOUSEMOVE, 0, xy(37, 100));
  SendMessage(editor, WM_MOUSEHOVER, 0, 0);
  std::array<wchar_t, 2048> text{};
  TOOLINFOW info{}; info.cbSize = TTTOOLINFOW_V2_SIZE;
  info.hwnd = editor; info.uId = reinterpret_cast<UINT_PTR>(editor); info.lpszText = text.data();
  SendMessageW(TooltipFor(editor), TTM_GETTEXTW, text.size(), reinterpret_cast<LPARAM>(&info));
  assert(std::wstring(text.data()).find(L"Input gain before every grid module") != std::wstring::npos);
  SendMessage(editor, WM_MOUSELEAVE, 0, 0);
  if (!screenshot.empty()) CaptureEditor(editor, screenshot);
}

static HWND Parent()
{
  auto parent = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"NAM Grid editor check",
    WS_POPUP, 0, 0, 2200, 1500, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
  assert(parent);
  ShowWindow(parent, SW_SHOWNOACTIVATE);
  return parent;
}

static void CheckCLAP(const char* file, double scale, const std::filesystem::path& out)
{
  const auto path = std::filesystem::absolute(file);
  auto library = LoadLibraryW(path.c_str()); assert(library);
  auto* entry = reinterpret_cast<const clap_plugin_entry*>(GetProcAddress(library, "clap_entry"));
  assert(entry && entry->init(path.string().c_str()));
  auto* factory = static_cast<const clap_plugin_factory*>(entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  EditorHost host;
  auto* plugin = factory->create_plugin(factory, &host.host, factory->get_plugin_descriptor(factory, 0)->id);
  assert(plugin && plugin->init(plugin));
  auto* gui = static_cast<const clap_plugin_gui*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
  auto* params = static_cast<const clap_plugin_params*>(plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  clap_id input = CLAP_INVALID_ID;
  for (uint32_t i = 0; i < params->count(plugin); ++i)
  {
    clap_param_info info{}; assert(params->get_info(plugin, i, &info));
    if (std::string(info.name) == "Input") input = info.id;
  }
  assert(input != CLAP_INVALID_ID);
  const auto inputDB = [&] {
    double value; char text[128]{};
    assert(params->get_value(plugin, input, &value));
    assert(params->value_to_text(plugin, input, value, text, sizeof(text)));
    return std::stod(text);
  };
  for (int pass = 0; pass < 2; ++pass)
  {
    std::cout << "CLAP editor pass " << pass << ": create\n";
    assert(gui->create(plugin, CLAP_WINDOW_API_WIN32, false));
    auto parent = Parent();
    host.parent = parent;
    clap_window window{}; window.api = CLAP_WINDOW_API_WIN32; window.win32 = parent;
    assert(gui->set_parent(plugin, &window));
    std::cout << "CLAP editor attached; set scale\n";
    assert(gui->set_scale(plugin, scale));
    // The pinned iPlug2 backend opens its child during set_parent().
    assert(IsWindowVisible(GetWindow(parent, GW_CHILD)));
    std::cout << "CLAP editor shown; check controls\n";
    if (pass) assert(std::abs(inputDB() + 3.5) < .01);
    CheckEditor(parent, scale, inputDB, out.empty() ? out : out / ("clap-" + std::to_string(pass) + ".bmp"));
    gui->destroy(plugin); DestroyWindow(parent);
  }
  plugin->destroy(plugin); entry->deinit(); FreeLibrary(library);
  std::cout << "CLAP: numeric entry, Escape, tooltip, scaled bounds and reopening passed\n";
}

static void CheckVST3(const char* file, double scale, const std::filesystem::path& out)
{
  using namespace Steinberg;
  using namespace Steinberg::Vst;
  auto library = LoadLibraryW(std::filesystem::absolute(file).c_str()); assert(library);
  auto init = reinterpret_cast<bool (*)()>(GetProcAddress(library, "InitDll"));
  auto exit = reinterpret_cast<bool (*)()>(GetProcAddress(library, "ExitDll"));
  if (init) assert(init());
  auto getFactory = reinterpret_cast<GetFactoryProc>(GetProcAddress(library, "GetPluginFactory")); assert(getFactory);
  auto* factory = getFactory(); assert(factory);
  IEditController* controller = nullptr;
  for (int i = 0; i < factory->countClasses(); ++i)
  {
    PClassInfo info{}; assert(factory->getClassInfo(i, &info) == kResultOk);
    if (factory->createInstance(info.cid, INLINE_UID_OF(IEditController), reinterpret_cast<void**>(&controller)) == kResultOk) break;
  }
  assert(controller && controller->initialize(nullptr) == kResultOk);
  ParamID input = 0xffffffff;
  for (int i = 0; i < controller->getParameterCount(); ++i)
  {
    ParameterInfo info{}; assert(controller->getParameterInfo(i, info) == kResultOk);
    if (std::u16string(info.title) == u"Input") input = info.id;
  }
  assert(input != 0xffffffff);
  const auto inputDB = [&] {
    String128 text{};
    assert(controller->getParamStringByValue(input, controller->getParamNormalized(input), text) == kResultOk);
    std::string value; for (auto c : text) { if (!c) break; value += static_cast<char>(c); }
    return std::stod(value);
  };
  for (int pass = 0; pass < 2; ++pass)
  {
    auto* view = controller->createView(ViewType::kEditor); assert(view);
    auto parent = Parent();
    EditorFrame frame; frame.parent = parent; assert(view->setFrame(&frame) == kResultOk);
    assert(view->attached(parent, kPlatformTypeHWND) == kResultOk);
    IPlugViewContentScaleSupport* scaling = nullptr;
    assert(view->queryInterface(INLINE_UID_OF(IPlugViewContentScaleSupport), reinterpret_cast<void**>(&scaling)) == kResultOk);
    assert(scaling->setContentScaleFactor(static_cast<float>(scale)) == kResultOk);
    scaling->release();
    if (pass) assert(std::abs(inputDB() + 3.5) < .01);
    CheckEditor(parent, scale, inputDB, out.empty() ? out : out / ("vst3-" + std::to_string(pass) + ".bmp"));
    assert(view->removed() == kResultOk); view->setFrame(nullptr); view->release(); DestroyWindow(parent);
    assert(frame.refs == 1);
  }
  assert(controller->terminate() == kResultOk); controller->release(); factory->release();
  if (exit) assert(exit()); FreeLibrary(library);
  std::cout << "VST3: numeric entry, Escape, tooltip, scaled bounds and reopening passed\n";
}

int main(int argc, char** argv) try
{
  std::cout << std::unitbuf;
  if (argc != 3 && argc != 5 && argc != 7)
  {
    std::cerr << "Usage: nam-editor-host-tests <nam-grid.clap> <nam-grid.vst3 binary> [--scale 1.25] [--screenshots <directory>]\n";
    return 1;
  }
  double requested = 0.; std::filesystem::path out;
  for (int i = 3; i < argc; i += 2)
    if (std::string(argv[i]) == "--scale")
    {
      size_t used = 0;
      requested = std::stod(argv[i + 1], &used);
      if (used != std::strlen(argv[i + 1]) || !std::isfinite(requested) || requested <= 0.)
        throw std::runtime_error("--scale requires a positive, finite number such as 1.25");
    }
    else if (std::string(argv[i]) == "--screenshots") out = argv[i + 1];
    else { std::cerr << "Unknown option: " << argv[i] << '\n'; return 1; }
  SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  auto probe = Parent(); const double scale = GetDpiForWindow(probe) / 96.; DestroyWindow(probe);
  if (requested && std::abs(requested - scale) > .01)
  {
    std::cerr << "Active display scale is " << scale << "; set Windows display scaling to " << requested * 100 << "% before this check.\n";
    return 1;
  }
  if (!out.empty()) std::filesystem::create_directories(out);
  IsolatedUserHome settings("nam-grid-editor-host-tests");
  std::cout << "Checking actual monitor scale " << scale * 100 << "%\n";
  CheckCLAP(argv[1], scale, out); CheckVST3(argv[2], scale, out);
  return 0;
}
catch (const std::exception& error)
{
  std::cerr << "Editor check failed: " << error.what() << '\n';
  return 1;
}
