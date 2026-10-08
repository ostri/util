#pragma once
/**
 * @file
 * @brief sf_node - this process's own snowflake node id, claimed once at startup
 *
 * unique_id(thread) keeps every call WITHIN one process distinct on its own: last_state is a
 * single process-wide std::atomic and each call takes its (ms, seq) pair through one
 * compare_exchange_weak (see unique-id.cpp), so threads never collide regardless of what
 * `thread` is passed. What the parameter actually does is tell CONCURRENT PROCESSES apart - each
 * process starts its own last_state from zero, so two of them passing the same value can hand out
 * the same id for two unrelated rows.
 *
 * Hence one value per process, not per thread: main() claims it once through init_sf_node(), and
 * every unique_id() call site passes sf_node().
 */
#include "logger/logger.hpp"
#include "os3/util/err-data.hpp"
#include <cstdint>
#include <string>

namespace os3::util
{

  /**
   * @brief claims this process's node id, or logs a critical message and exits
   *
   * Call once from main(), before anything generates an id. Failure is not recoverable: without a
   * node id of its own the process would have to fall back on a value another process may already
   * be using, and silently hand out colliding ids - so this exits rather than returning an error
   * for the caller to overlook, the same way logger::Logger::create_or_exit() does.
   *
   * The claim is held for the lifetime of the process and released by the kernel when it ends
   * (see sf_number's own class comment on why no cleanup is needed, even after a crash).
   *
   * @param log where the failure is reported before exiting
   * @param name_root pool every process competes in - all of them must pass the SAME root, or
   *        processes using different roots can claim the same id and collide
   */
  void init_sf_node(const logger::Logger& log, const str_t& name_root);

  /**
   * @brief this process's node id - what every unique_id() call should pass
   *
   * Valid only after init_sf_node() has returned; calling it earlier asserts (debug) and yields 0.
   */
  [[nodiscard]] std::uint16_t sf_node() noexcept;

  /// @brief the pool every process shares - one pool, so no two processes can claim the same id
  inline constexpr const char* sf_node_root = "ach.node";
} // namespace os3::util
