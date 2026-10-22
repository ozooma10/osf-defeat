#requires -Version 7.2
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$ControlFile,
    [Parameter(Mandatory)][ValidateSet('Save','Restore','ReloadCheck','Probe')][string]$Action,
    [ValidatePattern('^osfworld_[a-z0-9_]+$')][string]$Name,
    [string]$Checkpoint,
    [ValidateSet('Inspect','Away','Returned','ObserveResolution')][string]$Observation='Inspect',
    [switch]$Retrieved
)
# Background-only operations in an already owned world session. No input, focus,
# teleport, actor mutation, launch, or automatic retries of a timed-out command.
$repo=Split-Path $PSScriptRoot -Parent
$harness=Join-Path (Split-Path $repo -Parent) 'OSF Test Harness'
. (Join-Path $harness 'scripts/Common.ps1')
$control=Read-JsonFile ([IO.Path]::GetFullPath($ControlFile))
if (-not $control -or -not $control.ready) { throw 'Need a ready world session control.json.' }
$manifest=Read-JsonFile $control.manifest
$profile=Assert-ChildPath (Join-Path $script:Config.MO2 'profiles') $control.profile
$owner=Read-JsonFile (Join-Path $profile '.osf-defeat-shadows-world.json')
if (-not $owner -or $owner.project -cne $repo) { throw 'World profile belongs to another project.' }
$script:Run=$control.run
$script:StateRoot=Assert-ChildPath $script:Run.path $control.stateRoot
$script:SaveRoot=Assert-ChildPath $manifest.saves $control.saveRoot
$script:ProfileRoot=$profile
if (-not (Get-OwnedGame)) { throw 'Owned game is not running.' }
$state=Get-GameState
if (-not $state.loaded -or $state.snapshotStale) { throw 'Need fresh loaded gameplay.' }
if ('MessageBoxMenu' -in $state.menus -or ($Action -in @('Restore','ReloadCheck') -and $state.pauseRequestCount -gt 0)) { throw 'Close the modal/pause menu before this operation; background checks never dismiss UI.' }
if ($Action -eq 'ReloadCheck' -and $Observation -notin @('Inspect','Away')) { throw 'ReloadCheck supports only Inspect or Away; Returned and death observations are separate steps.' }
$operation=[DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss-fff')+'-'+$Action+'-'+[guid]::NewGuid().ToString('N').Substring(0,8)
$evidence=Assert-ChildPath $script:Run.path (Join-Path $script:Run.path "checkpoints/$operation")
[IO.Directory]::CreateDirectory($evidence) | Out-Null
$nativeLog=Join-Path $script:GameDocuments 'SFSE/Logs/OSF Defeat.log'
$persistenceLog=Join-Path $script:GameDocuments 'SFSE/Logs/OSF Animation.log'
$worldLog=Join-Path $script:GameDocuments 'Logs/Script/User/OSFDefeatShadowsWorld.0.log'
$timer=[Diagnostics.Stopwatch]::StartNew()
$result=@{action=$Action;outcome='running';worldAcceptanceVerified=$false;sessionId=$state.sessionId;pid=$state.pid;processStartFileTime=$state.processStartFileTime;control=[IO.Path]::GetFullPath($ControlFile)}

function Read-Log([string]$Path) {
    if (Test-Path -LiteralPath $Path) { return [string](Get-Content -LiteralPath $Path -Raw) }
    return ''
}
function Read-FreshLog([string]$Path, [string]$Before) {
    $now=Read-Log $Path
    if (-not $now.StartsWith($Before,[StringComparison]::Ordinal)) { throw 'Log rotated or changed during operation; no result inferred.' }
    return $now.Substring($Before.Length)
}
function Save-WorldCheckpoint {
    if (-not $Name) { throw 'Save and ReloadCheck require a new -Name.' }
    foreach ($extension in @('sfs','osf','sfse')) {
        if (Test-Path -LiteralPath (Join-Path $script:SaveRoot "$Name.$extension")) { throw 'Checkpoint already exists; use a new name.' }
    }
    $before=Read-Log $nativeLog
    $null=Invoke-ConsoleCommand "save $Name"
    $native=Wait-Observed 'fresh native save callback' {
        $text=Read-FreshLog $nativeLog $before
        if ($text -notmatch '\[OSFDefeat\] saved state:' -and 'MessageBoxMenu' -in (Get-GameState).menus) { throw '[blocked] Game opened a modal during save; inspect it before another command.' }
        $text
    } {param($s) $s -match '\[OSFDefeat\] saved state:'} 30
    $null=Wait-Observed 'complete checkpoint files' {
        @('sfs','osf','sfse' | ForEach-Object { $p=Join-Path $script:SaveRoot "$Name.$_"; (Test-Path -LiteralPath $p) -and (Get-Item -LiteralPath $p).Length -gt 0 })
    } {param($s) @($s | Where-Object { -not $_ }).Count -eq 0} 30
    $files=@(foreach ($extension in @('sfs','osf','sfse')) {
        $path=Join-Path $script:SaveRoot "$Name.$extension"
        # Require stable contents across the copy, not only file existence.
        $hash=(Get-FileHash -LiteralPath $path).Hash
        $copy=Join-Path $evidence ([IO.Path]::GetFileName($path))
        Copy-Item -LiteralPath $path -Destination $copy
        if ($hash -cne (Get-FileHash -LiteralPath $copy).Hash -or $hash -cne (Get-FileHash -LiteralPath $path).Hash) { throw 'Save still changed during copy; retained evidence is incomplete.' }
        @{name=[IO.Path]::GetFileName($path);sha256=$hash;bytes=(Get-Item -LiteralPath $path).Length}
    })
    $saved=@{name=$Name;saveRoot=$script:SaveRoot;sessionId=$state.sessionId;manifest=$control.manifest;files=$files;state=(Get-GameState);native=$native}
    $path=Join-Path $evidence 'checkpoint.json'
    Write-JsonFile $path $saved
    return $path
}
function Restore-WorldCheckpoint([string]$Path) {
    if (-not $Path) { throw 'Restore requires -Checkpoint checkpoint.json.' }
    $ready=Get-GameState
    if ($ready.pauseRequestCount -gt 0 -or 'MessageBoxMenu' -in $ready.menus) { throw '[blocked] A pause/modal menu blocks reload; dismiss it before a new operation.' }
    $saved=Read-JsonFile ([IO.Path]::GetFullPath($Path))
    if (-not $saved -or $saved.saveRoot -cne $script:SaveRoot -or $saved.sessionId -cne $state.sessionId -or $saved.name -notmatch '^osfworld_[a-z0-9_]+$') { throw 'Checkpoint must belong to this owned session; use prepare-shadows-world.ps1 for a cold start.' }
    if (@(Compare-Object @('osf','sfs','sfse') @($saved.files | ForEach-Object { [IO.Path]::GetExtension($_.name).TrimStart('.') })).Count) { throw 'Incomplete checkpoint inventory.' }
    foreach ($file in $saved.files) {
        $source=Assert-ChildPath $script:SaveRoot (Join-Path $script:SaveRoot $file.name)
        if ($file.name -cne ($saved.name+[IO.Path]::GetExtension($file.name)) -or (Get-FileHash -LiteralPath $source).Hash -cne $file.sha256) { throw 'Checkpoint changed; refusing reload.' }
    }
    $before=Read-Log $nativeLog
    $beforePersistence=Read-Log $persistenceLog
    $loadedPattern="sidecar load '"+[regex]::Escape($saved.name)+"\.osf': parsed \(blocks=\d+, loaded=\d+, unknown=\d+, corrupt=0, callbackFailures=0\)"
    $null=Invoke-ConsoleCommand "load $($saved.name)"
    $result.loadCallbacks=Wait-Observed 'fresh native revert and load callbacks' {
        if ('MessageBoxMenu' -in (Get-GameState).menus) { throw '[blocked] Game opened a modal during reload; inspect it before another command.' }
        @{defeat=(Read-FreshLog $nativeLog $before);persistence=(Read-FreshLog $persistenceLog $beforePersistence)}
    } {param($s) $s.defeat -match 'reverted all runtime defeat state \(epoch' -and $s.persistence -match $loadedPattern} 60
    $result.loadedState=Wait-Observed 'loaded checkpoint gameplay' { Get-GameState } {param($s) $s.loaded -and -not $s.snapshotStale -and $s.sessionId -ceq $state.sessionId -and 'LoadingMenu' -notin $s.menus -and $s.cellFormId -eq $saved.state.cellFormId} 60
    $result.checkpoint=[IO.Path]::GetFullPath($Path)
}
function Invoke-WorldObservation {
    # Resolve the quest ID from a fresh response in this load order.
    $before=Read-Log $worldLog
    $null=Invoke-ConsoleCommand 'cgf "OSFDefeatShadowsWorldProbe.StartProbe"'
    $bootstrap=Wait-Observed 'probe bootstrap' {
        $now=Read-Log $worldLog
        if ($now.StartsWith($before,[StringComparison]::Ordinal)) { return $now.Substring($before.Length) }
        # OpenUserLog rotates once when first opened after loading a save. Accept
        # that boundary only when the previous bytes survive in the next rotation.
        # Other observations still require append-only logs.
        $previous=Read-Log ($worldLog -replace '\.0\.log$','.1.log')
        if ($before.Length -gt 0 -and $previous.StartsWith($before,[StringComparison]::Ordinal) -and $now -match '^\[\d\d:\d\d:\d\d\] OSFDefeatShadowsWorld log opened') {
            [IO.File]::WriteAllText((Join-Path $evidence 'bootstrap-previous.log'),$previous)
            return $now
        }
        throw 'Unexpected probe log replacement; no result inferred.'
    } {param($s) $s -match 'PROBE\|id=\d+\|running=true' -and $s -match '(WATCH-DEATH\||BLOCKED\|Death observation requires)'} 15
    $questID=[regex]::Match($bootstrap,'PROBE\|id=(\d+)\|running=true').Groups[1].Value
    $questHex='{0:X8}' -f [int]$questID
    $before=Read-Log $worldLog
    $suffix=if ($Observation -eq 'ObserveResolution' -and $Retrieved) { ' True' } else { '' }
    $null=Invoke-ConsoleCommand "cqf $questHex $Observation$suffix"
    $end= switch ($Observation) {
        Inspect { 'VISUAL-REQUIRED\|' }
        Away { '\|AWAY\|' }
        Returned { 'VISUAL-REQUIRED\|' }
        ObserveResolution { if ($Retrieved) { '\|PASS\|stolen-weapon-retrieved' } else { '\|PASS\|death-reward-matches-engine-killer' } }
    }
    $observed=Wait-Observed "$Observation result" { Read-FreshLog $worldLog $before } {param($s) $s -match $end -or $s -match '\|(FAIL|BLOCKED)\|'} 20
    $result.probe=$observed
    [IO.File]::WriteAllText((Join-Path $evidence 'probe.log'),$observed)
    if ($observed -match '\|(FAIL|BLOCKED)\|') { throw 'World observation failed or blocked; see retained probe.log.' }
    $result.assertions=@([regex]::Matches($observed,'\|PASS\|([^\r\n]+)') | ForEach-Object { $_.Groups[1].Value })
}
$operationLock=[IO.File]::Open((Join-Path $script:Run.path 'checkpoint.lock'),'OpenOrCreate','ReadWrite','None')
try {
    switch ($Action) {
        Save { $result.checkpoint=Save-WorldCheckpoint }
        Restore { Restore-WorldCheckpoint $Checkpoint }
        ReloadCheck {
            # Fail before saving if the selected observation is already invalid.
            Invoke-WorldObservation
            $result.beforeReload=$result.probe
            $result.checkpoint=Save-WorldCheckpoint
            Restore-WorldCheckpoint $result.checkpoint
            Invoke-WorldObservation
        }
        Probe { Invoke-WorldObservation }
    }
    $result.outcome='passed'
} catch {
    $result.outcome=if ($_.Exception.Message -like '[[]blocked]*') { 'blocked' } else { 'failed' }
    $result.error=$_.Exception.Message
    throw
} finally {
    try {
        $result.elapsedSeconds=[Math]::Round($timer.Elapsed.TotalSeconds,2)
        $result.finishedAt=[DateTimeOffset]::UtcNow.ToString('o')
        Write-JsonFile (Join-Path $evidence 'result.json') $result
        Write-Host "World checkpoint $($result.outcome): $evidence ($($result.elapsedSeconds)s)"
    } finally { $operationLock.Dispose() }
}
