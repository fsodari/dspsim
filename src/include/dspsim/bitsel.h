#pragma once
#include <concepts>
#include <cstdint>
#include <iterator>
#include <string>
#include <vector>

namespace dspsim
{
    class SignalBase;
    template <typename T>
    class DerivedSignal;

    /// Inclusive bit range [hi:lo], as in SystemVerilog sig[hi:lo].
    struct BitRange
    {
        int hi;
        int lo;
    };

    /// A contiguous range of bits [hi:lo] of one signal.
    struct BitPart
    {
        SignalBase *signal;
        int hi;
        int lo;

        int width() const { return hi - lo + 1; }
        bool operator==(const BitPart &) const = default;
    };

    /*
        Untyped selection of bits from one or more signals: a slice (sig[hi:lo]), a concatenation
        (pack(a, b) == SystemVerilog {a, b}), or any combination. Selections always refer to the bits of
        the underlying signals directly, so a slice of a pack (or a pack of slices) is flattened, and
        adjacent ranges of the same signal are merged.

        A BitSel is a lightweight handle with no storage of its own. Values are unsigned, as in
        SystemVerilog; use sext() from <dspsim/bits.h> to interpret them as signed.
            auto lo = sig.slice(7, 0);      // or sig[{7, 0}]
            auto bus = pack(a, b, c[{3, 0}]);
            bus.write(0x1234);              // writes through to a, b and c[3:0]

        Only integral signals can be selected, and a selection is at most 64 bits wide.
        The signals must outlive the selection.
    */
    class BitSel
    {
    public:
        /// Maximum width of a selection in bits.
        static constexpr int max_width = 64;

        /// Select all bits of a signal.
        BitSel(SignalBase &signal);
        /// Select bits [hi:lo] of a signal. Requires 0 <= lo <= hi < signal.width().
        /// Selecting bits of a derived signal selects the bits of its sources.
        BitSel(SignalBase &signal, int hi, int lo);

        /// Concatenate selections. The first selection is the most significant, as in SystemVerilog {a, b}.
        static BitSel concat(const std::vector<BitSel> &sels);

        /// Width of the selection in bits.
        int width() const { return width_; }
        /// The selected ranges, ordered from least to most significant.
        const std::vector<BitPart> &parts() const { return parts_; }

        /// Read the committed value of the selected bits.
        uint64_t read() const;
        /// Read the committed value of the selected bits as a two's complement number of width() bits.
        int64_t read_signed() const;
        /// Write value to the selected bits. Each underlying signal's pending value is updated in place,
        /// so several writes to different bits of a signal in the same cycle combine. Bits of value above
        /// width() are ignored.
        void write(uint64_t value) const;

        /// Select bits [hi:lo] of this selection. Requires 0 <= lo <= hi < width().
        BitSel slice(int hi, int lo) const;
        BitSel operator[](BitRange range) const { return slice(range.hi, range.lo); }
        BitSel operator[](int bit) const { return slice(bit, bit); }

        /// Create a signal holding the value of this selection, for use as a sensitivity (pos/neg/change events)
        /// or to bind ports to. Each call creates a new signal, owned by the context. Must be called before
        /// elaboration. The untyped version uses the smallest unsigned type that fits the selection.
        SignalBase &signal(const std::string &name = "") const;
        template <std::integral T>
        DerivedSignal<T> &signal(const std::string &name = "") const;

    private:
        BitSel() = default;

        // Add a part above the current most significant part, merging it with that part if they are contiguous.
        void append(const BitPart &part);

    private:
        std::vector<BitPart> parts_;
        int width_ = 0;
    };

    namespace detail
    {
        /// A range of signals, such as a SignalArray or SignalArrayView.
        template <typename R>
        concept SignalRange = requires(R &r) {
            { *std::begin(r) } -> std::convertible_to<SignalBase &>;
            std::end(r);
        };

        inline void pack_append(std::vector<BitSel> &sels, const BitSel &sel)
        {
            sels.push_back(sel);
        }

        template <SignalRange R>
        void pack_append(std::vector<BitSel> &sels, R &signals)
        {
            for (auto &signal : signals)
            {
                sels.emplace_back(signal);
            }
        }
    } // namespace detail

    /*
        Concatenate signals, selections and signal arrays, most significant first, as in SystemVerilog {a, b}.
        Arrays and array views are packed in row-major order with element 0 most significant, like {arr[0], arr[1], ...}.
    */
    template <typename... Args>
    BitSel pack(Args &&...args)
    {
        std::vector<BitSel> sels;
        (detail::pack_append(sels, args), ...);
        return BitSel::concat(sels);
    }
} // namespace dspsim
