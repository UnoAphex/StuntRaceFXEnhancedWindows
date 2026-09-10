param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath,
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\..\..\build'),
    [string]$OutputDirectory = (Join-Path $env:TEMP ('StuntRaceFXWE-preset-matrix-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))),
    [ValidateRange(90, 10000)]
    [int]$Frames = 240,
    [ValidateRange(0, 1000)]
    [int]$WarmupFrames = 60,
    [switch]$Fast
)

$ErrorActionPreference = 'Stop'
$rom = (Resolve-Path -LiteralPath $RomPath).Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$inputScript = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'first-track-bridge-input.txt')).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path

$presets = @(
    @{ Name='visual-original'; Visual='0'; Size='1600x900' },
    @{ Name='visual-crisp'; Visual='1'; Size='1600x900' },
    @{ Name='visual-ultrawide'; Visual='2'; Size='1600x900' },
    @{ Name='visual-sprite-safe'; Visual='3'; Size='1600x900' },
    @{ Name='visual-n64-style'; Visual='4'; Size='1600x900' },
    @{ Name='compatibility-performance'; Motion='1'; Materials='0'; Size='1600x900' },
    @{ Name='compatibility-enhanced'; Motion='1'; Materials='1'; Size='1600x900' },
    @{ Name='sprite-safe-smooth'; Motion='1'; Materials='0'; Size='1600x900' },
    @{ Name='true-wide-1920x1080'; Motion='1'; Materials='0'; Wide='1'; Size='1920x1080' },
    @{ Name='true-wide-3440x1440'; Motion='1'; Materials='0'; Wide='1'; Size='3440x1440' },
    @{ Name='true-wide-hd-1920x1080'; Motion='1'; Materials='1'; Wide='1'; Hd='1'; Size='1920x1080' },
    @{ Name='true-wide-hd-3440x1440'; Motion='1'; Materials='1'; Wide='1'; Hd='1'; Size='3440x1440' },
    @{ Name='first-track-enhanced-3440x1440'; Motion='1'; Materials='1'; Wide='1'; Hd='1'; Size='3440x1440' },
    @{ Name='windowed-vsync-audit'; Motion='1'; Materials='0'; Vsync='1'; Size='1600x900' },
    @{ Name='fullscreen-audit-3440x1440'; Motion='1'; Materials='1'; Wide='1'; Hd='1'; Fullscreen='1'; Size='3440x1440' }
)

function Get-Percentile([double[]]$Values, [double]$Percentile) {
    if (-not $Values.Count) { return 0.0 }
    [Array]::Sort($Values)
    $index = [Math]::Min($Values.Count - 1, [Math]::Floor(($Values.Count - 1) * $Percentile))
    return $Values[$index]
}

