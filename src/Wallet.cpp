#include "Wallet.h"

#include "Hex.h"
#include "Keccak.h"

#include <Windows.h>
#include <bcrypt.h>
#include <secp256k1.h>
#include <secp256k1_recovery.h>

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cctype>
#include <memory>
#include <span>
#include <stdexcept>
#include <vector>

namespace {
class Secp256k1Context {
public:
    Secp256k1Context() : context_(secp256k1_context_create(SECP256K1_CONTEXT_NONE)) {
        if (context_ == nullptr) {
            throw std::runtime_error("Failed to create secp256k1 context");
        }

        std::array<std::uint8_t, 32> seed{};
        fillRandom(seed);
        if (secp256k1_context_randomize(context_, seed.data()) != 1) {
            throw std::runtime_error("Failed to randomize secp256k1 context");
        }
    }

    ~Secp256k1Context() {
        secp256k1_context_destroy(context_);
    }

    Secp256k1Context(const Secp256k1Context&) = delete;
    Secp256k1Context& operator=(const Secp256k1Context&) = delete;

    secp256k1_context* get() const {
        return context_;
    }

    static void fillRandom(std::span<std::uint8_t> output) {
        NTSTATUS status = BCryptGenRandom(nullptr, output.data(), static_cast<ULONG>(output.size()), BCRYPT_USE_SYSTEM_PREFERRED_RNG);
        if (status < 0) {
            throw std::runtime_error("BCryptGenRandom failed");
        }
    }

private:
    secp256k1_context* context_;
};

Secp256k1Context& getSecp256k1Context() {
    static Secp256k1Context context;
    return context;
}

std::string normalizePrefix(std::string_view prefix) {
    if (prefix.starts_with("0x") || prefix.starts_with("0X")) {
        prefix.remove_prefix(2);
    }

    if (prefix.empty()) {
        throw std::invalid_argument("Vanity prefix cannot be empty");
    }

    std::string normalized(prefix);
    std::transform(normalized.begin(), normalized.end(), normalized.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });

    bool valid = std::all_of(normalized.begin(), normalized.end(), [](unsigned char value) {
        return std::isxdigit(value) != 0;
    });
    if (!valid || normalized.size() > 40) {
        throw std::invalid_argument("Vanity prefix must contain from 1 to 40 hexadecimal digits");
    }

    return normalized;
}
}

Wallet::Wallet() = default;

Wallet Wallet::load(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Failed to open wallet file");
    }

    QJsonParseError error;
    QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject()) {
        throw std::runtime_error("Invalid wallet JSON");
    }
    return fromPrivateKeyHex(document.object().value(QStringLiteral("privateKey")).toString().toStdString());
}

Wallet Wallet::fromPrivateKeyHex(std::string_view privateKeyHex) {
    std::vector<std::uint8_t> bytes = hexToBytes(privateKeyHex);
    if (bytes.size() != 32) {
        throw std::invalid_argument("Private key must contain exactly 32 bytes");
    }

    Wallet wallet;
    std::copy(bytes.begin(), bytes.end(), wallet.privateKey_.begin());
    if (secp256k1_ec_seckey_verify(getSecp256k1Context().get(), wallet.privateKey_.data()) != 1) {
        throw std::invalid_argument("Private key is outside the secp256k1 range");
    }

    wallet.deriveAddress();
    return wallet;
}

void Wallet::generate() {
    do {
        Secp256k1Context::fillRandom(privateKey_);
    } while (secp256k1_ec_seckey_verify(getSecp256k1Context().get(), privateKey_.data()) != 1);

    deriveAddress();
}

std::uint64_t Wallet::generateVanity(std::string_view prefix) {
    std::string normalizedPrefix = normalizePrefix(prefix);
    std::uint64_t attempts = 0;

    do {
        generate();
        ++attempts;
    } while (!getAddress().substr(2).starts_with(normalizedPrefix));

    return attempts;
}

void Wallet::save(const QString& path) const {
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        throw std::runtime_error("Failed to open wallet file for writing");
    }

    QJsonObject document {
        {QStringLiteral("network"), QStringLiteral("sepolia")},
        {QStringLiteral("address"), QString::fromStdString(getAddress())},
        {QStringLiteral("privateKey"), QString::fromStdString(getPrivateKeyHex())}
    };
    if (file.write(QJsonDocument(document).toJson(QJsonDocument::Indented)) < 0) {
        throw std::runtime_error("Failed to write wallet file");
    }
}

std::string Wallet::getPrivateKeyHex() const {
    if (!initialized_) {
        throw std::logic_error("Wallet is not initialized");
    }

    return bytesToHex(privateKey_);
}

std::string Wallet::getAddress() const {
    if (!initialized_) {
        throw std::logic_error("Wallet is not initialized");
    }

    return bytesToHex(address_);
}

RecoverableSignature Wallet::signHash(const std::array<std::uint8_t, 32>& hash) const {
    if (!initialized_) {
        throw std::logic_error("Wallet is not initialized");
    }

    secp256k1_ecdsa_recoverable_signature signature;
    if (secp256k1_ecdsa_sign_recoverable(getSecp256k1Context().get(), &signature, hash.data(), privateKey_.data(), nullptr, nullptr) != 1) {
        throw std::runtime_error("Failed to sign transaction hash");
    }

    std::array<std::uint8_t, 64> compact{};
    RecoverableSignature result;
    if (secp256k1_ecdsa_recoverable_signature_serialize_compact(getSecp256k1Context().get(), compact.data(), &result.recoveryId, &signature) != 1) {
        throw std::runtime_error("Failed to serialize transaction signature");
    }
    std::copy_n(compact.begin(), result.r.size(), result.r.begin());
    std::copy_n(compact.begin() + result.r.size(), result.s.size(), result.s.begin());
    return result;
}

void Wallet::deriveAddress() {
    secp256k1_pubkey publicKey;
    if (secp256k1_ec_pubkey_create(getSecp256k1Context().get(), &publicKey, privateKey_.data()) != 1) {
        throw std::runtime_error("Failed to derive public key");
    }

    std::array<std::uint8_t, 65> serializedPublicKey{};
    std::size_t publicKeyLength = serializedPublicKey.size();
    if (secp256k1_ec_pubkey_serialize(getSecp256k1Context().get(), serializedPublicKey.data(), &publicKeyLength, &publicKey,
                                      SECP256K1_EC_UNCOMPRESSED) != 1 || publicKeyLength != serializedPublicKey.size()) {
        throw std::runtime_error("Failed to serialize public key");
    }

    std::array<std::uint8_t, 32> digest = keccak256(std::span<const std::uint8_t>(serializedPublicKey).subspan(1));
    std::copy(digest.end() - address_.size(), digest.end(), address_.begin());
    initialized_ = true;
}
