param(
    [string]$ProjectRoot = (Resolve-Path "$PSScriptRoot/..").Path
)

$ErrorActionPreference = "Stop"

$platformioDir = Join-Path $ProjectRoot "PlatformIO"
$compileDbPath = Join-Path $platformioDir "compile_commands.json"

if (!(Test-Path $platformioDir)) {
    throw "未找到 PlatformIO 目录: $platformioDir"
}

Write-Host "Project root: $ProjectRoot"
Write-Host "PlatformIO dir: $platformioDir"
Write-Host "刷新 compile_commands.json ..."

Push-Location $platformioDir
try {
    python -m platformio run -t compiledb
}
finally {
    Pop-Location
}

if (Test-Path $compileDbPath) {
    Write-Host "完成: $compileDbPath"
} else {
    throw "生成失败，未找到: $compileDbPath"
}
