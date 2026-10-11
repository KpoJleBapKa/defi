pragma solidity ^0.8.34;

contract GovernanceToken {
    string public constant name = "Solovian Coin";
    string public constant symbol = "SLC";
    uint8 public constant decimals = 18;
    uint256 public totalSupply;
    mapping(address => uint256) public balanceOf;
    mapping(address => mapping(address => uint256)) public allowance;

    constructor(uint256 supply) {
        totalSupply = supply;
        balanceOf[msg.sender] = supply;
    }

    function transfer(address recipient, uint256 amount) external returns (bool) {
        move(msg.sender, recipient, amount);
        return true;
    }

    function approve(address spender, uint256 amount) external returns (bool) {
        allowance[msg.sender][spender] = amount;
        return true;
    }

    function transferFrom(address sender, address recipient, uint256 amount) external returns (bool) {
        uint256 approved = allowance[sender][msg.sender];
        require(approved >= amount, "Insufficient allowance");
        allowance[sender][msg.sender] = approved - amount;
        move(sender, recipient, amount);
        return true;
    }

    function move(address sender, address recipient, uint256 amount) private {
        require(balanceOf[sender] >= amount, "Insufficient balance");
        balanceOf[sender] -= amount;
        balanceOf[recipient] += amount;
    }
}
