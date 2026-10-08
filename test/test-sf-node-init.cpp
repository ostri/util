// test-sf-node-init.cpp
/**
 * @file
 * @brief claims this test binary's own snowflake node id before any test runs
 *
 * Catch2 provides main(), so there is no place for the init_sf_node() call every other util program
 * makes from its own main() - this listener's testRunStarting() is the equivalent hook. Without it
 * the first test that generates an id (several build a temp directory name from one) would hit
 * sf_node()'s own assert.
 */
#include "os3/util/sf-node.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include "logger/logger.hpp"

namespace
{
  class sf_node_listener : public Catch::EventListenerBase
  {
  public:
    using Catch::EventListenerBase::EventListenerBase;

    void testRunStarting(const Catch::TestRunInfo& /*info*/) override
    {
      static auto log = logger::Logger::create_or_exit(logger::logger_config{.app_name = "util_test"});
      os3::util::init_sf_node(*log, os3::util::sf_node_root);
    }
  };
} // namespace

CATCH_REGISTER_LISTENER(sf_node_listener)
