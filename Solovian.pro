QT += core gui network widgets

TEMPLATE = app
TARGET = Solovian
CONFIG += c++2a windows
DEFINES += SOLOVIAN_DATA_DIR=\\\"$$PWD\\\"
DESTDIR = $$OUT_PWD

INCLUDEPATH += \
    $$PWD/include \
    $$PWD/ThirdParty/secp256k1/include

HEADERS += \
    include/EthereumRPC.h \
    include/Hex.h \
    include/Keccak.h \
    include/MainWindow.h \
    include/Transaction.h \
    include/Wallet.h

SOURCES += \
    src/EthereumRPC.cpp \
    src/Hex.cpp \
    src/Keccak.cpp \
    src/main.cpp \
    src/MainWindow.cpp \
    src/Transaction.cpp \
    src/Wallet.cpp

CONFIG(debug, debug|release) {
    LIBS += $$PWD/ThirdParty/secp256k1/lib/Debug/libsecp256k1.lib
} else {
    LIBS += $$PWD/ThirdParty/secp256k1/lib/Release/libsecp256k1.lib
}

LIBS += bcrypt.lib

OPENSSL_DIR = $$PWD/ThirdParty/OpenSSL/bin
QMAKE_POST_LINK += $$QMAKE_COPY "$$shell_path($$OPENSSL_DIR/libcrypto-1_1-x64.dll)" "$$shell_path($$OUT_PWD)" $$escape_expand(\n\t)
QMAKE_POST_LINK += $$QMAKE_COPY "$$shell_path($$OPENSSL_DIR/libssl-1_1-x64.dll)" "$$shell_path($$OUT_PWD)" $$escape_expand(\n\t)
