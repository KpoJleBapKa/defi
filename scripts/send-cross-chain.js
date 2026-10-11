import { readFile } from "node:fs/promises";
import { ethers } from "ethers";

const SEPOLIA_CHAIN_ID = 11155111n;
const ARBITRUM_SEPOLIA_CHAIN_ID = 421614n;
const ARBITRUM_SEPOLIA_SELECTOR = 3478487238524512106n;
const SEPOLIA_ROUTER = "0x0BF3dE8c5D3e8A2B34D2BEeB17ABfCeBaf363A59";
const ARBITRUM_SEPOLIA_ROUTER = "0x2a9C5afB0d0e4BAb2BCdaE109EC4b0c4Be15a165";

async function loadArtifact(name) {
    const path = `artifacts/contracts/${name}.sol/${name}.json`;
    return JSON.parse(await readFile(path, "utf8"));
}

async function deploy(name, wallet, argument) {
    const artifact = await loadArtifact(name);
    const factory = new ethers.ContractFactory(artifact.abi, artifact.bytecode, wallet);
    const contract = await factory.deploy(argument);
    await contract.deploymentTransaction().wait(2);
    await contract.waitForDeployment();
    return contract;
}

async function retry(action) {
    let lastError;
    for (let attempt = 1; attempt <= 5; ++attempt) {
        try {
            return await action();
        } catch (error) {
            lastError = error;
            await new Promise(resolve => setTimeout(resolve, 3000));
        }
    }
    throw lastError;
}

async function requireChain(provider, expectedId, name) {
    const network = await provider.getNetwork();
    if (network.chainId !== expectedId) {
        throw new Error(`${name} RPC returned chain ID ${network.chainId}`);
    }
}

async function main() {
    const privateKey = process.env.PRIVATE_KEY;
    if (!privateKey || privateKey === "YOUR_PRIVATE_KEY") {
        throw new Error("Set PRIVATE_KEY in .env");
    }

    const sepoliaProvider = new ethers.JsonRpcProvider(process.env.SEPOLIA_RPC_URL);
    const arbitrumProvider = new ethers.JsonRpcProvider(process.env.ARBITRUM_SEPOLIA_RPC_URL);
    await requireChain(sepoliaProvider, SEPOLIA_CHAIN_ID, "Sepolia");
    await requireChain(arbitrumProvider, ARBITRUM_SEPOLIA_CHAIN_ID, "Arbitrum Sepolia");

    const sepoliaWallet = new ethers.Wallet(privateKey, sepoliaProvider);
    const arbitrumWallet = new ethers.Wallet(privateKey, arbitrumProvider);
    const arbitrumBalance = await arbitrumProvider.getBalance(arbitrumWallet.address);
    if (arbitrumBalance === 0n) {
        throw new Error(`No Arbitrum Sepolia ETH at ${arbitrumWallet.address}`);
    }

    console.log(`Wallet: ${sepoliaWallet.address}`);
    const receiver = await deploy("MessageReceiver", arbitrumWallet, ARBITRUM_SEPOLIA_ROUTER);
    console.log(`Receiver on Arbitrum Sepolia: ${await receiver.getAddress()}`);

    const sender = await deploy("CrossChainMessenger", sepoliaWallet, SEPOLIA_ROUTER);
    console.log(`Sender on Ethereum Sepolia: ${await sender.getAddress()}`);

    const text = process.env.MESSAGE || "Hello from Ethereum Sepolia";
    const receiverAddress = await receiver.getAddress();
    const fee = await retry(() => sender.getFee(ARBITRUM_SEPOLIA_SELECTOR, receiverAddress, text));
    const fundingAmount = fee * 110n / 100n;
    const funding = await sepoliaWallet.sendTransaction({to: await sender.getAddress(), value: fundingAmount});
    await funding.wait();
    console.log(`CCIP fee: ${ethers.formatEther(fee)} ETH`);

    const transaction = await sender.sendMessage(ARBITRUM_SEPOLIA_SELECTOR, receiverAddress, text);
    const receipt = await transaction.wait();
    const event = receipt.logs.map(log => {
        try {
            return sender.interface.parseLog(log);
        } catch {
            return null;
        }
    }).find(log => log?.name === "MessageSent");
    const messageId = event.args.messageId;

    console.log(`Source transaction: https://sepolia.etherscan.io/tx/${receipt.hash}`);
    console.log(`CCIP message ID: ${messageId}`);
    console.log(`CCIP Explorer: https://ccip.chain.link/msg/${messageId}`);
    console.log("Cross-chain message submitted successfully.");
}

main().catch(error => {
    console.error(`[ERROR] ${error.message}`);
    process.exitCode = 1;
});
