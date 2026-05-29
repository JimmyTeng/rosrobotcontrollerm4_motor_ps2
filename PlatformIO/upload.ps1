param(
    [string]$Environment = "release"
)

$ErrorActionPreference = "Stop"

Write-Host "PlatformIO dir: $PSScriptRoot"
Write-Host "开始下载环境: $Environment"

Push-Location $PSScriptRoot
try {
    python -m platformio run -e $Environment -t upload
}
finally {
    Pop-Location
}
