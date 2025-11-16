#include "htmldecode.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <cctype>

std::string HtmlDecode(std::string_view str) {
    std::unordered_map<std::string, char> mnemonics = {
        {"lt", '<'},
        {"gt", '>'},
        {"amp", '&'},
        {"apos", '\''},
        {"quot", '"'}
    };

    std::string res;
    int n = str.size();
    int i = 0;

    while (i < n) {
        if (str[i] == '&') {
            int start = i + 1;
            while (i + 1 < n && (std::isalpha(str[i + 1]))) {
                i++;
            }
            std::string key = std::string(str.substr(start, i - start + 1));

            bool is_upper = true, is_lower = true;
            for (char c : key) {
                if (!std::isupper(c)) is_upper = false;
                if (!std::islower(c)) is_lower = false;
            }

            if (is_upper || is_lower) {
                std::string key_lower;
                for (char c : key) key_lower += std::tolower(c);

                if (mnemonics.count(key_lower)) {
                    res += mnemonics[key_lower];
                    if (i + 1 < n && str[i + 1] == ';') i++; 
                    i++;
                    continue;
                }
            }
            res += '&';
        } else {
            res += str[i];
        }
        i++;
    }

    return res;
}
