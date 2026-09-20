#pragma once

#include <array>
#include <cstdint>
#include <span>

std::array<std::uint8_t, 32> keccak256(std::span<const std::uint8_t> data);

