param(
    [string]$EngineRoot = 'D:\Epic Games\UE_4.27',
    [ValidateSet('Development','DebugGame','Shipping')][string]$Configuration = 'Development',
    [switch]$Game
)
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$buildFile = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
$projectFile = Join-Path $projectRoot 'AshenRift.uproject'
$targetName = if ($Game) { 'AshenRift' } else { 'AshenRiftEditor' }
if (-not (Test-Path -LiteralPath $buildFile)) { throw "UE4 build tool missing: $buildFile" }
if (-not $Game -and $Configuration -eq 'Shipping') { throw 'Editor does not support Shipping.' }
New-Item -ItemType Directory -Path (Join-Path $projectRoot 'Saved') -Force | Out-Null
& $buildFile $targetName Win64 $Configuration $projectFile -WaitMutex -NoHotReloadFromIDE 2>&1 |
    Tee-Object -FilePath (Join-Path $projectRoot 'Saved\build_cpp.log')
$buildResult = $LASTEXITCODE
if ($buildResult -ne 0) { throw "UE4 compilation failed with exit code $buildResult. See Saved/build_cpp.log." }
