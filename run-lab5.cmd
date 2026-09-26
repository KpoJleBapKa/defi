@echo off
setlocal
chcp 65001 >nul
title Solovian - DeFi Laboratory 5

set "NODE_DIRECTORY=F:\Dev\node-v24.21.0-win-x64"
set "PATH=%NODE_DIRECTORY%;%PATH%"
cd /d "%~dp0"

if not exist "%NODE_DIRECTORY%\node.exe" goto node_error

echo ========================================
echo   Solovian - DeFi Laboratory 5
echo ========================================
echo Node.js:
node.exe --version
echo npm:
call npm.cmd --version
echo.

if not exist "node_modules\" (
    echo Installing dependencies...
    call npm.cmd install
    if errorlevel 1 goto command_error
    echo.
)

echo [1/2] Building Solidity contracts...
call npm.cmd run build
if errorlevel 1 goto command_error
echo.

echo [2/2] Running lending simulation...
call npm.cmd run simulate
if errorlevel 1 goto command_error
echo.

echo ========================================
echo   LENDING PROTECTION PASSED
echo ========================================
goto finish

:node_error
echo Node.js was not found at:
echo %NODE_DIRECTORY%
goto failed

:command_error
echo.
echo Laboratory execution failed.

:failed
if /i "%~1"=="--no-pause" exit /b 1
pause
exit /b 1

:finish
if /i "%~1"=="--no-pause" exit /b 0
pause
