#include <dspsim/utils/unique_stack.h>

#include <ranges>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp> // Required header
#include <set>

#include "dummy_model.h"

using namespace dspsim;

TEST_CASE("UniqueStack push_back and pop_back", "[unique_stack]")
{
    UniqueStack<DummyModel> stack;
    REQUIRE(stack.empty());

    auto test_data = std::vector<DummyModel>{1, 1, 2, 2, 3, 3, 4, 4};
    auto expected = std::vector<DummyModel>{1, 3, 2, 4}; // Order doesn't matter

    for (const auto &item : test_data)
    {
        stack.push_back(item);
    }

    std::vector<DummyModel> result;
    while (!stack.empty())
    {
        result.push_back(stack.back());
        stack.pop_back();
    }

    std::sort(result.begin(), result.end());
    std::sort(expected.begin(), expected.end());
    REQUIRE(result == expected);
}

TEST_CASE("UniqueStack back_inserter", "[unique_stack]")
{
    UniqueStack<DummyModel> stack;
    REQUIRE(stack.empty());

    auto test_data = std::vector<DummyModel>{1, 1, 2, 2, 3, 3, 4, 4};
    // expected data should be unique.
    auto expected = std::set<DummyModel>(test_data.begin(), test_data.end());

    std::ranges::copy(test_data, std::back_inserter(stack));

    std::vector<DummyModel> result;
    while (!stack.empty())
    {
        result.push_back(stack.back());
        stack.pop_back();
    }

    // Sort the result to compare with the expected set.
    std::sort(result.begin(), result.end());
    REQUIRE_THAT(result, Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("UniqueStack push_range", "[unique_stack]")
{
    UniqueStack<DummyModel> stack;
    REQUIRE(stack.empty());

    auto test_data = std::initializer_list<DummyModel>{1, 1, 2, 2, 3, 3, 4, 4};
    // expected data should be unique.
    auto expected = std::set<DummyModel>(test_data.begin(), test_data.end());

    stack.push_range(test_data);

    std::vector<DummyModel> result;
    while (!stack.empty())
    {
        result.push_back(stack.back());
        stack.pop_back();
    }

    // Sort the result to compare with the expected set.
    std::sort(result.begin(), result.end());
    REQUIRE_THAT(result, Catch::Matchers::RangeEquals(expected));
}

TEST_CASE("UniqueStack find", "[unique_stack]")
{
    UniqueStack<DummyModel> stack;
    auto test_data = std::vector<DummyModel>{1, 2, 3, 4};
    for (const auto &item : test_data)
    {
        stack.push_back(item);
    }

    for (const auto &item : test_data)
    {
        auto it = stack.find(item);
        REQUIRE(it != stack.end());
        REQUIRE(*it == item);
    }

    DummyModel not_in_stack{5};
    auto it = stack.find(not_in_stack);
    REQUIRE(it == stack.end());
}

TEST_CASE("UniqueStack erasing", "[unique_stack]")
{
    UniqueStack<DummyModel> stack;
    auto test_data = std::vector<DummyModel>{1, 2, 3, 4};
    for (const auto &item : test_data)
    {
        stack.push_back(item);
    }

    // Erase by iterator
    auto it = stack.find(test_data[1]);
    REQUIRE(it != stack.end());
    stack.erase(it);
    REQUIRE(stack.find(test_data[1]) == stack.end());

    // Erase by value
    stack.erase(test_data[2]);
    REQUIRE(stack.find(test_data[2]) == stack.end());

    // Ensure other elements are still present
    REQUIRE(stack.find(test_data[0]) != stack.end());
    REQUIRE(stack.find(test_data[3]) != stack.end());
}