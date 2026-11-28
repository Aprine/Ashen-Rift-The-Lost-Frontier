param(
    [string]$EngineRoot = 'D:\Epic Games\UE_4.27',
    [int]$Seed = 9,
    [ValidateSet(1, 2, 3)][int[]]$Tier = @(1, 2, 3),
    [ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Label = 'current'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $PSScriptRoot
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UE4Editor-Cmd.exe'
$project = Join-Path $projectRoot 'AshenRift.uproject'
$outputDir = Join-Path $projectRoot "Saved\RuinsBalance_$Label"
New-Item -ItemType Directory -Path $outputDir -Force | Out-Null

$cases = foreach ($level in $Tier) {
    foreach ($role in @('Warrior', 'Mage')) {
        foreach ($route in @('Blue', 'Gold')) {
            $filePrefix = "$Seed-$level-$role-$route"
            $log = Join-Path $outputDir "$filePrefix.log"
            $reportDir = Join-Path $outputDir $filePrefix
            $arguments = @(
                $project, '/Game/AshenRift/Maps/Map_Hub', '-game', '-ARTestProfile',
                "-ARExpeditionSeed=$Seed", "-ARBalanceTier=$level", '-UseFixedTimeStep', '-FPS=30',
                '-unattended', '-nop4', '-nosplash', '-nullrhi', '-stdout', '-FullStdOutLogOutput',
                '-ExecCmds=Automation RunTests AshenRift.Balance.Ruins',
                '-TestExit=Automation Test Queue Empty', "-ReportExportPath=$reportDir"
            )
            if ($role -eq 'Mage') { $arguments += '-ARBalanceMage' }
            if ($route -eq 'Gold') { $arguments += '-ARBalanceGold' }

            $startedAt = Get-Date
            & $editor @arguments *> $log
            $exitCode = $LASTEXITCODE
            $reportFile = Join-Path $reportDir 'index.json'
            if ($exitCode -ne 0 -or -not (Test-Path -LiteralPath $reportFile) -or
                (Get-Item -LiteralPath $reportFile).LastWriteTime -lt $startedAt.AddSeconds(-2)) {
                throw "The engine did not write a new report for $filePrefix. See $log"
            }
            $report = Get-Content -LiteralPath $reportFile -Raw | ConvertFrom-Json
            $sample = @(Select-String -LiteralPath $log -Pattern 'LogTemp: Display: AR_RUINS_SAMPLE (.+)$')
            if ($report.succeeded -ne 1 -or $report.failed -ne 0 -or $report.notRun -ne 0 -or $sample.Count -ne 1) {
                throw "Missing or invalid sample for $filePrefix. See $log"
            }
            Write-Host $sample[0].Matches[0].Value
            $fields = [ordered]@{}
            foreach ($field in $sample[0].Matches[0].Groups[1].Value.Split(' ')) {
                $key, $value = $field.Split('=', 2)
                $fields[$key] = $value
            }
            if ($fields.seed -ne "$Seed" -or $fields.class -ne $role -or $fields.tier -ne "$level" -or $fields.route -ne $route) {
                throw "Sample identifiers do not match $filePrefix."
            }
            [pscustomobject]$fields
        }
    }
}

$csv = Join-Path $outputDir "samples-$Seed-$($Tier -join '-').csv"
$cases | Export-Csv -LiteralPath $csv -NoTypeInformation -Encoding UTF8
Write-Output $csv
