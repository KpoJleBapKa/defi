import { network } from "hardhat";

const connection = await network.create();
const { ethers } = connection;
const [owner, user] = await ethers.getSigners();
const units = ethers.parseEther;
const format = ethers.formatEther;

const stablecoin = await ethers.deployContract("StableCoin");
await stablecoin.waitForDeployment();

const engine = await ethers.deployContract("StableEngine", [await stablecoin.getAddress()]);
await engine.waitForDeployment();
await (await stablecoin.transferOwnership(await engine.getAddress())).wait();

const collateral = units("2");
await (await engine.connect(user).depositCollateral({ value: collateral })).wait();

const collateralValue = await engine.getUsdValue(collateral);
const maximumDebt = collateralValue * 100n / await engine.COLLATERAL_RATIO();
await (await engine.connect(user).mintStablecoin(maximumDebt)).wait();
const healthFactorAtLimit = await engine.getHealthFactor(user.address);

let protectionWorked = false;
try {
    await (await engine.connect(user).withdrawCollateral(units("1"))).wait();
} catch {
    protectionWorked = true;
}

if (!protectionWorked) {
    throw new Error("Unsafe withdrawal was not rejected");
}

const collateralAfterRejection = await engine.collateralDeposited(user.address);
await (await engine.connect(user).burnStablecoin(maximumDebt / 2n)).wait();
await (await engine.connect(user).withdrawCollateral(units("1"))).wait();

console.log(`Stablecoin: ${await stablecoin.name()} (${await stablecoin.symbol()})`);
console.log(`Stablecoin contract: ${await stablecoin.getAddress()}`);
console.log(`StableEngine contract: ${await engine.getAddress()}`);
console.log(`Stablecoin owner is Engine: ${await stablecoin.owner() === await engine.getAddress()}`);
console.log(`ETH price: $${format(await engine.mockEthUsdPrice())}`);
console.log(`Collateral deposited: ${format(collateral)} ETH`);
console.log(`Collateral value: $${format(collateralValue)}`);
console.log(`Maximum debt minted: ${format(maximumDebt)} SUSD`);
console.log(`Health factor at the limit: ${format(healthFactorAtLimit)}`);
console.log("Attempted withdrawal: 1.0 ETH");
console.log(`Protection worked: ${protectionWorked}`);
console.log(`Collateral after rejection: ${format(collateralAfterRejection)} ETH`);
console.log(`Debt burned: ${format(maximumDebt / 2n)} SUSD`);
console.log(`Collateral withdrawn after burn: ${format(await engine.collateralDeposited(user.address))} ETH remains`);
