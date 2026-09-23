@echo off
powershell.exe -NoExit -ExecutionPolicy Bypass -Command "Set-Location -LiteralPath '%~dp0'; . '%~dp0use-node.ps1'"

