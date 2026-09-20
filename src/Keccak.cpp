#include "Keccak.h"

#include <array>
#include <bit>
#include <cstddef>
#include <cstring>

namespace {
constexpr std::size_t rate = 136;
constexpr std::array<std::uint64_t, 24> roundConstants = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808aULL,
    0x8000000080008000ULL, 0x000000000000808bULL, 0x0000000080000001ULL,
    0x8000000080008081ULL, 0x8000000000008009ULL, 0x000000000000008aULL,
    0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000aULL,
    0x000000008000808bULL, 0x800000000000008bULL, 0x8000000000008089ULL,
    0x8000000000008003ULL, 0x8000000000008002ULL, 0x8000000000000080ULL,
    0x000000000000800aULL, 0x800000008000000aULL, 0x8000000080008081ULL,
    0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};
constexpr std::array<int, 25> rotationOffsets = {
    0, 1, 62, 28, 27,
    36, 44, 6, 55, 20,
    3, 10, 43, 25, 39,
    41, 45, 15, 21, 8,
    18, 2, 61, 56, 14
};

std::uint64_t loadLittleEndian(const std::uint8_t* data) {
    std::uint64_t value = 0;
    for (std::size_t index = 0; index < 8; ++index) {
        value |= static_cast<std::uint64_t>(data[index]) << (index * 8);
    }

    return value;
}

void storeLittleEndian(std::uint64_t value, std::uint8_t* output) {
    for (std::size_t index = 0; index < 8; ++index) {
        output[index] = static_cast<std::uint8_t>(value >> (index * 8));
    }
}

void permute(std::array<std::uint64_t, 25>& state) {
    for (std::uint64_t roundConstant : roundConstants) {
        std::array<std::uint64_t, 5> columns{};
        std::array<std::uint64_t, 5> differences{};
        std::array<std::uint64_t, 25> transformed{};

        for (std::size_t x = 0; x < 5; ++x) {
            columns[x] = state[x] ^ state[x + 5] ^ state[x + 10] ^ state[x + 15] ^ state[x + 20];
        }

        for (std::size_t x = 0; x < 5; ++x) {
            differences[x] = columns[(x + 4) % 5] ^ std::rotl(columns[(x + 1) % 5], 1);
        }

        for (std::size_t y = 0; y < 5; ++y) {
            for (std::size_t x = 0; x < 5; ++x) {
                std::size_t index = x + 5 * y;
                state[index] ^= differences[x];
                std::size_t destination = y + 5 * ((2 * x + 3 * y) % 5);
                transformed[destination] = std::rotl(state[index], rotationOffsets[index]);
            }
        }

        for (std::size_t y = 0; y < 5; ++y) {
            for (std::size_t x = 0; x < 5; ++x) {
                state[x + 5 * y] = transformed[x + 5 * y] ^ (~transformed[(x + 1) % 5 + 5 * y] & transformed[(x + 2) % 5 + 5 * y]);
            }
        }

        state[0] ^= roundConstant;
    }
}

void absorbBlock(std::array<std::uint64_t, 25>& state, const std::uint8_t* block) {
    for (std::size_t index = 0; index < rate / 8; ++index) {
        state[index] ^= loadLittleEndian(block + index * 8);
    }

    permute(state);
}
}

std::array<std::uint8_t, 32> keccak256(std::span<const std::uint8_t> data) {
    std::array<std::uint64_t, 25> state{};
    while (data.size() >= rate) {
        absorbBlock(state, data.data());
        data = data.subspan(rate);
    }

    std::array<std::uint8_t, rate> finalBlock{};
    if (!data.empty()) {
        std::memcpy(finalBlock.data(), data.data(), data.size());
    }

    finalBlock[data.size()] = 0x01;
    finalBlock.back() |= 0x80;
    absorbBlock(state, finalBlock.data());

    std::array<std::uint8_t, 32> digest{};
    for (std::size_t index = 0; index < digest.size() / 8; ++index) {
        storeLittleEndian(state[index], digest.data() + index * 8);
    }

    return digest;
}
