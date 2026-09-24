param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.6',
    [ValidateRange(0.5,60)][double]$FixedFPS = 5
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $root 'Saved\Logs\FullUnloadTest.log'
$fpsArgument = '-fps=' + $FixedFPS.ToString([Globalization.CultureInfo]::InvariantCulture)
& $editor (Join-Path $root 'PortSim.uproject') /Engine/Maps/Entry -game -nullrhi -nosound -unattended -nosplash -benchmark $fpsArgument '-ini:Game:[/Script/Engine.WorldSettings]:MaxUndilatedFrameTime=2.0' -PortSimFullUnloadTest "-abslog=$log"
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $log -Pattern 'PORTSIM_SITE_PASS:' -Quiet)) {
    Get-Content -LiteralPath $log -Tail 40
    throw "Full unloading failed. See $log"
}
Select-String -LiteralPath $log -Pattern 'SITE_INVENTORY:|RECEIVING_METRICS:|DISPATCH_METRICS:|PORTSIM_SITE_PASS:'
