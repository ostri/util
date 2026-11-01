#include "os3/util/x-str.hpp"
#include "os3/util/xerces-mgr.hpp"
#include <catch2/catch_test_macros.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace
{
  const os3::util::xerces_mgr g_xerces_guard; // NOLINT(cert-err58-cpp, bugprone-throwing-static-initialization)

  constexpr std::string_view    latin_utf8  = "\xC5\xA1\xC4\x91\xC4\x8D\xC4\x87\xC5\xBE\xC5\xA0\xC4\x90\xC4\x8C\xC4\x86\xC5\xBD";
  constexpr std::u16string_view latin_utf16 = u"šđčćžŠĐČĆŽ";
  constexpr std::string_view    emoji_utf8  = "\xF0\x9F\x98\x80";
  constexpr std::u16string_view emoji_utf16 = u"\U0001F600";
  constexpr std::string_view    euro_utf8   = "\xE2\x82\xAC";
  constexpr std::string_view    replacement = "\xEF\xBF\xBD";
} // namespace

using os3::util::x_str;

TEST_CASE("to_utf8 and to_utf16 convert ASCII, letters with diacritics, three and four byte characters", "[x_str][utf]")
{
  CHECK(os3::util::to_utf8(u"MsgId-1") == "MsgId-1");
  CHECK(os3::util::to_utf8(latin_utf16) == latin_utf8);
  CHECK(os3::util::to_utf8(u"€") == euro_utf8);
  CHECK(os3::util::to_utf8(emoji_utf16) == emoji_utf8);
  CHECK(os3::util::to_utf16("MsgId-1") == u"MsgId-1");
  CHECK(os3::util::to_utf16(latin_utf8) == latin_utf16);
  CHECK(os3::util::to_utf16(euro_utf8) == u"€");
  CHECK(os3::util::to_utf16(emoji_utf8) == emoji_utf16);
  CHECK(os3::util::to_utf8(u"").empty());
  CHECK(os3::util::to_utf16("").empty());
}

TEST_CASE("utf8_length and utf16_length give the exact size of the conversion", "[x_str][utf]")
{
  CHECK(os3::util::utf8_length(latin_utf16) == latin_utf8.size());
  CHECK(os3::util::utf8_length(emoji_utf16) == 4);
  CHECK(os3::util::utf16_length(latin_utf8) == latin_utf16.size());
  CHECK(os3::util::utf16_length(emoji_utf8) == 2);
}

TEST_CASE("a lone surrogate becomes the replacement character", "[x_str][utf][negative]")
{
  const std::u16string high_alone{char16_t{0xD83D}, u'A'};
  const std::u16string low_alone{u'A', char16_t{0xDE00}};
  const std::u16string high_last{u'A', char16_t{0xD83D}};
  CHECK(os3::util::to_utf8(high_alone) == std::string(replacement) + "A");
  CHECK(os3::util::to_utf8(low_alone) == "A" + std::string(replacement));
  CHECK(os3::util::to_utf8(high_last) == "A" + std::string(replacement));
}

TEST_CASE("ill formed UTF-8 becomes the replacement character", "[x_str][utf][negative]")
{
  CHECK(os3::util::to_utf16("\x80") == u"�");
  CHECK(os3::util::to_utf16("A\xC5") == u"A�");
  CHECK(os3::util::to_utf16("\xC0\xAF") == u"��");
  CHECK(os3::util::to_utf16("\xED\xA0\x80") == u"���");
  CHECK(os3::util::to_utf16("\xF4\x90\x80\x80") == u"����");
  CHECK(os3::util::to_utf16("\xC5\xA1"
                            "B") == u"šB");
}

TEST_CASE("append_utf8 with a pending surrogate joins a pair split between two calls", "[x_str][utf]")
{
  std::string out;
  char16_t    pending = 0;
  os3::util::append_utf8(out, emoji_utf16.substr(0, 1), pending);
  CHECK(out.empty());
  CHECK(pending != 0);
  os3::util::append_utf8(out, emoji_utf16.substr(1), pending);
  CHECK(out == emoji_utf8);
  CHECK(pending == 0);
}

TEST_CASE("append_utf8 appends after the text already in the string", "[x_str][utf]")
{
  std::string out = "/Doc/";
  os3::util::append_utf8(out, latin_utf16);
  CHECK(out == "/Doc/" + std::string(latin_utf8));
}

TEST_CASE("x_str keeps letters with diacritics from UTF-8 and gives them back", "[x_str][utf]")
{
  const x_str s(latin_utf8);
  CHECK(s.length() == latin_utf16.size());
  CHECK(s.to_u16string() == latin_utf16);
  CHECK(s.to_string() == latin_utf8);
  CHECK(s == latin_utf8);
  CHECK(s == latin_utf16);
}

TEST_CASE("x_str from a u16string_view takes only the units of the view", "[x_str][utf]")
{
  const std::u16string text = u"FIToFICstmrCdtTrf";
  const x_str          s(std::u16string_view(text).substr(0, 4));
  CHECK(s.length() == 4);
  CHECK(s.to_string() == "FITo");
}

TEST_CASE("x_str from a pointer and a length copies that many units", "[x_str][utf]")
{
  const std::u16string text = u"CdtrAgt";
  const x_str          s(text.data(), 4);
  CHECK(s.length() == 4);
  CHECK(s.to_string() == "Cdtr");
  CHECK(x_str(text.data(), 0).empty());
  CHECK(x_str(nullptr, 3).empty());
}

TEST_CASE("x_str keeps its length through a move and leaves the source empty", "[x_str][utf]")
{
  x_str       source(emoji_utf8);
  const x_str target(std::move(source));
  CHECK(target.length() == 2);
  CHECK(target.to_string() == emoji_utf8);
  CHECK(source.length() == 0); // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved,readability-container-size-empty)
  CHECK(source.empty());       // NOLINT(bugprone-use-after-move,hicpp-invalid-access-moved)
}
