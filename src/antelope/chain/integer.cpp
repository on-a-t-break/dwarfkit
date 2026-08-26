#include <dwarfkit/antelope/chain/integer.hpp>

#include <algorithm>

#include <dwarfkit/antelope/utils.hpp>

namespace dwarfkit {

json intToJSON(int64_t value) {
    // match FCs behavior and return strings for anything above 32-bit;
    // BN.bitLength works on the magnitude
    const uint64_t magnitude =
        value < 0 ? ~static_cast<uint64_t>(value) + 1 : static_cast<uint64_t>(value);
    if (magnitude > 0xffffffffull) {
        return std::to_string(value);
    }
    return value;
}

json intToJSON(uint64_t value) {
    if (value > 0xffffffffull) {
        return std::to_string(value);
    }
    return value;
}

namespace {

// 128-bit helpers on four 32-bit limbs, little endian
using Limbs = std::array<uint32_t, 4>;

constexpr Limbs toLimbs(uint64_t lo, uint64_t hi) {
    return {static_cast<uint32_t>(lo), static_cast<uint32_t>(lo >> 32),
            static_cast<uint32_t>(hi), static_cast<uint32_t>(hi >> 32)};
}

constexpr std::pair<uint64_t, uint64_t> fromLimbs(const Limbs& limbs) {
    return {static_cast<uint64_t>(limbs[0]) | (static_cast<uint64_t>(limbs[1]) << 32),
            static_cast<uint64_t>(limbs[2]) | (static_cast<uint64_t>(limbs[3]) << 32)};
}

// value = value * factor + addend, returns the carry out (nonzero on overflow)
constexpr uint32_t mulAdd(Limbs& value, uint32_t factor, uint32_t addend) {
    uint64_t carry = addend;
    for (auto& limb : value) {
        const uint64_t product = static_cast<uint64_t>(limb) * factor + carry;
        limb = static_cast<uint32_t>(product);
        carry = product >> 32;
    }
    return static_cast<uint32_t>(carry);
}

// value = value / divisor, returns the remainder
constexpr uint32_t divMod(Limbs& value, uint32_t divisor) {
    uint64_t remainder = 0;
    for (int i = 3; i >= 0; --i) {
        const uint64_t current = (remainder << 32) | value[static_cast<size_t>(i)];
        value[static_cast<size_t>(i)] = static_cast<uint32_t>(current / divisor);
        remainder = current % divisor;
    }
    return static_cast<uint32_t>(remainder);
}

constexpr bool isZero(const Limbs& value) {
    return value[0] == 0 && value[1] == 0 && value[2] == 0 && value[3] == 0;
}

std::string unsignedToString(uint64_t lo, uint64_t hi) {
    auto limbs = toLimbs(lo, hi);
    std::string digits;
    do {
        digits.push_back(static_cast<char>('0' + divMod(limbs, 10)));
    } while (!isZero(limbs));
    std::reverse(digits.begin(), digits.end());
    return digits;
}

struct ParsedUnsigned {
    uint64_t lo = 0;
    uint64_t hi = 0;
    bool negative = false;
    bool overflowed = false;  // magnitude did not fit 128 bits
    std::string normalized;   // sign + digits without leading zeros
};

Result<ParsedUnsigned> parseDecimal(std::string_view value) {
    ParsedUnsigned parsed;
    size_t i = 0;
    if (i < value.size() && (value[i] == '-' || value[i] == '+')) {
        parsed.negative = value[i] == '-';
        i++;
    }
    if (i >= value.size()) {
        return err(ErrorKind::Invalid, "Invalid number");
    }
    Limbs magnitude{};
    for (; i < value.size(); ++i) {
        const char c = value[i];
        if (c < '0' || c > '9') {
            return err(ErrorKind::Invalid, "Invalid number");
        }
        if (mulAdd(magnitude, 10, static_cast<uint32_t>(c - '0'))) {
            parsed.overflowed = true;
        }
    }
    std::tie(parsed.lo, parsed.hi) = fromLimbs(magnitude);
    const size_t firstDigit = value.find_first_not_of('0', value.find_first_of("0123456789"));
    std::string digits(firstDigit == std::string_view::npos || value[firstDigit] < '0' ||
                               value[firstDigit] > '9'
                           ? "0"
                           : value.substr(firstDigit));
    if (parsed.negative && digits != "0") {
        parsed.normalized = "-" + digits;
    } else {
        parsed.normalized = digits;
        parsed.negative = parsed.negative && digits != "0";
    }
    return parsed;
}

std::array<uint8_t, 16> toLEBytes(uint64_t lo, uint64_t hi) {
    std::array<uint8_t, 16> bytes{};
    for (int i = 0; i < 8; i++) {
        bytes[static_cast<size_t>(i)] = static_cast<uint8_t>(lo >> (i * 8));
        bytes[static_cast<size_t>(i + 8)] = static_cast<uint8_t>(hi >> (i * 8));
    }
    return bytes;
}

std::pair<uint64_t, uint64_t> fromLEBytes(std::span<const uint8_t, 16> le) {
    uint64_t lo = 0, hi = 0;
    for (int i = 7; i >= 0; i--) {
        lo = (lo << 8) | le[static_cast<size_t>(i)];
        hi = (hi << 8) | le[static_cast<size_t>(i + 8)];
    }
    return {lo, hi};
}

}  // namespace

Result<UInt128> UInt128::from(std::string_view value, OverflowBehavior overflow) {
    DK_TRY(parsed, parseDecimal(value));
    switch (overflow) {
        case OverflowBehavior::Throw:
            if (parsed.negative) {
                return err(ErrorKind::Invalid,
                           "Number " + parsed.normalized + " underflows uint128");
            }
            if (parsed.overflowed) {
                return err(ErrorKind::Invalid, "Number " + parsed.normalized + " overflows uint128");
            }
            break;
        case OverflowBehavior::Truncate:
            if (parsed.negative) {
                return UInt128{~parsed.lo, ~parsed.hi}.adding(UInt128(uint64_t(1)));
            }
            break;
        case OverflowBehavior::Clamp:
            if (parsed.negative) return UInt128::min();
            if (parsed.overflowed) return UInt128::max();
            break;
    }
    return UInt128{parsed.lo, parsed.hi};
}

Result<UInt128> UInt128::random() {
    DK_TRY(bytes, secureRandom(16));
    return fromByteArray(std::span<const uint8_t, 16>(bytes.data(), 16));
}

std::array<uint8_t, 16> UInt128::byteArray() const { return toLEBytes(lo, hi); }

UInt128 UInt128::fromByteArray(std::span<const uint8_t, 16> le) {
    const auto [lo, hi] = fromLEBytes(le);
    return {lo, hi};
}

std::string UInt128::toString() const { return unsignedToString(lo, hi); }

json UInt128::toJSON() const {
    if (hi != 0 || lo > 0xffffffffull) {
        return toString();
    }
    return lo;
}

Result<Int128> Int128::from(std::string_view value, OverflowBehavior overflow) {
    DK_TRY(parsed, parseDecimal(value));
    // magnitude limits: 2^127 - 1 positive, 2^127 negative
    const bool overflows =
        parsed.overflowed ||
        (!parsed.negative && (parsed.hi >> 63)) ||
        (parsed.negative && (parsed.hi > 0x8000000000000000ull ||
                             (parsed.hi == 0x8000000000000000ull && parsed.lo != 0)));
    Int128 result{parsed.lo, parsed.hi};
    if (parsed.negative) {
        result = result.negated();
    }
    switch (overflow) {
        case OverflowBehavior::Throw:
            if (overflows) {
                return err(ErrorKind::Invalid,
                           "Number " + parsed.normalized +
                               (parsed.negative ? " underflows int128" : " overflows int128"));
            }
            break;
        case OverflowBehavior::Truncate:
            break;  // keep the low 128 bits of the two's complement pattern
        case OverflowBehavior::Clamp:
            if (overflows) return parsed.negative ? Int128::min() : Int128::max();
            break;
    }
    return result;
}

Result<Int128> Int128::random() {
    DK_TRY(bytes, secureRandom(16));
    return fromByteArray(std::span<const uint8_t, 16>(bytes.data(), 16));
}

std::array<uint8_t, 16> Int128::byteArray() const { return toLEBytes(lo, hi); }

Int128 Int128::fromByteArray(std::span<const uint8_t, 16> le) {
    const auto [lo, hi] = fromLEBytes(le);
    return {lo, hi};
}

std::string Int128::toString() const {
    if (isNegative()) {
        const Int128 magnitude = negated();
        return "-" + unsignedToString(magnitude.lo, magnitude.hi);
    }
    return unsignedToString(lo, hi);
}

json Int128::toJSON() const {
    const Int128 magnitude = isNegative() ? negated() : *this;
    if (magnitude.hi != 0 || magnitude.lo > 0xffffffffull) {
        return toString();
    }
    return isNegative() ? json(-static_cast<int64_t>(magnitude.lo)) : json(magnitude.lo);
}

}  // namespace dwarfkit
