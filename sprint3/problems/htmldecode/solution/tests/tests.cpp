#include <catch2/catch_test_macros.hpp>

#include "../src/htmldecode.h"

using namespace std::literals;

TEST_CASE("Text without mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode(""sv) == ""s);
    CHECK(HtmlDecode("hello"sv) == "hello"s);
}

TEST_CASE("Text with HTML mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode("M&amp;M&apos;s"sv) == "M&M's"s);
    CHECK(HtmlDecode("5 &lt; 10 &gt; 2"sv) == "5 < 10 > 2"s);
    CHECK(HtmlDecode("Quote: &quot;Hello&quot;"sv) == "Quote: \"Hello\""s);
}

TEST_CASE("Empty string", "[HtmlDecode]") {
    CHECK(HtmlDecode(""sv) == ""s);
}

TEST_CASE("Mnemonics in uppercase", "[HtmlDecode]") {
    CHECK(HtmlDecode("&AMP;&LT;&GT;&APOS;&QUOT;"sv) == "&<>'\""s);
}

TEST_CASE("Mnemonics in mixed case", "[HtmlDecode]") {
    CHECK(HtmlDecode("&aMp;&Lt;&Gt;"sv) == "&aMp;&Lt;&Gt;"s);
}

TEST_CASE("Mnemonics at beginning, middle, and end", "[HtmlDecode]") {
    CHECK(HtmlDecode("&lt;start&gt; middle &amp; end&lt;"sv) == "<start> middle & end<"s);
}

TEST_CASE("Incomplete mnemonics", "[HtmlDecode]") {
    CHECK(HtmlDecode("&am"sv) == "&am"s);
    CHECK(HtmlDecode("&q"sv) == "&q"s);
    CHECK(HtmlDecode("&apos"sv) == "&apos"s);
}

TEST_CASE("Mnemonics with and without semicolon", "[HtmlDecode]") {
    CHECK(HtmlDecode("&lt"sv) == "<"s);
    CHECK(HtmlDecode("&gt;"sv) == ">"s);
    CHECK(HtmlDecode("&amp"sv) == "&"s);
    CHECK(HtmlDecode("&quot"sv) == "\""s);
    CHECK(HtmlDecode("&apos;"sv) == "'"s);
}
