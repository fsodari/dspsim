#pragma once
#include <cstddef>
#include <compare>
#include <functional>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <string>
#include <vector>

namespace dspsim
{
    using Shape = std::vector<std::size_t>;
    using Index = std::vector<std::size_t>;

    // A resolved selection along one dimension: count consecutive elements starting at start.
    // If drop is set the dimension is removed from the result (it must have count == 1).
    struct Range
    {
        std::ptrdiff_t start = 0;
        std::size_t count = 0;
        bool drop = false;
    };

    // A python-style slice of one dimension, without a step. Negative start/stop count from the end and
    // out of range bounds are clamped. An unset start/stop means "to the end".
    struct Slice
    {
        std::optional<std::ptrdiff_t> start;
        std::optional<std::ptrdiff_t> stop;
        bool drop = false;

        Slice() = default;
        Slice(std::optional<std::ptrdiff_t> start, std::optional<std::ptrdiff_t> stop);

        // The whole dimension.
        static Slice all();
        // A single index. The dimension is dropped from the result.
        static Slice at(std::ptrdiff_t i);

        // The range selected in a dimension of the given extent. Throws std::out_of_range for a bad index.
        Range resolve(std::size_t extent) const;
    };
    using Slices = std::vector<Slice>;

    namespace detail
    {
        // Resolve one Slice per leading dimension of an array of the given shape.
        std::vector<Range> resolve_slices(const Shape &shape, const Slices &slices);

        struct Selection
        {
            Shape shape;
            std::vector<std::size_t> flat;
        };

        // Row-major flat indices (into an array of the given shape) selected by the ranges.
        Selection select_flat(const Shape &shape, const std::vector<Range> &ranges);

        template <typename Elem>
        concept Readable = requires(const Elem &e) { e.read(); };
        template <typename Elem>
        concept Writable = requires(Elem &e, const std::remove_cvref_t<decltype(std::declval<const Elem &>().read())> &v) { e.write(v); };

        template <typename Elem>
        using value_of = std::remove_cvref_t<decltype(std::declval<const Elem &>().read())>;

        // Bulk read/write of the elements of a range of Elem&, in row-major order.
        template <typename Elem, typename Range>
        std::vector<value_of<Elem>> read_all(const Range &r)
        {
            std::vector<value_of<Elem>> out;
            out.reserve(r.size());
            for (auto &e : r)
                out.push_back(e.read());
            return out;
        }
        template <typename Elem, typename Range>
        void write_all(const Range &r, const value_of<Elem> &value)
        {
            for (auto &e : r)
                e.write(value);
        }
        template <typename Elem, typename Range>
        void write_all(const Range &r, const std::vector<value_of<Elem>> &values)
        {
            if (values.size() != r.size())
                throw std::invalid_argument("write: number of values does not match array size");
            std::size_t i = 0;
            for (auto &e : r)
                e.write(values[i++]);
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
            Elem &at(const Index &idx) const { return *elems_[flat_index(idx.data(), idx.size())]; }
            Elem &operator[](const Index &idx) const { return at(idx); }
            // arr[{i, j}] and arr.at({i, j}) take these: an index written in braces is not made into an Index,
            // which would allocate on every access.
            Elem &at(std::initializer_list<std::size_t> idx) const { return *elems_[flat_index(idx.begin(), idx.size())]; }
            Elem &operator[](std::initializer_list<std::size_t> idx) const { return at(idx); }

            using iterator = DerefIterator<Elem, typename std::vector<Elem *>::const_iterator>;
            iterator begin() const { return iterator(elems_.begin()); }
            iterator end() const { return iterator(elems_.end()); }

            // Bulk access in row-major order. read() needs a readable Elem, write() a writable one. They are
            // templates so that an array of elements without a value (structs) does not declare them.
            template <Readable E = Elem>
            std::vector<value_of<E>> read() const { return read_all<E>(*this); }
            template <Writable E = Elem>
            void write(const value_of<E> &value) const { write_all<E>(*this, value); }
            template <Writable E = Elem>
            void write(const std::vector<value_of<E>> &values) const { write_all<E>(*this, values); }

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
            std::size_t flat_index(const std::size_t *idx, std::size_t rank) const
            {
                if (rank != shape_.size())
                    throw std::out_of_range("index rank does not match array rank");
                std::size_t flat = 0;
                for (std::size_t d = 0; d < rank; ++d)
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

            // Bulk access in row-major order. read() needs a readable Elem, write() a writable one. They are
            // templates so that an array of elements without a value (structs) does not declare them.
            template <Readable E = Elem>
            std::vector<value_of<E>> read() const { return read_all<E>(*this); }
            template <Writable E = Elem>
            void write(const value_of<E> &value) const { write_all<E>(*this, value); }
            template <Writable E = Elem>
            void write(const std::vector<value_of<E>> &values) const { write_all<E>(*this, values); }

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
            Elem &at(const Index &idx) const { return *elems_[flat_index(idx.data(), idx.size())]; }
            Elem &operator[](const Index &idx) const { return at(idx); }
            // arr[{i, j}] and arr.at({i, j}) take these: an index written in braces is not made into an Index,
            // which would allocate on every access.
            Elem &at(std::initializer_list<std::size_t> idx) const { return *elems_[flat_index(idx.begin(), idx.size())]; }
            Elem &operator[](std::initializer_list<std::size_t> idx) const { return at(idx); }

        private:
            std::size_t flat_index(const std::size_t *idx, std::size_t rank) const
            {
                if (rank != shape_.size())
                    throw std::out_of_range("index rank does not match array rank");
                std::size_t flat = 0;
                for (std::size_t d = 0; d < rank; ++d)
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
