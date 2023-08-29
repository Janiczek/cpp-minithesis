#ifndef PBT_TEST_EXCEPTION_H
#define PBT_TEST_EXCEPTION_H

#include <string>
#include <utility>

class TestException : public std::exception {
private:
    std::string message;

public:
    explicit TestException(std::string msg) : message(std::move(msg)) {}
    std::string what() { return message; }
};

#endif//PBT_TEST_EXCEPTION_H
