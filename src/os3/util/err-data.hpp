#pragma once
/**
 * @file
 * @brief err_data - minimal error payload of the util classes (code and message)
 */
#include <string>

namespace os3::util
{
  using str_t = std::string;

  /// @brief error payload carried by std::unexpected
  struct err_data
  {
    int   code; ///< numeric code, one of err_code::*
    str_t msg;  ///< text describing why
  };

  /// @brief err_data::code values produced by util classes
  namespace err_code
  {
    inline constexpr int sf_number_pool_exhausted = 160; ///< sf_number: all 1024 node ids are already claimed by other live processes
    inline constexpr int sf_number_name_too_long  = 161; ///< sf_number: the name root does not leave room for the ".<id>" suffix
    inline constexpr int sf_number_socket_failed  = 162; ///< sf_number: socket()/bind() failed for a reason other than "slot taken"
  } // namespace err_code
} // namespace os3::util
