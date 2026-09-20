#include "Hex.h"

#include <stdexcept>

namespace {
std::uint8_t hexDigitValue(char value) {
    if (value >= '0' && value <= '9') {
        return static_cast<std::uint8_t>(value - '0');
    }

    if (value >= 'a' && value <= 'f') {
        return static_cast<std::uint8_t>(value - 'a' + 10);
    }

    if (value >= 'A' && value <= 'F') {
        return static_cast<std::uint8_t>(value - 'A' + 10);
    }

    throw std::invalid_argument("Invalid hexadecimal character");
}
}

std::string bytesToHex(std::span<const std::uint8_t> bytes, bool includePrefix) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(bytes.size() * 2 + (includePrefix ? 2 : 0));

    if (includePrefix) {
        result.append("0x");
    }

    for (std::uint8_t byte : bytes) {
        result.push_back(digits[byte >> 4]);
        result.push_back(digits[byte & 0x0f]);
    }

    return result;
}

std::vector<std::uint8_t> hexToBytes(std::string_view value) {
    if (value.starts_with("0x") || value.starts_with("0X")) {
        value.remove_prefix(2);
    }

    if (value.size() % 2 != 0) {
        throw std::invalid_argument("Hexadecimal value must contain an even number of digits");
    }

    std::vector<std::uint8_t> result(value.size() / 2);
    for (std::size_t index = 0; index < result.size(); ++index) {
        std::uint8_t high = hexDigitValue(value[index * 2]);
        std::uint8_t low = hexDigitValue(value[index * 2 + 1]);
        result[index] = static_cast<std::uint8_t>((high << 4) | low);
    }

    return result;
}

