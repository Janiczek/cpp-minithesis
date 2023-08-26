#include "pbt.h"
#include <iostream>

// TODO somehow abstract the boilerplate code in each test?

void test_constant() {
    auto name = "constant(42) should always generate 42";
    auto gen = Gen::constant(42);
    auto test = [](int num) {
        if (num != 42) {
            throw TestException("Got something other than what we put into the constant Gen!");
        }
    };
    auto result = run(gen, test);
    std::cout << name << ": " << to_string(result) << std::endl;
}

void test_constant_bad() {
    auto name = "constant(42) should always generate 100";
    auto gen = Gen::constant(42);
    auto test = [](int num) {
      if (num != 100) {
          throw TestException("Got something other than the bad value we expected!");
      }
    };
    auto result = run(gen, test);
    std::cout << name << ": " << to_string(result) << std::endl;
}

int main() {
    test_constant();
    test_constant_bad();
    return 0;
}
