@echo off
setlocal
chcp 65001 >nul
echo Stopping Laboratory 3 services...
powershell.exe -NoProfile -ExecutionPolicy Bypass -Command "$ports = 8545, 3000, 5173; $ids = Get-NetTCPConnection -State Listen -ErrorAction SilentlyContinue | Where-Object { $_.LocalPort -in $ports } | Select-Object -ExpandProperty OwningProcess -Unique; foreach ($id in $ids) { $process = Get-Process -Id $id -ErrorAction SilentlyContinue; if ($process -and $process.ProcessName -eq 'node') { Stop-Process -Id $id -Force } }"
echo Done.
pause
