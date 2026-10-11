#include <dspsim/ndarray.h>

#include <algorithm>
#include <stdexcept>

namespace dspsim
{
    Slice::Slice(std::optional<std::ptrdiff_t> start, std::optional<std::ptrdiff_t> stop)
        : start(start), stop(stop)
    {
    }

    Slice Slice::all()
    {
        return {};
    }

    Slice Slice::at(std::ptrdiff_t i)
    {
        Slice s;
        s.start = i;
        s.drop = true;
        return s;
    }

    Range Slice::resolve(std::size_t extent) const
    {
        const auto n = static_cast<std::ptrdiff_t>(extent);
        if (drop)
        {
            auto i = *start < 0 ? *start + n : *start;
            if (i < 0 || i >= n)
            {
                throw std::out_of_range("array index out of range");
            }
            return {i, 1, true};
        }
        auto norm = [&](std::ptrdiff_t v)
        {
            if (v < 0)
            {
                v += n;
            }
            return std::clamp<std::ptrdiff_t>(v, 0, n);
        };
        std::ptrdiff_t b = start ? norm(*start) : 0;
        std::ptrdiff_t e = stop ? norm(*stop) : n;
        return {b, e > b ? static_cast<std::size_t>(e - b) : 0, false};
    }

    std::vector<Range> detail::resolve_slices(const Shape &shape, const Slices &slices)
    {
        if (slices.size() > shape.size())
        {
            throw std::out_of_range("too many slices for array dimensions");
        }
        std::vector<Range> ranges;
        for (std::size_t d = 0; d < slices.size(); ++d)
        {
            ranges.push_back(slices[d].resolve(shape[d]));
        }
        return ranges;
    }

    detail::Selection detail::select_flat(const Shape &shape, const std::vector<Range> &ranges)
    {
        if (ranges.size() > shape.size())
        {
            throw std::out_of_range("too many ranges for array dimensions");
        }
        std::vector<Range> rs = ranges;
        for (std::size_t d = rs.size(); d < shape.size(); ++d)
        {
            rs.push_back({0, shape[d], false});
        }

        std::vector<std::size_t> strides(shape.size(), 1);
        for (std::size_t d = shape.size(); d-- > 1;)
        {
            strides[d - 1] = strides[d] * shape[d];
        }

        Selection sel;
        std::size_t total = 1;
        for (std::size_t d = 0; d < rs.size(); ++d)
        {
            const auto &r = rs[d];
            if (r.count > 0)
            {
                auto last = r.start + static_cast<std::ptrdiff_t>(r.count - 1);
                if (r.start < 0 || last >= static_cast<std::ptrdiff_t>(shape[d]))
                {
                    throw std::out_of_range("array index out of range");
                }
            }
            if (!r.drop)
            {
                sel.shape.push_back(r.count);
            }
            total *= r.count;
        }
        sel.flat.reserve(total);
        std::vector<std::size_t> pos(rs.size(), 0);
        for (std::size_t n = 0; n < total; ++n)
        {
            std::ptrdiff_t f = 0;
            for (std::size_t d = 0; d < rs.size(); ++d)
            {
                f += (rs[d].start + static_cast<std::ptrdiff_t>(pos[d])) * static_cast<std::ptrdiff_t>(strides[d]);
            }
            sel.flat.push_back(static_cast<std::size_t>(f));
            for (std::size_t d = rs.size(); d-- > 0;)
            {
                if (++pos[d] < rs[d].count)
                {
                    break;
                }
                pos[d] = 0;
            }
        }
        return sel;
    }
} // namespace dspsim
