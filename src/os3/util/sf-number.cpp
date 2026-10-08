#include "os3/util/sf-number.hpp"
#include <fmt/format.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <charconv>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <iterator>
#include <sstream>
#include <utility>

namespace os3::util
{
  namespace
  {
    /// usable bytes of sockaddr_un::sun_path - the abstract namespace spends the first one on the
    /// leading NUL, and the name that follows is NOT NUL-terminated (its length comes from addrlen)
    constexpr std::size_t abstract_name_capacity = sizeof(sockaddr_un{}.sun_path) - 1;
    /// widest suffix node_id() appends to the root: '.' plus "1023"
    constexpr std::size_t max_suffix_len = 5;
    /// scratch size for strerror_r() - longer than any errno string glibc produces
    constexpr std::size_t errno_buf_size = 256;

    /// @brief errno as text, without strerror()'s own thread-safety caveat
    str_t errno_text(int err)
    {
      std::array<char, errno_buf_size> buf{};
      // GNU strerror_r may answer in its own static buffer rather than in buf - use what it returns
      const char* const msg = ::strerror_r(err, buf.data(), buf.size());
      return {msg};
    }

    /// @brief the numeric suffix of "<root>.<id>", if @p name is one and the id is in range
    std::optional<std::uint16_t> node_id_of(const str_t& name, const str_t& prefix)
    {
      if (! name.starts_with(prefix)) return std::nullopt;
      const str_t   digits = name.substr(prefix.size());
      std::uint16_t id{};
      const auto [ptr, ec] = std::from_chars(digits.c_str(), std::next(digits.c_str(), static_cast<std::ptrdiff_t>(digits.size())), id);
      // the whole suffix must be the number: "ach.node.3x" is not id 3, it is a foreign name
      if (ec != std::errc{} || *ptr != '\0' || id >= sf_number::node_id_count) return std::nullopt;
      return id;
    }

    /// @brief first line of @p path with trailing whitespace removed, empty if unreadable
    str_t read_line(const std::filesystem::path& path)
    {
      std::ifstream in(path);
      str_t         line;
      if (! std::getline(in, line)) return {};
      while (! line.empty() && (line.back() == '\n' || line.back() == '\0')) line.pop_back();
      return line;
    }

    /// @brief /proc/<pid>/cmdline as one line - its NUL separators become spaces
    str_t read_cmdline(const std::filesystem::path& proc_pid)
    {
      const std::ifstream in(proc_pid / "cmdline", std::ios::binary);
      if (! in) return {};
      std::ostringstream buf;
      buf << in.rdbuf();
      str_t out = buf.str();
      std::ranges::replace(out, '\0', ' ');
      while (! out.empty() && out.back() == ' ') out.pop_back();
      return out;
    }

    /// @brief the process's own real uid from /proc/<pid>/status, -1 when unreadable
    int read_uid(const std::filesystem::path& proc_pid)
    {
      std::ifstream in(proc_pid / "status");
      str_t         line;
      while (std::getline(in, line))
        if (line.starts_with("Uid:"))
        {
          std::istringstream fields(line.substr(4));
          int                uid = -1;
          fields >> uid; // the first of the four is the real uid
          return uid;
        }
      return -1;
    }

    /// scratch for strftime()'s "YYYY-MM-DD HH:MM:SS" - 20 bytes with the NUL, rounded up
    constexpr std::size_t time_buf_size = 32;

    /// @brief when the process started, as the mtime of its own /proc entry
    str_t read_start_time(const std::filesystem::path& proc_pid)
    {
      struct stat st{};
      if (::stat(proc_pid.c_str(), &st) != 0) return {};
      std::tm tm{};
      if (::localtime_r(&st.st_mtime, &tm) == nullptr) return {};
      std::array<char, time_buf_size> buf{};
      const std::size_t               len = std::strftime(buf.data(), buf.size(), "%Y-%m-%d %H:%M:%S", &tm);
      return {buf.data(), len};
    }

    /// @brief fills in @p out's holder fields from whichever process has @p inode open
    void resolve_holder(proc_dscr& out) noexcept
    {
      const str_t     want = fmt::format("socket:[{}]", out.inode);
      std::error_code ec; // every filesystem call below is nothrow - /proc races with us
      for (const auto& entry : std::filesystem::directory_iterator("/proc", ec))
      {
        const str_t pid_dir = entry.path().filename().string();
        if (pid_dir.empty() || std::isdigit(static_cast<unsigned char>(pid_dir.front())) == 0) continue;

        for (const auto& fd : std::filesystem::directory_iterator(entry.path() / "fd", ec))
        {
          // another user's process, or one that exited mid-scan - not an error, just not ours to see
          if (ec) break;
          if (std::filesystem::read_symlink(fd.path(), ec) != want || ec) continue;

          // pid_dir is all digits (checked above), so from_chars cannot fail here
          std::from_chars(pid_dir.data(), std::next(pid_dir.data(), static_cast<std::ptrdiff_t>(pid_dir.size())), out.pid);
          out.comm       = read_line(entry.path() / "comm");
          out.cmdline    = read_cmdline(entry.path());
          out.uid        = read_uid(entry.path());
          out.claimed_at = read_start_time(entry.path());
          return;
        }
        ec.clear();
      }
    }
  } // namespace

  sf_number::sf_number(str_t name_root) noexcept
  : name_root_(std::move(name_root))
  {
  }

