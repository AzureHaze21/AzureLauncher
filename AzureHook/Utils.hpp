#pragma once

#include <random>
#pragma comment(lib, "crypt32")

namespace Utils
{
    static const auto EndsWith(std::wstring const& fullString, std::wstring const& ending)
    {
        return fullString.length() >= ending.length() ? !fullString.compare(fullString.length() - ending.length(), ending.length(), ending) : false;
    }

    static std::string RandomString(int size)
    {
        static std::random_device rd;
        static std::mt19937 generator(rd());

        std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

        std::shuffle(str.begin(), str.end(), generator);

        return str.substr(0, size);
    }
}