param(
    [string]$ProjectRoot = (Resolve-Path "$PSScriptRoot/../..").Path,
    [string]$Port,
    [int]$Baud = 115200,
    [string]$InitSequence,
    [string]$Firmware,
    [switch]$Build,
    [string]$Environment = "release",
    [switch]$OnlyUnprotect,
    [switch]$OnlyErase,
    [switch]$SkipUnprotect,
    [switch]$SkipVerify,
    [switch]$NoRun,
    [switch]$Menu,
    [switch]$Auto,
    [switch]$NoInit
)

$ErrorActionPreference = "Stop"
$Script:ConfigPath = Join-Path $PSScriptRoot "flash-stm32-uart.config.json"

# FlyMCU 标准：DTR 低电平复位，RTS 高电平进入 BootLoader
# 序列含义为信号线电平（0=低，1=高），与 CH9102/CH340 的 DtrEnable 反相
$Script:DefaultInitSequence = "dtr=0,rts=1,-100,dtr=1"
$Script:FlyMcuInitLabel = "DTR低电平复位 RTS高电平进入BootLoader"

function Get-FlashConfig {
    if (Test-Path $Script:ConfigPath) {
        try {
            return Get-Content $Script:ConfigPath -Raw -Encoding UTF8 | ConvertFrom-Json
        }
        catch {
            Write-Warning "配置文件损坏，将使用默认值。"
        }
    }
    return $null
}

function Save-FlashConfig {
    param($Config)
    $Config | ConvertTo-Json -Depth 4 | Set-Content $Script:ConfigPath -Encoding UTF8
}

function New-FlashState {
    $saved = Get-FlashConfig
    $init = if (![string]::IsNullOrWhiteSpace($InitSequence)) { $InitSequence }
            elseif ($saved -and $saved.InitSequence) { $saved.InitSequence }
            else { $Script:DefaultInitSequence }

    [PSCustomObject]@{
        ProjectRoot   = $ProjectRoot
        Port          = if ($Port) { $Port } elseif ($saved -and $saved.Port) { $saved.Port } else { $null }
        Baud          = if ($PSBoundParameters.ContainsKey('Baud')) { $Baud } elseif ($saved -and $saved.Baud) { [int]$saved.Baud } else { 115200 }
        InitSequence  = $init
        UseInit       = if ($NoInit) { $false } elseif ($saved -and $null -ne $saved.UseInit) { [bool]$saved.UseInit } else { $true }
        Firmware      = $Firmware
        Environment   = $Environment
        SkipUnprotect = $SkipUnprotect.IsPresent
        SkipVerify    = $SkipVerify.IsPresent
        NoRun         = $NoRun.IsPresent
    }
}

function Resolve-Stm32Flash {
    $cmd = Get-Command stm32flash -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }

    $localRoots = @(
        (Join-Path $PSScriptRoot "tools\stm32flash"),
        (Join-Path $PSScriptRoot "tools")
    )
    foreach ($root in $localRoots) {
        if (!(Test-Path $root)) { continue }
        $localExe = Get-ChildItem -Path $root -Recurse -Filter "stm32flash.exe" -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($localExe) { return $localExe.FullName }
    }

    throw @"
未找到 stm32flash。

请先运行（无需 Chocolatey）:
  .\scripts\windows\install-stm32flash.bat

或手动下载解压:
  https://sourceforge.net/projects/stm32flash/files/stm32flash-0.5-win64.zip/download
"@
}

function Get-SerialPortList {
    $ports = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object {
        if ($_ -match 'COM(\d+)') { [int]$Matches[1] } else { 9999 }
    })
    if ($ports.Count -eq 0) {
        $ports = @(Get-CimInstance Win32_SerialPort -ErrorAction SilentlyContinue |
            ForEach-Object { $_.DeviceID } |
            Sort-Object { if ($_ -match 'COM(\d+)') { [int]$Matches[1] } else { 9999 } })
    }
    return @($ports | Where-Object { $_ } | Select-Object -Unique)
}

