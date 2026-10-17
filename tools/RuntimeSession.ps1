function Move-DefeatPrivateFile([string]$Source, [string]$Destination, [int]$RetryMs = 5000) {
    $until=[DateTime]::UtcNow.AddMilliseconds($RetryMs)
    while ($true) {
        try { [IO.File]::Move($Source,$Destination,$true); return }
        catch {
            # PowerShell wraps static .NET failures. On Windows, a reader that
            # denies delete can surface as ACCESS_DENIED rather than SHARING.
            $cause=$_.Exception.GetBaseException()
            $locked=($cause -is [IO.IOException] -or $cause -is [UnauthorizedAccessException]) -and
                ($cause.HResult -band 0xffff) -in @(5,32,33)
            if (-not $locked -or [DateTime]::UtcNow -ge $until) { throw }
            Start-Sleep -Milliseconds 100
        }
    }
}

# Dot-sourced by test-runtime.ps1; every launch uses a new mailbox and token.
function Invoke-DefeatRuntimeSession([string[]]$RequiredMods, [string]$Label) {
    $script:AnimationMode=$true
    try { $null=Start-TestGame -RequiredTestMods $RequiredMods -AllowProductionSettings }
    finally { $script:AnimationMode=$false }
    $identity=Get-ProcessIdentity (Get-OwnedGame)
    $snapshotPath=Join-Path $script:DefeatMailbox 'snapshot.json'
    $snapshot=Wait-Observed 'Defeat test-build handshake' {Read-JsonFile $snapshotPath} {param($s) $s -and $s.state -in @('ready','failed')} 20
    if ($snapshot.state -eq 'failed') { throw "[blocked] Runtime preflight: $($snapshot.error)" }
    Test-DefeatRuntimeSnapshot $snapshot $identity $script:DefeatLaunchToken
    if ($script:BlackoutColdStart) {
        # Auto Load has already proven that only the blackout seed was visible.
        # Add the retained healthy pair now, solely for the final cleanup load.
        $healthy=Join-Path $script:Run.path 'saves/osfdefeathealthy'
        $null=Read-DefeatCosave (Join-Path $healthy 'osfdefeathealthy.osf') @{}
        Copy-Item -LiteralPath (Join-Path $healthy 'osfdefeathealthy.sfs'),(Join-Path $healthy 'osfdefeathealthy.osf') -Destination $script:SaveRoot
    }
    $null=Save-Capture "$Label-before"
    $runId=[guid]::NewGuid().ToString('N')
    Write-JsonFile (Join-Path $script:DefeatMailbox 'start.json') @{pid=$identity.pid;processStartFileTime=$identity.processStartFileTime;launchToken=$script:DefeatLaunchToken;runId=$runId;expiresAtUtcMs=[DateTimeOffset]::UtcNow.AddSeconds(15).ToUnixTimeMilliseconds()}
    $limit=[DateTimeOffset]::UtcNow.AddSeconds(640)
    $lastAction=0
    do {
        Start-Sleep -Milliseconds 500
        if (-not (Get-OwnedGame)) { throw '[failed] Owned game exited during Defeat tests.' }
        $snapshot=Read-JsonFile $snapshotPath
        if ($snapshot.state -eq 'ready') { if ([DateTimeOffset]::UtcNow -ge $limit) { throw '[blocked] Start request was not consumed.' }; continue }
        Test-DefeatRuntimeSnapshot $snapshot $identity $script:DefeatLaunchToken $runId
        if ($snapshot.ContainsKey('action') -and $snapshot.action.id -gt $lastAction) {
            Write-Host "Defeat acceptance: $($snapshot.action.kind) $($snapshot.action.name)"
            Invoke-DefeatRuntimeAction $snapshot
            $lastAction=$snapshot.action.id
        }
        if ($snapshot.state -in @('passed','failed')) { break }
        if ([DateTimeOffset]::UtcNow -ge $limit) { throw '[blocked] Runtime suite timed out.' }
    } while ($true)
    Write-JsonFile (Join-Path $script:Run.path "$Label-result.json") $snapshot
    $null=Save-Capture "$Label-after"
    if ($snapshot.state -ne 'passed') { throw "[failed] Runtime assertion: $($snapshot.error)" }
    return @{snapshot=$snapshot;identity=$identity;launchToken=$script:DefeatLaunchToken}
}

