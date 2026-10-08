#include "os3/util/sf-node.hpp"
#include "os3/util/sf-number.hpp"
#include <cassert>
#include <cstdlib>
#include <optional>

namespace os3::util
{
  namespace
  {
    /// The claim itself, held for the lifetime of the process.
    /// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    std::optional<sf_number>& claim() noexcept
    {
      static std::optional<sf_number> instance;
      return instance;
    }

    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    std::uint16_t g_node_id = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
    bool g_initialized = false;
  } // namespace

  void init_sf_node(const logger::Logger& log, const str_t& name_root)
  {
    assert(! g_initialized); // init_sf_node() is a startup step, not something to repeat

    const auto id = claim().emplace(name_root).node_id(); // emplace() returns the contained object
    if (! id)
    {
      log.critical("cannot claim a snowflake node id: {}", id.error().msg);
      log.flush();
      std::exit(id.error().code); // NOLINT(concurrency-mt-unsafe) - single-threaded startup, before any worker exists
    }

    g_node_id     = *id;
    g_initialized = true;
    log.info("snowflake node id {} claimed from pool '{}'", g_node_id, name_root);
  }

  std::uint16_t sf_node() noexcept
  {
    assert(g_initialized); // see init_sf_node()'s own call site in each program's main()
    return g_node_id;
  }
} // namespace os3::util
