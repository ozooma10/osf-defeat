#requires -Version 7.2
[CmdletBinding()]
param(
    [switch]$RunGame,
    [switch]$Plan,
    [ValidateSet('debug','releasedbg')][string]$Mode = 'releasedbg',
    [string]$Harness = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'OSF Test Harness'),
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'TestSupport.ps1')
$coverage = @(Get-Content (Join-Path $repo 'tests/coverage.json') -Raw | ConvertFrom-Json)
$suites = @($coverage.suite | Sort-Object -Unique)
$runtimeCases = @(Get-Content (Join-Path $repo 'tests/runtime/cases.json') -Raw | ConvertFrom-Json)
Test-DefeatInventory $repo $coverage $runtimeCases
$stages = @('configure','native-build') + @($suites | ForEach-Object { "native-$_" }) + @('runner-contracts','payload','production-build','runtime','source-unchanged')
if ($Plan) {
    @{stages=$stages;mode=$Mode;runGame=[bool]$RunGame;coverage=$coverage;runtimeCases=$runtimeCases} | ConvertTo-Json -Depth 8
    exit 0
}
$directory = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else {
    Join-Path $repo ('build/smoke/' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
}
if (Test-Path -LiteralPath $directory) { throw 'Use a new output directory; previous evidence is never overwritten.' }
[IO.Directory]::CreateDirectory($directory) | Out-Null
$report = [ordered]@{
    schemaVersion=1;startedAt=[DateTimeOffset]::UtcNow.ToString('o');finishedAt=$null;outcome='running'
    scope=$(if ($RunGame) {'offline+runtime'} else {'offline'});automatedPassed=$false;runtimeVerified=$false
    source=$null;stages=@();caseCount=0;checkCount=0;coverage=$coverage
    unverified=@($coverage.limits) + @(if (-not $RunGame) {'All in-game checks skipped; use -RunGame.'})
}
$exitCode=2; $lock=$null; $oldSkip=$env:OSF_DEFEAT_SKIP_DEPLOY
function Save-Report {
    $report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath (Join-Path $directory 'summary.json') -Encoding utf8
    $lines=@('# OSF Defeat automated tests','',"Outcome: $($report.outcome)","Scope: $($report.scope)","Cases: $($report.caseCount); checks: $($report.checkCount)",'', '| Stage | Result | Log |','| --- | --- | --- |')
    foreach ($stage in $report.stages) { $lines += "| $($stage.name) | $($stage.status) | [$($stage.name)]($($stage.name).log) |" }
    $lines += @('','## Remaining acceptance boundaries','') + @($report.unverified | ForEach-Object { "- $_" })
    $lines | Set-Content -LiteralPath (Join-Path $directory 'report.md') -Encoding utf8
}
function Run-Stage([string]$Name,[scriptblock]$Body) {
    $entry=[ordered]@{name=$Name;status='running';seconds=0;error=$null}
    $report.stages+=,$entry; Save-Report; $watch=[Diagnostics.Stopwatch]::StartNew()
    Write-Host "Defeat tests: $Name"
    try { & $Body *>&1 | Tee-Object -FilePath (Join-Path $directory "$Name.log") | Out-Host; $entry.status='passed' }
    catch { $entry.status=if ($_.Exception.Message.StartsWith('[failed]')) {'failed'} else {'blocked'}; $entry.error=$_.Exception.Message; throw }
    finally { $entry.seconds=[Math]::Round($watch.Elapsed.TotalSeconds,3); Save-Report }
}
function Run-Command([string]$Program,[string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE) { throw "[failed] $Program exited $LASTEXITCODE" }
}
Push-Location $repo
try {
    [IO.Directory]::CreateDirectory((Join-Path $repo 'build')) | Out-Null
    $lock=[IO.File]::Open((Join-Path $repo 'build/tests.lock'),'OpenOrCreate','ReadWrite','None')
    $identity=Get-DefeatSourceIdentity $repo; $report.source=$identity
    $env:OSF_DEFEAT_SKIP_DEPLOY='1'
    Run-Stage configure { Run-Command xmake @('f','-p','windows','-a','x64','-m',$Mode,'-y') }
    Run-Stage native-build { Run-Command xmake @('build','-y','osfdefeat-tests') }
    foreach ($suite in $suites) {
        Run-Stage "native-$suite" {
            $expected=@([regex]::Matches((Get-Content (Join-Path $repo "tests/native/${suite}_tests.cpp") -Raw),'(?m)^TEST\((\w+)\)') | ForEach-Object { $_.Groups[1].Value })
            $result=Join-Path $directory "$suite.json"
            Run-Command (Join-Path $repo "build/tests/osfdefeat-$suite-tests.exe") @('--report',$result)
            $cases=Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
            Test-DefeatCaseReport $cases $expected
            $report.caseCount+=@($cases.cases).Count; $report.checkCount+=$cases.checks
        }
    }
    Run-Stage runner-contracts { Run-Command pwsh @('-NoProfile','-File',(Join-Path $repo 'tests/runner-tests.ps1')) }
    Run-Stage payload { Run-Command pwsh @('-NoProfile','-File',(Join-Path $repo 'scripts/verify.ps1')) }
    Run-Stage production-build { Run-Command xmake @('build','-y','OSF Defeat') }
    if ($RunGame) {
        Run-Stage runtime {
            $runtime=Join-Path $directory 'runtime.json'
            & pwsh -NoProfile -File (Join-Path $PSScriptRoot 'test-runtime.ps1') -Harness $Harness -ResultPath $runtime -SourceHash $identity.sha256
            $code=$LASTEXITCODE
            if ($code -ne 0) { throw "$(if ($code -eq 1) {'[failed]'} else {'[blocked]'}) Runtime suite exited $code; see $runtime" }
            $receipt=Get-Content $runtime -Raw | ConvertFrom-Json
            if ($receipt.outcome -ne 'passed' -or $receipt.sourceHash -cne $identity.sha256) { throw '[blocked] Runtime receipt does not match source.' }
            $report.runtimeVerified=$true
        }
    } else { $report.stages+=@{name='runtime';status='skipped';seconds=0;error=$null} }
    Run-Stage source-unchanged {
        if ((Get-DefeatSourceIdentity $repo).sha256 -cne $identity.sha256) { throw '[blocked] Source changed during the run.' }
    }
    $report.outcome='passed'; $report.automatedPassed=$true; $exitCode=0
} catch {
    $report.outcome=if ($_.Exception.Message.StartsWith('[failed]')) {'failed'} else {'blocked'}
    $report['error']=$_.Exception.Message; Write-Warning $_.Exception.Message
    $exitCode=if ($report.outcome -eq 'failed') {1} else {2}
} finally {
    $env:OSF_DEFEAT_SKIP_DEPLOY=$oldSkip; if ($lock) {$lock.Dispose()}; Pop-Location
    $report.finishedAt=[DateTimeOffset]::UtcNow.ToString('o'); Save-Report
    Write-Host "Report: $(Join-Path $directory 'summary.json')"
}
exit $exitCode
