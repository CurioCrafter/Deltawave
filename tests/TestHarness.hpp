#pragma once

#include <iostream>
#include <stdexcept>
#include <string>

namespace viz::tests {

using TestFn = void (*)();

struct TestCase {
    const char* name;
    TestFn fn;
};

inline void require(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

inline int runTests(const TestCase* tests, int count)
{
    int failures = 0;
    for (int i = 0; i < count; ++i) {
        try {
            tests[i].fn();
            std::cout << "[pass] " << tests[i].name << "\n";
        } catch (const std::exception& error) {
            ++failures;
            std::cerr << "[fail] " << tests[i].name << ": " << error.what() << "\n";
        }
    }

    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    return 0;
}

} // namespace viz::tests
