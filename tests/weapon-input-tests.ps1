# Keep every fixed matrix input within the real helper's 40..1000 ms bound.
# Mock the I/O only; exercise the production action dispatcher and reject names
# outside its allowlist, without starting a game or sending desktop input.
function Test-WeaponInputBounds {
    . (Join-Path $root 'tools/RuntimeActions.ps1')
    function Invoke-Input([string]$Command,[string[]]$Arguments) {
        if ($Command -eq 'focus') { return }
        if ($Command -ne 'click') { throw 'Unexpected matrix input command.' }
        $index=[Array]::IndexOf($Arguments,'--hold-ms')
        if ($index -lt 0 -or [int]$Arguments[$index+1] -lt 40 -or [int]$Arguments[$index+1] -gt 1000) { throw 'Invalid mouse hold duration.' }
    }
    function Send-Key([int]$Key,[int]$Hold=100) {
        if ($Key -notin @(71,82,86) -or $Hold -lt 40 -or $Hold -gt 1000) { throw 'Invalid matrix keyboard input.' }
    }
    function Start-Sleep { param([int]$Milliseconds) }
    function Write-JsonFile($Path,$Value) {
        if ($Value.ok -ne $true -or $Value.id -ne 1) { throw 'Missing action acknowledgement.' }
    }
    $script:DefeatMailbox=Join-Path $root 'build/unused-weapon-input-contract'
    $snapshot=@{pid=1;processStartFileTime='2';launchToken='test';runId='test';action=@{id=1;kind='weapon-attack';name=''}}
    $inventory=@(Get-Content (Join-Path $root 'tests/runtime/weapon-cases.json') -Raw | ConvertFrom-Json)
    foreach ($case in $inventory | Where-Object {$_ -notin @('healthy-baseline','weapons-healthy-save','weapons-fixture','weapons-passive-controls','weapons-cleanup-load')}) {
        $snapshot.action.name=$case.Substring('weapons-'.Length)
        Invoke-DefeatRuntimeAction $snapshot
        $script:checks++
    }
    $snapshot.action.name='unknown'; Reject { Invoke-DefeatRuntimeAction $snapshot }
    $snapshot.action.kind='weapon-passive'; Reject { Invoke-DefeatRuntimeAction $snapshot }
    $snapshot.action.name='reload-aim'; Invoke-DefeatRuntimeAction $snapshot; $script:checks++
}
Test-WeaponInputBounds
