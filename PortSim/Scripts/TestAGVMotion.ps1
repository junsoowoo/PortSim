param(
    [string]$Engine = 'C:\Program Files\Epic Games\UE_5.6',
    [ValidateSet(1,2,4,8,16)][int]$Playback = 8
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$log = Join-Path $root 'Saved\Logs\AGVMotionTest.log'
$image = Join-Path $root 'Saved\Screenshots\AGV_LoadedMotion.png'
& $editor (Join-Path $root 'PortSim.uproject') /Engine/Maps/Entry -game -RenderOffscreen -nosound -unattended -nosplash -PortSimAGVProof "-PortSimPlayback=$Playback" "-abslog=$log"
if ($LASTEXITCODE -ne 0 -or -not (Select-String -LiteralPath $log -Pattern 'AGV_RENDER_PROOF_PASS:' -Quiet)) {
    throw "Loaded AGV movement test failed. See $log"
}
if (-not (Test-Path -LiteralPath $image)) { throw 'Missing rendered movement capture.' }
Select-String -LiteralPath $log -Pattern 'AGV_RENDER_PROOF_PASS:'
Write-Output "Capture: $image"
