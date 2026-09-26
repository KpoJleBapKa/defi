import { mkdirSync, readFileSync, rmSync, writeFileSync } from "node:fs";
import { ContractFactory, JsonRpcProvider, formatEther, parseEther } from "ethers";

const rpcUrl = "http://127.0.0.1:8545";
const provider = new JsonRpcProvider(rpcUrl);
const accounts = await provider.listAccounts();

if (accounts.length < 2) {
    throw new Error("Hardhat node must provide at least two accounts");
}

const owner = await provider.getSigner(0);
const trader = await provider.getSigner(1);
const ownerAddress = await owner.getAddress();
const traderAddress = await trader.getAddress();

function loadArtifact(name) {
    return JSON.parse(readFileSync(`artifacts/contracts/${name}.sol/${name}.json`, "utf8"));
}

async function deploy(name, argumentsList, signer) {
    const artifact = loadArtifact(name);
    const factory = new ContractFactory(artifact.abi, artifact.bytecode, signer);
    const contract = await factory.deploy(...argumentsList);
    await contract.waitForDeployment();
    return contract;
}

const tokenA = await deploy("AssetToken", ["Solovian Coin", "SLC", 100000n], owner);
const tokenB = await deploy("AssetToken", ["Mock Fiat", "MFT", 100000n], owner);
const pool = await deploy("DexPool", [await tokenA.getAddress(), await tokenB.getAddress()], owner);
const deploymentReceipt = await pool.deploymentTransaction().wait();

const poolAddress = await pool.getAddress();
const liquidityA = parseEther("1000");
const liquidityB = parseEther("2000");
const trades = [parseEther("10"), parseEther("20"), parseEther("60")];
const totalTrade = trades.reduce((sum, value) => sum + value, 0n);

await (await tokenA.approve(poolAddress, liquidityA)).wait();
await (await tokenB.approve(poolAddress, liquidityB)).wait();
await (await pool.addLiquidity(liquidityA, liquidityB)).wait();
await (await tokenA.transfer(traderAddress, totalTrade)).wait();
await (await tokenA.connect(trader).approve(poolAddress, totalTrade)).wait();

for (const amount of trades) {
    const [, feeBps] = await pool.quoteAForB(amount);
    const transaction = await pool.connect(trader).swapAForB(amount);
    const receipt = await transaction.wait();
    console.log(`Swap ${formatEther(amount)} SLC, fee ${Number(feeBps) / 100}%, tx ${receipt.hash}`);
}

mkdirSync(".lab3", { recursive: true });
for (const file of [".lab3/swaps.sqlite", ".lab3/swaps.sqlite-shm", ".lab3/swaps.sqlite-wal"]) {
    rmSync(file, { force: true });
}

const network = await provider.getNetwork();
writeFileSync(".lab3/deployment.json", JSON.stringify({
    rpcUrl,
    chainId: Number(network.chainId),
    poolAddress,
    deploymentBlock: deploymentReceipt.blockNumber,
    ownerAddress,
    traderAddress
}, null, 2));

console.log("");
console.log("DexPool:", poolAddress);
console.log("Indexed trader:", traderAddress);
console.log("Deployment saved to .lab3/deployment.json");

await provider.destroy();
