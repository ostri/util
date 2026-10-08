#include "os3/util/t2-calendar.hpp"
#include <fmt/format.h>

namespace os3::util
{
  namespace
  {
    using std::chrono::day;
    using std::chrono::days;
    using std::chrono::December;
    using std::chrono::January;
    using std::chrono::May;
    using std::chrono::month;
    using std::chrono::month_day;
    using std::chrono::Saturday;
    using std::chrono::Sunday;
    using std::chrono::sys_days;
    using std::chrono::weekday;
    using std::chrono::year;
    using std::chrono::year_month_day;

    constexpr unsigned christmas_day        = 25;
    constexpr unsigned st_stephens_day      = 26;
    constexpr int      easter_month_divisor = 31;

    bool is_weekend(sys_days day) noexcept
    {
      const weekday wd{day};
      return wd == Saturday || wd == Sunday;
    }

    bool is_fixed_holiday(year_month_day date) noexcept
    {
      const month_day md{date.month(), date.day()};
      return md == January / 1 || md == May / 1 || md == December / christmas_day || md == December / st_stephens_day;
    }
  } // namespace

  /**
   * @brief Gregorian Easter Sunday of @p y
   *
   * - Meeus/Jones/Butcher algorithm, integer arithmetic only
   *
   * @param y year to compute Easter for
   * @return Easter Sunday of that year
   */
  std::chrono::year_month_day t2_calendar::easter_sunday(year y) noexcept
  {
    const int yr = static_cast<int>(y);
    const int a  = yr % 19;
    const int b  = yr / 100;
    const int c  = yr % 100;
    const int d  = b / 4;
    const int e  = b % 4;
    const int f  = (b + 8) / 25;
    const int g  = (b - f + 1) / 3;
    const int h  = (19 * a + b - d - g + 15) % 30;
    const int i  = c / 4;
    const int k  = c % 4;
    const int l  = (32 + 2 * e + 2 * i - h - k) % 7;
    const int m  = (a + 11 * h + 22 * l) / 451;
    const int n  = h + l - 7 * m + 114;
    return y / month{static_cast<unsigned>(n / easter_month_divisor)} / day{static_cast<unsigned>(n % easter_month_divisor + 1)};
  }

  /**
   * @brief whether @p date is a weekend or a T2 holiday
   *
   * - closed: Saturday, Sunday, 1 Jan, Good Friday, Easter Monday, 1 May, 25 Dec, 26 Dec
   * - national holidays are not considered
   *
   * @param date date to test
   * @return true for a closed day; false for an open day or a non-existent date
   */
  bool t2_calendar::is_holiday(year_month_day date) noexcept
  {
    if (! date.ok()) return false;
    const sys_days sd{date};
    if (is_weekend(sd)) return true;
    if (is_fixed_holiday(date)) return true;
    const sys_days easter{easter_sunday(date.year())};
    return sd == easter - days{2} || sd == easter + days{1};
  }

  /**
   * @brief whether @p date is a T2 business day
   *
   * @param date date to test
   * @return true when the date exists and is not a weekend or T2 holiday
   */
  bool t2_calendar::is_business_day(year_month_day date) noexcept { return date.ok() && ! is_holiday(date); }

  /**
   * @brief moves @p date by @p count T2 business days
   *
   * - positive @p count moves forward, negative backward
   * - zero returns @p date unchanged, even when it is not a business day
   * - the result is always a business day when @p count is not zero
   *
   * @param date start date
   * @param count number of business days to move
   * @return the resulting date, or t2_err_code::invalid_date when @p date does not exist
   */
  e_date t2_calendar::add_business_days(year_month_day date, int count)
  {
    if (! date.ok())
      return std::unexpected(err_data{.code = t2_err_code::invalid_date,
                                      .msg  = fmt::format("invalid date {}-{:02}-{:02}",
                                                          static_cast<int>(date.year()),
                                                          static_cast<unsigned>(date.month()),
                                                          static_cast<unsigned>(date.day()))});
    sys_days   current{date};
    const days step{count >= 0 ? 1 : -1};
    for (int remaining = count >= 0 ? count : -count; remaining > 0;)
    {
      current += step;
      if (! is_holiday(year_month_day{current})) --remaining;
    }
    return year_month_day{current};
  }
} // namespace os3::util
