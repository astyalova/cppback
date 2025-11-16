#include <gtest/gtest.h>
#include "../src/urlencode.h"

using namespace std::literals;

TEST(UrlEncodeTestSuite, EmptyString) {
    EXPECT_EQ(UrlEncode(""sv), ""s);
}

TEST(UrlEncodeTestSuite, OrdinaryCharsAreNotEncoded) {
    EXPECT_EQ(UrlEncode("hello"sv), "hello"s);
    EXPECT_EQ(UrlEncode("ABCxyz123-_.~"sv), "ABCxyz123-_.~"s);
}

TEST(UrlEncodeTestSuite, ReservedCharsAreEncoded) {
    EXPECT_EQ(UrlEncode("!#$&'()*+,/:;=?@[]"sv),
              "%21%23%24%26%27%28%29%2B%2C%2F%3A%3B%3D%3F%40%5B%5D"s);
}

TEST(UrlEncodeTestSuite, SpacesAreConvertedToPlus) {
    EXPECT_EQ(UrlEncode("hello world"sv), "hello+world"s);
    EXPECT_EQ(UrlEncode(" a b c "sv), "+a+b+c+"s);
}

TEST(UrlEncodeTestSuite, NonAsciiCharsAreEncoded) {
    // символы с кодами < 31 и >= 128
    std::string input = "\x01\x02\x7F\x80\xFF";
    EXPECT_EQ(UrlEncode(input),
              "%01%02%7F%80%FF"s);
}

TEST(UrlEncodeTestSuite, MixedString) {
    EXPECT_EQ(UrlEncode("Hello, World! Привет!"sv),
              "Hello%2C+World%21+%D0%9F%D1%80%D0%B8%D0%B2%D0%B5%D1%82%21"s);
}
