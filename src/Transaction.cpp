#include "Transaction.h"

#include "Hex.h"
#include "Keccak.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>

namespace {
using Bytes = std::vector<std::uint8_t>;

struct Transaction {
    std::uint64_t chainId;
    std::uint64_t nonce;
    std::uint64_t priorityFee;
    std::uint64_t maxFee;
    std::uint64_t gasLimit;
    std::array<std::uint8_t, 20> recipient{};
    Bytes value;
};

unsigned int hexDigit(char value) {
    if (value >= '0' && value <= '9') {
        return static_cast<unsigned int>(value - '0');
    }
    if (value >= 'a' && value <= 'f') {
        return static_cast<unsigned int>(value - 'a' + 10);
    }
    if (value >= 'A' && value <= 'F') {
        return static_cast<unsigned int>(value - 'A' + 10);
    }
    throw std::invalid_argument("Invalid Ethereum quantity");
}

void validateQuantity(std::string_view value) {
    if (value.size() < 3 || !value.starts_with("0x") || (value.size() > 3 && value[2] == '0')) {
        throw std::invalid_argument("Invalid Ethereum quantity");
    }
    for (char digit : value.substr(2)) {
        hexDigit(digit);
    }
}

Bytes trimZeros(std::span<const std::uint8_t> value) {
    auto first = std::find_if(value.begin(), value.end(), [](std::uint8_t byte) {
        return byte != 0;
    });
    return Bytes(first, value.end());
}

Bytes integerBytes(std::uint64_t value) {
    Bytes result;
    while (value > 0) {
        result.push_back(static_cast<std::uint8_t>(value & 0xff));
        value >>= 8;
    }
    std::reverse(result.begin(), result.end());
    return result;
}

Bytes encodeLength(std::size_t length, std::uint8_t shortOffset, std::uint8_t longOffset) {
    if (length <= 55) {
        return {static_cast<std::uint8_t>(shortOffset + length)};
    }
    Bytes lengthBytes;
    while (length > 0) {
        lengthBytes.push_back(static_cast<std::uint8_t>(length & 0xff));
        length >>= 8;
    }
    std::reverse(lengthBytes.begin(), lengthBytes.end());
    Bytes result = {static_cast<std::uint8_t>(longOffset + lengthBytes.size())};
    result.insert(result.end(), lengthBytes.begin(), lengthBytes.end());
    return result;
}

Bytes encodeBytes(std::span<const std::uint8_t> value) {
    if (value.size() == 1 && value.front() < 0x80) {
        return {value.front()};
    }
    Bytes result = encodeLength(value.size(), 0x80, 0xb7);
    result.insert(result.end(), value.begin(), value.end());
    return result;
}

Bytes encodeList(const std::vector<Bytes>& items) {
    std::size_t length = 0;
    for (const Bytes& item : items) {
        length += item.size();
    }
    Bytes result = encodeLength(length, 0xc0, 0xf7);
    for (const Bytes& item : items) {
        result.insert(result.end(), item.begin(), item.end());
    }
    return result;
}

Bytes parseEther(std::string_view value) {
    std::size_t separator = value.find('.');
    if (value.empty() || value.front() == '-' || value.front() == '+' || (separator != std::string_view::npos && value.find('.', separator + 1) != std::string_view::npos)) {
        throw std::invalid_argument("Invalid ETH amount");
    }
    std::string integerPart(value.substr(0, separator));
    std::string fraction = separator == std::string_view::npos ? "" : std::string(value.substr(separator + 1));
    if (integerPart.empty()) {
        integerPart = "0";
    }
    if (fraction.size() > 18 || !std::all_of(integerPart.begin(), integerPart.end(), ::isdigit) || !std::all_of(fraction.begin(), fraction.end(), ::isdigit)) {
        throw std::invalid_argument("Invalid ETH amount");
    }
    fraction.append(18 - fraction.size(), '0');

    Bytes result = {0};
    for (char digit : integerPart + fraction) {
        unsigned int carry = static_cast<unsigned int>(digit - '0');
        for (auto iterator = result.rbegin(); iterator != result.rend(); ++iterator) {
            unsigned int current = static_cast<unsigned int>(*iterator) * 10 + carry;
            *iterator = static_cast<std::uint8_t>(current & 0xff);
            carry = current >> 8;
        }
        if (carry > 0) {
            result.insert(result.begin(), static_cast<std::uint8_t>(carry));
        }
    }
    result = trimZeros(result);
    if (result.empty() || result.size() > 32) {
        throw std::invalid_argument("ETH amount must be greater than zero and fit uint256");
    }
    return result;
}

std::string bytesToQuantity(std::span<const std::uint8_t> value) {
    Bytes normalized = trimZeros(value);
    if (normalized.empty()) {
        return "0x0";
    }
    std::string digits = bytesToHex(normalized, false);
    return "0x" + digits.substr(digits.find_first_not_of('0'));
}

std::vector<Bytes> transactionFields(const Transaction& transaction) {
    return {
        encodeBytes(integerBytes(transaction.chainId)),
        encodeBytes(integerBytes(transaction.nonce)),
        encodeBytes(integerBytes(transaction.priorityFee)),
        encodeBytes(integerBytes(transaction.maxFee)),
        encodeBytes(integerBytes(transaction.gasLimit)),
        encodeBytes(transaction.recipient),
        encodeBytes(transaction.value),
        encodeBytes({}),
        encodeList({})
    };
}
}

