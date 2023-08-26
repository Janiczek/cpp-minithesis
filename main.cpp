#include "pbt.h"
#include <iostream>

int main() {
    auto gen = constant(42U);
    auto test = [](unsigned int num){
        if (num == 100) {
            throw std::runtime_error("Got a hundred, gasp!");
        }
    };
    TestResult<unsigned int> result = run(gen, test);

    std::cout << "result: " << to_string(result) << std::endl;
    return 0;
}
