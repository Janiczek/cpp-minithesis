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

void test_constant_shrinking() {
    run_test("constant(42) - does a failure not shrink?",
             Gen::constant(42),
             [](int num) {
                 if (num != 100) { throw TestException("Should be still 42 after shrinking"); }
             });
}

void test_unsigned_int_max_bounds() {
    run_test("unsigned_int(10) should generate 0..10 inclusive",
             Gen::unsigned_int(10),
             [](unsigned int num) {
                 if (num < 0)  { throw TestException("Got something below 0: "  + std::to_string(num)); }
                 if (num > 10) { throw TestException("Got something above 10: " + std::to_string(num)); }
             });
}

void test_unsigned_int_max_shrinking() {
    run_test("unsigned_int(10) - does a failure shrink to 0?",
             Gen::unsigned_int(10),
             [](unsigned int num) {
               if (num <= 10) { throw TestException("Should be shrunk to 0"); }
             });
}

void test_unsigned_int_min_max_bounds() {
    run_test("unsigned_int(3,10) should generate 3..10 inclusive",
             Gen::unsigned_int(3,10),
             [](unsigned int num) {
                 if (num < 3)  { throw TestException("Got something below 3: "  + std::to_string(num)); }
                 if (num > 10) { throw TestException("Got something above 10: " + std::to_string(num)); }
             });
}

void test_unsigned_int_min_max_shrinking() {
    run_test("unsigned_int(3,10) - does a failure shrink to 3?",
             Gen::unsigned_int(3,10),
             [](unsigned int num) {
                 if (num <= 10) { throw TestException("Should be shrunk to 3"); }
             });
}

void test_reject() {
    run_test("reject() fails with the rejection message",
             Gen::reject<int>("My reason for failing"),
             [](int) {
                 throw TestException("Shouldn't have even got to the test function");
             });
}

void test_map() {
    run_test("map() transforms the value",
             Gen::unsigned_int(10).map([](auto n){return n * 2;}),
             [](unsigned int n) {
                 if (n % 2 == 1) {
                     throw TestException("Somehow we got an odd value when .map() should have turned them all into evens");
                 }
             });
}

void test_map_shrinking() {
    run_test("map() - shrinker still provides mapped values",
             Gen::unsigned_int(10).map([](auto n){return n * 100;}),
             [](unsigned int n) {
                 if (n > 321) {
                     throw TestException("Should be shrunk to 400");
                 }
             });
}

int main() {
    test_constant();
    test_constant_shrinking();
    test_unsigned_int_max_bounds();
    test_unsigned_int_max_shrinking();
    test_unsigned_int_min_max_bounds();
    test_unsigned_int_min_max_shrinking();
    test_reject();
    test_map();
    test_map_shrinking();
    return 0;
}
