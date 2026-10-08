#include "os3/util/unique-id.hpp"
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>

namespace os3::util
{
  namespace
  {
    constexpr unsigned thread_bits   = 10;
    constexpr unsigned sequence_bits = 12;
    constexpr uint64_t sequence_mask = (uint64_t{1} << sequence_bits) - 1;
    constexpr uint16_t thread_mask   = (uint16_t{1} << thread_bits) - 1;

    /// 2025-01-01T00:00:00Z in milliseconds since the Unix epoch - shifts the
    /// 41-bit timestamp field's range forward so it does not run out until
    /// well past this project's lifetime, same reasoning real snowflake ids
    /// use their own custom epoch for
    constexpr uint64_t custom_epoch_ms = 1735689600000;

    uint64_t now_ms() noexcept
    {
      return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    }

    /// packed (ms_since_epoch << sequence_bits) | sequence - one atomic
    /// instead of two so a single compare_exchange keeps both in step
    /// across threads racing this function at once. Deliberately mutable
    /// and file-scope: unique_id() needs state shared by every caller, in
    /// every thread, for the lifetime of the process.
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    std::atomic<uint64_t> last_state{0}; // NOLINT(cert-err58-cpp) - trivial constant init, cannot throw
  } // namespace
  std::uint64_t unique_id(std::uint16_t thread) noexcept
  {
    const auto t = static_cast<uint64_t>(thread & thread_mask);
    for (;;)
    {
      const uint64_t wall_ms = now_ms() - custom_epoch_ms;
      uint64_t       prev    = last_state.load(std::memory_order_relaxed);
      const uint64_t prev_ms = prev >> sequence_bits;

      // std::chrono::system_clock is wall-clock time, not monotonic - an NTP
      // adjustment (or just scheduling/TSC jitter across cores) can make a
      // later call observe an EARLIER wall_ms than an already-published
      // prev_ms. Clamping ms to max(wall_ms, prev_ms) keeps last_state's own
      // timestamp field monotonically non-decreasing, so the (ms, seq) pair
      // handed out by any single compare_exchange_weak() below can never
      // collide with one already handed out by an earlier call - without
      // this clamp, the clock stepping backward and then forward again could
      // reproduce an exact (ms, seq) pair a second time (each occurrence
      // individually race-free, since compare_exchange_weak() itself never
      // spuriously succeeds - the duplicate comes from ms itself repeating,
      // not from a broken CAS). Confirmed by direct repro: test-unique-id.cpp's
      // own "never repeats" TEST_CASE reproducibly hands out a duplicate id
      // roughly 1 run in 25 without this clamp, table-stakes proof this was
      // never mere test flakiness.
      const uint64_t ms = wall_ms > prev_ms ? wall_ms : prev_ms;

      uint64_t seq = 0;
      if (ms == prev_ms)
      {
        seq = (prev & sequence_mask) + 1;
        if (seq > sequence_mask)
        {
          // this thread's 4096 ids for this millisecond are used up - wait
          // for the clock to move on rather than hand out a colliding id
          std::this_thread::sleep_for(std::chrono::microseconds(100)); // NOLINT(readability-magic-numbers)
          continue;
        }
      }

      const uint64_t next = (ms << sequence_bits) | seq;
      if (last_state.compare_exchange_weak(prev, next, std::memory_order_relaxed))
        return (ms << (thread_bits + sequence_bits)) | (t << sequence_bits) | seq;
      // another thread won the race - retry with a fresh timestamp/sequence
    }
  }
} // namespace os3::util
