param(
    [switch]$Force
)

$ErrorActionPreference = "Stop"

$toolsDir = Join-Path $PSScriptRoot "tools\stm32flash"
$zipPath = Join-Path $PSScriptRoot "tools\stm32flash-0.5-win64.zip"
$downloadUrl = "https://downloads.sourceforge.net/project/stm32flash/stm32flash-0.5-win64.zip"

function Find-Stm32FlashExe {
    param([string]$Root)
    if (!(Test-Path $Root)) { return $null }
    $exe = Get-ChildItem -Path $Root -Recurse -Filter "stm32flash.exe" -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($exe) { return $exe.FullName }
    return $null
}

$existing = Find-Stm32FlashExe -Root $toolsDir
if ($existing -and !$Force) {
    Write-Host "stm32flash already installed: $existing"
    Write-Host "Re-run with -Force to reinstall."
    exit 0
}

New-Item -ItemType Directory -Path (Split-Path $toolsDir) -Force | Out-Null
if (Test-Path $toolsDir) { Remove-Item $toolsDir -Recurse -Force }
New-Item -ItemType Directory -Path $toolsDir -Force | Out-Null

Write-Host "Downloading stm32flash..."
Write-Host $downloadUrl

$pyScript = @"
import sys, zipfile, urllib.request
url = sys.argv[1]
zip_path = sys.argv[2]
dest = sys.argv[3]
print('fetch', url)
urllib.request.urlretrieve(url, zip_path)
with open(zip_path, 'rb') as f:
    if f.read(2) != b'PK':
        raise SystemExit('not a zip file')
with zipfile.ZipFile(zip_path) as z:
    z.extractall(dest)
print('ok')
"@

$pyFile = Join-Path $env:TEMP "install_stm32flash.py"
Set-Content -Path $pyFile -Value $pyScript -Encoding UTF8

python $pyFile $downloadUrl $zipPath $toolsDir
if ($LASTEXITCODE -ne 0) {
    throw "Download/extract failed. Manual: $downloadUrl"
}

Remove-Item $pyFile -Force -ErrorAction SilentlyContinue
Remove-Item $zipPath -Force -ErrorAction SilentlyContinue

$exe = Find-Stm32FlashExe -Root $toolsDir
if (!$exe) {
    throw "stm32flash.exe not found under $toolsDir"
}

Write-Host ""
Write-Host "Done: $exe" -ForegroundColor Green
Write-Host "Run flash-stm32-uart.bat - no PATH or choco needed."
