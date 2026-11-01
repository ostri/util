#include "os3/util/x-str.hpp"
#include <xercesc/util/PlatformUtils.hpp>
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <span>
#include <utility>

namespace os3::util
{
  namespace
  {
    constexpr std::uint32_t one_byte_end        = 0x80;
    constexpr std::uint32_t two_byte_end        = 0x800;
    constexpr std::uint32_t supplementary_first = 0x10000;
    constexpr std::uint32_t high_first          = 0xD800;
    constexpr std::uint32_t high_last           = 0xDBFF;
    constexpr std::uint32_t low_first           = 0xDC00;
    constexpr std::uint32_t low_last            = 0xDFFF;
    constexpr std::uint32_t replacement_char    = 0xFFFD;
    constexpr std::uint32_t surrogate_bits      = 10;
    constexpr std::uint32_t six_bits            = 6;
    constexpr std::uint32_t twelve_bits         = 12;
    constexpr std::uint32_t eighteen_bits       = 18;
    constexpr std::uint32_t low_six             = 0x3F;
    constexpr std::uint32_t low_ten             = 0x3FF;
    constexpr std::uint32_t follow_byte         = 0x80;
    constexpr std::uint32_t lead_two            = 0xC0;
    constexpr std::uint32_t lead_three          = 0xE0;
    constexpr std::uint32_t lead_four           = 0xF0;
    constexpr std::uint32_t lead_two_first      = 0xC2;
    constexpr std::uint32_t lead_two_last       = 0xDF;
    constexpr std::uint32_t lead_three_last     = 0xEF;
    constexpr std::uint32_t lead_four_last      = 0xF4;
    constexpr std::uint32_t follow_last         = 0xBF;
    constexpr std::uint32_t lead_three_low      = 0xE0;
    constexpr std::uint32_t lead_three_surr     = 0xED;
    constexpr std::uint32_t lead_four_low       = 0xF0;
    constexpr std::uint32_t follow_after_e0     = 0xA0;
    constexpr std::uint32_t follow_before_surr  = 0x9F;
    constexpr std::uint32_t follow_after_f0     = 0x90;
    constexpr std::uint32_t follow_before_f4    = 0x8F;
    constexpr std::uint32_t mask_two            = 0x1F;
    constexpr std::uint32_t mask_three          = 0x0F;
    constexpr std::uint32_t mask_four           = 0x07;

    [[nodiscard]] std::size_t utf8_size(std::uint32_t c) noexcept
    {
      if (c < one_byte_end) return 1;
      if (c < two_byte_end) return 2;
      if (c < supplementary_first) return 3;
      return 4;
    }

    template <typename It>
    It put_utf8(It at, std::uint32_t c)
    {
      const auto put = [&at](std::uint32_t byte) { *at++ = static_cast<char>(byte); };
      if (c < one_byte_end)
      {
        put(c);
        return at;
      }
      if (c < two_byte_end)
      {
        put(lead_two | (c >> six_bits));
        put(follow_byte | (c & low_six));
        return at;
      }
      if (c < supplementary_first)
      {
        put(lead_three | (c >> twelve_bits));
        put(follow_byte | ((c >> six_bits) & low_six));
        put(follow_byte | (c & low_six));
        return at;
      }
      put(lead_four | (c >> eighteen_bits));
      put(follow_byte | ((c >> twelve_bits) & low_six));
      put(follow_byte | ((c >> six_bits) & low_six));
      put(follow_byte | (c & low_six));
      return at;
    }

    template <typename It>
    It put_utf16(It at, std::uint32_t c)
    {
      if (c < supplementary_first)
      {
        *at++ = static_cast<char16_t>(c);
        return at;
      }
      const auto v = c - supplementary_first;
      *at++        = static_cast<char16_t>(high_first + (v >> surrogate_bits));
      *at++        = static_cast<char16_t>(low_first + (v & low_ten));
      return at;
    }

    template <typename Fn>
    std::uint32_t walk_utf16(std::u16string_view text, std::uint32_t high, Fn fn)
    {
      for (const char16_t unit : text)
      {
        const auto c = static_cast<std::uint32_t>(unit);
        if (high != 0)
        {
          if (c >= low_first && c <= low_last)
          {
            fn(supplementary_first + ((high - high_first) << surrogate_bits) + (c - low_first));
            high = 0;
            continue;
          }
          fn(replacement_char);
          high = 0;
        }
        if (c >= high_first && c <= high_last)
        {
          high = c;
          continue;
        }
        fn(c >= low_first && c <= low_last ? replacement_char : c);
      }
      return high;
    }

    struct utf8_lead
    {
      std::size_t   follow = 0;
      std::uint32_t value  = 0;
      std::uint32_t low    = follow_byte;
      std::uint32_t high   = follow_last;
    };

