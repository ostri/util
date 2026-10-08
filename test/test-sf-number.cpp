// test-sf-number.cpp
/**
 * @file
 * @brief sf_number against a live abstract-namespace socket pool
 *
 * No database needed - sf_number talks only to the kernel (see its own class comment). Every case
 * here uses a name root unique to the case, so a parallel ctest run (or a second developer on the
 * same machine, since the abstract namespace is per-kernel and NOT per-directory) cannot collide
 * with it and turn a pass into a spurious failure.
 */
#include "os3/util/sf-number.hpp"
#include <catch2/catch_test_macros.hpp>
#include <fmt/format.h>
#include <unistd.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace
{
  /// a name root no other test case (or concurrent run) shares - the abstract namespace is
  /// kernel-global, so the pid is what keeps concurrent ctest invocations out of each other's way
  os3::util::str_t unique_root(const os3::util::str_t& tag) { return fmt::format("ach.test.{}.{}", tag, ::getpid()); }
} // namespace

TEST_CASE("sf_number: claims the lowest free id, ascending", "[sf_number]")
{
  const auto                        root = unique_root("ascending");
  std::vector<os3::util::sf_number> held;

  for (std::uint16_t expected = 0; expected < 4; ++expected)
  {
    held.emplace_back(root);
    const auto id = held.back().node_id();
    REQUIRE(id.has_value());
    CHECK(*id == expected);
    CHECK(held.back().allocated());
  }
}

TEST_CASE("sf_number: node_id() is idempotent", "[sf_number]")
{
  os3::util::sf_number claim{unique_root("idempotent")};

  const auto first = claim.node_id();
  REQUIRE(first.has_value());
  const auto second = claim.node_id();
  REQUIRE(second.has_value());
  CHECK(*first == *second);
}

TEST_CASE("sf_number: releasing returns the id to the pool", "[sf_number]")
{
  const auto root = unique_root("release");

  {
    os3::util::sf_number claim{root};
    const auto           id = claim.node_id();
    REQUIRE(id.has_value());
    CHECK(*id == 0);
  } // destructor closes the socket, unbinding the abstract name

  os3::util::sf_number again{root};
  const auto           id = again.node_id();
  REQUIRE(id.has_value());
  CHECK(*id == 0); // free again, so the lowest id comes back
}

TEST_CASE("sf_number: release() drops the claim explicitly", "[sf_number]")
{
  const auto           root = unique_root("explicit-release");
  os3::util::sf_number claim{root};
  REQUIRE(claim.node_id().has_value());

  claim.release();
  CHECK_FALSE(claim.allocated());

  // the same object can claim again after release()
  const auto id = claim.node_id();
  REQUIRE(id.has_value());
  CHECK(*id == 0);
}

TEST_CASE("sf_number: two roots do not share a pool", "[sf_number]")
{
  os3::util::sf_number a{unique_root("pool-a")};
  os3::util::sf_number b{unique_root("pool-b")};

  const auto id_a = a.node_id();
  const auto id_b = b.node_id();
  REQUIRE(id_a.has_value());
  REQUIRE(id_b.has_value());
  CHECK(*id_a == 0); // each root has its own 0
  CHECK(*id_b == 0);
}

TEST_CASE("sf_number: moving transfers the claim", "[sf_number]")
{
  const auto           root = unique_root("move");
  os3::util::sf_number original{root};
  const auto           claimed = original.node_id();
  REQUIRE(claimed.has_value());

  os3::util::sf_number moved{std::move(original)};
  CHECK(moved.allocated());
  CHECK(*moved.node_id() == *claimed); // cached, so this does not re-scan
  // NOLINTNEXTLINE(bugprone-use-after-move,hicpp-invalid-access-moved) - asserting the moved-from state itself
  CHECK_FALSE(original.allocated());

  // the claim moved rather than being duplicated or dropped: a fresh claim gets the NEXT id
  os3::util::sf_number other{root};
  const auto           next = other.node_id();
  REQUIRE(next.has_value());
  CHECK(*next == static_cast<std::uint16_t>(*claimed + 1));
}

