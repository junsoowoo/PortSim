param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.6')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'PortSim.uproject'
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$testLog = Join-Path $root 'Saved\Logs\CraneSmoke.log'
& $editor $project /Engine/Maps/Entry -game -nullrhi -nosound -unattended -nosplash -benchmark -fps=60 -PortSimSmokeTest "-abslog=$testLog"
if (-not (Test-Path -LiteralPath $testLog)) { throw 'Test did not create a log.' }
$passed = Select-String -LiteralPath $testLog -Pattern 'PORTSIM_SMOKE_PASS:' -Quiet
if (-not $passed) { Get-Content -LiteralPath $testLog -Tail 30; throw 'Crane simulation test failed. See CraneSmoke.log.' }
Select-String -LiteralPath $testLog -Pattern 'PORTSIM_SMOKE_PASS:'
