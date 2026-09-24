param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.6')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$project = Join-Path $root 'PortSim.uproject'
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$testLog = Join-Path $root 'Saved\Logs\EquipmentTest.log'
& $editor $project /Engine/Maps/Entry -game -nullrhi -nosound -unattended -nosplash -PortSimEquipmentTest "-abslog=$testLog"
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $testLog -Pattern 'PORTSIM_EQUIPMENT_CAMERA_PASS:' -Quiet)) {
    throw "Equipment/camera test failed. See $testLog"
}
Select-String -LiteralPath $testLog -Pattern 'PORTSIM_EQUIPMENT_CAMERA_PASS:'
