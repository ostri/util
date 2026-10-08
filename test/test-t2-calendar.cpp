#include "os3/util/t2-calendar.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>

namespace
{
  using os3::util::t2_calendar;
  using std::chrono::day;
  using std::chrono::month;
  using std::chrono::year;
  using std::chrono::year_month_day;

  year_month_day ymd(int y, unsigned m, unsigned d) { return year{y} / month{m} / day{d}; }
} // namespace

TEST_CASE("t2_calendar: easter_sunday", "[t2_calendar]")
{
  CHECK(t2_calendar::easter_sunday(year{2024}) == ymd(2024, 3, 31));
  CHECK(t2_calendar::easter_sunday(year{2025}) == ymd(2025, 4, 20));
  CHECK(t2_calendar::easter_sunday(year{2026}) == ymd(2026, 4, 5));
  CHECK(t2_calendar::easter_sunday(year{2038}) == ymd(2038, 4, 25));
  CHECK(t2_calendar::easter_sunday(year{1818}) == ymd(1818, 3, 22));
}

TEST_CASE("t2_calendar: weekends are not business days", "[t2_calendar]")
{
  CHECK_FALSE(t2_calendar::is_business_day(ymd(2026, 10, 10)));
  CHECK_FALSE(t2_calendar::is_business_day(ymd(2026, 10, 11)));
  CHECK(t2_calendar::is_business_day(ymd(2026, 10, 9)));
  CHECK(t2_calendar::is_business_day(ymd(2026, 10, 12)));
}

TEST_CASE("t2_calendar: fixed holidays", "[t2_calendar]")
{
  CHECK_FALSE(t2_calendar::is_business_day(ymd(2026, 1, 1)));
  CHECK_FALSE(t2_calendar::is_business_day(ymd(2026, 5, 1)));
  CHECK_FALSE(t2_calendar::is_business_day(ymd(2026, 12, 25)));
  CHECK_FALSE(t2_calendar::is_business_day(ymd(2026, 12, 26)));
  CHECK(t2_calendar::is_business_day(ymd(2026, 12, 24)));
  CHECK(t2_calendar::is_business_day(ymd(2026, 12, 28)));
  CHECK(t2_calendar::is_business_day(ymd(2026, 12, 31)));
}

TEST_CASE("t2_calendar: Good Friday and Easter Monday", "[t2_calendar]")
{
  CHECK_FALSE(t2_calendar::is_business_day(ymd(2026, 4, 3)));
  CHECK_FALSE(t2_calendar::is_business_day(ymd(2026, 4, 6)));
  CHECK(t2_calendar::is_business_day(ymd(2026, 4, 2)));
  CHECK(t2_calendar::is_business_day(ymd(2026, 4, 7)));
}

TEST_CASE("t2_calendar: national holidays are business days", "[t2_calendar]")
{
  CHECK(t2_calendar::is_business_day(ymd(2026, 6, 25)));
  CHECK(t2_calendar::is_business_day(ymd(2025, 8, 15)));
}

TEST_CASE("t2_calendar: invalid date", "[t2_calendar]")
{
  const auto bad = ymd(2026, 2, 30);
  CHECK_FALSE(t2_calendar::is_business_day(bad));
  CHECK_FALSE(t2_calendar::is_holiday(bad));
  const auto res = t2_calendar::add_business_days(bad, 1);
  REQUIRE_FALSE(res.has_value());
  CHECK(res.error().code == os3::util::t2_err_code::invalid_date);
  CHECK(res.error().msg.contains("2026-02-30"));
}

TEST_CASE("t2_calendar: add_business_days forward", "[t2_calendar]")
{
  CHECK(*t2_calendar::add_business_days(ymd(2026, 10, 8), 0) == ymd(2026, 10, 8));
  CHECK(*t2_calendar::add_business_days(ymd(2026, 10, 8), 1) == ymd(2026, 10, 9));
  CHECK(*t2_calendar::add_business_days(ymd(2026, 10, 8), 2) == ymd(2026, 10, 12));
  CHECK(*t2_calendar::add_business_days(ymd(2026, 10, 8), 15) == ymd(2026, 10, 29));
  CHECK(*t2_calendar::add_business_days(ymd(2026, 10, 10), 1) == ymd(2026, 10, 12));
}

TEST_CASE("t2_calendar: add_business_days skips holidays", "[t2_calendar]")
{
  CHECK(*t2_calendar::add_business_days(ymd(2026, 4, 2), 1) == ymd(2026, 4, 7));
  CHECK(*t2_calendar::add_business_days(ymd(2026, 12, 24), 1) == ymd(2026, 12, 28));
  CHECK(*t2_calendar::add_business_days(ymd(2026, 12, 31), 1) == ymd(2027, 1, 4));
  CHECK(*t2_calendar::add_business_days(ymd(2026, 4, 30), 1) == ymd(2026, 5, 4));
}

TEST_CASE("t2_calendar: add_business_days backward", "[t2_calendar]")
{
  CHECK(*t2_calendar::add_business_days(ymd(2026, 10, 12), -1) == ymd(2026, 10, 9));
  CHECK(*t2_calendar::add_business_days(ymd(2026, 4, 7), -1) == ymd(2026, 4, 2));
  CHECK(*t2_calendar::add_business_days(ymd(2027, 1, 4), -1) == ymd(2026, 12, 31));
  CHECK(*t2_calendar::add_business_days(ymd(2026, 10, 29), -15) == ymd(2026, 10, 8));
}
