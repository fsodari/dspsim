#pragma once
#include <cstdint>

/*
     Dummy class to test UniqueStack.
     UniqueStack requires the element to implement ->id();
    */
class DummyModel
{
public:
    DummyModel(uint32_t id) : _id(id) {}
    uint32_t id() const { return _id; }

    // overload -> operator so this can mimic a pointer type.
    DummyModel *operator->() { return this; }
    const DummyModel *operator->() const { return this; }

    // operator to compare with another Dummy
    bool operator==(const DummyModel &other) const { return _id == other._id; }

    // operator to cast as size_t
    operator uint32_t() const { return _id; }
    // operator to compare with size_t and int
    bool operator==(uint32_t id) const { return _id == id; }

private:
    uint32_t _id;
};