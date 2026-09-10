param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath,
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\..\..\build'),
    [string]$OutputDirectory = (Join-Path $env:TEMP ('StuntRaceFXWE-benchmark-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))),
    [ValidateSet(100, 150, 200, 250, 300, 350, 400, 450, 500)]
    [int]$GsuClock = 300,
    [ValidateSet('compat', 'native', 'hybrid')]
    [string]$Renderer = 'compat',
    [int]$CaptureFrame = 0,
    [ValidateRange(1, 100000)]
    [int]$MaxFrames = 3400,
    [ValidateRange(1, 120)]
    [int]$CaptureFrames = 1,
    [switch]$GeometryProbe,
    [switch]$DrawStream,
    [switch]$Wide,
    [switch]$HdCenter,
    [switch]$Materials,
    [ValidateSet('512x384','960x540','1280x540','1920x1080','2560x1440','3440x1440','3840x2160')]
    [string]$OutputSize = '512x384',
    [ValidateSet('latest','previous')]
    [string]$DrawStreamPhase = 'latest',
    [ValidatePattern('^[0-9a-fA-F]{6}$')]
    [string]$GeometryWatch = 'FFFFFF',
    [switch]$RealTime
)

$ErrorActionPreference = 'Stop'
if ($GeometryProbe -and $CaptureFrame -le 0) { throw 'GeometryProbe requires CaptureFrame.' }
$rom = (Resolve-Path -LiteralPath $RomPath).Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$inputScript = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'first-track-bridge-input.txt')).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
$runtime = Join-Path $env:TEMP ('StuntRaceFXWE-runtime-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runtime | Out-Null

try {
    # Never inherit the user's save RAM, ROM copies or recordings from a build.
    foreach ($file in (Get-ChildItem -LiteralPath $build -File -Recurse)) {
        if ($file.Extension.ToLowerInvariant() -notin @('.exe','.dll','.cmd','.txt','.bmp')) { continue }
        $relative = $file.FullName.Substring($build.TrimEnd('\').Length + 1)
        $destination = Join-Path $runtime $relative
        New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
        Copy-Item -LiteralPath $file.FullName -Destination $destination
    }
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = Join-Path $runtime 'StuntRaceFX.exe'
    $start.Arguments = '"' + $rom.Replace('"', '\"') + '"'
    $start.WorkingDirectory = $runtime
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    # Make a benchmark independent of inherited launcher settings.
    foreach ($key in @($start.EnvironmentVariables.Keys)) {
        if ($key.StartsWith('SRF_')) { $start.EnvironmentVariables.Remove($key) }
    }
    $start.EnvironmentVariables['SRF_PROFILE_LOG'] = Join-Path $output 'first-track.csv'
    $start.EnvironmentVariables['SRF_INPUT_SCRIPT'] = $inputScript
    $start.EnvironmentVariables['SRF_MAX_FRAMES'] = "$([Math]::Max($MaxFrames, $CaptureFrame))"
    $start.EnvironmentVariables['SRF_NO_AUDIO'] = '1'
    $start.EnvironmentVariables['SRF_HIDDEN'] = '1'
    $start.EnvironmentVariables['SRF_WINDOW'] = $OutputSize
    $start.EnvironmentVariables['SRF_GSU_CLOCK'] = "$GsuClock%"
    $start.EnvironmentVariables['SRF_RENDERER'] = $Renderer
    $start.EnvironmentVariables['SRF_FILTER'] = 'edge'
    $start.EnvironmentVariables['SRF_MOTION'] = '1'
    $start.EnvironmentVariables['SRF_MATERIALS'] = if($Materials) {'1'} else {'0'}
    if ($Wide) { $start.EnvironmentVariables['SRF_COMPAT_WIDE'] = '1' }
    if ($HdCenter) {
        $start.EnvironmentVariables['SRF_COMPAT_WIDE'] = '1'
        $start.EnvironmentVariables['SRF_COMPAT_HD_CENTER'] = '1'
    }
    if ($DrawStream) { $start.EnvironmentVariables['SRF_COMPAT_DRAW_STREAM'] = if ($DrawStreamPhase -eq 'previous') {'2'} else {'1'} }
    if ($CaptureFrame -gt 0) {
        $start.EnvironmentVariables['SRF_GSU_CAPTURE_DIR'] = $output
        $start.EnvironmentVariables['SRF_GSU_CAPTURE_FROM'] = "$( [Math]::Max(1, $CaptureFrame - $CaptureFrames + 1) )"
        $start.EnvironmentVariables['SRF_GSU_CAPTURE_TO'] = "$CaptureFrame"
        if ($GeometryProbe) {
            $start.EnvironmentVariables['SRF_GEOMETRY_PROBE'] = '1'
            $start.EnvironmentVariables['SRF_GEOMETRY_WATCH'] = $GeometryWatch
        }
        $start.EnvironmentVariables['SRF_PRESENT_CAPTURE_DIR'] = $output
        $start.EnvironmentVariables['SRF_PRESENT_CAPTURE_FROM'] = "$CaptureFrame"
        $start.EnvironmentVariables['SRF_PRESENT_CAPTURE_TO'] = "$CaptureFrame"
        $start.EnvironmentVariables['SRF_PRESENT_CAPTURE_STEP'] = '1'
    }
    if (-not $RealTime) { $start.EnvironmentVariables['SRF_FAST'] = '1' }
    $process = [System.Diagnostics.Process]::Start($start)
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) { throw "Benchmark exited with code $($process.ExitCode)." }
    Write-Host "Profile: $(Join-Path $output 'first-track.csv')"
    Write-Host "Summary: $(Join-Path $output 'first-track.csv.summary.txt')"
} finally {
    $tempRoot = [IO.Path]::GetFullPath($env:TEMP).TrimEnd('\') + '\'
    $resolvedRuntime = [IO.Path]::GetFullPath($runtime)
    if ($resolvedRuntime.StartsWith($tempRoot, [StringComparison]::OrdinalIgnoreCase)) {
        Remove-Item -LiteralPath $resolvedRuntime -Recurse -Force -ErrorAction SilentlyContinue
    }
}
