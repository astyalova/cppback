#pragma once

#include <string>
#include <cctype>

/*
Возвращает URL-декодированное представление строки str.
Пример: "Hello+World%20%21" должна превратиться в "Hello World !"
В случае ошибки выбрасывает исключение std::invalid_argument
*/

int check(char a);
int hexByteFromTwoChars(char a, char b);
std::string UrlDecode(std::string_view str);