    [[nodiscard]] utf8_lead lead_of(std::uint32_t b) noexcept
    {
      if (b >= lead_two_first && b <= lead_two_last) return {.follow = 1, .value = b & mask_two};
      if (b >= lead_three_low && b <= lead_three_last)
      {
        return {.follow = 2,
                .value  = b & mask_three,
                .low    = b == lead_three_low ? follow_after_e0 : follow_byte,
                .high   = b == lead_three_surr ? follow_before_surr : follow_last};
      }
      if (b >= lead_four_low && b <= lead_four_last)
      {
        return {.follow = 3,
                .value  = b & mask_four,
                .low    = b == lead_four_low ? follow_after_f0 : follow_byte,
                .high   = b == lead_four_last ? follow_before_f4 : follow_last};
      }
      return {};
    }

    template <typename Fn>
    void walk_utf8(cstr_t text, Fn fn)
    {
      utf8_lead     lead;
      std::size_t   done = 0;
      std::uint32_t c    = 0;
      for (const char ch : text)
      {
        const auto b = static_cast<std::uint32_t>(static_cast<unsigned char>(ch));
        if (lead.follow != 0)
        {
          const auto low  = done == 0 ? lead.low : follow_byte;
          const auto high = done == 0 ? lead.high : follow_last;
          if (b >= low && b <= high)
          {
            c = (c << six_bits) | (b & low_six);
            if (++done == lead.follow)
            {
              fn(c);
              lead = {};
            }
            continue;
          }
          fn(replacement_char);
          lead = {};
        }
        if (b < one_byte_end)
        {
          fn(b);
          continue;
        }
        lead = lead_of(b);
        if (lead.follow == 0)
        {
          fn(replacement_char);
          continue;
        }
        c    = lead.value;
        done = 0;
      }
      if (lead.follow != 0) { fn(replacement_char); }
    }

    [[nodiscard]] XMLCh* allocate_units(std::size_t units)
    { return static_cast<XMLCh*>(xercesc::XMLPlatformUtils::fgMemoryManager->allocate((units + 1) * sizeof(XMLCh))); }

    [[nodiscard]] XMLCh* copy_units(const XMLCh* ptr, std::size_t units)
    {
      XMLCh*                 out = allocate_units(units);
      const std::span<XMLCh> dst(out, units + 1);
      const auto             end = std::copy_n(ptr, units, dst.begin());
      *end                       = 0;
      return out;
    }
  } // namespace

  std::size_t utf8_length(std::u16string_view text) noexcept
  {
    std::size_t n    = 0;
    const auto  high = walk_utf16(text, 0, [&n](std::uint32_t c) { n += utf8_size(c); });
    return high != 0 ? n + utf8_size(replacement_char) : n;
  }

  std::size_t utf16_length(cstr_t text) noexcept
  {
    std::size_t n = 0;
    walk_utf8(text, [&n](std::uint32_t c) { n += c < supplementary_first ? 1 : 2; });
    return n;
  }

  void append_utf8(str_t& out, std::u16string_view text)
  {
    const auto old = out.size();
    out.resize_and_overwrite(old + utf8_length(text),
                             [old, text](char* p, std::size_t n)
                             {
                               const std::span<char> dst(p, n);
                               auto                  at   = std::next(dst.begin(), static_cast<std::ptrdiff_t>(old));
                               const auto            high = walk_utf16(text, 0, [&at](std::uint32_t c) { at = put_utf8(at, c); });
                               if (high != 0) { put_utf8(at, replacement_char); }
                               return n;
                             });
  }

  void append_utf8(str_t& out, std::u16string_view text, char16_t& pending_high)
  {
    std::size_t add  = 0;
    const auto  high = walk_utf16(text, pending_high, [&add](std::uint32_t c) { add += utf8_size(c); });
    const auto  old  = out.size();
    out.resize_and_overwrite(old + add,
                             [old, text, pending = pending_high](char* p, std::size_t n)
                             {
                               const std::span<char> dst(p, n);
                               auto                  at = std::next(dst.begin(), static_cast<std::ptrdiff_t>(old));
                               walk_utf16(text, pending, [&at](std::uint32_t c) { at = put_utf8(at, c); });
                               return n;
                             });
    pending_high = static_cast<char16_t>(high);
  }

  str_t to_utf8(std::u16string_view text)
  {
    str_t out;
    append_utf8(out, text);
    return out;
  }

  std::u16string to_utf16(cstr_t text)
  {
    std::u16string out;
    out.resize_and_overwrite(utf16_length(text),
                             [text](char16_t* p, std::size_t n)
                             {
                               const std::span<char16_t> dst(p, n);
                               auto                      at = dst.begin();
                               walk_utf8(text, [&at](std::uint32_t c) { at = put_utf16(at, c); });
                               return n;
                             });
    return out;
  }

