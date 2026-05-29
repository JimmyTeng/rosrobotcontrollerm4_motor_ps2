param(
    [string]$Environment = "release"
)

$ErrorActionPreference = "Stop"

$compileDbPath = Join-Path $PSScriptRoot "compile_commands.json"

Write-Host "PlatformIO dir: $PSScriptRoot"
Write-Host "刷新 compile_commands.json (env: $Environment)"

Push-Location $PSScriptRoot
try {
    python -m platformio run -e $Environment -t compiledb
}
finally {
    Pop-Location
}

if (Test-Path $compileDbPath) {
    Write-Host "完成: $compileDbPath"
} else {
    throw "生成失败，未找到: $compileDbPath"
}