$results = @()
$stateBaseline = $null
foreach ($preset in $presets) {
    $name = $preset.Name
    $runOutput = Join-Path $output $name
    $runtime = Join-Path $env:TEMP ('StuntRaceFXWE-preset-runtime-' + [guid]::NewGuid().ToString('N'))
    New-Item -ItemType Directory -Path $runOutput -Force | Out-Null
    New-Item -ItemType Directory -Path $runtime | Out-Null
    try {
        foreach ($file in (Get-ChildItem -LiteralPath $build -File -Recurse)) {
            if ($file.Extension.ToLowerInvariant() -notin @('.exe','.dll','.cmd','.txt','.bmp')) { continue }
            $relative = $file.FullName.Substring($build.TrimEnd('\').Length + 1)
            $destination = Join-Path $runtime $relative
            New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
            Copy-Item -LiteralPath $file.FullName -Destination $destination
        }

        $profilePath = Join-Path $runOutput 'frames.csv'
        $start = New-Object System.Diagnostics.ProcessStartInfo
        $start.FileName = Join-Path $runtime 'StuntRaceFX.exe'
        $start.Arguments = '"' + $rom.Replace('"', '\"') + '"'
        $start.WorkingDirectory = $runtime
        $start.UseShellExecute = $false
        $start.CreateNoWindow = $true
        foreach ($key in @($start.EnvironmentVariables.Keys)) {
            if ($key.StartsWith('SRF_')) { $start.EnvironmentVariables.Remove($key) }
        }
        $settings = @{
            SRF_RENDERER='compat'; SRF_PRESET_NAME=$name; SRF_PROFILE_LOG=$profilePath;
            SRF_INPUT_SCRIPT=$inputScript; SRF_MAX_FRAMES="$Frames"; SRF_NO_AUDIO='1';
            SRF_HIDDEN='1'; SRF_WINDOW=$preset.Size; SRF_GSU_CLOCK='300%'; SRF_CAP='60';
            SRF_VSYNC='0'; SRF_RUNAHEAD='0'; SRF_GSU_CAPTURE_DIR=$runOutput;
            SRF_GSU_CAPTURE_FROM="$Frames"; SRF_GSU_CAPTURE_TO="$Frames"
        }
        if ($preset.Visual) { $settings.SRF_VISUAL_PRESET = $preset.Visual }
        else {
            $settings.SRF_FILTER = 'edge'
            $settings.SRF_MOTION = $preset.Motion
            $settings.SRF_MATERIALS = $preset.Materials
            if ($preset.Wide) { $settings.SRF_COMPAT_WIDE = '1' }
            if ($preset.Hd) { $settings.SRF_COMPAT_HD_CENTER = '1' }
        }
        if ($preset.Vsync) { $settings.SRF_VSYNC = '1' }
        if ($preset.Fullscreen) { $settings.SRF_FULLSCREEN = '1' }
        if ($Fast) { $settings.SRF_FAST = '1' }
        foreach ($entry in $settings.GetEnumerator()) {
            $start.EnvironmentVariables[$entry.Key] = [string]$entry.Value
        }

        $process = [System.Diagnostics.Process]::Start($start)
        $process.WaitForExit()
        if ($process.ExitCode -ne 0) { throw "$name exited with code $($process.ExitCode)." }

        $rows = @(Import-Csv -LiteralPath $profilePath)
        $usable = @($rows | Select-Object -Skip ([Math]::Min($WarmupFrames, $rows.Count - 2)) |
            Select-Object -SkipLast 1)
        [double[]]$frameTimes = @($usable | ForEach-Object { [double]$_.total_ms })
        $average = ($frameTimes | Measure-Object -Average).Average
        $p99 = Get-Percentile $frameTimes 0.99
        $missing = @($rows | Where-Object { [int]$_.presentation_callbacks -eq 0 }).Count
        $multiple = @($rows | Where-Object { [int]$_.presentation_callbacks -gt 1 }).Count
        $state = @{}
        foreach ($kind in @('wram','ram','regs')) {
            $path = Join-Path $runOutput ('snes9x_f{0:D6}_{1}.bin' -f $Frames,$kind)
            $state[$kind] = (Get-FileHash -Algorithm SHA256 -LiteralPath $path).Hash
        }
        if (-not $stateBaseline) { $stateBaseline = $state }
        $stateMatch = $state.wram -eq $stateBaseline.wram -and
            $state.ram -eq $stateBaseline.ram -and $state.regs -eq $stateBaseline.regs
        $target = 1000.0 / 60.0988
        $pacingPass = $Fast -or ([Math]::Abs($average - $target) -le 1.0 -and $p99 -le 20.0)
        $pass = $pacingPass -and $missing -eq 0 -and $multiple -eq 0 -and $stateMatch
        $results += [pscustomobject]@{
            preset=$name; frames=$rows.Count; output=$preset.Size; fast=[bool]$Fast;
            average_frame_ms=[Math]::Round($average,4); p99_frame_ms=[Math]::Round($p99,4);
            missing_presentations=$missing; multiple_presentations=$multiple;
            state_match=$stateMatch; pass=$pass; wram_sha256=$state.wram;
            gsu_ram_sha256=$state.ram; registers_sha256=$state.regs
        }
    } finally {
        $tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
        $resolvedRuntime = [IO.Path]::GetFullPath($runtime)
        if ($resolvedRuntime.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
            Remove-Item -LiteralPath $resolvedRuntime -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

$matrixPath = Join-Path $output 'preset-matrix.csv'
$results | Export-Csv -LiteralPath $matrixPath -NoTypeInformation
$results | Format-Table preset,average_frame_ms,p99_frame_ms,missing_presentations,multiple_presentations,state_match,pass
if (@($results | Where-Object { -not $_.pass }).Count) {
    throw "One or more compatibility presets failed. See $matrixPath"
}
Write-Host "All compatibility presets passed. Results: $matrixPath"
