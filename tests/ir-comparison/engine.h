#pragma once
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <vector>

namespace ir_bench
{
inline thread_local bool tracking = false;
inline thread_local size_t allocations = 0, releases = 0;
inline void* Malloc(size_t size)
{
  if (tracking)
    ++allocations;
  return std::malloc(size);
}
inline void* Realloc(void* ptr, size_t size)
{
  if (tracking)
    ++allocations;
  return std::realloc(ptr, size);
}
inline void Free(void* ptr)
{
  if (tracking && ptr)
    ++releases;
  std::free(ptr);
}
class Engine
{
public:
  virtual ~Engine() = default;
  virtual void Process(double* const* input, double** output, int frames) = 0;
  virtual int Latency() const { return 0; }
};
std::unique_ptr<Engine> MakeWdl(const std::vector<float>& weights, int maxBlock);
} // namespace ir_bench
