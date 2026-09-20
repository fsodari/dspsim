#include <dspsim/utils/unique_stack.h>

#include <ranges>
#include <vector>
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_range_equals.hpp> // Required header
#include <set>

using namespace dspsim;

namespace
{
    /*
     Dummy class to test UniqueStack.
     UniqueStack requires the element to implement ->id();
    */
    class Dummy
    {
    public:
        Dummy(size_t id) : _id(id) {}
        size_t id() const { return _id; }

        // overload -> operator so this can mimic a pointer type.
        Dummy *operator->() { return this; }
        const Dummy *operator->() const { return this; }

        // operator to compare with another Dummy
        bool operator==(const Dummy &other) const { return _id == other._id; }

        // operator to cast as size_t
        operator size_t() const { return _id; }
        // operator to compare with size_t and int
        bool operator==(size_t id) const { return _id == id; }
        bool operator==(int id) const { return _id == static_cast<size_t>(id); }

    private:
        size_t _id;
    };
}

TEST_CASE("UniqueStack push_back and pop_back", "[unique_stack]")
{
    UniqueStack<Dummy> stack;
    REQUIRE(stack.empty());

    auto test_data = std::vector<Dummy>{1, 1, 2, 2, 3, 3, 4, 4};
    auto expected = std::vector<Dummy>{1, 3, 2, 4}; // Order doesn't matter

    for (const auto &item : test_data)
    {
        stack.push_back(item);
    }

    std::vector<Dummy> result;
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
    UniqueStack<Dummy> stack;
    REQUIRE(stack.empty());

    auto test_data = std::vector<Dummy>{1, 1, 2, 2, 3, 3, 4, 4};
    auto expected = std::set<Dummy>(test_data.begin(), test_data.end());

    std::ranges::copy(test_data, std::back_inserter(stack));

    std::vector<Dummy> result;
    while (!stack.empty())
    {
        result.push_back(stack.back());
        stack.pop_back();
    }

    std::sort(result.begin(), result.end());
    REQUIRE_THAT(result, Catch::Matchers::RangeEquals(expected));
}