pragma solidity ^0.8.34;

import {IERC20} from "@openzeppelin/contracts/token/ERC20/IERC20.sol";
import {SafeERC20} from "@openzeppelin/contracts/token/ERC20/utils/SafeERC20.sol";
import {ReentrancyGuard} from "@openzeppelin/contracts/utils/ReentrancyGuard.sol";

contract DexPool is ReentrancyGuard {
    using SafeERC20 for IERC20;

    uint256 public constant BASIS_POINTS = 10000;
    uint256 public constant LARGE_TRADE_THRESHOLD_BPS = 500;
    uint256 public constant SMALL_TRADE_FEE_BPS = 10;
    uint256 public constant LARGE_TRADE_FEE_BPS = 100;

    IERC20 public immutable tokenA;
    IERC20 public immutable tokenB;

    uint256 public reserveA;
    uint256 public reserveB;

    event LiquidityAdded(address indexed provider, uint256 amountA, uint256 amountB);
    event Swap(address indexed trader, uint256 amountIn, uint256 amountOut, uint256 feeBps);

    constructor(address tokenAAddress, address tokenBAddress) {
        require(tokenAAddress != address(0) && tokenBAddress != address(0), "Zero token address");
        require(tokenAAddress != tokenBAddress, "Identical token addresses");
        tokenA = IERC20(tokenAAddress);
        tokenB = IERC20(tokenBAddress);
    }

    function addLiquidity(uint256 amountA, uint256 amountB) external nonReentrant {
        require(amountA > 0 && amountB > 0, "Invalid liquidity amount");
        require(reserveA == 0 && reserveB == 0, "Liquidity already added");

        tokenA.safeTransferFrom(msg.sender, address(this), amountA);
        tokenB.safeTransferFrom(msg.sender, address(this), amountB);

        reserveA = amountA;
        reserveB = amountB;

        emit LiquidityAdded(msg.sender, amountA, amountB);
    }

    function getFeeBps(uint256 amountIn) public view returns (uint256) {
        require(reserveA > 0, "Pool is empty");
        return amountIn * BASIS_POINTS >= reserveA * LARGE_TRADE_THRESHOLD_BPS ? LARGE_TRADE_FEE_BPS : SMALL_TRADE_FEE_BPS;
    }

    function quoteAForB(uint256 amountIn) public view returns (uint256 amountOut, uint256 feeBps) {
        require(amountIn > 0, "Amount must be greater than zero");
        require(reserveA > 0 && reserveB > 0, "Pool is empty");

        feeBps = getFeeBps(amountIn);
        uint256 amountInWithFee = amountIn * (BASIS_POINTS - feeBps);
        amountOut = amountInWithFee * reserveB / (reserveA * BASIS_POINTS + amountInWithFee);
    }

    function swapAForB(uint256 amountIn) external nonReentrant returns (uint256 amountOut) {
        uint256 feeBps;
        (amountOut, feeBps) = quoteAForB(amountIn);
        require(amountOut > 0 && amountOut < reserveB, "Insufficient output");

        tokenA.safeTransferFrom(msg.sender, address(this), amountIn);

        reserveA += amountIn;
        reserveB -= amountOut;

        tokenB.safeTransfer(msg.sender, amountOut);

        emit Swap(msg.sender, amountIn, amountOut, feeBps);
    }

    function constantProduct() external view returns (uint256) {
        return reserveA * reserveB;
    }
}