TEST_CASE("sf_number: rejects a name root that leaves no room for the suffix", "[sf_number]")
{
  // sockaddr_un::sun_path is 108 bytes, so any root this long cannot fit whatever the suffix is
  constexpr std::size_t over_long_root = 200;
  os3::util::sf_number  claim{os3::util::str_t(over_long_root, 'x')};

  const auto id = claim.node_id();
  REQUIRE_FALSE(id.has_value());
  CHECK(id.error().code == os3::util::err_code::sf_number_name_too_long);
  CHECK_FALSE(claim.allocated());
}

TEST_CASE("sf_number: rejects an empty name root", "[sf_number]")
{
  os3::util::sf_number claim{""};

  const auto id = claim.node_id();
  REQUIRE_FALSE(id.has_value());
  CHECK(id.error().code == os3::util::err_code::sf_number_name_too_long);
}

TEST_CASE("sf_number: reports an exhausted pool", "[sf_number]")
{
  const auto                        root = unique_root("exhausted");
  std::vector<os3::util::sf_number> held;
  held.reserve(os3::util::sf_number::node_id_count);

  for (std::uint16_t i = 0; i < os3::util::sf_number::node_id_count; ++i)
  {
    held.emplace_back(root);
    REQUIRE(held.back().node_id().has_value());
  }

  os3::util::sf_number one_too_many{root};
  const auto           id = one_too_many.node_id();
  REQUIRE_FALSE(id.has_value());
  CHECK(id.error().code == os3::util::err_code::sf_number_pool_exhausted);
}

// Catch2's CHECK/REQUIRE expand into branches the cognitive-complexity metric counts, so a case
// being hard to read.
// being hard to read - same reasoning as test_ach_hook_db.cpp's own suppression.
// NOLINTNEXTLINE(readability-function-cognitive-complexity)
TEST_CASE("sf_number: list() reports the ids this process holds, and who holds them", "[sf_number]")
{
  const auto root = unique_root("list");

  // nothing claimed yet - the pool exists only once something binds a name in it
  CHECK(os3::util::sf_number::list(root).empty());

  std::vector<os3::util::sf_number> held;
  for (std::uint16_t i = 0; i < 3; ++i)
  {
    held.emplace_back(root);
    REQUIRE(held.back().node_id().has_value());
  }

  const auto rows = os3::util::sf_number::list(root);
  REQUIRE(rows.size() == 3);

  std::uint16_t expected = 0;
  for (const auto& row : rows)
  {
    CHECK(row.node_id == expected);                          // ascending by node_id, as documented
    CHECK(row.name == fmt::format("{}.{}", root, expected)); // the name the kernel actually holds
    CHECK(row.inode != 0);

    // the holder is this very test process, so /proc will name it - a row that stays unresolved
    // here would mean the inode -> pid walk is broken, not that a permission hid it
    REQUIRE(row.resolved());
    CHECK(row.pid == static_cast<int>(::getpid()));
    CHECK(std::cmp_equal(row.uid, ::getuid()));
    CHECK_FALSE(row.comm.empty());
    CHECK_FALSE(row.claimed_at.empty());
    ++expected;
  }

  // releasing is what makes an id disappear from the listing - the kernel drops the name with the fd
  held.pop_back();
  CHECK(os3::util::sf_number::list(root).size() == 2);
}

TEST_CASE("sf_number: list() reports nothing for a root nobody claims under", "[sf_number]")
{
  const auto root = unique_root("list-empty");

  CHECK(os3::util::sf_number::list(root).empty());
  CHECK(os3::util::sf_number::list("").empty()); // an empty root claims nothing, so it lists nothing

  // a claim under a DIFFERENT root must not show up here: "<root>." is matched literally, so
  // neither a longer root sharing this prefix nor an unrelated one may leak into the listing
  os3::util::sf_number other{root + ".deeper"};
  REQUIRE(other.node_id().has_value());
  CHECK(os3::util::sf_number::list(root).empty());
}
