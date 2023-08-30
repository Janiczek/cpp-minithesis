#include "pbt.h"

void test_constant() {
    run_test("constant(42) should always generate 42",
             Gen::constant(42),
             [](int num) {
                 if (num != 42) {
                     throw TestException("This shouldn't be possible");
                 }
             });
}

void test_constant_shrinking() {
    run_test("constant(42) - does a failure not shrink?",
             Gen::constant(42),
             [](int num) { throw TestException("Should be shrunk to 42"); });
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
             [](unsigned int num) { throw TestException("Should be shrunk to 0"); });
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
             [](unsigned int num) { throw TestException("Should be shrunk to 3"); });
}

void test_reject() {
    run_test("reject() fails with the rejection message",
             Gen::reject<int>("My reason for failing"),
             [](int) {
                 throw TestException("This shouldn't be possible");
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

void test_filter() {
    run_test("filter() - doesn't let certain values through",
             Gen::unsigned_int(10).filter([](auto n){return n % 2 == 0;}),
             [](unsigned int n) { if (n % 2 == 1) { throw TestException("This shouldn't be possible"); } });
}

void test_filter_degenerate_case() {
    run_test("filter() - if too strict, will reject all the time",
             Gen::unsigned_int(10).filter([](auto n){return false;}),
             [](unsigned int n) { throw TestException("This shouldn't be possible"); });
}

void test_filter_shrinking() {
    run_test("filter() - shrinker provides only filtered values",
             Gen::unsigned_int(3,10).filter([](auto n){return n > 3;}),
             [](unsigned int n) { throw TestException("Should be shrunk to 4"); });
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
    test_filter();
    test_filter_degenerate_case();
    test_filter_shrinking();
    return 0;
}
