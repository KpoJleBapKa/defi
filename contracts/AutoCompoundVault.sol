pragma solidity ^0.8.34;

import {ERC20} from "@openzeppelin/contracts/token/ERC20/ERC20.sol";
import {IERC20} from "@openzeppelin/contracts/token/ERC20/IERC20.sol";
import {SafeERC20} from "@openzeppelin/contracts/token/ERC20/utils/SafeERC20.sol";
import {ReentrancyGuard} from "@openzeppelin/contracts/utils/ReentrancyGuard.sol";
import {IUniswapV2Router02} from "@uniswap/v2-periphery/contracts/interfaces/IUniswapV2Router02.sol";

contract AutoCompoundVault is ERC20, ReentrancyGuard {
    using SafeERC20 for IERC20;

    IERC20 public immutable underlyingAsset;
    IERC20 public immutable rewardToken;
    IUniswapV2Router02 public immutable router;

    event Deposited(address indexed user, uint256 assets, uint256 shares);
    event Withdrawn(address indexed user, uint256 assets, uint256 shares);
    event Compounded(uint256 rewards, uint256 assetsReinvested);

    constructor(address assetAddress, address rewardAddress, address routerAddress) ERC20("Vault Solovian Coin", "vSLC") {
        require(assetAddress != address(0) && rewardAddress != address(0) && routerAddress != address(0), "Zero address");
        require(assetAddress != rewardAddress, "Identical tokens");
        underlyingAsset = IERC20(assetAddress);
        rewardToken = IERC20(rewardAddress);
        router = IUniswapV2Router02(routerAddress);
    }

    function totalAssets() public view returns (uint256) {
        return underlyingAsset.balanceOf(address(this));
    }

    function convertToShares(uint256 assets) public view returns (uint256) {
        uint256 supply = totalSupply();
        return supply == 0 ? assets : assets * supply / totalAssets();
    }

    function convertToAssets(uint256 shares) public view returns (uint256) {
        uint256 supply = totalSupply();
        return supply == 0 ? shares : shares * totalAssets() / supply;
    }

    function deposit(uint256 assets) external nonReentrant returns (uint256 shares) {
        require(assets > 0, "Zero assets");
        shares = convertToShares(assets);
        require(shares > 0, "Zero shares");
        _mint(msg.sender, shares);
        underlyingAsset.safeTransferFrom(msg.sender, address(this), assets);
        emit Deposited(msg.sender, assets, shares);
    }

    function withdraw(uint256 shares) external nonReentrant returns (uint256 assets) {
        require(shares > 0, "Zero shares");
        assets = convertToAssets(shares);
        require(assets > 0, "Zero assets");
        _burn(msg.sender, shares);
        underlyingAsset.safeTransfer(msg.sender, assets);
        emit Withdrawn(msg.sender, assets, shares);
    }

    function compound(uint256 amountOutMin) external nonReentrant returns (uint256 assetsReinvested) {
        uint256 rewards = rewardToken.balanceOf(address(this));
        require(rewards > 0, "No rewards");
        uint256 assetsBefore = totalAssets();

        rewardToken.forceApprove(address(router), rewards);
        address[] memory path = new address[](2);
        path[0] = address(rewardToken);
        path[1] = address(underlyingAsset);
        router.swapExactTokensForTokens(rewards, amountOutMin, path, address(this), block.timestamp + 300);
        rewardToken.forceApprove(address(router), 0);

        assetsReinvested = totalAssets() - assetsBefore;
        emit Compounded(rewards, assetsReinvested);
    }
}
