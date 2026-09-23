import { network } from "hardhat";

const connection = await network.create();
const { ethers } = connection;
const [owner, trader] = await ethers.getSigners();
const units = ethers.parseEther;

const tokenA = await ethers.deployContract("AssetToken", ["Solovian Coin", "SLC", 100000n]);
const tokenB = await ethers.deployContract("AssetToken", ["Mock Fiat", "MFT", 100000n]);
await tokenA.waitForDeployment();
await tokenB.waitForDeployment();

const pool = await ethers.deployContract("DexPool", [await tokenA.getAddress(), await tokenB.getAddress()]);
await pool.waitForDeployment();

const poolAddress = await pool.getAddress();
const liquidityA = units("1000");
const liquidityB = units("2000");
const tradeAmount = units("50");

await (await tokenA.approve(poolAddress, liquidityA)).wait();
await (await tokenB.approve(poolAddress, liquidityB)).wait();
await (await pool.addLiquidity(liquidityA, liquidityB)).wait();

const initialReserveA = await pool.reserveA();
const initialReserveB = await pool.reserveB();
const initialK = await pool.constantProduct();

await (await tokenA.transfer(trader.address, tradeAmount)).wait();
await (await tokenA.connect(trader).approve(poolAddress, tradeAmount)).wait();

const [quotedOutput, feeBps] = await pool.quoteAForB(tradeAmount);
await (await pool.connect(trader).swapAForB(tradeAmount)).wait();

const finalReserveA = await pool.reserveA();
const finalReserveB = await pool.reserveB();
const finalK = await pool.constantProduct();
const traderBalanceB = await tokenB.balanceOf(trader.address);

if (traderBalanceB !== quotedOutput || finalK <= initialK) {
    throw new Error("AMM simulation failed");
}

console.log("Solovian Coin:", await tokenA.getAddress());
console.log("Mock Fiat:", await tokenB.getAddress());
console.log("DexPool:", poolAddress);
console.log("");
console.log("Initial reserves:", ethers.formatEther(initialReserveA), "SLC /", ethers.formatEther(initialReserveB), "MFT");
console.log("Initial k:", initialK.toString());
console.log("Trade:", ethers.formatEther(tradeAmount), "SLC");
console.log("Dynamic fee:", `${Number(feeBps) / 100}%`);
console.log("Trader received:", ethers.formatEther(traderBalanceB), "MFT");
console.log("Final reserves:", ethers.formatEther(finalReserveA), "SLC /", ethers.formatEther(finalReserveB), "MFT");
console.log("Final k:", finalK.toString());
console.log("k increased:", finalK > initialK);

await connection.close();
