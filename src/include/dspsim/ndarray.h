#pragma once
#include <cstddef>
#include <compare>
#include <functional>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace dspsim
{
    using Shape = std::vector<std::size_t>;
    using Index = std::vector<std::size_t>;

    namespace detail
    {
        /*
            Owns a row-major, runtime-shaped, N-dimensional collection of Elem objects.
            Elements are heap allocated because Models are neither copyable nor movable.
            Access: arr.at({i, j, k}) (bounds checked) or arr[{i, j, k}].
        */
        template <typename Elem>
        class NdArray
        {
        public:
            using element_type = Elem;
            using Factory = std::function<std::unique_ptr<Elem>(const std::string &, std::size_t)>;

            // make(element_name, flat_index) constructs each element. Names are base + "[i][j]...".
            NdArray(const std::string &name, Shape shape,
                    const Factory &make)
                : shape_(std::move(shape))
            {
                std::size_t n = 1;
                for (auto d : shape_)
                    n *= d;

                // Row-major strides.
                strides_.assign(shape_.size(), 1);
                for (std::size_t d = shape_.size(); d-- > 1;)
                    strides_[d - 1] = strides_[d] * shape_[d];

                elems_.reserve(n);
                Index idx(shape_.size(), 0);
                for (std::size_t flat = 0; flat < n; ++flat)
                {
                    std::string elem_name = name;
                    for (auto i : idx)
                        elem_name += "[" + std::to_string(i) + "]";
                    elems_.push_back(make(elem_name, flat));

                    for (std::size_t d = shape_.size(); d-- > 0;)
                    {
                        if (++idx[d] < shape_[d])
                            break;
                        idx[d] = 0;
                    }
                }
            }

            NdArray(const NdArray &) = delete;
            NdArray &operator=(const NdArray &) = delete;

            std::size_t ndim() const { return shape_.size(); }
            const Shape &shape() const { return shape_; }
            std::size_t extent(std::size_t dim) const { return shape_.at(dim); }
            std::size_t size() const { return elems_.size(); }

            // Row-major flat access.
            Elem &flat(std::size_t i) const { return *elems_.at(i); }

            // Iterates over every element in row-major order, yielding Elem&.
            class iterator
            {
            public:
                using iterator_category = std::random_access_iterator_tag;
                using value_type = Elem;
                using difference_type = std::ptrdiff_t;
                using pointer = Elem *;
                using reference = Elem &;

                iterator() = default;
                reference operator*() const { return **it_; }
                pointer operator->() const { return it_->get(); }
                reference operator[](difference_type n) const { return *it_[n]; }
                iterator &operator++()
                {
                    ++it_;
                    return *this;
                }
                iterator operator++(int)
                {
                    auto t = *this;
                    ++it_;
                    return t;
                }
                iterator &operator--()
                {
                    --it_;
                    return *this;
                }
                iterator operator--(int)
                {
                    auto t = *this;
                    --it_;
                    return t;
                }
                iterator &operator+=(difference_type n)
                {
                    it_ += n;
                    return *this;
                }
                iterator &operator-=(difference_type n)
                {
                    it_ -= n;
                    return *this;
                }
                friend iterator operator+(iterator a, difference_type n) { return a += n; }
                friend iterator operator+(difference_type n, iterator a) { return a += n; }
                friend iterator operator-(iterator a, difference_type n) { return a -= n; }
                friend difference_type operator-(const iterator &a, const iterator &b) { return a.it_ - b.it_; }
                friend bool operator==(const iterator &a, const iterator &b) { return a.it_ == b.it_; }
                friend auto operator<=>(const iterator &a, const iterator &b) { return a.it_ <=> b.it_; }

            private:
                friend class NdArray;
                using Inner = typename std::vector<std::unique_ptr<Elem>>::const_iterator;
                explicit iterator(Inner it) : it_(it) {}
                Inner it_{};
            };

            iterator begin() const { return iterator(elems_.begin()); }
            iterator end() const { return iterator(elems_.end()); }

            // Bounds-checked multidimensional access. Throws std::out_of_range.
            Elem &at(const Index &idx) const { return *elems_[flat_index(idx)]; }
            Elem &operator[](const Index &idx) const { return at(idx); }

        private:
            std::size_t flat_index(const Index &idx) const
            {
                if (idx.size() != shape_.size())
                    throw std::out_of_range("index rank does not match array rank");
                std::size_t flat = 0;
                for (std::size_t d = 0; d < idx.size(); ++d)
                {
                    if (idx[d] >= shape_[d])
                        throw std::out_of_range("array index out of range");
                    flat += idx[d] * strides_[d];
                }
                return flat;
            }

            Shape shape_;
            std::vector<std::size_t> strides_;
            std::vector<std::unique_ptr<Elem>> elems_;
        };
    } // namespace detail
} // namespace dspsim
