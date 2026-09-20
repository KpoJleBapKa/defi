#pragma once

#include <array>
#include <cstdint>
#include <QString>
#include <string>
#include <string_view>

struct RecoverableSignature {
    std::array<std::uint8_t, 32> r{};
    std::array<std::uint8_t, 32> s{};
    int recoveryId = 0;
};

class Wallet {
public:
    Wallet();

    static Wallet load(const QString& path);

    std::uint64_t generateVanity(std::string_view prefix);
    void save(const QString& path) const;
    std::string getPrivateKeyHex() const;
    std::string getAddress() const;
    RecoverableSignature signHash(const std::array<std::uint8_t, 32>& hash) const;

private:
    std::array<std::uint8_t, 32> privateKey_{};
    std::array<std::uint8_t, 20> address_{};
    bool initialized_ = false;

    static Wallet fromPrivateKeyHex(std::string_view privateKeyHex);
    void generate();
    void deriveAddress();
};
