#include "json_logger.h"

#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/date_time.hpp>

namespace json_logger {

using namespace std::literals;
namespace keywords = boost::log::keywords;
namespace expr = boost::log::expressions;
namespace logging = boost::log;
namespace sinks = boost::log::sinks;
namespace json = boost::json;

BOOST_LOG_ATTRIBUTE_KEYWORD(timestamp, "TimeStamp", boost::posix_time::ptime)

void InitLogger() {
    logging::add_common_attributes();

    //set new format
    logging::add_console_log(
        std::cout,
        keywords::format = &LogFormatter,
        keywords::auto_flush = true
    );
}

void LogFormatter(boost::log::record_view const& rec, boost::log::formatting_ostream& strm) {
    auto ts = *rec[timestamp];

    auto message_object = json::object{
        {"timestamp"s, to_iso_extended_string(ts)}
    };

    message_object["data"] = *rec[additional_data];

    message_object["message"] = *rec[logging::expressions::smessage];

    strm << json::serialize(message_object);
}

void LogData(std::string_view message, const boost::json::value& additional_data_value) {
    BOOST_LOG_TRIVIAL(info) << boost::log::add_value(additional_data, additional_data_value) << message;
}

}  // namespace json_logger