function Resolve-FirmwarePath {
    param([string]$Root, [string]$InputPath)

    if (![string]::IsNullOrWhiteSpace($InputPath)) {
        foreach ($p in @($InputPath, (Join-Path $Root $InputPath))) {
            if (Test-Path $p) { return (Resolve-Path $p).Path }
        }
        throw "固件文件不存在: $InputPath"
    }

    $defaultHex = Join-Path $Root "MDK-ARM\RosRobotControllerM4\RosRobotControllerM4.hex"
    if (Test-Path $defaultHex) { return (Resolve-Path $defaultHex).Path }

    $candidates = Get-ChildItem -Path $Root -Recurse -File -Include *.hex, *.bin -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime -Descending
    if ($candidates.Count -gt 0) { return $candidates[0].FullName }

    throw "未找到可用固件（*.hex/*.bin），请通过设置指定 -Firmware。"
}

function Test-IsDtrRtsInitSequence {
    param([string]$Sequence)
    return ($Sequence -match '(?i)dtr\s*=' -or $Sequence -match '(?i)rts\s*=')
}

function Set-SerialLineLevel {
    param(
        [System.IO.Ports.SerialPort]$Port,
        [ValidateSet('Dtr', 'Rts')]
        [string]$Line,
        [int]$Level
    )

    # FlyMCU 的 dtr/rts=0/1 表示信号线电平；CH9102/CH340 上 DTR 与 DtrEnable 反相
    if ($Line -eq 'Dtr') {
        $Port.DtrEnable = ($Level -eq 0)
    }
    else {
        $Port.RtsEnable = ($Level -eq 1)
    }
}

function Invoke-SerialDtrRtsSequence {
    param(
        [string]$PortName,
        [int]$Baud = 115200,
        [string]$Sequence = $Script:DefaultInitSequence
    )

    if ([string]::IsNullOrWhiteSpace($Sequence)) { return }

    $port = New-Object System.IO.Ports.SerialPort
    $port.PortName = $PortName
    $port.BaudRate = $Baud
    $port.DataBits = 8
    $port.Parity = [System.IO.Ports.Parity]::Even
    $port.StopBits = [System.IO.Ports.StopBits]::One
    $port.ReadTimeout = 500
    $port.WriteTimeout = 500
    $port.DtrEnable = $false
    $port.RtsEnable = $false

    try {
        $port.Open()
        foreach ($step in ($Sequence -split ',')) {
            $step = $step.Trim()
            if ($step -match '(?i)^dtr\s*=\s*(\d+)$') {
                Set-SerialLineLevel -Port $port -Line Dtr -Level ([int]$Matches[1])
            }
            elseif ($step -match '(?i)^rts\s*=\s*(\d+)$') {
                Set-SerialLineLevel -Port $port -Line Rts -Level ([int]$Matches[1])
            }
            elseif ($step -match '^-(\d+)$') {
                Start-Sleep -Milliseconds ([int]$Matches[1])
            }
            else {
                Write-Warning "未知 DTR/RTS 步骤: $step"
            }
        }
        Start-Sleep -Milliseconds 50
    }
    finally {
        if ($port.IsOpen) { $port.Close() }
        $port.Dispose()
    }
}

function Get-BaseFlashArgs {
    param($State)
    $baseArgs = @("-b", "$($State.Baud)")
    # stm32flash 0.5 的 -i 仅支持 GPIO 编号序列；DTR/RTS 由 Invoke-SerialDtrRtsSequence 处理
    if ($State.UseInit -and ![string]::IsNullOrWhiteSpace($State.InitSequence)) {
        if (!(Test-IsDtrRtsInitSequence $State.InitSequence)) {
            $baseArgs += @("-i", $State.InitSequence)
        }
    }
    return $baseArgs
}

function Invoke-Stm32FlashOnPort {
    param(
        [string]$Exe,
        [object]$State,
        [string]$PortName,
        [string[]]$ExtraArgs,
        [switch]$AllowFail
    )

    if ($State.UseInit -and (Test-IsDtrRtsInitSequence $State.InitSequence)) {
        Invoke-SerialDtrRtsSequence -PortName $PortName -Baud $State.Baud -Sequence $State.InitSequence
        Start-Sleep -Milliseconds 100
    }

    $flashArgs = (Get-BaseFlashArgs $State) + $ExtraArgs + $PortName
    return Invoke-Stm32Flash -Exe $Exe -FlashArgs $flashArgs -AllowFail:$AllowFail
}

