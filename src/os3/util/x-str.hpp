#pragma once

#include <xercesc/util/XMLString.hpp>
#include <compare>
#include <cstddef>
#include <functional>
#include <string>
#include <string_view>

namespace os3::util
{
  using cstr_t = std::string_view;
  using str_t  = std::string;

  [[nodiscard]] std::size_t    utf8_length(std::u16string_view text) noexcept;
  [[nodiscard]] std::size_t    utf16_length(cstr_t text) noexcept;
  void                         append_utf8(str_t& out, std::u16string_view text);
  void                         append_utf8(str_t& out, std::u16string_view text, char16_t& pending_high);
  [[nodiscard]] str_t          to_utf8(std::u16string_view text);
  [[nodiscard]] std::u16string to_utf16(cstr_t text);

  class x_str
  {
  public:
    x_str() noexcept = default;
    ~x_str();
    explicit x_str(XMLCh* ptr) noexcept;
    x_str(const XMLCh* ptr, XMLSize_t size);
    explicit x_str(cstr_t utf8);
    explicit x_str(std::u16string_view u16);
    x_str(const x_str& other);
    x_str(x_str&& other) noexcept;
    /// operators
    x_str&                       operator=(const x_str& other);
    x_str&                       operator=(x_str&& other) noexcept;
    void                         assign(const XMLCh* other);
    void                         assign(cstr_t other);
    void                         reset() noexcept;
    void                         reset(XMLCh* ptr) noexcept;
    void                         reset(XMLCh* ptr, XMLSize_t size) noexcept;
    [[nodiscard]] str_t          to_string() const;
    [[nodiscard]] std::u16string to_u16string() const;
    [[nodiscard]] cstr_t         to_string_view() const;
    bool                         operator==(const x_str& other) const noexcept;
    bool                         operator!=(const x_str& other) const noexcept;
    std::strong_ordering         operator<=>(const x_str& other) const noexcept;
    bool                         operator==(const XMLCh* other) const noexcept;
    bool                         operator!=(const XMLCh* other) const noexcept;
    std::strong_ordering         operator<=>(const XMLCh* other) const noexcept;
    bool                         operator==(cstr_t utf8) const;
    bool                         operator!=(cstr_t utf8) const;
    bool                         operator==(std::u16string_view u16) const;
    bool                         operator!=(std::u16string_view u16) const;
    explicit                     operator const XMLCh*() const noexcept;
    [[nodiscard]] const XMLCh*   c_str() const noexcept;
    [[nodiscard]] XMLCh*         data() const noexcept;
    [[nodiscard]] bool           empty() const noexcept;
    [[nodiscard]] std::size_t    length() const noexcept;
  private:
    XMLCh*        data_ = nullptr;
    XMLSize_t     size_ = 0;
    mutable str_t cached_utf8_; //< utf8 equivalent of data_
  };
} // namespace os3::util

// Hash specializacija
template <>
struct std::hash<os3::util::x_str>
{
  std::size_t operator()(const os3::util::x_str& s) const { return std::hash<os3::util::str_t>{}(s.to_string()); }
};
