param([string]$Engine = 'C:\Program Files\Epic Games\UE_5.6')
$project = Join-Path (Split-Path $PSScriptRoot -Parent) 'PortSim.uproject'
$editor = Join-Path $Engine 'Engine\Binaries\Win64\UnrealEditor.exe'
& $editor $project /Engine/Maps/Entry -game -windowed -ResX=1440 -ResY=900 -dx11 -nosplash '-ExecCmds=sg.ViewDistanceQuality 1,sg.ShadowQuality 1,sg.PostProcessQuality 1,sg.TextureQuality 1,sg.EffectsQuality 1,r.DynamicGlobalIlluminationMethod 0,r.ReflectionMethod 0'
