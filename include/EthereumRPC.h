#pragma once

#include <QJsonArray>
#include <QJsonValue>

#include <optional>
#include <string>
#include <string_view>

struct TransactionReceipt {
    std::string transactionHash;
    std::string blockNumber;
    std::string status;
    std::string gasUsed;
    std::string effectiveGasPrice;
};

class EthereumRPC {
public:
    explicit EthereumRPC(std::string rpcUrl);

    std::string getBalance(std::string_view address);
    std::string getChainId();
    std::string getTransactionCount(std::string_view address);
    std::string getMaxPriorityFeePerGas();
    std::string getLatestBaseFeePerGas();
    std::string estimateGas(std::string_view from, std::string_view to, std::string_view value);
    std::string sendRawTransaction(std::string_view rawTransaction);
    std::optional<TransactionReceipt> getTransactionReceipt(std::string_view transactionHash);

private:
    std::string rpcUrl_;

    QJsonValue call(std::string_view method, const QJsonArray& parameters);
};
