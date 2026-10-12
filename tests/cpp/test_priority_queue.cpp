#include <dspsim/utils/priority_queue.h>
#include <vector>
#include <catch2/catch_test_macros.hpp>

using namespace dspsim;

TEST_CASE("Basic priority queue operations.", "[priority_queue]")
{
    PriorityQueue<int> stack;
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

TEST_CASE("Elements can be removed from a priority queue.", "[priority_queue]")
{
    PriorityQueue<int> queue;
    for (int item : {7, 4, 1, 8, 3, 2, 6, 5})
    {
        queue.push(item);
    }

    // Nothing matches: nothing changes.
    REQUIRE(queue.erase_if([](int item)
                           { return item > 100; }) == 0);
    REQUIRE(queue.size() == 8);

    REQUIRE(queue.erase_if([](int item)
                           { return item % 2 == 0; }) == 4);

    std::vector<int> result;
    while (!queue.empty())
    {
        result.push_back(queue.top());
        queue.pop();
    }
    REQUIRE(result == std::vector<int>{1, 3, 5, 7});
}
