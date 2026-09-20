#pragma once

#include "EthereumRPC.h"
#include "Wallet.h"

#include <QMainWindow>

#include <functional>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

class MainWindow : public QMainWindow {
public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QLineEdit* addressEdit_;
    QLineEdit* privateKeyEdit_;
    QLabel* balanceLabel_;
    QLineEdit* recipientEdit_;
    QLineEdit* amountEdit_;
    QLineEdit* hashEdit_;
    QPlainTextEdit* logEdit_;
    QPushButton* generateButton_;
    QPushButton* balanceButton_;
    QPushButton* sendButton_;
    QPushButton* receiptButton_;

    QString walletPath() const;
    Wallet loadWallet() const;
    void createInterface();
    void connectActions();
    void displayWallet(const Wallet& wallet);
    void generateWallet();
    void refreshBalance();
    void sendTransaction();
    void checkReceipt();
    void displayReceipt(const TransactionReceipt& receipt);
    void appendLog(const QString& text);
    void runAction(const std::function<void()>& action);
    void setBusy(bool busy);
};