  sf_number::~sf_number() { release(); }

  sf_number::sf_number(sf_number&& other) noexcept
  : name_root_(std::move(other.name_root_))
  , fd_(std::exchange(other.fd_, -1))
  , node_id_(other.node_id_)
  {
  }

  sf_number& sf_number::operator=(sf_number&& other) noexcept
  {
    if (this != &other)
    {
      release();
      name_root_ = std::move(other.name_root_);
      fd_        = std::exchange(other.fd_, -1);
      node_id_   = other.node_id_;
    }
    return *this;
  }

  void sf_number::release() noexcept
  {
    if (fd_ >= 0)
    {
      ::close(fd_); // NOLINT(cppcoreguidelines-no-int-to-ptr) - closing unbinds the abstract name
      fd_ = -1;
    }
  }

  e_node_id sf_number::node_id() noexcept
  {
    if (allocated()) return node_id_;

    if (name_root_.empty() || name_root_.size() + max_suffix_len > abstract_name_capacity)
      return std::unexpected(err_data{
        .code = err_code::sf_number_name_too_long,
        .msg = fmt::format("sf_number: name root '{}' must be 1-{} characters long", name_root_, abstract_name_capacity - max_suffix_len)});

    // SOCK_DGRAM needs no listen()/accept() to hold its name, SOCK_CLOEXEC keeps the claim from
    // leaking into exec()ed children (which would keep the id taken after this process released it)
    const int fd = ::socket(AF_UNIX, SOCK_DGRAM | SOCK_CLOEXEC, 0);
    if (fd < 0)
      return std::unexpected(
        err_data{.code = err_code::sf_number_socket_failed, .msg = fmt::format("sf_number: socket() failed: {}", errno_text(errno))});

    // built once, outside the scan loop - only the suffix after `root_end` is rewritten per attempt
    sockaddr_un addr{};
    addr.sun_family  = AF_UNIX;
    addr.sun_path[0] = '\0'; // abstract namespace marker
    std::memcpy(&addr.sun_path[1], name_root_.data(), name_root_.size());
    const std::size_t root_end = 1 + name_root_.size();

    for (std::uint16_t id = 0; id < node_id_count; ++id)
    {
      // fmt::format_to_n into the address itself - no allocation, no std::to_string temporary
      // NOLINTNEXTLINE(cppcoreguidelines-pro-bounds-constant-array-index) - root_end is bounds-checked above
      const std::size_t suffix_len = fmt::format_to_n(&addr.sun_path[root_end], max_suffix_len, ".{}", id).size;
      const std::size_t len        = root_end + suffix_len;

      const auto addr_len = static_cast<socklen_t>(offsetof(sockaddr_un, sun_path) + len);
      if (::bind(fd, reinterpret_cast<const sockaddr*>(&addr), addr_len) == 0) // NOLINT(cppcoreguidelines-pro-type-reinterpret-cast)
      {
        fd_      = fd;
        node_id_ = id;
        return node_id_;
      }
      if (errno != EADDRINUSE) // anything else is a real failure, not "this slot is taken"
      {
        const int err = errno;
        ::close(fd);
        return std::unexpected(err_data{.code = err_code::sf_number_socket_failed,
                                        .msg  = fmt::format("sf_number: bind() failed for id {}: {}", id, errno_text(err))});
      }
    }

    ::close(fd);
    return std::unexpected(err_data{.code = err_code::sf_number_pool_exhausted,
                                    .msg  = fmt::format("sf_number: all {} node ids under '{}' are in use", node_id_count, name_root_)});
  }

  std::vector<proc_dscr> sf_number::list(const str_t& name_root)
  {
    std::vector<proc_dscr> out;
    if (name_root.empty()) return out;

    // /proc/net/unix is the only place a bound abstract name is visible: the kernel keeps it, but
    // offers no API to enumerate or attribute it - a refused bind() only ever says EADDRINUSE.
    std::ifstream unix_sockets("/proc/net/unix");
    if (! unix_sockets) return out; // no procfs - nothing to report, and nothing that is an error

    const str_t prefix = name_root + '.';
    str_t       line;
    std::getline(unix_sockets, line); // header row: "Num RefCount Protocol Flags Type St Inode Path"

    while (std::getline(unix_sockets, line))
    {
      // fields are fixed in order; only the last two are wanted, and Path is absent for unnamed
      // sockets (most of them), which is why the name is read as the optional 8th field
      std::istringstream fields(line);
      str_t              num;
      str_t              refcount;
      str_t              protocol;
      str_t              flags;
      str_t              type;
      str_t              state;
      std::uint64_t      inode{};
      str_t              path;
      if (! (fields >> num >> refcount >> protocol >> flags >> type >> state >> inode)) continue;
      if (! (fields >> path)) continue; // unnamed socket - no path column at all

      // '@' is how procfs spells the abstract namespace's leading NUL
      if (! path.starts_with('@')) continue;
      const str_t name = path.substr(1);

      const auto id = node_id_of(name, prefix);
      if (! id) continue;

      proc_dscr row;
      row.name    = name;
      row.node_id = *id;
      row.inode   = inode;
      resolve_holder(row);
      out.push_back(std::move(row));
    }

    std::ranges::sort(out, {}, &proc_dscr::node_id);
    return out;
  }
} // namespace os3::util
