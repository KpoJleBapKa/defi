import { JsonRpcProvider } from "ethers";

const provider = new JsonRpcProvider("http://127.0.0.1:8545");
const deadline = Date.now() + 30000;

while (Date.now() < deadline) {
    try {
        await provider.getBlockNumber();
        await provider.destroy();
        process.exit(0);
    } catch {
        await new Promise((resolve) => setTimeout(resolve, 500));
    }
}

await provider.destroy();
throw new Error("Hardhat RPC did not start within 30 seconds");
