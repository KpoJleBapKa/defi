#include "EthereumRPC.h"

#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <utility>

namespace {
QString toQString(std::string_view value) {
    return QString::fromUtf8(value.data(), static_cast<int>(value.size()));
}

std::string getStringResult(const QJsonValue& result, std::string_view method) {
    if (!result.isString()) {
        throw std::runtime_error(std::string(method) + " returned a non-string result");
    }
    return result.toString().toStdString();
}

bool isAddress(std::string_view address) {
    return address.size() == 42 && address.starts_with("0x") && std::all_of(address.begin() + 2, address.end(), [](unsigned char digit) {
        return std::isxdigit(digit) != 0;
    });
}
}

EthereumRPC::EthereumRPC(std::string rpcUrl) : rpcUrl_(std::move(rpcUrl)) {
    if (rpcUrl_.empty()) {
        throw std::invalid_argument("RPC URL cannot be empty");
    }
}

std::string EthereumRPC::getBalance(std::string_view address) {
    if (!isAddress(address)) {
        throw std::invalid_argument("Invalid Ethereum address");
    }
    return getStringResult(call("eth_getBalance", {toQString(address), QStringLiteral("latest")}), "eth_getBalance");
}

std::string EthereumRPC::getChainId() {
    return getStringResult(call("eth_chainId", {}), "eth_chainId");
}

std::string EthereumRPC::getTransactionCount(std::string_view address) {
    if (!isAddress(address)) {
        throw std::invalid_argument("Invalid Ethereum address");
    }
    return getStringResult(call("eth_getTransactionCount", {toQString(address), QStringLiteral("pending")}), "eth_getTransactionCount");
}

std::string EthereumRPC::getMaxPriorityFeePerGas() {
    return getStringResult(call("eth_maxPriorityFeePerGas", {}), "eth_maxPriorityFeePerGas");
}

std::string EthereumRPC::getLatestBaseFeePerGas() {
    QJsonValue result = call("eth_getBlockByNumber", {QStringLiteral("latest"), false});
    if (!result.isObject() || !result.toObject().value(QStringLiteral("baseFeePerGas")).isString()) {
        throw std::runtime_error("Latest block does not contain baseFeePerGas");
    }
    return result.toObject().value(QStringLiteral("baseFeePerGas")).toString().toStdString();
}

std::string EthereumRPC::estimateGas(std::string_view from, std::string_view to, std::string_view value) {
    if (!isAddress(from) || !isAddress(to)) {
        throw std::invalid_argument("Invalid Ethereum address for gas estimation");
    }
    QJsonObject transaction {
        {QStringLiteral("from"), toQString(from)},
        {QStringLiteral("to"), toQString(to)},
        {QStringLiteral("value"), toQString(value)}
    };
    return getStringResult(call("eth_estimateGas", {transaction}), "eth_estimateGas");
}

std::string EthereumRPC::sendRawTransaction(std::string_view rawTransaction) {
    return getStringResult(call("eth_sendRawTransaction", {toQString(rawTransaction)}), "eth_sendRawTransaction");
}

std::optional<TransactionReceipt> EthereumRPC::getTransactionReceipt(std::string_view transactionHash) {
    QJsonValue result = call("eth_getTransactionReceipt", {toQString(transactionHash)});
    if (result.isNull()) {
        return std::nullopt;
    }
    if (!result.isObject()) {
        throw std::runtime_error("eth_getTransactionReceipt returned an invalid result");
    }

    QJsonObject object = result.toObject();
    TransactionReceipt receipt;
    receipt.transactionHash = object.value(QStringLiteral("transactionHash")).toString().toStdString();
    receipt.blockNumber = object.value(QStringLiteral("blockNumber")).toString().toStdString();
    receipt.status = object.value(QStringLiteral("status")).toString().toStdString();
    receipt.gasUsed = object.value(QStringLiteral("gasUsed")).toString().toStdString();
    receipt.effectiveGasPrice = object.value(QStringLiteral("effectiveGasPrice")).toString().toStdString();
    return receipt;
}

QJsonValue EthereumRPC::call(std::string_view method, const QJsonArray& parameters) {
    QJsonObject requestObject {
        {QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
        {QStringLiteral("method"), toQString(method)},
        {QStringLiteral("params"), parameters},
        {QStringLiteral("id"), 1}
    };
    QByteArray requestBody = QJsonDocument(requestObject).toJson(QJsonDocument::Compact);

    QNetworkAccessManager manager;
    QNetworkRequest request(QUrl(QString::fromStdString(rpcUrl_)));
    request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);
    QNetworkReply* reply = manager.post(request, requestBody);
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    timer.start(30000);
    loop.exec();

    if (!timer.isActive()) {
        reply->abort();
        reply->deleteLater();
        throw std::runtime_error("RPC request timed out");
    }
    timer.stop();

    QByteArray responseBody = reply->readAll();
    QNetworkReply::NetworkError networkError = reply->error();
    QString networkErrorText = reply->errorString();
    int statusCode = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    reply->deleteLater();

    if (networkError != QNetworkReply::NoError) {
        throw std::runtime_error("RPC request failed: " + networkErrorText.toStdString());
    }
    if (statusCode < 200 || statusCode >= 300) {
        throw std::runtime_error("RPC HTTP status: " + std::to_string(statusCode));
    }

    QJsonParseError parseError;
    QJsonDocument response = QJsonDocument::fromJson(responseBody, &parseError);
    if (parseError.error != QJsonParseError::NoError || !response.isObject()) {
        throw std::runtime_error("Invalid RPC JSON response: " + parseError.errorString().toStdString());
    }

    QJsonObject responseObject = response.object();
    if (responseObject.contains(QStringLiteral("error"))) {
        QByteArray error = QJsonDocument(responseObject.value(QStringLiteral("error")).toObject()).toJson(QJsonDocument::Compact);
        throw std::runtime_error("RPC " + std::string(method) + " error: " + error.toStdString());
    }
    if (!responseObject.contains(QStringLiteral("result"))) {
        throw std::runtime_error("RPC response does not contain a result");
    }
    return responseObject.value(QStringLiteral("result"));
}
