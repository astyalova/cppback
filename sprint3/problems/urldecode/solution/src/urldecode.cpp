#include "urldecode.h"

#include <charconv>
#include <stdexcept>

#pragma once

#include <string>
#include <cctype>
/*
Возвращает URL-декодированное представление строки str.
Пример: "Hello+World%20%21" должна превратиться в "Hello World !"
В случае ошибки выбрасывает исключение std::invalid_argument
*/

int check(char a) {
    if(a >= 'A' && a <= 'F') {
        return a - 'A' + 10;
    } else if(a >= 'a' && a <= 'f') {
        return a - 'a' + 10;
    }
    throw std::invalid_argument("incorrect hex");
}

int hexByteFromTwoChars(char a, char b) {
    int a_num = 0;
    int b_num = 0;

    if(isdigit(a)) {
        a_num = a - '0';
    } else {
        a_num = check(a);
    }

    if(isdigit(b)) {
        b_num = b - '0';
    } else {
        b_num = check(b);
    }
    return a_num * 16 + b_num;
}

std::string UrlDecode(std::string_view str) {
    int n = str.size();
    int i = 0;
    std::string res;
    while(i < n) {
        if(str[i] == '%') {
            if(i + 2 < n) {
                int hex = hexByteFromTwoChars(str[i + 1], str[i + 2]);
                res += static_cast<char>(hex);
                i += 3;
            } else {
                throw std::invalid_argument("incorrect str");
            }
        } else if(str[i] == '+') {
            res += ' ';
            i++;
        } else {
            res += str[i];
            i++;
        }
    }
    return res;
}