function Invoke-Stm32Flash {
    param(
        [string]$Exe,
        [string[]]$FlashArgs,
        [switch]$AllowFail
    )

    Write-Host ">>> $Exe $($FlashArgs -join ' ')" -ForegroundColor DarkGray
    $output = & $Exe @FlashArgs 2>&1
    $text = ($output | Out-String).Trim()
    if ($text) { Write-Host $text }

    if ($LASTEXITCODE -ne 0 -and !$AllowFail) {
        throw "stm32flash 失败，退出码: $LASTEXITCODE"
    }
    return @{ Ok = ($LASTEXITCODE -eq 0); Text = $text; ExitCode = $LASTEXITCODE }
}

function Test-BootloaderPort {
    param(
        [string]$Exe,
        [object]$State,
        [string]$PortName
    )

    for ($attempt = 1; $attempt -le 3; $attempt++) {
        if ($attempt -gt 1) {
            Write-Host "  重试 $attempt/3..." -ForegroundColor DarkGray
            Start-Sleep -Milliseconds 300
        }

        # 连接并读取设备信息（失败则换口）
        $result = Invoke-Stm32FlashOnPort -Exe $Exe -State $State -PortName $PortName -ExtraArgs @() -AllowFail
        if ($result.Ok -and ($result.Text -match 'BootLoader|Version\s*[:=]|Device ID|STM32')) {
            return $true
        }
    }

    return $false
}

function Find-BootloaderPort {
    param(
        [string]$Exe,
        [object]$State,
        [string[]]$PreferPorts
    )

    $all = Get-SerialPortList
    if ($all.Count -eq 0) {
        throw "未检测到任何串口，请检查 USB 连接与驱动。"
    }

    $tryList = @()
    if ($State.Port) { $tryList += $State.Port }
    $tryList += $PreferPorts | Where-Object { $_ }
    $tryList += $all
    $tryList = @($tryList | Select-Object -Unique)

    Write-Host "自动扫描串口: $($all -join ', ')"
    Write-Host "按顺序尝试连接 BootLoader..."

    foreach ($p in $tryList) {
        if ($all -notcontains $p) { continue }
        Write-Host "  尝试 $p ..." -NoNewline
        if (Test-BootloaderPort -Exe $Exe -State $State -PortName $p) {
            Write-Host " 成功" -ForegroundColor Green
            return $p
        }
        Write-Host " 无响应" -ForegroundColor Yellow
    }

    throw @"
未找到可用的 BootLoader 串口。请确认：
  1. 板子已供电，COM 口未被 FlyMCU / 串口助手等占用
  2. DTR/RTS 模式与 FlyMCU 一致：$($Script:FlyMcuInitLabel)
  3. 若仍失败：手动将 BOOT0 置高 -> 按 RST -> 再试（设置里可关闭 DTR/RTS 自动进 Boot）
"@
}

function Invoke-PlatformioBuild {
    param([object]$State)
    $platformioDir = Join-Path $State.ProjectRoot "PlatformIO"
    if (!(Test-Path $platformioDir)) {
        throw "未找到 PlatformIO 目录: $platformioDir"
    }
    Write-Host "构建固件: env=$($State.Environment)" -ForegroundColor Cyan
    Push-Location $platformioDir
    try {
        python -m platformio run -e $State.Environment
        if ($LASTEXITCODE -ne 0) { throw "PlatformIO 构建失败，退出码: $LASTEXITCODE" }
    }
    finally {
        Pop-Location
    }
}

