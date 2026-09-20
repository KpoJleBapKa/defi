#include "MainWindow.h"

#include "Transaction.h"

#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <cctype>
#include <optional>
#include <stdexcept>

namespace {
constexpr const char* rpcUrl = "https://ethereum-sepolia-rpc.publicnode.com";
constexpr const char* defaultRecipient = "0x83553432c77dad24277ac88484f3a7bd07642efd";

QString text(const std::string& value) {
    return QString::fromStdString(value);
}

QString receiptText(const TransactionReceipt& receipt) {
    return QStringLiteral("Status: %1\nBlock: %2\nGas used: %3\nGas price: %4 Wei\nTX hash: %5")
        .arg(receipt.status == "0x1" ? QStringLiteral("SUCCESS") : QStringLiteral("FAILED"))
        .arg(text(quantityToDecimal(receipt.blockNumber)))
        .arg(text(quantityToDecimal(receipt.gasUsed)))
        .arg(text(quantityToDecimal(receipt.effectiveGasPrice)))
        .arg(text(receipt.transactionHash));
}
}

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    createInterface();
    connectActions();
    if (QFileInfo::exists(walletPath())) {
        runAction([this]() {
            displayWallet(loadWallet());
            appendLog(QStringLiteral("Wallet loaded."));
        });
    }
}

QString MainWindow::walletPath() const {
    return QDir(QStringLiteral(SOLOVIAN_DATA_DIR)).filePath(QStringLiteral("wallet.json"));
}

Wallet MainWindow::loadWallet() const {
    return Wallet::load(walletPath());
}

void MainWindow::createInterface() {
    setWindowTitle(QStringLiteral("Solovian - Sepolia Wallet"));
    resize(760, 560);

    QWidget* content = new QWidget(this);
    QVBoxLayout* root = new QVBoxLayout(content);

    QGroupBox* walletGroup = new QGroupBox(QStringLiteral("Wallet 0x12..."), content);
    QFormLayout* walletLayout = new QFormLayout(walletGroup);
    addressEdit_ = new QLineEdit(walletGroup);
    privateKeyEdit_ = new QLineEdit(walletGroup);
    addressEdit_->setReadOnly(true);
    privateKeyEdit_->setReadOnly(true);
    walletLayout->addRow(QStringLiteral("Address"), addressEdit_);
    walletLayout->addRow(QStringLiteral("Private key"), privateKeyEdit_);

    QHBoxLayout* walletButtons = new QHBoxLayout();
    generateButton_ = new QPushButton(QStringLiteral("Generate wallet"), walletGroup);
    balanceButton_ = new QPushButton(QStringLiteral("Check balance"), walletGroup);
    balanceLabel_ = new QLabel(QStringLiteral("Balance not checked"), walletGroup);
    walletButtons->addWidget(generateButton_);
    walletButtons->addWidget(balanceButton_);
    walletButtons->addWidget(balanceLabel_, 1);
    walletLayout->addRow(walletButtons);
    root->addWidget(walletGroup);

    QGroupBox* transactionGroup = new QGroupBox(QStringLiteral("Send Sepolia ETH"), content);
    QFormLayout* transactionLayout = new QFormLayout(transactionGroup);
    recipientEdit_ = new QLineEdit(QString::fromLatin1(defaultRecipient), transactionGroup);
    amountEdit_ = new QLineEdit(QStringLiteral("0.0001"), transactionGroup);
    sendButton_ = new QPushButton(QStringLiteral("Sign and send"), transactionGroup);
    transactionLayout->addRow(QStringLiteral("Recipient"), recipientEdit_);
    transactionLayout->addRow(QStringLiteral("Amount, ETH"), amountEdit_);
    transactionLayout->addRow(sendButton_);
    root->addWidget(transactionGroup);

    QHBoxLayout* receiptLayout = new QHBoxLayout();
    hashEdit_ = new QLineEdit(content);
    hashEdit_->setPlaceholderText(QStringLiteral("Transaction hash"));
    receiptButton_ = new QPushButton(QStringLiteral("Check receipt"), content);
    receiptLayout->addWidget(hashEdit_, 1);
    receiptLayout->addWidget(receiptButton_);
    root->addLayout(receiptLayout);

    logEdit_ = new QPlainTextEdit(content);
    logEdit_->setReadOnly(true);
    root->addWidget(logEdit_, 1);
    setCentralWidget(content);
}

void MainWindow::connectActions() {
    connect(generateButton_, &QPushButton::clicked, this, [this]() {
        runAction([this]() {
            generateWallet();
        });
    });
    connect(balanceButton_, &QPushButton::clicked, this, [this]() {
        runAction([this]() {
            refreshBalance();
        });
    });
    connect(sendButton_, &QPushButton::clicked, this, [this]() {
        runAction([this]() {
            sendTransaction();
        });
    });
    connect(receiptButton_, &QPushButton::clicked, this, [this]() {
        runAction([this]() {
            checkReceipt();
        });
    });
}

void MainWindow::displayWallet(const Wallet& wallet) {
    addressEdit_->setText(text(wallet.getAddress()));
    privateKeyEdit_->setText(text(wallet.getPrivateKeyHex()));
}

