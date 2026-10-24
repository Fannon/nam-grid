#pragma once
#include <clap/clap.h>
#include <json.hpp>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <utility>
#include <vector>
#ifdef _WIN32
  #include <windows.h>
#else
  #include <dlfcn.h>
#endif
using json = nlohmann::json;
struct Stream
{
  std::vector<uint8_t> bytes;
  size_t pos = 0;
  static int64_t write(const clap_ostream* stream, const void* data, uint64_t n)
  {
    auto& self = *static_cast<Stream*>(stream->ctx);
    auto* ptr = static_cast<const uint8_t*>(data);
    self.bytes.insert(self.bytes.end(), ptr, ptr + n);
    return n;
  }
  static int64_t read(const clap_istream* stream, void* data, uint64_t n)
  {
    auto& self = *static_cast<Stream*>(stream->ctx);
    n = std::min<uint64_t>(n, self.bytes.size() - self.pos);
    std::memcpy(data, self.bytes.data() + self.pos, n);
    self.pos += n;
    return n;
  }
  void string(const std::string& value)
  {
    int n = static_cast<int>(value.size());
    auto* p = reinterpret_cast<uint8_t*>(&n);
    bytes.insert(bytes.end(), p, p + 4);
    bytes.insert(bytes.end(), value.begin(), value.end());
  }
  json config() const
  {
    int headerSize;
    std::memcpy(&headerSize, bytes.data(), 4);
    int textSize;
    std::memcpy(&textSize, bytes.data() + 4 + headerSize, 4);
    return json::parse(std::string(reinterpret_cast<const char*>(bytes.data() + 8 + headerSize), textSize));
  }
};
struct Host
{
  bool restart = false, callback = false, dirty = false, flushRequested = false;
  clap_param_rescan_flags rescanned = 0;
#ifdef __linux__
  // Linux editors run on the host's main-thread timer and X11 connection, as in Bitwig or REAPER.
  std::vector<std::pair<clap_id, uint32_t>> timers;
  std::vector<int> fds;
  clap_id nextTimer = 1;
#endif
  static const void* extension(const clap_host*, const char* id)
  {
#ifdef __linux__
    static const clap_host_timer_support timer{
      [](const clap_host* h, uint32_t period, clap_id* timerId) {
        auto* self = static_cast<Host*>(h->host_data);
        *timerId = self->nextTimer++;
        self->timers.emplace_back(*timerId, period);
        return true;
      },
      [](const clap_host* h, clap_id timerId) {
        auto& timers = static_cast<Host*>(h->host_data)->timers;
        return std::erase_if(timers, [&](const auto& entry) { return entry.first == timerId; }) == 1;
      }};
    static const clap_host_posix_fd_support posixFd{
      [](const clap_host* h, int fd, clap_posix_fd_flags_t) {
        static_cast<Host*>(h->host_data)->fds.push_back(fd);
        return true;
      },
      [](const clap_host*, int, clap_posix_fd_flags_t) { return true; },
      [](const clap_host* h, int fd) { return std::erase(static_cast<Host*>(h->host_data)->fds, fd) == 1; }};
    if (std::strcmp(id, CLAP_EXT_TIMER_SUPPORT) == 0)
      return &timer;
    if (std::strcmp(id, CLAP_EXT_POSIX_FD_SUPPORT) == 0)
      return &posixFd;
#endif
    static const clap_host_latency latency{[](const clap_host*) {}};
    static const clap_host_state state{[](const clap_host* h) { static_cast<Host*>(h->host_data)->dirty = true; }};
    static const clap_host_params params{
      [](const clap_host* h, clap_param_rescan_flags flags) { static_cast<Host*>(h->host_data)->rescanned |= flags; },
      [](const clap_host*, clap_id, clap_param_clear_flags) {},
      [](const clap_host* h) { static_cast<Host*>(h->host_data)->flushRequested = true; }};
    if (std::strcmp(id, CLAP_EXT_STATE) == 0)
      return &state;
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0)
      return &params;
    return std::strcmp(id, CLAP_EXT_LATENCY) == 0 ? &latency : nullptr;
  }
  clap_host host{CLAP_VERSION_INIT,
                 this,
                 "NAM smoke host",
                 "local",
                 "",
                 "1",
                 extension,
                 [](const clap_host* h) { static_cast<Host*>(h->host_data)->restart = true; },
                 [](const clap_host*) {},
                 [](const clap_host* h) { static_cast<Host*>(h->host_data)->callback = true; }};
};
void pump()
{
#ifdef _WIN32
  MSG msg;
  while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }
#endif
}