std::uint64_t quantityToUint64(std::string_view value) {
    validateQuantity(value);
    std::uint64_t result = 0;
    for (char digit : value.substr(2)) {
        unsigned int numericDigit = hexDigit(digit);
        if (result > (std::numeric_limits<std::uint64_t>::max() - numericDigit) / 16) {
            throw std::overflow_error("Ethereum quantity does not fit uint64");
        }
        result = result * 16 + numericDigit;
    }
    return result;
}

std::uint64_t recommendedMaxFee(std::uint64_t baseFee, std::uint64_t priorityFee) {
    if (baseFee > (std::numeric_limits<std::uint64_t>::max() - priorityFee) / 2) {
        throw std::overflow_error("Fee overflow");
    }
    return baseFee * 2 + priorityFee;
}

std::string quantityToDecimal(std::string_view value) {
    validateQuantity(value);
    std::string decimal = "0";
    for (char digit : value.substr(2)) {
        unsigned int carry = hexDigit(digit);
        for (auto iterator = decimal.rbegin(); iterator != decimal.rend(); ++iterator) {
            unsigned int current = static_cast<unsigned int>(*iterator - '0') * 16 + carry;
            *iterator = static_cast<char>('0' + current % 10);
            carry = current / 10;
        }
        while (carry > 0) {
            decimal.insert(decimal.begin(), static_cast<char>('0' + carry % 10));
            carry /= 10;
        }
    }
    std::size_t first = decimal.find_first_not_of('0');
    return first == std::string::npos ? "0" : decimal.substr(first);
}

std::string quantityToEther(std::string_view value) {
    std::string wei = quantityToDecimal(value);
    if (wei.size() <= 18) {
        wei.insert(0, 18 - wei.size() + 1, '0');
    }
    wei.insert(wei.end() - 18, '.');
    while (wei.back() == '0') {
        wei.pop_back();
    }
    if (wei.back() == '.') {
        wei.pop_back();
    }
    return wei;
}

std::string etherToWeiQuantity(std::string_view value) {
    return bytesToQuantity(parseEther(value));
}

std::string createSignedTransaction(std::uint64_t chainId, std::uint64_t nonce, std::uint64_t priorityFee, std::uint64_t maxFee,
                                    std::uint64_t gasLimit, std::string_view recipient, std::string_view amount, const Wallet& wallet) {
    std::vector<std::uint8_t> recipientBytes = hexToBytes(recipient);
    if (recipientBytes.size() != 20 || maxFee < priorityFee) {
        throw std::invalid_argument("Invalid transaction parameters");
    }

    Transaction transaction{chainId, nonce, priorityFee, maxFee, gasLimit};
    std::copy(recipientBytes.begin(), recipientBytes.end(), transaction.recipient.begin());
    transaction.value = parseEther(amount);

    Bytes unsignedData = encodeList(transactionFields(transaction));
    unsignedData.insert(unsignedData.begin(), 0x02);
    RecoverableSignature signature = wallet.signHash(keccak256(unsignedData));
    if (signature.recoveryId < 0 || signature.recoveryId > 1) {
        throw std::runtime_error("Invalid EIP-1559 recovery ID");
    }

    std::vector<Bytes> fields = transactionFields(transaction);
    fields.push_back(encodeBytes(integerBytes(static_cast<std::uint64_t>(signature.recoveryId))));
    fields.push_back(encodeBytes(trimZeros(signature.r)));
    fields.push_back(encodeBytes(trimZeros(signature.s)));
    Bytes signedData = encodeList(fields);
    signedData.insert(signedData.begin(), 0x02);
    return bytesToHex(signedData);
}












