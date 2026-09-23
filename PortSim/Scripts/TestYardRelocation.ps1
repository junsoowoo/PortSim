param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.6',
    [switch]$MixedTraffic,
    [switch]$QuickRoundTrip,
    [ValidateSet(0,1,2,4,8,16)][int]$Playback = 0,
    [ValidateRange(10,60)][int]$FixedFPS = 20
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'PortSim.uproject'
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$name = if ($MixedTraffic) { 'YardMixedTest' } else { 'YardRelocationTest' }
$testLog = Join-Path $root "Saved\Logs\$name.log"
$arguments = @($project, '/Engine/Maps/Entry', '-game', '-nullrhi', '-nosound', '-unattended', '-nosplash', '-benchmark', "-fps=$FixedFPS", '-PortSimTerminalTest', "-abslog=$testLog")
if ($MixedTraffic) { $arguments += '-PortSimMixedTest' }
if ($QuickRoundTrip) { $arguments += '-PortSimRoundTripTest' }
if ($Playback -gt 0) { $arguments += "-PortSimPlayback=$Playback" }
& $editor @arguments
if ($LASTEXITCODE -ne 0) { throw "Yard relocation test process failed: $LASTEXITCODE. See $testLog" }
if (-not (Test-Path -LiteralPath $testLog)) { throw 'Yard relocation test did not create a log.' }
if (-not (Select-String -LiteralPath $testLog -Pattern 'PORTSIM_TERMINAL_PASS:' -Quiet)) {
    Get-Content -LiteralPath $testLog -Tail 40
    throw "Yard relocation test failed. See $testLog"
}
Select-String -LiteralPath $testLog -Pattern 'PORTSIM_TERMINAL_PASS:'
