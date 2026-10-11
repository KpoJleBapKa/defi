import { network } from "hardhat";

const connection = await network.create();
const { ethers } = connection;
const [owner] = await ethers.getSigners();
const units = ethers.parseEther;
const format = ethers.formatEther;

const token = await ethers.deployContract("GovernanceToken", [units("1000000")]);
await token.waitForDeployment();
const tokenAddress = await token.getAddress();

const oracle = await ethers.deployContract("VulnerableOracle");
await oracle.waitForDeployment();
const pool = await ethers.deployContract("MockFlashLoanPool", [tokenAddress]);
await pool.waitForDeployment();
const attacker = await ethers.deployContract("FlashLoanAttacker", [await pool.getAddress(), tokenAddress, await oracle.getAddress()]);
await attacker.waitForDeployment();

await (await token.transfer(await pool.getAddress(), units("500000"))).wait();
await (await token.transfer(await attacker.getAddress(), units("100"))).wait();
const loanAmount = units("100000");
const poolBefore = await token.balanceOf(await pool.getAddress());
const priceBefore = await oracle.price();
await (await attacker.initiateAttack(loanAmount)).wait();
const poolAfter = await token.balanceOf(await pool.getAddress());
const priceAfter = await oracle.price();

const dao = await ethers.deployContract("SimpleDAO", [tokenAddress, 60]);
await dao.waitForDeployment();
const protocol = await ethers.deployContract("MockProtocol");
await protocol.waitForDeployment();
const daoAddress = await dao.getAddress();
const protocolAddress = await protocol.getAddress();
await (await protocol.transferOwnership(daoAddress)).wait();

const payload = protocol.interface.encodeFunctionData("setFee", [10]);
await (await dao.createProposal(protocolAddress, payload, "Set protocol fee to 10")).wait();
await (await dao.vote(0, true)).wait();
await ethers.provider.send("evm_increaseTime", [61]);
await ethers.provider.send("evm_mine", []);
await (await dao.executeProposal(0)).wait();

const proposal = await dao.proposals(0);
const finalFee = await protocol.fee();
const premium = poolAfter - poolBefore;

if (priceAfter !== 1n || premium !== units("90")) {
    throw new Error("Flash Loan simulation failed");
}
if (finalFee !== 10n || !proposal.executed || await protocol.owner() !== daoAddress) {
    throw new Error("DAO execution failed");
}

console.log("Governance token:", tokenAddress);
console.log("");
console.log("FLASH LOAN SIMULATION");
console.log("Pool:", await pool.getAddress());
console.log("Attacker:", await attacker.getAddress());
console.log("Borrowed:", format(loanAmount), "SLC");
console.log("Oracle price before:", priceBefore.toString());
console.log("Oracle price after attack:", priceAfter.toString());
console.log("Premium repaid:", format(premium), "SLC");
console.log("Loan repaid successfully:", poolAfter > poolBefore);
console.log("");
console.log("DAO GOVERNANCE");
console.log("SimpleDAO:", daoAddress);
console.log("MockProtocol:", protocolAddress);
console.log("MockProtocol owner is DAO:", await protocol.owner() === daoAddress);
console.log("Proposal payload:", payload);
console.log("Yes votes:", format(proposal.yesVotes), "SLC");
console.log("Proposal executed:", proposal.executed);
console.log("Protocol fee after execution:", finalFee.toString());
console.log("Laboratory successful:", finalFee === 10n);

await connection.close();
