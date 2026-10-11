pragma solidity ^0.8.34;

contract MockProtocol {
    address public owner;
    uint256 public fee;

    constructor() {
        owner = msg.sender;
    }

    modifier onlyOwner() {
        require(msg.sender == owner, "Only owner");
        _;
    }

    function transferOwnership(address newOwner) external onlyOwner {
        require(newOwner != address(0), "Invalid owner");
        owner = newOwner;
    }

    function setFee(uint256 newFee) external onlyOwner {
        fee = newFee;
    }
}
