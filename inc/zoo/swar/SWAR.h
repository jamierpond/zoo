#pragma once

/// \file Swar.h SWAR operations

#include "zoo/swar/metaLog.h"

#include <type_traits>

namespace zoo { namespace swar {

using u64 = uint64_t;
using u32 = uint32_t;
using u16 = uint16_t;
using u8 = uint8_t;

/// Repeats the given pattern in the whole of the argument
/// \tparam T the desired integral type
/// \tparam Progression how big the pattern is
/// \tparam Remaining how many more times to copy the pattern
template<typename T, int Progression, int Remaining> struct BitmaskMaker {
    constexpr static T repeat(T v) noexcept {
        return
            BitmaskMaker<T, Progression, Remaining - 1>::repeat(v | (v << Progression));
    }
};
template<typename T, int P> struct BitmaskMaker<T, P, 0> {
    constexpr static T repeat(T v) noexcept { return v; }
};

/// Front end to \c BitmaskMaker with the repeating count set to the whole size
template<int size, typename T>
constexpr T makeBitmask(T v) noexcept {
    return BitmaskMaker<T, size, sizeof(T)*8/size>::repeat(v);
}

/// Core implementation details
namespace detail {
    template<int level>
    constexpr auto popcountMask =
        makeBitmask<1 << (level + 1)>(
            BitmaskMaker<uint64_t, 1, (1 << level) - 1>::repeat(1)
        );

    static_assert(makeBitmask<2>(1ull) == popcountMask<0>);
}

template<int Bits> struct UInteger_impl;
template<> struct UInteger_impl<8> { using type = uint8_t; };
template<> struct UInteger_impl<16> { using type = uint16_t; };
template<> struct UInteger_impl<32> { using type = uint32_t; };
template<> struct UInteger_impl<64> { using type = uint64_t; };

template<int Bits> using UInteger = typename UInteger_impl<Bits>::type;

template<int Level>
constexpr uint64_t popcount_logic(uint64_t arg) noexcept {
    auto v = popcount_logic<Level - 1>(arg);
    constexpr auto shifter = 1 << Level;
    return
        ((v >> shifter) & detail::popcountMask<Level>) +
        (v & detail::popcountMask<Level>);
}
/// Hamming weight of each bit pair
template<>
constexpr uint64_t popcount_logic<0>(uint64_t v) noexcept {
    // 00: 00; 00
    // 01: 01; 01
    // 10: 01; 01
    // 11: 10; 10
    return v - ((v >> 1) & detail::popcountMask<0>);
}

template<int Level>
constexpr uint64_t popcount_builtin(uint64_t v) noexcept {
    using UI = UInteger<1 << (Level + 3)>;
    constexpr auto times = 8*sizeof(v);
    uint64_t rv = 0;
    for(auto n = times; n; ) {
        n -= 8*sizeof(UI);
        UI tmp = v >> n;
        tmp = __builtin_popcountll(tmp);
        rv |= uint64_t(tmp) << n;
    }
    return rv;
}

namespace detail {

