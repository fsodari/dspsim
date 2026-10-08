#pragma once
#include <cstddef>
#include <algorithm>
#include <compare>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace dspsim
{
    using Shape = std::vector<std::size_t>;
    using Index = std::vector<std::size_t>;

    // A resolved selection along one dimension: count elements starting at start, advancing by step.
    // If drop is set the dimension is removed from the result (it must have count == 1).
    struct Range
    {
        std::ptrdiff_t start = 0;
        std::ptrdiff_t step = 1;
        std::size_t count = 0;
        bool drop = false;
    };

    // A python-style slice of one dimension. Negative start/stop count from the end, out of range bounds
    // are clamped, and a negative stride walks backwards. An unset start/stop means "to the end".
    struct Slice
    {
        std::optional<std::ptrdiff_t> start;
        std::optional<std::ptrdiff_t> stop;
        std::ptrdiff_t stride = 1;
        bool drop = false;

        Slice() = default;
        Slice(std::optional<std::ptrdiff_t> start, std::optional<std::ptrdiff_t> stop, std::ptrdiff_t stride = 1)
            : start(start), stop(stop), stride(stride) {}

        // The whole dimension.
        static Slice all() { return {}; }
        // A single index. The dimension is dropped from the result.
        static Slice at(std::ptrdiff_t i)
        {
            Slice s;
            s.start = i;
            s.drop = true;
            return s;
        }

        Range resolve(std::size_t extent) const
        {
            const auto n = static_cast<std::ptrdiff_t>(extent);
            if (drop)
            {
                auto i = *start < 0 ? *start + n : *start;
                if (i < 0 || i >= n)
                    throw std::out_of_range("array index out of range");
                return {i, 1, 1, true};
            }
            if (stride == 0)
                throw std::invalid_argument("slice stride cannot be zero");
            auto norm = [&](std::ptrdiff_t v, std::ptrdiff_t lo, std::ptrdiff_t hi)
            {
                if (v < 0)
                    v += n;
                return std::clamp<std::ptrdiff_t>(v, lo, hi);
            };
            std::ptrdiff_t b, e;
            if (stride > 0)
            {
                b = start ? norm(*start, 0, n) : 0;
                e = stop ? norm(*stop, 0, n) : n;
            }
            else
            {
                b = start ? norm(*start, -1, n - 1) : n - 1;
                e = stop ? norm(*stop, -1, n - 1) : -1;
            }
            std::size_t count = 0;
            if (stride > 0 && e > b)
                count = (e - b + stride - 1) / stride;
            else if (stride < 0 && b > e)
                count = (b - e + (-stride) - 1) / (-stride);
            return {b, stride, count, false};
        }
    };
    using Slices = std::vector<Slice>;

    namespace detail
    {
        inline std::vector<Range> resolve_slices(const Shape &shape, const Slices &slices)
        {
            if (slices.size() > shape.size())
                throw std::out_of_range("too many slices for array dimensions");
            std::vector<Range> ranges;
            for (std::size_t d = 0; d < slices.size(); ++d)
                ranges.push_back(slices[d].resolve(shape[d]));
            return ranges;
        }

        struct Selection
        {
            Shape shape;
            std::vector<std::size_t> flat;
        };

        // Row-major flat indices (into an array of the given shape) selected by the ranges.
        inline Selection select_flat(const Shape &shape, const std::vector<Range> &ranges)
        {
            if (ranges.size() > shape.size())
                throw std::out_of_range("too many ranges for array dimensions");
            std::vector<Range> rs = ranges;
            for (std::size_t d = rs.size(); d < shape.size(); ++d)
                rs.push_back({0, 1, shape[d], false});

            std::vector<std::size_t> strides(shape.size(), 1);
            for (std::size_t d = shape.size(); d-- > 1;)
                strides[d - 1] = strides[d] * shape[d];

            Selection sel;
            std::size_t total = 1;
            for (std::size_t d = 0; d < rs.size(); ++d)
            {
                const auto &r = rs[d];
                if (r.count > 0)
                {
                    auto last = r.start + r.step * static_cast<std::ptrdiff_t>(r.count - 1);
                    if (r.start < 0 || last < 0 || r.start >= static_cast<std::ptrdiff_t>(shape[d]) ||
                        last >= static_cast<std::ptrdiff_t>(shape[d]))
                        throw std::out_of_range("array index out of range");
                }
                if (!r.drop)
                    sel.shape.push_back(r.count);
                total *= r.count;
            }
            sel.flat.reserve(total);
            std::vector<std::size_t> pos(rs.size(), 0);
            for (std::size_t n = 0; n < total; ++n)
            {
                std::ptrdiff_t f = 0;
                for (std::size_t d = 0; d < rs.size(); ++d)
                    f += (rs[d].start + rs[d].step * static_cast<std::ptrdiff_t>(pos[d])) * static_cast<std::ptrdiff_t>(strides[d]);
                sel.flat.push_back(static_cast<std::size_t>(f));
                for (std::size_t d = rs.size(); d-- > 0;)
                {
                    if (++pos[d] < rs[d].count)
                        break;
                    pos[d] = 0;
                }
            }
            return sel;
        }

        // Random access iterator that dereferences through a pointer-like element (unique_ptr<Elem> or Elem*).
        template <typename Elem, typename Inner>
        class DerefIterator
        {
        public:
            using iterator_category = std::random_access_iterator_tag;
            using value_type = Elem;
            using difference_type = std::ptrdiff_t;
            using pointer = Elem *;
            using reference = Elem &;

            DerefIterator() = default;
            explicit DerefIterator(Inner it) : it_(it) {}
            reference operator*() const { return **it_; }
            pointer operator->() const { return std::addressof(**it_); }
            reference operator[](difference_type n) const { return *it_[n]; }
            DerefIterator &operator++() { ++it_; return *this; }
            DerefIterator operator++(int) { auto t = *this; ++it_; return t; }
            DerefIterator &operator--() { --it_; return *this; }
            DerefIterator operator--(int) { auto t = *this; --it_; return t; }
            DerefIterator &operator+=(difference_type n) { it_ += n; return *this; }
            DerefIterator &operator-=(difference_type n) { it_ -= n; return *this; }
            friend DerefIterator operator+(DerefIterator a, difference_type n) { return a += n; }
            friend DerefIterator operator+(difference_type n, DerefIterator a) { return a += n; }
            friend DerefIterator operator-(DerefIterator a, difference_type n) { return a -= n; }
            friend difference_type operator-(const DerefIterator &a, const DerefIterator &b) { return a.it_ - b.it_; }
            friend bool operator==(const DerefIterator &a, const DerefIterator &b) { return a.it_ == b.it_; }
            friend auto operator<=>(const DerefIterator &a, const DerefIterator &b) { return a.it_ <=> b.it_; }

        private:
            Inner it_{};
        };

        /*
            Non-owning, shaped view of selected elements of an NdArray (or another view), in row-major order.
            The source array must outlive the view.
        */
        template <typename Elem>
        class NdView
        {
        public:
            using element_type = Elem;

            NdView(Shape shape, std::vector<Elem *> elems) : shape_(std::move(shape)), elems_(std::move(elems)) {}

            std::size_t ndim() const { return shape_.size(); }
            const Shape &shape() const { return shape_; }
            std::size_t extent(std::size_t dim) const { return shape_.at(dim); }
            std::size_t size() const { return elems_.size(); }

            Elem &flat(std::size_t i) const { return *elems_.at(i); }
            Elem &at(const Index &idx) const { return *elems_[flat_index(idx)]; }
            Elem &operator[](const Index &idx) const { return at(idx); }

            using iterator = DerefIterator<Elem, typename std::vector<Elem *>::const_iterator>;
            iterator begin() const { return iterator(elems_.begin()); }
            iterator end() const { return iterator(elems_.end()); }

            NdView<Elem> view() const { return *this; }
            NdView<Elem> slice(const Slices &slices) const { return select(resolve_slices(shape_, slices)); }
            NdView<Elem> select(const std::vector<Range> &ranges) const
            {
                auto sel = select_flat(shape_, ranges);
                std::vector<Elem *> ptrs;
                ptrs.reserve(sel.flat.size());
                for (auto f : sel.flat)
                    ptrs.push_back(elems_[f]);
                return NdView<Elem>(std::move(sel.shape), std::move(ptrs));
            }

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
                    flat = flat * shape_[d] + idx[d];
                }
                return flat;
            }

            Shape shape_;
            std::vector<Elem *> elems_;
        };

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

            using iterator = DerefIterator<Elem, typename std::vector<std::unique_ptr<Elem>>::const_iterator>;
            iterator begin() const { return iterator(elems_.begin()); }
            iterator end() const { return iterator(elems_.end()); }

            // A view over every element.
            NdView<Elem> view() const { return select({}); }

            // Select a sub-array with one Slice per leading dimension (missing trailing dimensions are kept whole).
            NdView<Elem> slice(const Slices &slices) const { return select(resolve_slices(shape_, slices)); }

            // Select with already-resolved ranges.
            NdView<Elem> select(const std::vector<Range> &ranges) const
            {
                auto sel = select_flat(shape_, ranges);
                std::vector<Elem *> ptrs;
                ptrs.reserve(sel.flat.size());
                for (auto f : sel.flat)
                    ptrs.push_back(elems_[f].get());
                return NdView<Elem>(std::move(sel.shape), std::move(ptrs));
            }

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
