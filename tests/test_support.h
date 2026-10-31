#pragma once

#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

inline void require(const bool condition, const std::string_view message)
{
    if (!condition)
    {
        throw std::runtime_error(std::string(message));
    }
}

template<typename Test>
int runTests(Test test)
{
    try
    {
        test();
        std::cout << "Passed\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
