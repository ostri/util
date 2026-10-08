#pragma once
/**
 * @file
 * @brief unique_id - Twitter-snowflake-style 64-bit id generator
 */
#include <cstdint>

namespace os3::util
{
  /**
   * @brief a 64-bit, monotonically increasing, Twitter-snowflake-style id
   *
   * Layout (MSB to LSB): 41 bits milliseconds since 2025-01-01T00:00:00Z,
   * 10 bits thread, 12 bits sequence. The sequence resets to 0 every time
   * the millisecond changes and increments (mod 4096) within it, so up to
   * 4096 ids can be generated per thread per millisecond before the call
   * blocks-in-place (busy-waits) for the next millisecond to roll over.
   * Ordering only holds within one thread value: two
   * different threads can produce ids out of timestamp order relative to
   * each other, same as real snowflake ids across nodes.
   *
   * @param thread caller-chosen 10-bit slot (0-1023) - callers that never
   *        run concurrently with each other can all safely pass 0; the
   *        one place this actually matters is two threads that might
   *        generate an id in the same millisecond
   * @return a new, unique, mostly-increasing id
   */
  [[nodiscard]] std::uint64_t unique_id(std::uint16_t thread) noexcept;
} // namespace os3::util
