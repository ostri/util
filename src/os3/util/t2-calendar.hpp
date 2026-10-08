#pragma once
/**
 * @file
 * @brief t2_calendar - Target 2 (T2) business day calendar
 */
#include "os3/util/err-data.hpp"
#include <chrono>
#include <expected>

namespace os3::util
{
  /// @brief a date or why it could not be produced
  using e_date = std::expected<std::chrono::year_month_day, err_data>;

  /// @brief err_data::code values produced by t2_calendar
  namespace t2_err_code
  {
    inline constexpr int invalid_date = 170; ///< the input date does not exist
  } // namespace t2_err_code

  /// @brief T2 calendar: weekends, 1 Jan, Good Friday, Easter Monday, 1 May, 25 and 26 Dec are closed
  class t2_calendar
  {
  public:
    /// @brief whether the date is a weekend or a T2 holiday - false for a non-existent date
    [[nodiscard]] static bool is_holiday(std::chrono::year_month_day date) noexcept;


    /// @brief whether the date exists and is a T2 business day
    [[nodiscard]] static bool is_business_day(std::chrono::year_month_day date) noexcept;


    /// @brief the date @p count T2 business days away - backwards for negative @p count
    [[nodiscard]] static e_date add_business_days(std::chrono::year_month_day date, int count);


    /// @brief Gregorian Easter Sunday of the given year
    [[nodiscard]] static std::chrono::year_month_day easter_sunday(std::chrono::year year) noexcept;
  };
} // namespace os3::util
