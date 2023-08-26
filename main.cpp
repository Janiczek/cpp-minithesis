#include "pbt.h"

void test_constant() {
    run_test("constant(42) should always generate 42",
             Gen::constant(42),
             [](int num) {
                 if (num != 42) {
                     throw TestException("Got something other than what we put into the constant Gen!");
                 }
             });
}

void test_constant_bad() {
    run_test("constant(42) should always generate 100",
             Gen::constant(42),
             [](int num) {
                 if (num != 100) {
                     throw TestException("Got something other than the bad value we expected!");
                 }
             });
}

void test_unsigned_int() {
    run_test("unsigned_int(10) should never generate above 10",
             Gen::unsigned_int(10),
             [](unsigned int num) {
                 //std::cout << "Generated: " << num << std::endl;
                 if (num > 10) { throw TestException("Got something above 10: " + std::to_string(num)); }
             });
}

int main() {
    test_constant();
    test_constant_bad();
    test_unsigned_int();
    return 0;
}
