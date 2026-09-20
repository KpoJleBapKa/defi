#pragma once

#include "Wallet.h"

#include <cstdint>
#include <string>
#include <string_view>

std::uint64_t quantityToUint64(std::string_view value);
std::uint64_t recommendedMaxFee(std::uint64_t baseFee, std::uint64_t priorityFee);
std::string quantityToDecimal(std::string_view value);
std::string quantityToEther(std::string_view value);
std::string etherToWeiQuantity(std::string_view value);
std::string createSignedTransaction(std::uint64_t chainId, std::uint64_t nonce, std::uint64_t priorityFee, std::uint64_t maxFee, std::uint64_t gasLimit, std::string_view recipient, std::string_view amount, const Wallet& wallet);
