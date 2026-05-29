param(
    [string]$Environment = "release"
)

$ErrorActionPreference = "Stop"

Write-Host "PlatformIO dir: $PSScriptRoot"
Write-Host "开始编译环境: $Environment"

Push-Location $PSScriptRoot
try {
    python -m platformio run -e $Environment
}
finally {
    Pop-Location
}
