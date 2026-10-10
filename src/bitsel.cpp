#include <dspsim/bitsel.h>
#include <dspsim/bits.h>
#include <dspsim/signal.h>

#include <algorithm>
#include <stdexcept>
#include <string>

namespace dspsim
{
    void check_bit_range(int hi, int lo, int width, const std::string &what)
    {
        if (lo < 0 || hi < lo || hi >= width)
        {
            throw std::out_of_range("Bit range [" + std::to_string(hi) + ":" + std::to_string(lo) +
                                    "] is out of range for " + what + " of width " + std::to_string(width));
        }
    }

    BitSel::BitSel(SignalBase &signal) : BitSel(signal, signal.width() - 1, 0)
    {
    }

    BitSel::BitSel(SignalBase &signal, int hi, int lo)
    {
        if (!signal.is_integral())
        {
            throw std::invalid_argument("Cannot select bits of non-integral signal " + signal.hier_name());
        }
        check_bit_range(hi, lo, signal.width(), "signal " + signal.hier_name());
        if (const BitSel *source = signal.source_selection())
        {
            *this = source->slice(hi, lo);
        }
        else
        {
            append({&signal, hi, lo});
        }
    }

    BitSel BitSel::concat(const std::vector<BitSel> &sels)
    {
        if (sels.empty())
        {
            throw std::invalid_argument("Cannot concatenate an empty list of selections");
        }
        BitSel result;
        // The last selection is the least significant.
        for (auto sel = sels.rbegin(); sel != sels.rend(); ++sel)
        {
            for (const auto &part : sel->parts_)
            {
                result.append(part);
            }
        }
        return result;
    }

    uint64_t BitSel::read() const
    {
        uint64_t value = 0;
        int offset = 0;
        for (const auto &part : parts_)
        {
            value |= bits(part.signal->read_bits(), part.hi, part.lo) << offset;
            offset += part.width();
        }
        return value;
    }

    int64_t BitSel::read_signed() const
    {
        return sext(read(), width_);
    }

    void BitSel::write(uint64_t value) const
    {
        int offset = 0;
        for (const auto &part : parts_)
        {
            const uint64_t part_mask = mask(part.width());
            part.signal->write_bits(((value >> offset) & part_mask) << part.lo, part_mask << part.lo);
            offset += part.width();
        }
    }

    BitSel BitSel::slice(int hi, int lo) const
    {
        check_bit_range(hi, lo, width_, "selection");
        BitSel result;
        int offset = 0;
        for (const auto &part : parts_)
        {
            // Intersect [hi:lo] with the bits this part occupies in the selection.
            const int part_hi = offset + part.width() - 1;
            const int sel_hi = std::min(hi, part_hi);
            const int sel_lo = std::max(lo, offset);
            if (sel_lo <= sel_hi)
            {
                result.append({part.signal, part.lo + (sel_hi - offset), part.lo + (sel_lo - offset)});
            }
            offset += part.width();
        }
        return result;
    }

    void BitSel::append(const BitPart &part)
    {
        if (width_ + part.width() > max_width)
        {
            throw std::invalid_argument("Bit selections are limited to " + std::to_string(max_width) + " bits");
        }
        if (!parts_.empty() && parts_.back().signal == part.signal && parts_.back().hi + 1 == part.lo)
        {
            parts_.back().hi = part.hi;
        }
        else
        {
            parts_.push_back(part);
        }
        width_ += part.width();
    }
} // namespace dspsim
