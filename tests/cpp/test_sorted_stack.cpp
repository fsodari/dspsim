#include <dspsim/utils/sorted_stack.h>
#include <vector>
#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

TEST_CASE("basic_sorted_stack", "[sorted_stack]")
{
    SortedStack<int> stack;
    REQUIRE(stack.empty());

    auto test_data = std::vector<int>{4, 1, 3, 2, 5};
    auto expected = std::vector<int>{1, 2, 3, 4, 5};

    for (const auto &item : test_data)
    {
        stack.push(item);
    }

    std::vector<int> result;
    while (!stack.empty())
    {
        result.push_back(stack.top());
        stack.pop();
    }

    REQUIRE(result == expected);
}
