#pragma once
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace wav_fixture
{
// Explicit LE bytes keep malformed-size and endpoint fixtures independent of
// the decoder. declaredBytes may intentionally disagree with the payload.
inline void Write(const std::filesystem::path& path, int bits, bool floating, const std::vector<uint32_t>& values,
                  int declaredBytes = -1, int payloadBytes = -1, bool extensible = false)
{
  const int width = bits / 8;
  if (payloadBytes < 0)
    payloadBytes = static_cast<int>(values.size()) * width;
  if (declaredBytes < 0)
    declaredBytes = payloadBytes;
  std::ofstream file(path, std::ios::binary);
  auto number = [&](uint32_t value, int bytes) {
    for (int i = 0; i < bytes; ++i)
      file.put(static_cast<char>(value >> (8 * i)));
  };
  const int fmtSize = extensible ? 40 : 16;
  file.write("RIFF", 4);
  number(4 + 8 + fmtSize + (extensible ? 12 : 0) + 8 + payloadBytes + (payloadBytes % 2), 4);
  file.write("WAVEfmt ", 8);
  number(fmtSize, 4);
  number(extensible ? 65534 : floating ? 3 : 1, 2);
  number(1, 2);
  number(48000, 4);
  number(48000 * width, 4);
  number(width, 2);
  number(bits, 2);
  if (extensible)
  {
    number(22, 2);
    number(bits, 2);
    number(4, 4);
    number(floating ? 3 : 1, 4);
    number(0x00100000, 4);
    number(0xaa000080, 4);
    number(0x719b3800, 4);
    file.write("fact", 4);
    number(4, 4);
    number(static_cast<uint32_t>(values.size()), 4);
  }
  file.write("data", 4);
  number(static_cast<uint32_t>(declaredBytes), 4);
  for (int i = 0; i < payloadBytes; ++i)
  {
    const size_t sample = static_cast<size_t>(i / width);
    const auto value = sample < values.size() ? values[sample] : 0;
    file.put(static_cast<char>(value >> (8 * (i % width))));
  }
  if (payloadBytes % 2)
    file.put(0);
}
} // namespace wav_fixture