void MainWindow::generateWallet() {
    if (QFileInfo::exists(walletPath()) && QMessageBox::warning(this, QStringLiteral("Replace wallet"), QStringLiteral("The current funded wallet will be replaced. Continue?"), QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes) {
        return;
    }
    Wallet wallet;
    appendLog(QStringLiteral("Searching for address starting with 0x12..."));
    std::uint64_t attempts = wallet.generateVanity("12");
    wallet.save(walletPath());
    displayWallet(wallet);
    appendLog(QStringLiteral("Wallet found after %1 attempts.\nAddress: %2\nPrivate key: %3").arg(attempts).arg(text(wallet.getAddress()), text(wallet.getPrivateKeyHex())));
}

void MainWindow::refreshBalance() {
    Wallet wallet = loadWallet();
    std::string balance = EthereumRPC(rpcUrl).getBalance(wallet.getAddress());
    QString value = QStringLiteral("%1 ETH (%2 Wei)").arg(text(quantityToEther(balance)), text(quantityToDecimal(balance)));
    balanceLabel_->setText(value);
    appendLog(QStringLiteral("Balance: %1").arg(value));
}

void MainWindow::sendTransaction() {
    Wallet wallet = loadWallet();
    QString recipientText = recipientEdit_->text().trimmed();
    QString amountText = amountEdit_->text().trimmed();
    std::string recipient = recipientText.toStdString();
    std::string normalizedRecipient = recipient;
    std::transform(normalizedRecipient.begin(), normalizedRecipient.end(), normalizedRecipient.begin(), [](unsigned char value) {
        return static_cast<char>(std::tolower(value));
    });
    if (recipientText.isEmpty() || amountText.isEmpty() || normalizedRecipient == wallet.getAddress()) {
        throw std::invalid_argument("Invalid recipient or amount");
    }
    if (QMessageBox::question(this, QStringLiteral("Send transaction"), QStringLiteral("Send %1 Sepolia ETH to %2?").arg(amountText, recipientText)) != QMessageBox::Yes) {
        return;
    }

    EthereumRPC rpc(rpcUrl);
    std::uint64_t chainId = quantityToUint64(rpc.getChainId());
    if (chainId != 11155111) {
        throw std::runtime_error("RPC is not connected to Sepolia");
    }
    std::uint64_t nonce = quantityToUint64(rpc.getTransactionCount(wallet.getAddress()));
    std::uint64_t priorityFee = quantityToUint64(rpc.getMaxPriorityFeePerGas());
    std::uint64_t maxFee = recommendedMaxFee(quantityToUint64(rpc.getLatestBaseFeePerGas()), priorityFee);
    std::uint64_t gasLimit = quantityToUint64(rpc.estimateGas(wallet.getAddress(), recipient, etherToWeiQuantity(amountText.toStdString())));
    std::string rawTransaction = createSignedTransaction(chainId, nonce, priorityFee, maxFee, gasLimit, recipient, amountText.toStdString(), wallet);

    appendLog(QStringLiteral("From: %1\nTo: %2\nValue: %3 ETH, nonce: %4, gas limit: %5").arg(text(wallet.getAddress()), recipientText, amountText).arg(nonce).arg(gasLimit));
    std::string hash = rpc.sendRawTransaction(rawTransaction);
    hashEdit_->setText(text(hash));
    appendLog(QStringLiteral("Transaction submitted: %1\nWaiting for receipt...").arg(text(hash)));

    for (int attempt = 0; attempt < 40; ++attempt) {
        std::optional<TransactionReceipt> receipt = rpc.getTransactionReceipt(hash);
        if (receipt) {
            displayReceipt(*receipt);
            return;
        }
        QApplication::processEvents();
        QThread::msleep(3000);
    }
    appendLog(QStringLiteral("Receipt timeout. Check it later by transaction hash."));
}

void MainWindow::checkReceipt() {
    QString hash = hashEdit_->text().trimmed();
    if (hash.isEmpty()) {
        throw std::invalid_argument("Transaction hash is required");
    }
    std::optional<TransactionReceipt> receipt = EthereumRPC(rpcUrl).getTransactionReceipt(hash.toStdString());
    if (!receipt) {
        appendLog(QStringLiteral("Transaction is pending or unknown."));
        return;
    }
    displayReceipt(*receipt);
}

void MainWindow::displayReceipt(const TransactionReceipt& receipt) {
    QString value = receiptText(receipt);
    appendLog(QStringLiteral("Transaction confirmed.\n%1").arg(value));
    QMessageBox::information(this, QStringLiteral("Transaction receipt"), value);
}

void MainWindow::appendLog(const QString& text) {
    logEdit_->appendPlainText(text);
}

void MainWindow::runAction(const std::function<void()>& action) {
    setBusy(true);
    try {
        action();
    } catch (const std::exception& error) {
        QString message = QString::fromUtf8(error.what());
        appendLog(QStringLiteral("[ERROR] %1").arg(message));
        QMessageBox::critical(this, QStringLiteral("Error"), message);
    }
    setBusy(false);
}

void MainWindow::setBusy(bool busy) {
    generateButton_->setDisabled(busy);
    balanceButton_->setDisabled(busy);
    sendButton_->setDisabled(busy);
    receiptButton_->setDisabled(busy);
    busy ? QApplication::setOverrideCursor(Qt::WaitCursor) : QApplication::restoreOverrideCursor();
}
