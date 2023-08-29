#ifndef PBT_TEST_RESULT_H
#define PBT_TEST_RESULT_H

#include <algorithm>
#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

struct Passes {};

template<typename T>
struct FailsWith {
    T value;
    std::string error;
};

struct CannotGenerateValues {
    std::map<std::string, int> rejections;
};

template<typename T>
using TestResult = std::variant<Passes, FailsWith<T>, CannotGenerateValues>;

template<typename T>
std::string to_string(const TestResult<T> &result) {
    struct stringifier {
        std::string operator()(Passes) { return "Passes"; }
        std::string operator()(FailsWith<T> f) { return "Fails:\n - value: " + std::to_string(f.value) + "\n - error: \"" + f.error + "\""; }
        std::string operator()(const CannotGenerateValues &cgv) {

            // Partially sort the map (well, a vector of pairs) to get the top 5 rejections
            auto descByValue = [](const auto &a, const auto &b) { return a.second > b.second; };
            auto size = std::min(5, static_cast<int>(cgv.rejections.size()));
            std::vector<std::pair<std::string, int>> sorted_items(cgv.rejections.begin(), cgv.rejections.end());
            std::partial_sort(sorted_items.begin(),
                              sorted_items.begin() + size,
                              sorted_items.end(),
                              descByValue);

            std::string reasons;
            for (int i = 0; i < size; i++) {
                reasons += "\n - " + sorted_items[i].first;
            }

            return "Cannot generate values. Most common reasons:" + reasons;
        }
    };
    return std::visit(stringifier{}, result);
}

#endif//PBT_TEST_RESULT_H
