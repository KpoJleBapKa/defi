#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

std::string bytesToHex(std::span<const std::uint8_t> bytes, bool includePrefix = true);
std::vector<std::uint8_t> hexToBytes(std::string_view value);