    template<bool> struct Selector_impl {
        template<int Level>
        constexpr static uint64_t execute(uint64_t v) noexcept {
            return popcount_logic<Level>(v);
        }
    };
    template<> struct Selector_impl<true> {
        template<int Level>
        constexpr static uint64_t execute(uint64_t v) noexcept {
            return popcount_builtin<Level - 2>(v);
        }
    };

}

template<int Level>
constexpr uint64_t popcount(uint64_t a) noexcept {
    return detail::Selector_impl<2 < Level>::template execute<Level>(a);
}


template<typename T> constexpr typename std::make_unsigned<T>::type msb_index(T v) noexcept {
    return 8*sizeof(T) - 1 - __builtin_clzll(v);
}

template<typename T> constexpr typename std::make_unsigned<T>::type lsb_index(T v) noexcept {
    return __builtin_ctzll(v) + 1;
}

template<int NBits, typename T = uint64_t> struct SWAR {
    SWAR() = default;
    constexpr explicit SWAR(T v): m_v(v) {}
    constexpr explicit operator T() const noexcept { return m_v; }

    constexpr T value() const noexcept { return m_v; }

    constexpr SWAR operator|(SWAR o) const noexcept { return SWAR(m_v | o.m_v); }
    constexpr SWAR operator&(SWAR o) const noexcept { return SWAR(m_v & o.m_v); }
    constexpr SWAR operator^(SWAR o) const noexcept { return SWAR(m_v ^ o.m_v); }

    constexpr T at(int position) const noexcept {
        constexpr auto filter = (T(1) << NBits) - 1;
        return filter & (m_v >> (NBits * position));
    }

    constexpr SWAR clear(int position) const noexcept {
        constexpr auto filter = (T(1) << NBits) - 1;
        auto invertedMask = filter << (NBits * position);
        auto mask = ~invertedMask;
        return SWAR(m_v & mask);
    }

    /// Notably, this is the SWAR lane index that contains the MSB.
    constexpr int top() const noexcept { return msb_index(m_v) / NBits; }
    /// Notably, this is the SWAR lane index that contains the lsb.
    constexpr int lsbIndex() const noexcept { return __builtin_ctzll(m_v) / NBits; }

    constexpr SWAR set(int index, int bit) const noexcept {
        return SWAR(m_v | (T(1) << (index * NBits + bit)));
    }

    T m_v;
};

/// Defining operator== on base SWAR types is entirely too error prone. Force a verbose invocation.
template<int NBits, typename T = uint64_t> constexpr auto horizontalEquality(SWAR<NBits, T> left, SWAR<NBits, T> right) {
  return left.m_v == right.m_v;
}

template<int NBits, typename T = uint64_t>
constexpr auto isolate(T pattern) {
  return pattern & ((T(1)<<NBits)-1);
}

template<typename T = uint64_t>
constexpr auto clearLSB(T v) {
  return v & (v - 1);
}

template<typename T = uint64_t>
constexpr auto isolateLSB(T v) {
  return v & ~clearLSB(v);
}

template<int NBits, typename T = uint64_t>
constexpr auto maskLowBits() {
  return (T(1)<<(NBits-1)) | ((T(1)<<(NBits-1))-1);
}

template<int NBits, typename T = uint64_t>
constexpr auto clearLSBits(T v) {
  return v &(~(maskLowBits<NBits>() << metaLogFloor(isolateLSB<T>(v))));
}

template<int NBits, typename T = uint64_t>
constexpr auto isolateLSBits(T v) {
  return v &(maskLowBits<NBits>() << metaLogFloor(isolateLSB<T>(v)));
}

template<int NBits, typename T = uint64_t>
constexpr auto broadcast(SWAR<NBits, T> v) {
  constexpr T Ones = makeBitmask<NBits, T>(SWAR<NBits, T>{1}.value());
  return SWAR<NBits, T>(T(v) * Ones);
}

template<int NBits, typename T>
struct BooleanSWAR: SWAR<NBits, T> {
    // Booleanness is stored in MSB of a given swar.
    static constexpr T MSBs = broadcast<NBits, T>(SWAR<NBits, T>(T(1) << (NBits -1))).value();
    constexpr explicit BooleanSWAR(T v): SWAR<NBits, T>(v) {}

    constexpr BooleanSWAR clear(int bit) const noexcept {
        constexpr auto Bit = T(1) << (NBits - 1);
        return this->m_v ^ (Bit << (NBits * bit)); }
    constexpr int best() const noexcept { return this->top(); }

    constexpr operator bool() const noexcept { return this->m_v; }

    constexpr BooleanSWAR operator not() const noexcept {
        return MSBs ^ *this;
    }
 private:
    constexpr BooleanSWAR(SWAR<NBits, T> initializer) noexcept: SWAR<NBits, T>(initializer) {}

    template<int NB, typename TT> friend
    constexpr BooleanSWAR<NB, TT> greaterEqual_MSB_off(SWAR<NB, TT> left, SWAR<NB, TT> right) noexcept;

};

template<int NBits, typename T>
constexpr BooleanSWAR<NBits, T> greaterEqual_MSB_off(SWAR<NBits, T> left, SWAR<NBits, T> right) noexcept {
    constexpr auto MSOnes = BooleanSWAR<NBits, T>::MSBs;
    return SWAR<NBits, T>{
        (((left.value() | MSOnes) - right.value()) & MSOnes)
    };
}

template<int N, int NBits, typename T>
constexpr BooleanSWAR<NBits, T> greaterEqual(SWAR<NBits, T> v) noexcept {
    static_assert(1 < NBits, "Degenerated SWAR");
    //static_assert(metaLogCeiling(N) < NBits, "N is too big for this technique");  // ctzll isn't constexpr.
    constexpr auto msbPos  = NBits - 1;
    constexpr auto msb = T(1) << msbPos;
    constexpr auto msbMask = makeBitmask<NBits, T>(msb);
    constexpr auto subtraend = makeBitmask<NBits, T>(N);
    auto adjusted = v.value() | msbMask;
    auto rv = adjusted - subtraend;
    rv &= msbMask;
    return BooleanSWAR<NBits, T>(rv);
}

}}
