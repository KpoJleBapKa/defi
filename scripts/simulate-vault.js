import { readFileSync } from "node:fs";
import { ContractFactory } from "ethers";
import { network } from "hardhat";

const connection = await network.create();
const { ethers } = connection;
const [owner, investor, keeper] = await ethers.getSigners();
const units = ethers.parseEther;
const format = ethers.formatEther;

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

async function runKeeperOnce(vault, reward, router, threshold) {
    const vaultAddress = await vault.getAddress();
    const rewardBalance = await reward.balanceOf(vaultAddress);
    if (rewardBalance < threshold) {
        return false;
    }

    const path = [await reward.getAddress(), await vault.underlyingAsset()];
    const quote = await router.getAmountsOut(rewardBalance, path);
    const amountOutMin = quote[quote.length - 1] * 95n / 100n;
    await (await vault.connect(keeper).compound(amountOutMin)).wait();
    return true;
}

const weth = await deployExternal("node_modules/@uniswap/v2-periphery/build/WETH9.json", owner);
const factory = await deployExternal("node_modules/@uniswap/v2-core/build/UniswapV2Factory.json", owner, [owner.address]);
const router = await deployExternal("node_modules/@uniswap/v2-periphery/build/UniswapV2Router02.json", owner, [await factory.getAddress(), await weth.getAddress()]);

const asset = await ethers.deployContract("AssetToken", ["Solovian Coin", "SLC", 100000n]);
const reward = await ethers.deployContract("AssetToken", ["Mock Fiat", "MFT", 100000n]);
await asset.waitForDeployment();
await reward.waitForDeployment();

const assetAddress = await asset.getAddress();
const rewardAddress = await reward.getAddress();
const routerAddress = await router.getAddress();
const vault = await ethers.deployContract("AutoCompoundVault", [assetAddress, rewardAddress, routerAddress]);
await vault.waitForDeployment();
const vaultAddress = await vault.getAddress();

const poolAsset = units("10000");
const poolReward = units("10000");
await (await asset.approve(routerAddress, poolAsset)).wait();
await (await reward.approve(routerAddress, poolReward)).wait();
await (await router.addLiquidity(assetAddress, rewardAddress, poolAsset, poolReward, poolAsset, poolReward, owner.address, Math.floor(Date.now() / 1000) + 300)).wait();

const depositAmount = units("1000");
await (await asset.transfer(investor.address, depositAmount)).wait();
await (await asset.connect(investor).approve(vaultAddress, depositAmount)).wait();
await (await vault.connect(investor).deposit(depositAmount)).wait();

const shares = await vault.balanceOf(investor.address);
const assetsBefore = await vault.convertToAssets(shares);
const rewards = units("100");
await (await reward.transfer(vaultAddress, rewards)).wait();
const compounded = await runKeeperOnce(vault, reward, router, units("1"));
const assetsAfter = await vault.convertToAssets(shares);

if (!compounded || assetsAfter <= assetsBefore) {
    throw new Error("Vault share value did not increase");
}

const investorBalanceBefore = await asset.balanceOf(investor.address);
await (await vault.connect(investor).withdraw(shares)).wait();
const withdrawn = await asset.balanceOf(investor.address) - investorBalanceBefore;

console.log("Vault:", vaultAddress);
console.log("Underlying asset:", assetAddress);
console.log("Reward token:", rewardAddress);
console.log("Uniswap V2 Router:", routerAddress);
console.log("");
console.log("Investor deposited:", format(depositAmount), "SLC");
console.log("Shares received:", format(shares), "vSLC");
console.log("Share value before compound:", format(assetsBefore), "SLC");
console.log("Rewards sent to vault:", format(rewards), "MFT");
console.log("Keeper called compound:", compounded);
console.log("Share value after compound:", format(assetsAfter), "SLC");
console.log("Investor withdrew:", format(withdrawn), "SLC");
console.log("Profit:", format(withdrawn - depositAmount), "SLC");
console.log("Auto-compounding successful:", withdrawn > depositAmount);

await connection.close();
