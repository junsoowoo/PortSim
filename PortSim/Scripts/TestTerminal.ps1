param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.6')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'PortSim.uproject'
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$testLog = Join-Path $root 'Saved\Logs\TerminalTest.log'
& $editor $project /Engine/Maps/Entry -game -nullrhi -nosound -unattended -nosplash -benchmark -fps=20 -PortSimTerminalTest "-abslog=$testLog"
if (-not (Test-Path -LiteralPath $testLog)) { throw 'Terminal test did not create a log.' }
if (-not (Select-String -LiteralPath $testLog -Pattern 'PORTSIM_TERMINAL_PASS:' -Quiet)) {
    Get-Content -LiteralPath $testLog -Tail 40
    throw 'Terminal automation test failed. See TerminalTest.log.'
}
Select-String -LiteralPath $testLog -Pattern 'PORTSIM_TERMINAL_PASS:'