function Initialize-DefeatBlackoutRestart([int]$Checkpoint, $FirstSnapshot) {
    if (@(Get-GameProcess).Count) { throw '[blocked] Game is still running before cold restart preparation.' }
    # MO2 can flush/lock its profile after the game exits. Close our idle launcher
    # before changing the next process's save directory, as at initial setup.
    $moExecutable=Join-Path $script:Config.MO2 'ModOrganizer.exe'
    foreach ($mo in @(Get-Process -Name ModOrganizer -ErrorAction SilentlyContinue | Where-Object Path -eq $moExecutable)) {
        $null=$mo.CloseMainWindow()
        if (-not $mo.WaitForExit(10000)) { throw '[blocked] Idle MO2 did not close before cold restart.' }
    }
    $name=@{1='osfdefeatblackout';2='osfdefeattheft';3='osfdefeattime'}[$Checkpoint]
    if (-not $name) { throw 'Unknown blackout checkpoint.' }
    $pair=Join-Path $script:Run.path "saves/$name"
    $inspection=Read-JsonFile (Join-Path $pair 'inspection.json')
    $save=Join-Path $pair "$name.sfs"
    $sidecar=Join-Path $pair "$name.osf"
    if ((Get-FileHash -LiteralPath $save).Hash -cne $inspection.saveSha256 -or (Get-FileHash -LiteralPath $sidecar).Hash -cne $inspection.sha256) {
        throw '[blocked] Retained blackout save pair changed.'
    }
    $null=Read-DefeatCosave $sidecar @{DFT0=1;PCF0=1}
    $fixture=Read-JsonFile (Join-Path $script:FixtureRoot 'fixture.json')
    $script:FixtureRoot=Assert-ChildPath $script:Run.path (Join-Path $script:Run.path "blackout-fixture-$Checkpoint")
    $script:SaveRoot=Assert-ChildPath $script:GameDocuments (Join-Path $script:GameDocuments ('OSFDefeatTestingFixture/' + $script:Run.id + "-cold-$Checkpoint"))
    if ((Test-Path -LiteralPath $script:FixtureRoot) -or (Test-Path -LiteralPath $script:SaveRoot)) { throw '[blocked] Cold restart fixture already exists.' }
    Write-JsonFile (Join-Path $script:SaveRoot '.osf-test-harness.json') @{harnessRoot=$script:HarnessRoot}
    $seed=Join-Path $script:FixtureRoot 'saves'
    [IO.Directory]::CreateDirectory($seed) | Out-Null
    Copy-Item -LiteralPath $save,$sidecar -Destination $seed
    $fixture.sourceSave=$save
    $fixture.files=@(Get-ChildItem -LiteralPath $seed -File | ForEach-Object {@{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})
    Write-JsonFile (Join-Path $script:FixtureRoot 'fixture.json') $fixture
    foreach ($ini in @('Starfield.ini','StarfieldCustom.ini')) {
        # Edit a private sibling first: a transient sharing/byte-range lock must
        # not leave a partially written live INI. Replacement has a bounded retry.
        $target=Assert-ChildPath $script:ProfileRoot (Join-Path $script:ProfileRoot $ini)
        $temporary=Assert-ChildPath $script:ProfileRoot ($target+'.'+[guid]::NewGuid().ToString('N')+'.tmp')
        try {
            Copy-Item -LiteralPath $target -Destination $temporary
            Set-IniValue $temporary 'General' 'sLocalSavePath' ([IO.Path]::GetRelativePath($script:GameDocuments,$script:SaveRoot)+'\')
            Move-DefeatPrivateFile $temporary $target
        } finally {
            if (Test-Path -LiteralPath $temporary) { Remove-Item -LiteralPath $temporary -Force }
        }
    }
    $script:Run.coldSaveDirectory=$script:SaveRoot
    $script:DefeatMailbox=Join-Path $script:Run.path "blackout-cold-mailbox-$Checkpoint"
    [IO.Directory]::CreateDirectory($script:DefeatMailbox) | Out-Null
    $script:BlackoutColdStart=$true
    $script:BlackoutCheckpoint=$Checkpoint
    $script:BlackoutWeaponBaseline=if ($Checkpoint -gt 1) { $FirstSnapshot.blackoutBaselines[$name] } else { @() }
}
