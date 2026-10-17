# Dot-sourced after the existing harness helpers. These are fixed acceptance actions.
function Invoke-DefeatConsole([string]$Command) {
    if ($Command -notmatch '^(save|load) osfdefeat(healthy|down|clean|truce|blackout)$') { throw '[blocked] Unexpected acceptance console command.' }
    $null=Invoke-Input 'focus'
    Send-Key 192 200
    try {
        $null=Wait-Observed 'Defeat acceptance console opened' {Get-GameState} {param($s) 'Console' -in $s.menus -or 'ConsoleMenu' -in $s.menus} 3
    } catch {
        # Startup can consume the first key. Reobserve before a single retry so
        # a late open is never toggled closed, and don't type through other menus.
        $state=Get-GameState
        if ('Console' -notin $state.menus -and 'ConsoleMenu' -notin $state.menus) {
            if (-not $state.loaded -or $state.snapshotStale -or @($state.menus).Count) { throw }
            Send-Key 192 200
            $null=Wait-Observed 'Defeat acceptance console retry' {Get-GameState} {param($s) 'Console' -in $s.menus -or 'ConsoleMenu' -in $s.menus} 6
        }
    }
    foreach ($character in $Command.ToUpperInvariant().ToCharArray()) { Send-Key ([int]$character) 40 }
    Send-Key 13
    Send-Key 192
}

function Invoke-DefeatRuntimeAction($Snapshot) {
    $action=$Snapshot.action
    $name=[string]$action.name
    switch ($action.kind) {
        'save' {
            $save=Assert-ChildPath $script:SaveRoot (Join-Path $script:SaveRoot "$name.sfs")
            $sidecar=Assert-ChildPath $script:SaveRoot (Join-Path $script:SaveRoot "$name.osf")
            if ((Test-Path -LiteralPath $save) -or (Test-Path -LiteralPath $sidecar)) { throw '[blocked] Acceptance save already exists.' }
            Invoke-DefeatConsole "save $name"
            $null=Wait-Observed "Defeat persistence save callback $name" {Read-JsonFile (Join-Path $script:DefeatMailbox 'snapshot.json')} {param($s) $s.engine.saveCallbacks -gt $Snapshot.engine.saveCallbacks} 30
            $null=Wait-Observed "real save pair $name" { (Test-Path -LiteralPath $save) -and (Test-Path -LiteralPath $sidecar) } {param($s) $s} 30
            $expected=if ($name -eq 'osfdefeatdown') { @{DFT0=2;CPT0=1;CAP0=1;PCF0=1} } elseif ($name -eq 'osfdefeattruce') { @{CPT0=1;TRC0=1;PCF0=1} } elseif ($name -eq 'osfdefeatblackout') { @{DFT0=1;PCF0=1} } else { @{} }
            $inspection=Read-DefeatCosave $sidecar $expected
            $evidence=Join-Path $script:Run.path "saves/$name"
            [IO.Directory]::CreateDirectory($evidence) | Out-Null
            Copy-Item -LiteralPath $save,$sidecar -Destination $evidence
            $inspection.saveSha256=(Get-FileHash -LiteralPath $save).Hash
            Write-JsonFile (Join-Path $evidence 'inspection.json') $inspection
            Add-Evidence 'save-pair' $name $inspection
            $null=Save-Capture $name
        }
        'load' {
            if ($name -notin @('osfdefeathealthy','osfdefeatdown','osfdefeattruce','osfdefeatblackout')) { throw '[blocked] Unexpected acceptance load.' }
            Invoke-DefeatConsole "load $name"
            # Do not require gameplay immediately: the native suite also requires a new
            # TESLoadGameEvent and persistence epoch before advancing after this receipt.
            $null=Wait-Observed 'loaded acceptance gameplay' {Get-GameState} {param($s) $s.loaded -and -not $s.snapshotStale -and 'LoadingMenu' -notin $s.menus -and 'Console' -notin $s.menus -and 'ConsoleMenu' -notin $s.menus} 60
        }
        'attack-miss' {
            $null=Invoke-Input 'focus'
            if ($name -eq 'grenade') {
                Send-Key 71 200
            } elseif ($name -in @('melee','unarmed','ranged')) {
            # The first click may only draw after loading a holstered save.
            $null=Invoke-Input 'click' @('--at-cursor','--button','left','--hold-ms','100')
            Start-Sleep -Milliseconds 1200
            $null=Invoke-Input 'click' @('--at-cursor','--button','left','--hold-ms','100')
            } else { throw '[blocked] Unexpected attack fixture.' }
        }
        'move' { $null=Invoke-Input 'focus'; Send-Key 87 700 }
        'look' {
            $null=Invoke-Input 'focus'
            for($step=0;$step -lt 3;$step++) {
                $null=Invoke-Input 'move' @('--dx','60','--dy','12')
                Start-Sleep -Milliseconds 100
            }
        }
        default { throw '[blocked] Unknown acceptance action.' }
    }
    Write-JsonFile (Join-Path $script:DefeatMailbox 'action-result.json') @{
        id=$action.id;pid=$Snapshot.pid;processStartFileTime=$Snapshot.processStartFileTime
        launchToken=$Snapshot.launchToken;runId=$Snapshot.runId;ok=$true;expiresAtUtcMs=[DateTimeOffset]::UtcNow.AddSeconds(15).ToUnixTimeMilliseconds()
    }
}
