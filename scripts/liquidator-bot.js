import { readFileSync } from "node:fs";
import { ContractFactory, JsonRpcProvider, Wallet, formatEther, parseEther } from "ethers";

const rpcUrl = process.env.SEPOLIA_RPC_URL;
const privateKey = process.env.PRIVATE_KEY;
if (!rpcUrl || !privateKey) {
    throw new Error("SEPOLIA_RPC_URL and PRIVATE_KEY are required");
}

const provider = new JsonRpcProvider(rpcUrl);
const liquidator = new Wallet(privateKey, provider);
const borrower = Wallet.createRandom().connect(provider);
const priceFeed = "0x694AA1769357215DE4FAC081bf1f309aDC325306";

function loadArtifact(name) {
    return JSON.parse(readFileSync(`artifacts/contracts/${name}.sol/${name}.json`, "utf8"));
}

async function deploy(name, argumentsList = []) {
    const artifact = loadArtifact(name);
    const factory = new ContractFactory(artifact.abi, artifact.bytecode, liquidator);
    const contract = await factory.deploy(...argumentsList);
    await contract.waitForDeployment();
    return contract;
}

console.log(`Network: Sepolia (${(await provider.getNetwork()).chainId})`);
console.log(`Liquidator: ${liquidator.address}`);
console.log(`Borrower: ${borrower.address}`);

const stablecoin = await deploy("StableCoin");
const engine = await deploy("StableEngine", [await stablecoin.getAddress(), priceFeed]);
await (await stablecoin.transferOwnership(await engine.getAddress())).wait();

console.log(`SolovianUSD: ${await stablecoin.getAddress()}`);
console.log(`StableEngine: ${await engine.getAddress()}`);
console.log(`Chainlink ETH/USD: ${priceFeed}`);

await (await liquidator.sendTransaction({ to: borrower.address, value: parseEther("0.003") })).wait();
const collateral = parseEther("0.001");
await (await engine.connect(borrower).depositCollateral({ value: collateral })).wait();

const collateralValue = await engine.getUsdValue(collateral);
const maximumDebt = collateralValue * 100n / await engine.COLLATERAL_RATIO();
const debt = maximumDebt * 10n / 11n;
await (await engine.connect(borrower).mintStablecoin(debt)).wait();
await (await stablecoin.connect(borrower).transfer(liquidator.address, debt)).wait();

const healthyFactor = await engine.getHealthFactor(borrower.address);
console.log(`ETH price: $${formatEther(await engine.getUsdValue(parseEther("1")))}`);
console.log(`Collateral: ${formatEther(collateral)} ETH`);
console.log(`Debt: ${formatEther(debt)} SUSD`);
console.log(`Initial health factor: ${formatEther(healthyFactor)}`);

const targetCollateral = collateral * parseEther("0.95") / healthyFactor;
const reduction = collateral - targetCollateral;
console.log("Bot started. Monitoring borrower...");
console.log(`HF check: ${formatEther(await engine.getHealthFactor(borrower.address))}`);

const reductionTx = await engine.reduceCollateralForTesting(borrower.address, reduction);
await reductionTx.wait();
console.log(`Laboratory collateral reduction: ${reductionTx.hash}`);

await new Promise(resolve => setTimeout(resolve, 3000));
const unhealthyFactor = await engine.getHealthFactor(borrower.address);
console.log(`HF check: ${formatEther(unhealthyFactor)}`);

if (unhealthyFactor >= parseEther("1")) {
    throw new Error("Borrower position is still healthy");
}

console.log("Unsafe position detected. Sending liquidation transaction...");
const liquidationTx = await engine.liquidate(borrower.address);
console.log(`Liquidation TX: ${liquidationTx.hash}`);
console.log(`Explorer: https://sepolia.etherscan.io/tx/${liquidationTx.hash}`);
const receipt = await liquidationTx.wait();

console.log(`Status: ${receipt.status === 1 ? "SUCCESS" : "FAILED"}`);
console.log(`Block: ${receipt.blockNumber}`);
console.log(`Borrower debt after liquidation: ${formatEther(await engine.stablecoinMinted(borrower.address))} SUSD`);
console.log(`Liquidation successful: ${receipt.status === 1}`);
