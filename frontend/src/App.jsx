import React, { useEffect, useState } from "react";

const apiUrl = "http://127.0.0.1:3000";
const localChainId = "0x7A69";

async function selectLocalNetwork() {
    try {
        await window.ethereum.request({
            method: "wallet_switchEthereumChain",
            params: [{ chainId: localChainId }]
        });
    } catch (error) {
        if (error.code !== 4902) {
            throw error;
        }

        await window.ethereum.request({
            method: "wallet_addEthereumChain",
            params: [{
                chainId: localChainId,
                chainName: "Hardhat Local",
                nativeCurrency: { name: "ETH", symbol: "ETH", decimals: 18 },
                rpcUrls: ["http://127.0.0.1:8545"]
            }]
        });
    }
}

export default function App() {
    const [account, setAccount] = useState("");
    const [history, setHistory] = useState([]);
    const [status, setStatus] = useState("Підключіть MetaMask для перегляду історії");

    async function loadHistory(address) {
        setStatus("Завантаження історії...");
        try {
            const response = await fetch(`${apiUrl}/api/swaps?trader=${address}`);
            const data = await response.json();
            if (!response.ok) {
                throw new Error(data.error ?? "REST API error");
            }
            setHistory(data);
            setStatus(data.length > 0 ? `Знайдено обмінів: ${data.length}` : "Для цієї адреси обмінів немає");
        } catch (error) {
            setStatus(`Помилка: ${error.message}`);
        }
    }

    async function connectWallet() {
        if (!window.ethereum) {
            setStatus("Встановіть розширення MetaMask");
            return;
        }

        try {
            await selectLocalNetwork();
            const accounts = await window.ethereum.request({ method: "eth_requestAccounts" });
            const address = accounts[0];
            setAccount(address);
            await loadHistory(address);
        } catch (error) {
            setStatus(`Помилка MetaMask: ${error.message}`);
        }
    }

    useEffect(() => {
        if (!window.ethereum) {
            return undefined;
        }

        const handleAccountsChanged = (accounts) => {
            const address = accounts[0] ?? "";
            setAccount(address);
            setHistory([]);
            if (address) {
                loadHistory(address);
            }
        };

        window.ethereum.on("accountsChanged", handleAccountsChanged);
        return () => window.ethereum.removeListener("accountsChanged", handleAccountsChanged);
    }, []);

    return (
        <main>
            <section className="panel">
                <div className="heading">
                    <div>
                        <p className="eyebrow">Лабораторна робота № 3</p>
                        <h1>Історія обмінів DexPool</h1>
                    </div>
                    <button onClick={connectWallet}>{account ? "Змінити гаманець" : "Підключити MetaMask"}</button>
                </div>

                <div className="wallet">
                    <span>Гаманець</span>
                    <strong>{account || "Не підключено"}</strong>
                </div>
                <p className="status">{status}</p>

                <div className="tableWrapper">
                    <table>
                        <thead>
                            <tr>
                                <th>Блок</th>
                                <th>Хеш транзакції</th>
                                <th>Віддано, SLC</th>
                                <th>Отримано, MFT</th>
                                <th>Комісія</th>
                            </tr>
                        </thead>
                        <tbody>
                            {history.length === 0 ? (
                                <tr>
                                    <td colSpan="5" className="empty">Історія порожня</td>
                                </tr>
                            ) : history.map((swap) => (
                                <tr key={swap.transactionHash}>
                                    <td>{swap.blockNumber}</td>
                                    <td className="hash" title={swap.transactionHash}>{swap.transactionHash}</td>
                                    <td>{swap.amountIn}</td>
                                    <td>{swap.amountOut}</td>
                                    <td>{swap.feePercent}%</td>
                                </tr>
                            ))}
                        </tbody>
                    </table>
                </div>
            </section>
        </main>
    );
}
