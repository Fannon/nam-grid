#pragma once
#ifdef __linux__
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <chrono>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <thread>
#include <unordered_set>
#include "ClapTestHost.h"
#include "../NeuralAmpModeler/GridLayout.h"

// The CLAP editor embedded in an X11 parent window and driven, as Bitwig or REAPER drive it, by
// the host's timer and the X11 connection's file descriptor. Needs a display: run the smoke test
// under xvfb-run on headless machines. Without one the checks are skipped, not failed.
template <typename Config>
void TestLinuxEditor(const clap_plugin* plugin, Host& host, Config&& config, const char* screenshots)
{
  Display* display = XOpenDisplay(nullptr);
  if (!display)
  {
    std::cout << "No X display; Linux editor checks skipped (run under xvfb-run)\n";
    return;
  }
  int event, error, major, minor;
  assert(XTestQueryExtension(display, &event, &error, &major, &minor));
  auto* gui = static_cast<const clap_plugin_gui*>(plugin->get_extension(plugin, CLAP_EXT_GUI));
  auto* timer = static_cast<const clap_plugin_timer_support*>(plugin->get_extension(plugin, CLAP_EXT_TIMER_SUPPORT));
  auto* posixFd = static_cast<const clap_plugin_posix_fd_support*>(plugin->get_extension(plugin, CLAP_EXT_POSIX_FD_SUPPORT));
  assert(gui && timer && posixFd);
  assert(gui->is_api_supported(plugin, CLAP_WINDOW_API_X11, false));
  const int width = grid_layout::width, height = grid_layout::height;
  const Window parent = XCreateSimpleWindow(display, DefaultRootWindow(display), 0, 0, width, height, 0, 0, 0);
  XMapRaised(display, parent);
  XSync(display, False);
  const auto pumpFor = [&](int milliseconds) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
    while (std::chrono::steady_clock::now() < deadline)
    {
      for (const int fd : std::vector<int>(host.fds))
        posixFd->on_fd(plugin, fd, CLAP_POSIX_FD_READ);
      for (const auto& [id, period] : std::vector<std::pair<clap_id, uint32_t>>(host.timers))
        timer->on_timer(plugin, id);
      std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
  };
  const auto click = [&](int x, int y) {
    XTestFakeMotionEvent(display, -1, x, y, CurrentTime);
    XTestFakeButtonEvent(display, 1, True, CurrentTime);
    XTestFakeButtonEvent(display, 1, False, CurrentTime);
    XSync(display, False);
    pumpFor(250);
  };
  const size_t idleTimers = host.timers.size(); // The plugin's idle work runs on a host timer too.
  assert(idleTimers == 1);
  // Hosts destroy and recreate the editor whenever it is closed and reopened.
  for (int pass = 0; pass < 2; ++pass)
  {
    assert(gui->create(plugin, CLAP_WINDOW_API_X11, false));
    uint32_t editorWidth = 0, editorHeight = 0;
    assert(gui->get_size(plugin, &editorWidth, &editorHeight));
    assert(editorWidth == static_cast<uint32_t>(width) && editorHeight == static_cast<uint32_t>(height));
    clap_window window{};
    window.api = CLAP_WINDOW_API_X11;
    window.x11 = parent;
    assert(gui->set_parent(plugin, &window));
    gui->show(plugin); // iPlug2 opens the editor in set_parent, so show reports no change.
    assert(host.timers.size() == idleTimers + 1 && host.fds.size() == 1); // Rendering follows the host's timer.
    pumpFor(750);
    Window root, owner, *children = nullptr;
    unsigned int count = 0;
    assert(XQueryTree(display, parent, &root, &owner, &children, &count) && count == 1);
    const Window editor = children[0];
    XFree(children);
    // A uniform image means the GL context or the first frame failed.
    XImage* image = XGetImage(display, editor, 0, 0, width, height, AllPlanes, ZPixmap);
    assert(image);
    std::unordered_set<unsigned long> colours;
    for (int y = 0; y < height; y += 4)
      for (int x = 0; x < width; x += 4)
        colours.insert(XGetPixel(image, x, y));
    assert(colours.size() > 16);
    if (screenshots)
    {
      std::filesystem::create_directories(screenshots);
      std::ofstream file(std::filesystem::path(screenshots) / ("linux-editor-" + std::to_string(pass) + ".ppm"),
                         std::ios::binary);
      file << "P6\n" << width << " " << height << "\n255\n";
      for (int y = 0; y < height; ++y)
        for (int x = 0; x < width; ++x)
        {
          const unsigned long pixel = XGetPixel(image, x, y);
          const char rgb[3]{static_cast<char>(pixel >> 16), static_cast<char>(pixel >> 8), static_cast<char>(pixel)};
          file.write(rgb, 3);
        }
    }
    XDestroyImage(image);
    if (pass == 0)
    {
      // The header Tuner toggle, through the X server's own pointer input.
      click(810, 28);
      assert(config()["Tuner"] == 1.);
      click(810, 28);
      assert(config()["Tuner"] == 0.);
    }
    assert(gui->hide(plugin));
    gui->destroy(plugin);
    assert(host.timers.size() == idleTimers && host.fds.empty());
  }
  XDestroyWindow(display, parent);
  XCloseDisplay(display);
  std::cout << "Linux X11 editor rendering, host timer/fd, input and reopen passed\n";
}
#endif
