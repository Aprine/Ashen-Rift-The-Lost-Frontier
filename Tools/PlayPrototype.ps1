param([string]$EngineRoot = 'D:\Epic Games\UE_4.27')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$editorFile = Join-Path $EngineRoot 'Engine\Binaries\Win64\UE4Editor.exe'
$projectFile = Join-Path $projectRoot 'AshenRift.uproject'
if (-not (Test-Path -LiteralPath $editorFile)) { throw "UE4 editor not found: $editorFile" }
if (-not (Test-Path -LiteralPath (Join-Path $projectRoot 'Binaries\Win64\UE4Editor-AshenRift.dll'))) {
    throw 'Build the editor module with Tools/BuildProject.ps1 first.'
}
# This is an interactive game window using the local UE4 installation.
$previewArgs = '"{0}" /Game/AshenRift/Maps/L_FrontierDungeon -game -windowed -ResX=1280 -ResY=720' -f $projectFile
Start-Process -FilePath $editorFile -ArgumentList $previewArgs