function Invoke-FlashWorkflow {
    param(
        [string]$Exe,
        [object]$State,
        [ValidateSet('Full', 'Unprotect', 'Erase', 'WriteOnly')]
        [string]$Mode,
        [switch]$AutoPickPort,
        [switch]$DoBuild
    )

    if ($DoBuild) { Invoke-PlatformioBuild -State $State }

    $firmwarePath = Resolve-FirmwarePath -Root $State.ProjectRoot -InputPath $State.Firmware
    $State | Add-Member -NotePropertyName FirmwareResolved -NotePropertyValue $firmwarePath -Force

    if ($AutoPickPort -or [string]::IsNullOrWhiteSpace($State.Port)) {
        $State.Port = Find-BootloaderPort -Exe $Exe -State $State
    }
    else {
        $available = Get-SerialPortList
        if ($available -notcontains $State.Port) {
            Write-Warning "指定串口 $($State.Port) 当前不在系统列表中（$($available -join ', ')），仍将尝试。"
        }
    }

    $port = $State.Port
    $initDesc = if (!$State.UseInit) { '(关闭)' }
               elseif (Test-IsDtrRtsInitSequence $State.InitSequence) { "DTR/RTS - $($State.InitSequence)" }
               else { "GPIO - $($State.InitSequence)" }

    Write-Host ""
    Write-Host "======== 烧录参数 ========" -ForegroundColor Cyan
    Write-Host "串口     : $port"
    Write-Host "波特率   : $($State.Baud)"
    Write-Host "初始化   : $initDesc"
    Write-Host "固件     : $firmwarePath"
    Write-Host "模式     : $Mode"
    Write-Host "=========================="
    Write-Host ""

    switch ($Mode) {
        'Unprotect' {
            Write-Host "[1/1] 解除读保护" -ForegroundColor Cyan
            Invoke-Stm32FlashOnPort -Exe $Exe -State $State -PortName $port -ExtraArgs @("-k") | Out-Null
        }
        'Erase' {
            Write-Host "[1/1] 擦除 Flash" -ForegroundColor Cyan
            Invoke-Stm32FlashOnPort -Exe $Exe -State $State -PortName $port -ExtraArgs @("-o") | Out-Null
        }
        'WriteOnly' {
            Write-Host "[1/1] 写入固件" -ForegroundColor Cyan
            $writeArgs = @("-w", $firmwarePath)
            if (!$State.SkipVerify) { $writeArgs += "-v" }
            if (!$State.NoRun) { $writeArgs += @("-g", "0x0") }
            Invoke-Stm32FlashOnPort -Exe $Exe -State $State -PortName $port -ExtraArgs $writeArgs | Out-Null
        }
        'Full' {
            if (!$State.SkipUnprotect) {
                Write-Host "[1/3] 解除读保护" -ForegroundColor Cyan
                Invoke-Stm32FlashOnPort -Exe $Exe -State $State -PortName $port -ExtraArgs @("-k") | Out-Null
            }
            else {
                Write-Host "[1/3] 跳过解除读保护" -ForegroundColor DarkYellow
            }
            Write-Host "[2/3] 擦除 Flash" -ForegroundColor Cyan
            Invoke-Stm32FlashOnPort -Exe $Exe -State $State -PortName $port -ExtraArgs @("-o") | Out-Null
            Write-Host "[3/3] 写入固件" -ForegroundColor Cyan
            $writeArgs = @("-w", $firmwarePath)
            if (!$State.SkipVerify) { $writeArgs += "-v" }
            if (!$State.NoRun) { $writeArgs += @("-g", "0x0") }
            Invoke-Stm32FlashOnPort -Exe $Exe -State $State -PortName $port -ExtraArgs $writeArgs | Out-Null
        }
    }

    Save-FlashConfig @{
        Port         = $State.Port
        Baud         = $State.Baud
        InitSequence = $State.InitSequence
        UseInit      = $State.UseInit
        LastFirmware = $firmwarePath
        LastMode     = $Mode
    }

    Write-Host ""
    Write-Host "完成。" -ForegroundColor Green
}

function Show-PortMenu {
    param([object]$State)
    $ports = Get-SerialPortList
    Write-Host ""
    Write-Host "--- 选择串口 ---" -ForegroundColor Cyan
    if ($ports.Count -eq 0) {
        Write-Host "  (未检测到串口)"
    }
    else {
        for ($i = 0; $i -lt $ports.Count; $i++) {
            $mark = if ($State.Port -eq $ports[$i]) { " <-- 当前" } else { "" }
            Write-Host "  $($i + 1). $($ports[$i])$mark"
        }
    }
    Write-Host "  A. 自动扫描并连接"
    Write-Host "  0. 返回"
    $choice = Read-Host "请选择"
    if ($choice -eq '0') { return }
    if ($choice -match '^[aA]$') { $State.Port = $null; return }
    if ($choice -match '^\d+$' -and [int]$choice -ge 1 -and [int]$choice -le $ports.Count) {
        $State.Port = $ports[[int]$choice - 1]
    }
}

