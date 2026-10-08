#pragma once
/**
 * @file
 * @brief sf_number - allocates a process-unique snowflake node_id (0-1023)
 *
 * unique_id(thread) takes a caller-chosen 10-bit slot and only guarantees
 * uniqueness across callers that pass different slots. Once several processes
 * (or several instances of the same process) generate ids concurrently, each
 * needs its own slot; sf_number hands one out and holds it for as long as the
 * object lives.
 */
#include "os3/util/err-data.hpp"
#include <expected>
#include <cstdint>
#include <string>
#include <vector>

namespace os3::util
{

  using e_node_id = std::expected<std::uint16_t, err_data>;

  /**
   * @brief one claimed node id and whatever the kernel will say about its holder
   *
   * What is knowable differs per field, and the difference matters when reading a listing:
   * name/node_id/inode come from /proc/net/unix and are always present; pid/comm/cmdline/uid/
   * claimed_at come from walking /proc/<pid>/fd for that inode, and stay empty when the holder
   * belongs to another user, since /proc hides other users' fd links.
   *
   * The id itself cannot be learnt from a refused bind() - EADDRINUSE says "taken", never by whom -
   * so a listing reads /proc rather than probing the pool. That is an observation, not a claim: a
   * row here can already be stale by the time it is printed. Claiming stays with node_id(), which
   * is atomic across processes because the kernel decides it, not the caller.
   */
  // NOLINTBEGIN(misc-non-private-member-variables-in-classes) - a plain aggregate, not a class
  struct proc_dscr
  {
    str_t         name;       ///< abstract socket name as the kernel spells it, e.g. "ach.node.3"
    std::uint16_t node_id{};  ///< the id parsed out of name's ".<id>" suffix
    std::uint64_t inode{};    ///< socket inode - what ties the name to a holding process
    int           pid{-1};    ///< holder's pid, -1 when it could not be resolved
    str_t         comm;       ///< holder's executable name (/proc/<pid>/comm), empty if unresolved
    str_t         cmdline;    ///< holder's full command line, NUL separators turned into spaces
    int           uid{-1};    ///< holder's real uid, -1 when unresolved
    str_t         claimed_at; ///< holder's start time, "YYYY-MM-DD HH:MM:SS", empty if unresolved

    /// @brief whether the holding process was identified - false when /proc hid it from this user
    [[nodiscard]] bool resolved() const noexcept { return pid >= 0; }
  };
  // NOLINTEND(misc-non-private-member-variables-in-classes)

  /**
   * @brief claims a free snowflake node_id by binding an abstract unix domain socket
   *
   * The claim is an abstract-namespace (Linux-only) AF_UNIX socket named
   * "<name_root>.<id>". Abstract names are kernel-managed rather than
   * filesystem entries: the name exists exactly as long as some process holds
   * the socket open, and disappears the moment that fd is closed - including
   * when the process is SIGKILLed or crashes. That is what makes this reliable
   * without any lease/heartbeat/stale-lock cleanup of its own: there is no
   * on-disk artifact that can outlive its owner, unlike a pid file or a
   * filesystem-path unix socket.
   *
   * Cost is one socket() plus one bind() per already-taken id, paid once since the result is
   * cached. Measured on the development machine: ~9 us to claim id 0 from an empty pool, ~310 us
   * for the worst case where 1023 ids are already taken. Nothing is allocated inside the scan
   * loop: the name buffer is built once and only its numeric suffix is rewritten per attempt.
   *
   * Not thread-safe (one instance per thread/process); the underlying claim
   * however is atomic across processes, since bind() on an already-bound
   * abstract name fails with EADDRINUSE rather than stealing it.
   */
  class sf_number
  {
  public:
    /// @brief number of distinct snowflake node ids - unique_id()'s own 10-bit slot
    static constexpr std::uint16_t node_id_count = 1024;

    /**
     * @brief binds nothing yet - the claim happens on the first node_id() call
     * @param name_root prefix shared by every instance competing for the same
     *        pool; instances with different roots do not see each other's ids
     */
    explicit sf_number(str_t name_root) noexcept;

    /// @brief releases the claimed id, if any
    ~sf_number();

    sf_number(const sf_number&)            = delete;
    sf_number& operator=(const sf_number&) = delete;
    sf_number(sf_number&& other) noexcept;
    sf_number& operator=(sf_number&& other) noexcept;

    /**
     * @brief the claimed node_id, claiming the lowest free one in [0, node_id_count) on first call
     *
     * The claim is lazy and cached: the first call scans the pool, every later
     * call returns the id already held without touching the kernel again. A
     * failed call claims nothing, so a later call retries the scan - useful
     * when the pool was momentarily full.
     *
     * @return the claimed node_id, ready to pass to unique_id(), or why
     *         no id could be claimed (pool exhausted, name_root too long, or
     *         a socket()/bind() failure that is not "slot taken")
     */
    [[nodiscard]] e_node_id node_id() noexcept;

    /// @brief whether a node_id is currently held - false before the first successful node_id() call
    [[nodiscard]] bool allocated() const noexcept { return fd_ >= 0; }

    /// @brief drops the claim, making the id available to other processes again
    void release() noexcept;

    /**
     * @brief every id currently claimed under @p name_root, and who holds it
     *
     * Reads /proc/net/unix for the bound abstract names, then resolves each one's inode to its
     * holder by walking /proc/<pid>/fd. Static and const by nature: it observes the pool rather
     * than taking part in it, so it needs no instance and claims nothing.
     *
     * Unreadable /proc entries are skipped rather than reported - another user's process still
     * holds its id, and the caller wants the rows it can see, not a failure. Such a row comes back
     * with proc_dscr::resolved() false.
     *
     * @param name_root the same prefix node_id() claims under - "<name_root>." is matched literally
     * @return one entry per claimed id, ascending by node_id; empty when nothing is claimed or when
     *         /proc/net/unix cannot be read at all (a kernel without procfs)
     */
    [[nodiscard]] static std::vector<proc_dscr> list(const str_t& name_root);
  private:
    str_t         name_root_;  ///< caller-supplied prefix, "<name_root>.<id>" is the bound name
    int           fd_{-1};     ///< the bound socket - the claim itself; -1 when nothing is held
    std::uint16_t node_id_{0}; ///< the claimed id, valid only while fd_ >= 0
  };
} // namespace os3::util
