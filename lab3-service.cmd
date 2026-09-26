@echo off
setlocal
set "NODE_DIRECTORY=F:\Dev\node-v24.21.0-win-x64"
set "PATH=%NODE_DIRECTORY%;%PATH%"
cd /d "%~dp0"
call npm.cmd run %~1