function Show-FirmwareMenu {
    param([object]$State)
    $root = $State.ProjectRoot
    $files = @(
        Get-ChildItem -Path (Join-Path $root "MDK-ARM") -Recurse -File -Include *.hex, *.bin -ErrorAction SilentlyContinue
        Get-ChildItem -Path (Join-Path $root "PlatformIO\.pio\build") -Recurse -File -Include *.hex, *.bin -ErrorAction SilentlyContinue
    ) | Sort-Object LastWriteTime -Descending | Select-Object -First 15 -Unique

    Write-Host ""
    Write-Host "--- 选择固件 ---" -ForegroundColor Cyan
    for ($i = 0; $i -lt $files.Count; $i++) {
        $rel = $files[$i].FullName.Replace($root, '.')
        Write-Host "  $($i + 1). $rel"
    }
    Write-Host "  P. 手动输入路径"
    Write-Host "  0. 返回"
    $choice = Read-Host "请选择"
    if ($choice -eq '0') { return }
    if ($choice -match '^[pP]$') {
        $State.Firmware = Read-Host "固件完整路径"
        return
    }
    if ($choice -match '^\d+$' -and [int]$choice -ge 1 -and [int]$choice -le $files.Count) {
        $State.Firmware = $files[[int]$choice - 1].FullName
    }
}

function Show-SettingsMenu {
    param([object]$State)
    while ($true) {
        $fw = try { Split-Path (Resolve-FirmwarePath -Root $State.ProjectRoot -InputPath $State.Firmware) -Leaf } catch { "(未设置)" }
        Write-Host ""
        Write-Host "--- 设置 ---" -ForegroundColor Cyan
        Write-Host "  1. 串口      : $(if ($State.Port) { $State.Port } else { '(自动)' })"
        Write-Host "  2. 固件      : $fw"
        Write-Host "  3. 波特率    : $($State.Baud)"
        Write-Host "  4. 初始化序列: $($State.InitSequence)"
        Write-Host "  5. 使用 DTR/RTS 自动进 Boot: $(if ($State.UseInit) { '是' } else { '否' })"
        Write-Host "  6. 跳过解除读保护: $(if ($State.SkipUnprotect) { '是' } else { '否' })"
        Write-Host "  7. 跳过校验  : $(if ($State.SkipVerify) { '是' } else { '否' })"
        Write-Host "  8. 烧录后不运行: $(if ($State.NoRun) { '是' } else { '否' })"
        Write-Host "  9. 恢复默认初始化序列"
        Write-Host "  0. 返回主菜单"
        switch (Read-Host "请选择") {
            '1' { Show-PortMenu -State $State }
            '2' { Show-FirmwareMenu -State $State }
            '3' { $State.Baud = [int](Read-Host "波特率 (默认 115200)") }
            '4' { $State.InitSequence = Read-Host "DTR/RTS 序列 (如 dtr=0,rts=1,-100,dtr=1) 或 GPIO -i 序列" }
            '5' { $State.UseInit = !$State.UseInit }
            '6' { $State.SkipUnprotect = !$State.SkipUnprotect }
            '7' { $State.SkipVerify = !$State.SkipVerify }
            '8' { $State.NoRun = !$State.NoRun }
            '9' { $State.InitSequence = $Script:DefaultInitSequence }
            '0' { return }
        }
    }
}

