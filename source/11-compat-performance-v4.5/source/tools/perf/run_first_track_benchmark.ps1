param(
    [Parameter(Mandatory = $true)]
    [string]$RomPath,
    [string]$BuildDirectory = (Join-Path $PSScriptRoot '..\..\..\build'),
    [string]$OutputDirectory = (Join-Path $env:TEMP ('StuntRaceFXWE-benchmark-' + (Get-Date -Format 'yyyyMMdd-HHmmss'))),
    [ValidateSet(100, 150, 200, 250, 300, 350, 400, 450, 500)]
    [int]$GsuClock = 300,
    [switch]$RealTime
)

$ErrorActionPreference = 'Stop'
$rom = (Resolve-Path -LiteralPath $RomPath).Path
$build = (Resolve-Path -LiteralPath $BuildDirectory).Path
$inputScript = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'first-track-bridge-input.txt')).Path
New-Item -ItemType Directory -Path $OutputDirectory -Force | Out-Null
$output = (Resolve-Path -LiteralPath $OutputDirectory).Path
$runtime = Join-Path $env:TEMP ('StuntRaceFXWE-runtime-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $runtime | Out-Null

try {
    Copy-Item -Path (Join-Path $build '*') -Destination $runtime -Recurse -Force
    $start = New-Object System.Diagnostics.ProcessStartInfo
    $start.FileName = Join-Path $runtime 'StuntRaceFX.exe'
    $start.Arguments = '"' + $rom.Replace('"', '\"') + '"'
    $start.WorkingDirectory = $runtime
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.EnvironmentVariables['SRF_PROFILE_LOG'] = Join-Path $output 'first-track.csv'
    $start.EnvironmentVariables['SRF_INPUT_SCRIPT'] = $inputScript
    $start.EnvironmentVariables['SRF_MAX_FRAMES'] = '3400'
    $start.EnvironmentVariables['SRF_NO_AUDIO'] = '1'
    $start.EnvironmentVariables['SRF_HIDDEN'] = '1'
    $start.EnvironmentVariables['SRF_WINDOW'] = '512x384'
    $start.EnvironmentVariables['SRF_GSU_CLOCK'] = "$GsuClock%"
    $start.EnvironmentVariables['SRF_RENDERER'] = 'compat'
    $start.EnvironmentVariables['SRF_FILTER'] = 'edge'
    $start.EnvironmentVariables['SRF_MOTION'] = '1'
    $start.EnvironmentVariables['SRF_MATERIALS'] = '0'
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
