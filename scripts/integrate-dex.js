import { readFileSync } from "node:fs";
import { Contract, ContractFactory, ZeroAddress } from "ethers";
import { network } from "hardhat";

const connection = await network.create();
const { ethers } = connection;
const [owner, trader] = await ethers.getSigners();
const units = ethers.parseEther;

function loadJson(path) {
    return JSON.parse(readFileSync(path, "utf8"));
}

async function deployExternal(path, signer, argumentsList = []) {
    const artifact = loadJson(path);
    const factory = new ContractFactory(artifact.abi, artifact.bytecode, signer);
    const contract = await factory.deploy(...argumentsList);
    await contract.waitForDeployment();
    return contract;
}

const factoryArtifactPath = "node_modules/@uniswap/v2-core/build/UniswapV2Factory.json";
const pairArtifactPath = "node_modules/@uniswap/v2-core/build/UniswapV2Pair.json";
const wethArtifactPath = "node_modules/@uniswap/v2-periphery/build/WETH9.json";
const routerArtifactPath = "node_modules/@uniswap/v2-periphery/build/UniswapV2Router02.json";

const weth = await deployExternal(wethArtifactPath, owner);
const factory = await deployExternal(factoryArtifactPath, owner, [owner.address]);
const router = await deployExternal(routerArtifactPath, owner, [await factory.getAddress(), await weth.getAddress()]);

const tokenA = await ethers.deployContract("AssetToken", ["Solovian Coin", "SLC", 100000n]);
const tokenB = await ethers.deployContract("AssetToken", ["Mock Fiat", "MFT", 100000n]);
await tokenA.waitForDeployment();
await tokenB.waitForDeployment();

const integrator = await ethers.deployContract("DefiIntegrator", [await router.getAddress()]);
await integrator.waitForDeployment();

const tokenAAddress = await tokenA.getAddress();
const tokenBAddress = await tokenB.getAddress();
const integratorAddress = await integrator.getAddress();
const liquidityA = units("1000");
const liquidityB = units("2000");

await (await tokenA.approve(integratorAddress, liquidityA)).wait();
await (await tokenB.approve(integratorAddress, liquidityB)).wait();
await (await integrator.provideLiquidity(tokenAAddress, tokenBAddress, liquidityA, liquidityB, liquidityA, liquidityB)).wait();

const pairAddress = await factory.getPair(tokenAAddress, tokenBAddress);
if (pairAddress === ZeroAddress) {
    throw new Error("Uniswap V2 pair was not created");
}

const pairArtifact = loadJson(pairArtifactPath);
const pair = new Contract(pairAddress, pairArtifact.abi, owner);
const lpBalance = await pair.balanceOf(owner.address);

const tradeAmount = units("50");
await (await tokenA.transfer(trader.address, tradeAmount)).wait();
await (await tokenA.connect(trader).approve(integratorAddress, tradeAmount)).wait();

const path = [tokenAAddress, tokenBAddress];
const quotedAmounts = await router.getAmountsOut(tradeAmount, path);
const quotedOutput = quotedAmounts[quotedAmounts.length - 1];
const minimumOutput = quotedOutput * 95n / 100n;
const balanceBefore = await tokenB.balanceOf(trader.address);

await (await integrator.connect(trader).swapTokens(tokenAAddress, tokenBAddress, tradeAmount, minimumOutput)).wait();

const balanceAfter = await tokenB.balanceOf(trader.address);
const received = balanceAfter - balanceBefore;
const integratorBalanceA = await tokenA.balanceOf(integratorAddress);
const integratorBalanceB = await tokenB.balanceOf(integratorAddress);

if (received < minimumOutput || integratorBalanceA !== 0n || integratorBalanceB !== 0n) {
    throw new Error("DEX integration verification failed");
}

console.log("Uniswap V2 Factory:", await factory.getAddress());
console.log("Uniswap V2 Router02:", await router.getAddress());
console.log("DefiIntegrator:", integratorAddress);
console.log("SLC token:", tokenAAddress);
console.log("MFT token:", tokenBAddress);
console.log("SLC/MFT pair:", pairAddress);
console.log("");
console.log("Liquidity added:", ethers.formatEther(liquidityA), "SLC /", ethers.formatEther(liquidityB), "MFT");
console.log("LP tokens received:", ethers.formatEther(lpBalance));
console.log("Swap input:", ethers.formatEther(tradeAmount), "SLC");
console.log("Router quote:", ethers.formatEther(quotedOutput), "MFT");
console.log("amountOutMin:", ethers.formatEther(minimumOutput), "MFT");
console.log("Trader received:", ethers.formatEther(received), "MFT");
console.log("Integrator balances:", ethers.formatEther(integratorBalanceA), "SLC /", ethers.formatEther(integratorBalanceB), "MFT");
console.log("Integration successful:", true);

await connection.close();
