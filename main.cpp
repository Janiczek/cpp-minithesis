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
    std::cout << "[" << name << "] " << to_string(result) << std::endl;
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
    std::cout << "[" << name << "] " << to_string(result) << std::endl;
}

void test_unsigned_int() {
    auto name = "unsigned_int(10) should never generate above 10";
    auto gen = Gen::unsigned_int(10);
    auto test = [](unsigned int num) {
      //std::cout << "Generated: " << num << std::endl;
      if (num > 10) { throw TestException("Got something above 10: " + std::to_string(num)); }
    };
    auto result = run(gen, test);
    std::cout << "[" << name << "] " << to_string(result) << std::endl;
}

int main() {
    test_constant();
    test_constant_bad();
    test_unsigned_int();
    return 0;
}
