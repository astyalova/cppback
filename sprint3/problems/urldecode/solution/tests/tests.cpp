#define BOOST_TEST_MODULE urlencode tests
#include <boost/test/unit_test.hpp>

#include "../src/urldecode.h"

BOOST_AUTO_TEST_CASE(UrlDecode_tests) {
    using namespace std::literals;

    BOOST_TEST(UrlDecode(""sv) == ""s);
    BOOST_TEST(UrlDecode("Hello mem!"sv) == "Hello mem!"s);
    BOOST_TEST(UrlDecode("H"sv) == "H"s);
    BOOST_TEST(UrlDecode("HeLlO%20%4De%6D!"sv) == "Hello Mem!"s); 
    BOOST_CHECK_THROW(UrlDecode("HeLlO%G1%6D!"sv), std::invalid_argument);
    BOOST_CHECK_THROW(UrlDecode("HeLlO%G %6D!"sv), std::invalid_argument);
    BOOST_TEST(UrlDecode("HeLlO + mem + !"sv) == "Hello   mem   !"s);
}