  x_str::x_str(XMLCh* ptr) noexcept
  : data_(xercesc::XMLString::replicate(ptr))
  , size_(xercesc::XMLString::stringLen(data_))
  {
  }

  x_str::x_str(const XMLCh* ptr, XMLSize_t size)
  {
    if (ptr != nullptr && size != 0) { reset(copy_units(ptr, size), size); }
  }

  x_str::~x_str() { reset(); }

  x_str::x_str(cstr_t utf8)
  {
    if (utf8.empty()) return;
    const auto             units = utf16_length(utf8);
    XMLCh*                 out   = allocate_units(units);
    const std::span<XMLCh> dst(out, units + 1);
    auto                   at = dst.begin();
    walk_utf8(utf8, [&at](std::uint32_t c) { at = put_utf16(at, c); });
    *at = 0;
    reset(out, units);
  }

  x_str::x_str(std::u16string_view u16)
  : x_str(u16.data(), u16.size())
  {
  }

  x_str::x_str(const x_str& other)
  : x_str(other.data_, other.size_)
  {
  }

  x_str::x_str(x_str&& other) noexcept
  : data_(std::exchange(other.data_, nullptr))
  , size_(std::exchange(other.size_, 0))
  , cached_utf8_(std::move(other.cached_utf8_))
  { other.cached_utf8_.clear(); }

  x_str& x_str::operator=(const x_str& other)
  {
    if (this == &other) return *this;
    if (other.data_ == nullptr)
    {
      reset();
      return *this;
    }
    reset(copy_units(other.data_, other.size_), other.size_);
    return *this;
  }

  x_str& x_str::operator=(x_str&& other) noexcept
  {
    if (this != &other)
    {
      reset(std::exchange(other.data_, nullptr), std::exchange(other.size_, 0));
      cached_utf8_ = std::move(other.cached_utf8_);
      other.cached_utf8_.clear();
    }
    return *this;
  }

  void x_str::assign(const XMLCh* other)
  {
    if (other == nullptr)
    {
      reset();
      return;
    }
    reset(copy_units(other, xercesc::XMLString::stringLen(other)), xercesc::XMLString::stringLen(other));
  }

  void x_str::assign(const cstr_t other)
  {
    x_str tmp(other);
    *this = std::move(tmp);
  }

  void x_str::reset() noexcept
  {
    if (data_ != nullptr) { xercesc::XMLString::release(&data_); }
    data_ = nullptr;
    size_ = 0;
    cached_utf8_.clear();
  }

  void x_str::reset(XMLCh* ptr) noexcept { reset(ptr, xercesc::XMLString::stringLen(ptr)); }

  void x_str::reset(XMLCh* ptr, XMLSize_t size) noexcept
  {
    if (data_ != nullptr && data_ != ptr) { xercesc::XMLString::release(&data_); }
    data_ = ptr;
    size_ = ptr != nullptr ? size : 0;
    cached_utf8_.clear();
  }

  str_t x_str::to_string() const { return str_t(to_string_view()); }

  cstr_t x_str::to_string_view() const
  {
    if (empty()) return {};
    if (cached_utf8_.empty()) { append_utf8(cached_utf8_, std::u16string_view(data_, size_)); }
    return cached_utf8_;
  }

  std::u16string x_str::to_u16string() const
  {
    if (empty()) return {};
    return {data_, size_};
  }

  bool x_str::operator==(const x_str& other) const noexcept { return xercesc::XMLString::equals(data_, other.data_); }

  bool x_str::operator!=(const x_str& other) const noexcept { return ! (*this == other); }

  std::strong_ordering x_str::operator<=>(const x_str& other) const noexcept
  { return xercesc::XMLString::compareString(data_, other.data_) <=> 0; }

  bool x_str::operator==(const XMLCh* other) const noexcept { return xercesc::XMLString::equals(data_, other); }

  bool x_str::operator!=(const XMLCh* other) const noexcept { return ! (*this == other); }

  std::strong_ordering x_str::operator<=>(const XMLCh* other) const noexcept
  { return xercesc::XMLString::compareString(data_, other) <=> 0; }

  bool x_str::operator==(cstr_t utf8) const
  {
    const x_str temp(utf8);
    return *this == temp;
  }

  bool x_str::operator!=(cstr_t utf8) const { return ! (*this == utf8); }

  bool x_str::operator==(std::u16string_view u16) const { return std::u16string_view(empty() ? u"" : data_, size_) == u16; }

  bool x_str::operator!=(std::u16string_view u16) const { return ! (*this == u16); }

  x_str::operator const XMLCh*() const noexcept { return data_; }

  const XMLCh* x_str::c_str() const noexcept { return data_; }

  XMLCh* x_str::data() const noexcept { return data_; }

  bool x_str::empty() const noexcept { return data_ == nullptr || size_ == 0; }

  std::size_t x_str::length() const noexcept { return size_; }
} // namespace os3::util
