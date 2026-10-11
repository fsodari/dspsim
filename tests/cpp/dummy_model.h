#pragma once
#include <cstdint>

/*
     Dummy class to test UniqueStack.
     UniqueStack requires the element to implement ->id();
    */
class DummyModel
{
public:
    DummyModel(uint32_t id) : id_(id) {}
    uint32_t id() const { return id_; }

    // overload -> operator so this can mimic a pointer type.
    DummyModel *operator->() { return this; }
    const DummyModel *operator->() const { return this; }

    // operator to compare with another Dummy
    bool operator==(const DummyModel &other) const { return id_ == other.id_; }

    // operator to cast as size_t
    operator uint32_t() const { return id_; }
    // operator to compare with size_t and int
    bool operator==(uint32_t id) const { return id_ == id; }

private:
    uint32_t id_;
};