#requires -Version 7.2
$ErrorActionPreference='Stop'
. (Join-Path (Split-Path $PSScriptRoot -Parent) 'tools/TestSupport.ps1')
$checks=0
function Reject([scriptblock]$Body) { $rejected=$false; try { & $Body } catch { $rejected=$true }; if (-not $rejected) { throw 'Invalid evidence was accepted.' }; $script:checks++ }
$valid=@{schemaVersion=1;passed=$true;cases=@(@{name='one';passed=$true;checks=1})}
Test-DefeatCaseReport $valid @('one'); $checks++
Reject { Test-DefeatCaseReport $valid @() }
Reject { Test-DefeatCaseReport $valid @('other') }
$valid.cases+=@{name='one';passed=$true;checks=1}; Reject { Test-DefeatCaseReport $valid @('one','two') }
$valid.cases=@(@{name='one';passed='true';checks=1}); Reject { Test-DefeatCaseReport $valid @('one') }
$valid.cases=@(@{name='one';passed=$true;checks=0}); Reject { Test-DefeatCaseReport $valid @('one') }
$identity=@{pid=10;processStartFileTime='123'}
$snapshot=@{schemaVersion=1;pid=10;processStartFileTime='123';launchToken='token';runId='run';updatedAtUtcMs=[DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()}
Test-DefeatRuntimeSnapshot $snapshot $identity 'token' 'run'; $checks++
Reject { Test-DefeatRuntimeSnapshot $snapshot $identity 'wrong' 'run' }
Reject { Test-DefeatRuntimeSnapshot $snapshot $identity 'token' 'old' }
$snapshot.pid=11; Reject { Test-DefeatRuntimeSnapshot $snapshot $identity 'token' 'run' }; $snapshot.pid=10
$snapshot.updatedAtUtcMs=0; Reject { Test-DefeatRuntimeSnapshot $snapshot $identity 'token' 'run' }
$root=Split-Path $PSScriptRoot -Parent
$coverage=Get-Content (Join-Path $PSScriptRoot 'coverage.json') -Raw | ConvertFrom-Json
$runtime=@(Get-Content (Join-Path $PSScriptRoot 'runtime/cases.json') -Raw | ConvertFrom-Json)
Test-DefeatInventory $root $coverage $runtime; $checks++
Reject { Test-DefeatInventory $root @($coverage | Where-Object suite -ne 'damage') $runtime }
Reject { Test-DefeatInventory $root $coverage @('fake') }
Reject { Test-DefeatInventory $root $coverage @($runtime + $runtime[0]) }
. (Join-Path $PSScriptRoot 'cosave-evidence-tests.ps1')
. (Join-Path $PSScriptRoot 'ini-replacement-tests.ps1')
. (Join-Path $PSScriptRoot 'weapon-input-tests.ps1')
. (Join-Path $root 'tools/BlackoutTestGates.ps1')
$gateRoot=Join-Path $root ('build/gate-contract-'+[guid]::NewGuid().ToString('N'))
$gateSource=Join-Path $root 'shadows/Scripts/Source/OSFDefeatShadowsAftermath.psc'
$gated=Join-Path $gateRoot 'OSFDefeatShadowsAftermath.psc'
New-BlackoutTestSource $gateSource $gated
$gateText=[IO.File]::ReadAllText($gated)
if ([regex]::Matches($gateText,'While IsCurrent\(run\) && OSFDefeatAcceptanceProbe.HoldBlackout\([23]\)').Count -ne 2) { throw 'Checkpoint gates missing.' }; $checks++
$withoutGates=[regex]::Replace($gateText,'    While IsCurrent\(run\) && OSFDefeatAcceptanceProbe.HoldBlackout\([23]\)\n        Utility.Wait\(0.1\)\n    EndWhile\n','')
if ($withoutGates -cne [IO.File]::ReadAllText($gateSource).Replace("`r`n","`n")) { throw 'Instrumentation altered production behavior beyond checkpoint suspension.' }; $checks++
$invalid=Join-Path $gateRoot 'changed.psc'
[IO.File]::WriteAllText($invalid,$withoutGates.Replace('    Phase = 2','    Phase = 9'))
Reject { New-BlackoutTestSource $invalid (Join-Path $gateRoot 'invalid-output.psc') }
Write-Host "$checks runner evidence checks passed."
