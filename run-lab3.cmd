@echo off
setlocal
chcp 65001 >nul
title Solovian - DeFi Laboratory 3

set "NODE_DIRECTORY=F:\Dev\node-v24.21.0-win-x64"
set "PATH=%NODE_DIRECTORY%;%PATH%"
cd /d "%~dp0"

if not exist "%NODE_DIRECTORY%\node.exe" goto node_error

node.exe scripts\check-ports.js
if errorlevel 1 goto command_error

echo ========================================
echo   Solovian - DeFi Laboratory 3
echo ========================================
echo.
echo [1/5] Installing dependencies...
call npm.cmd install
if errorlevel 1 goto command_error

echo.
echo [2/5] Building Solidity contracts...
call npm.cmd run build
if errorlevel 1 goto command_error

echo.
echo [3/5] Starting local blockchain...
start "Lab 3 - Hardhat Blockchain" "%ComSpec%" /k call "%~dp0lab3-service.cmd" chain
node.exe scripts\wait-for-rpc.js
if errorlevel 1 goto command_error

echo.
echo [4/5] Deploying contracts and creating Swap events...
call npm.cmd run deploy:local
if errorlevel 1 goto command_error

echo.
echo [5/5] Starting backend and React frontend...
start "Lab 3 - Indexer API" "%ComSpec%" /k call "%~dp0lab3-service.cmd" backend
start "Lab 3 - React UI" "%ComSpec%" /k call "%~dp0lab3-service.cmd" frontend

echo.
echo ========================================
echo   LABORATORY 3 STARTED
echo ========================================
echo Frontend: http://127.0.0.1:5173
echo REST API: http://127.0.0.1:3000/api/health
echo.
echo Import the second Hardhat account into MetaMask.
echo Address:     0x70997970C51812dc3A010C7d01b50e0d17dc79C8
echo Private key: 0x59c6995e998f97a5a0044966f0945389dc9e86dae88c7a8412f4603b6b78690d
echo This key is public and valid only for the local development network.
echo.
pause
exit /b 0

:node_error
echo Node.js was not found at:
echo %NODE_DIRECTORY%
goto failed

:command_error
echo.
echo Laboratory execution failed.

:failed
pause
exit /b 1
