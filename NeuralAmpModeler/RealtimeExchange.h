#pragma once
#include <atomic>
#include <memory>

// One serialized control producer, one audio consumer. Never destroys on audio.
// Destruction and ResetStopped require the host to have stopped processing.
template <class T>
class RealtimeExchange
{
public:
  static_assert(std::atomic<T*>::is_always_lock_free);
  ~RealtimeExchange()
  {
    delete mPending.exchange(nullptr);
    delete mRetired.exchange(nullptr);
    delete mActive;
  }
  void Publish(std::unique_ptr<T> item)
  {
    Collect();
    delete mPending.exchange(item.release(), std::memory_order_acq_rel);
  }
  void Collect() { delete mRetired.exchange(nullptr, std::memory_order_acq_rel); }
  bool HasPending() const { return mPending.load(std::memory_order_acquire) != nullptr; }
  bool Adopt() noexcept
  {
    if (mRetired.load(std::memory_order_acquire))
      return false;
    auto* next = mPending.exchange(nullptr, std::memory_order_acq_rel);
    if (!next)
      return false;
    auto* old = mActive;
    mActive = next;
    mRetired.store(old, std::memory_order_release);
    return true;
  }
  T* Active() const { return mActive; } // audio only, or host stopped
  T* PendingStopped() const { return mPending.load(); }

private:
  std::atomic<T*> mPending{nullptr}, mRetired{nullptr};
  T* mActive = nullptr;
};
