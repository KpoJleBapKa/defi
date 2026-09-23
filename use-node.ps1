$nodeDirectory = "F:\Dev\node-v24.21.0-win-x64"
$nodeExecutable = Join-Path $nodeDirectory "node.exe"

if (!(Test-Path -LiteralPath $nodeExecutable)) {
    throw "Node.js 24 was not found at $nodeExecutable"
}

$env:Path = "$nodeDirectory;$env:Path"
Set-Alias -Name node -Value $nodeExecutable -Scope Global
Set-Alias -Name npm -Value (Join-Path $nodeDirectory "npm.cmd") -Scope Global
Set-Alias -Name npx -Value (Join-Path $nodeDirectory "npx.cmd") -Scope Global

Write-Host "Node.js environment activated: $(node --version)"
Write-Host "npm: $(npm --version)"
