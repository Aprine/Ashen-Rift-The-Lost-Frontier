param([string]$EngineRoot = 'D:\Epic Games\UE_4.27')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$editorFile = Join-Path $EngineRoot 'Engine\Binaries\Win64\UE4Editor-Cmd.exe'
$projectFile = Join-Path $projectRoot 'AshenRift.uproject'
foreach ($suite in @('Core', 'Integration.RunLoop', 'Integration.Progression', 'Integration.RandomDungeon', 'Integration.Camp', 'Integration.EnemyVariants', 'Integration.UserInterface', 'Integration.CombatFeedback')) {
    $reportDir = Join-Path $projectRoot ('Saved\TestReport_' + $suite.Replace('.', '_'))
    $logFile = Join-Path $projectRoot ('Saved\Test_' + $suite.Replace('.', '_') + '.log')
    $testArgs = @($projectFile)
    if ($suite -eq 'Integration.RunLoop') { $testArgs += @('/Game/AshenRift/Maps/L_FirstPlayable', '-game', '-ARTestProfile') }
    if ($suite -eq 'Integration.Progression') { $testArgs += @('/Game/AshenRift/Maps/L_Progression', '-game', '-ARTestProfile') }
    if ($suite -eq 'Integration.RandomDungeon') { $testArgs += @('/Game/AshenRift/Maps/L_RandomDungeon', '-game', '-ARTestProfile') }
    if ($suite -eq 'Integration.Camp') { $testArgs += @('/Game/AshenRift/Maps/L_RandomDungeon', '-game', '-ARTestProfile') }
    if ($suite -eq 'Integration.EnemyVariants') { $testArgs += @('/Game/AshenRift/Maps/L_RandomDungeon', '-game', '-ARTestProfile') }
    if ($suite -eq 'Integration.UserInterface') { $testArgs += @('/Game/AshenRift/Maps/L_RandomDungeon', '-game', '-ARTestProfile') }
    if ($suite -eq 'Integration.CombatFeedback') { $testArgs += @('/Game/AshenRift/Maps/L_RandomDungeon', '-game', '-ARTestProfile') }
    $testArgs += @('-unattended', '-nop4', '-nosplash', '-nullrhi', "-ExecCmds=Automation RunTests AshenRift.$suite", '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$reportDir", '-stdout', '-FullStdOutLogOutput')
    $startedAt = Get-Date
    & $editorFile @testArgs 1> $logFile 2>&1
    $testExitCode = $LASTEXITCODE
    $reportFile = Join-Path $reportDir 'index.json'
    if ($testExitCode -ne 0 -or -not (Test-Path -LiteralPath $reportFile)) { throw "Test process failed: $suite. See $logFile" }
    if ((Get-Item -LiteralPath $reportFile).LastWriteTime -lt $startedAt.AddSeconds(-2)) { throw "Stale test report: $suite" }
    $report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
    $expected = if ($suite -eq 'Core') { 3 } else { 1 }
    if ($report.failed -ne 0 -or $report.notRun -ne 0 -or $report.succeeded -ne $expected) { throw "Test assertions failed: $suite. See $reportFile" }
    Write-Output "$suite passed ($expected tests)."
}
