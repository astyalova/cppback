#include "urlencode.h"
#include <cctype>
#include <sstream>
#include <iomanip>

/*
 * URL-кодирует строку str.
 * Пробел заменяется на +,
 * Символы, отличные от букв английского алфавита, цифр и -._~ а также зарезервированные символы
 * заменяются на их %-кодированные последовательности.
 * Зарезервированные символы: !#$&'()*+,/:;=?@[]
 */

std::string UrlEncode(std::string_view str) {
    std::string res;
    const std::string reserved = "!#$&'()*+,/:;=?@[]";

    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            res += c;
        } else if (c == ' ') {
            res += '+'; 
        } else if (reserved.find(c) != std::string::npos) {
            std::ostringstream oss;
            oss << '%' << std::uppercase << std::hex << (static_cast<int>(static_cast<unsigned char>(c)));
            res += oss.str();
        } else {
            std::ostringstream oss;
            oss << '%' << std::uppercase << std::hex << (static_cast<int>(static_cast<unsigned char>(c)));
            res += oss.str();
        }
    }

    return res;
}
