pragma solidity ^0.8.34;

import { Ownable } from "@openzeppelin/contracts/access/Ownable.sol";
import { StableCoin } from "./StableCoin.sol";

contract StableEngine is Ownable {
    StableCoin public immutable stablecoin;

    uint256 public constant COLLATERAL_RATIO = 150;
    uint256 public constant MIN_HEALTH_FACTOR = 1e18;
    uint256 public mockEthUsdPrice = 2000e18;

    mapping(address => uint256) public collateralDeposited;
    mapping(address => uint256) public stablecoinMinted;

    constructor(address stablecoinAddress) Ownable(msg.sender) {
        stablecoin = StableCoin(stablecoinAddress);
    }

    function depositCollateral() external payable {
        require(msg.value > 0, "Amount must be positive");
        collateralDeposited[msg.sender] += msg.value;
    }

    function mintStablecoin(uint256 amount) external {
        require(amount > 0, "Amount must be positive");
        stablecoinMinted[msg.sender] += amount;
        _revertIfHealthFactorIsBroken(msg.sender);
        stablecoin.mint(msg.sender, amount);
    }

    function burnStablecoin(uint256 amount) external {
        require(amount > 0 && amount <= stablecoinMinted[msg.sender], "Invalid amount");
        stablecoinMinted[msg.sender] -= amount;
        stablecoin.burn(msg.sender, amount);
    }

    function withdrawCollateral(uint256 amount) external {
        require(amount > 0 && amount <= collateralDeposited[msg.sender], "Invalid amount");
        collateralDeposited[msg.sender] -= amount;
        _revertIfHealthFactorIsBroken(msg.sender);
        (bool success, ) = payable(msg.sender).call{ value: amount }("");
        require(success, "Transfer failed");
    }

    function setMockEthUsdPrice(uint256 newPrice) external onlyOwner {
        require(newPrice > 0, "Price must be positive");
        mockEthUsdPrice = newPrice;
    }

    function getUsdValue(uint256 ethAmount) public view returns (uint256) {
        return ethAmount * mockEthUsdPrice / 1e18;
    }

    function getHealthFactor(address user) public view returns (uint256) {
        uint256 debt = stablecoinMinted[user];
        if (debt == 0) {
            return type(uint256).max;
        }

        uint256 adjustedCollateral = getUsdValue(collateralDeposited[user]) * 100 / COLLATERAL_RATIO;
        return adjustedCollateral * 1e18 / debt;
    }

    function _revertIfHealthFactorIsBroken(address user) internal view {
        require(getHealthFactor(user) >= MIN_HEALTH_FACTOR, "Health factor is below 1");
    }
}