function Show-MainMenu {
    param([string]$Exe, [object]$State)

    while ($true) {
        $ports = Get-SerialPortList
        $portInfo = if ($State.Port) { $State.Port } else { "(自动)" }
        $fw = try {
            $p = Resolve-FirmwarePath -Root $State.ProjectRoot -InputPath $State.Firmware
            Split-Path $p -Leaf
        }
        catch { "(未找到，请先编译或设置)" }

        Clear-Host
        Write-Host "========================================" -ForegroundColor Cyan
        Write-Host "   STM32 串口烧录 (stm32flash)" -ForegroundColor Cyan
        Write-Host "========================================" -ForegroundColor Cyan
        Write-Host "检测到串口: $(if ($ports.Count) { $ports -join ', ' } else { '无' })"
        Write-Host "当前串口  : $portInfo"
        Write-Host "波特率    : $($State.Baud)"
        Write-Host "固件      : $fw"
        Write-Host "DTR/RTS   : $(if ($State.UseInit) { 'FlyMCU - ' + $Script:FlyMcuInitLabel + ' (' + $State.InitSequence + ')' } else { '关' })"
        Write-Host "----------------------------------------"
        Write-Host "  1. 完整烧录 (解除保护 + 擦除 + 写入 + 校验 + 运行)"
        Write-Host "  2. 仅解除读保护"
        Write-Host "  3. 仅擦除"
        Write-Host "  4. 仅写入固件 (跳过解除保护)"
        Write-Host "  5. 自动模式 (扫描串口并连接后完整烧录)" -ForegroundColor Yellow
        Write-Host "  6. 先 PlatformIO 编译再完整烧录"
        Write-Host "  7. 设置..."
        Write-Host "  8. 安装 stm32flash (本机无 choco 时用)" -ForegroundColor DarkCyan
        Write-Host "  0. 退出"
        Write-Host "----------------------------------------"

        $choice = Read-Host "请选择"
        try {
            switch ($choice) {
                '0' { return }
                '1' {
                    Invoke-FlashWorkflow -Exe $Exe -State $State -Mode Full -AutoPickPort:([string]::IsNullOrWhiteSpace($State.Port))
                    Read-Host "按 Enter 继续"
                }
                '2' {
                    Invoke-FlashWorkflow -Exe $Exe -State $State -Mode Unprotect -AutoPickPort:([string]::IsNullOrWhiteSpace($State.Port))
                    Read-Host "按 Enter 继续"
                }
                '3' {
                    Invoke-FlashWorkflow -Exe $Exe -State $State -Mode Erase -AutoPickPort:([string]::IsNullOrWhiteSpace($State.Port))
                    Read-Host "按 Enter 继续"
                }
                '4' {
                    $State.SkipUnprotect = $true
                    Invoke-FlashWorkflow -Exe $Exe -State $State -Mode WriteOnly -AutoPickPort:([string]::IsNullOrWhiteSpace($State.Port))
                    Read-Host "按 Enter 继续"
                }
                '5' {
                    $savedPort = $State.Port
                    $State.Port = $null
                    Invoke-FlashWorkflow -Exe $Exe -State $State -Mode Full -AutoPickPort
                    $State.Port = $savedPort
                    Read-Host "按 Enter 继续"
                }
                '6' {
                    Invoke-FlashWorkflow -Exe $Exe -State $State -Mode Full -DoBuild -AutoPickPort:([string]::IsNullOrWhiteSpace($State.Port))
                    Read-Host "按 Enter 继续"
                }
                '7' { Show-SettingsMenu -State $State }
                '8' {
                    $installPs1 = Join-Path $PSScriptRoot "install-stm32flash.ps1"
                    & powershell -ExecutionPolicy Bypass -File $installPs1
                    Read-Host "按 Enter 继续"
                }
                default { Write-Host "无效选项" -ForegroundColor Yellow; Start-Sleep -Seconds 1 }
            }
        }
        catch {
            Write-Host ""
            Write-Host "错误: $($_.Exception.Message)" -ForegroundColor Red
            Read-Host "按 Enter 继续"
        }
    }
}

function Test-CliMode {
    param($Bound)
    $cliSwitches = @(
        'OnlyUnprotect', 'OnlyErase', 'SkipUnprotect', 'SkipVerify', 'NoRun',
        'Build', 'Auto', 'Port', 'Firmware', 'InitSequence', 'NoInit', 'Baud'
    )
    foreach ($name in $cliSwitches) {
        if ($Bound.ContainsKey($name)) { return $true }
    }
    return $false
}

# ---------- 入口 ----------
$state = New-FlashState
$exe = Resolve-Stm32Flash
$isCli = Test-CliMode -Bound $PSBoundParameters

if ($Menu -or (!$isCli -and !$Auto)) {
    Show-MainMenu -Exe $exe -State $state
    exit 0
}

if ($Auto) {
    $mode = if ($OnlyUnprotect) { 'Unprotect' }
            elseif ($OnlyErase) { 'Erase' }
            else { 'Full' }
    Invoke-FlashWorkflow -Exe $exe -State $state -Mode $mode -AutoPickPort -DoBuild:$Build.IsPresent
    exit 0
}

# 命令行直跑（兼容旧参数）
if ($OnlyUnprotect -and $OnlyErase) { throw "-OnlyUnprotect 与 -OnlyErase 不能同时使用。" }

$mode = if ($OnlyUnprotect) { 'Unprotect' }
        elseif ($OnlyErase) { 'Erase' }
        else { 'Full' }

if ($OnlyErase -or $OnlyUnprotect) {
    # 保持原行为
}
elseif ($SkipUnprotect) {
    $mode = 'WriteOnly'
}

Invoke-FlashWorkflow -Exe $exe -State $state -Mode $mode -DoBuild:$Build.IsPresent `
    -AutoPickPort:([string]::IsNullOrWhiteSpace($state.Port))
