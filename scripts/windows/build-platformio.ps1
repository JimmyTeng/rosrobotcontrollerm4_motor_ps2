param(
    [string]$ProjectRoot = (Resolve-Path "$PSScriptRoot/../..").Path,
    [string]$Environment = "release"
)

$ErrorActionPreference = "Stop"

$platformioDir = Join-Path $ProjectRoot "PlatformIO"

if (!(Test-Path $platformioDir)) {
    throw "未找到 PlatformIO 目录: $platformioDir"
}

Write-Host "Project root: $ProjectRoot"
Write-Host "PlatformIO dir: $platformioDir"
Write-Host "开始编译环境: $Environment"

Push-Location $platformioDir
try {
    python -m platformio run -e $Environment
}
finally {
    Pop-Location
}
