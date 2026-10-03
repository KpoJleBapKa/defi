pragma solidity ^0.8.34;

import { AggregatorV3Interface } from "./AggregatorV3Interface.sol";
import { StableCoin } from "./StableCoin.sol";

contract StableEngine {
    StableCoin public immutable stablecoin;
    AggregatorV3Interface public immutable priceFeed;

    uint256 public constant COLLATERAL_RATIO = 150;
    uint256 public constant MIN_HEALTH_FACTOR = 1e18;
    uint256 public constant LIQUIDATION_BONUS = 10;

    mapping(address => uint256) public collateralDeposited;
    mapping(address => uint256) public stablecoinMinted;

    constructor(address stablecoinAddress, address priceFeedAddress) {
        stablecoin = StableCoin(stablecoinAddress);
        priceFeed = AggregatorV3Interface(priceFeedAddress);
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

    function liquidate(address user) external {
        require(getHealthFactor(user) < MIN_HEALTH_FACTOR, "Health factor is safe");

        uint256 debt = stablecoinMinted[user];
        uint256 collateralReward = debt * 1e18 / getUsdValue(1e18);
        collateralReward += collateralReward * LIQUIDATION_BONUS / 100;
        require(debt > 0 && collateralReward <= collateralDeposited[user], "Cannot liquidate");

        stablecoinMinted[user] = 0;
        collateralDeposited[user] -= collateralReward;
        stablecoin.burn(msg.sender, debt);

        (bool success, ) = payable(msg.sender).call{ value: collateralReward }("");
        require(success, "Transfer failed");
    }

    function reduceCollateralForTesting(address user, uint256 amount) external {
        require(amount > 0 && amount < collateralDeposited[user], "Invalid amount");
        collateralDeposited[user] -= amount;
    }

    function getHealthFactor(address user) public view returns (uint256) {
        uint256 debt = stablecoinMinted[user];
        if (debt == 0) {
            return type(uint256).max;
        }

        uint256 adjustedCollateral = getUsdValue(collateralDeposited[user]) * 100 / COLLATERAL_RATIO;
        return adjustedCollateral * 1e18 / debt;
    }

    function getUsdValue(uint256 ethAmount) public view returns (uint256) {
        (, int256 price, , , ) = priceFeed.latestRoundData();
        require(price > 0, "Invalid oracle price");
        return ethAmount * uint256(price) * 1e10 / 1e18;
    }

    function _revertIfHealthFactorIsBroken(address user) internal view {
        require(getHealthFactor(user) >= MIN_HEALTH_FACTOR, "Health factor is below 1");
    }
}
