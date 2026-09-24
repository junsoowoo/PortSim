param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.6',
    [ValidateRange(5,60)][int]$FixedFPS = 10
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'PortSim.uproject'
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$testLog = Join-Path $root 'Saved\Logs\SiteTest.log'
& $editor $project /Engine/Maps/Entry -game -nullrhi -nosound -unattended -nosplash -benchmark "-fps=$FixedFPS" -PortSimSiteTest "-abslog=$testLog"
if ($LASTEXITCODE -ne 0) { throw "Site test process failed: $LASTEXITCODE" }
if (-not (Select-String -LiteralPath $testLog -Pattern 'PORTSIM_SITE_PASS:' -Quiet)) {
    Get-Content -LiteralPath $testLog -Tail 40
    throw 'Site crane test failed. See SiteTest.log.'
}
Select-String -LiteralPath $testLog -Pattern 'PORTSIM_SITE_PASS:'
