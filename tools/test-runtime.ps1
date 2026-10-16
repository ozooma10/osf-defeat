#requires -Version 7.2
[CmdletBinding()]
param(
    [string]$Harness = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'OSF Test Harness'),
    [string]$ResultPath = '',
    [string]$SourceHash = '',
    [switch]$Preflight
)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$repo=Split-Path $PSScriptRoot -Parent
. (Join-Path $PSScriptRoot 'TestSupport.ps1')
. (Join-Path $PSScriptRoot 'CosaveEvidence.ps1')
. (Join-Path $PSScriptRoot 'RuntimeActions.ps1')
$required=@('scripts/Common.ps1','scripts/Environment.ps1','scripts/Reports.ps1','build/bin/OSFTestHarness.dll','build/bin/OSFTestInput.exe')
$missing=@($required | Where-Object { -not (Test-Path -LiteralPath (Join-Path $Harness $_) -PathType Leaf) })
if ($missing.Count) { Write-Error "[blocked] Missing OSF Test Harness files in ${Harness}: $($missing -join ', ')"; exit 2 }
. (Join-Path $Harness 'scripts/Common.ps1')
. (Join-Path $Harness 'scripts/Environment.ps1')
. (Join-Path $Harness 'scripts/Reports.ps1')
$script:Config.TestProfile='OSF Defeat Testing'
$script:StateRoot=Join-Path $Harness 'state/defeat'
$script:FixtureRoot=Join-Path $script:StateRoot 'fixture'
$script:ProfileRoot=Join-Path $script:Config.MO2 'profiles/OSF Defeat Testing'
$script:HarnessModName='OSF Defeat Testing - Harness'
$script:AutoLoadModName='OSF Defeat Testing - Auto Load'
$payloadName='OSF Defeat Testing - Payload'
$outputName='OSF Defeat Testing - Output'
$payload=Join-Path $script:ModRoot $payloadName
$animationMod=if ($script:Config.ContainsKey('DefeatAnimationMod')) { $script:Config.DefeatAnimationMod } else { 'OSF Animation' }
$expected=@(Get-Content (Join-Path $repo 'tests/runtime/cases.json') -Raw | ConvertFrom-Json)
$compiler=Join-Path $script:Config.Game 'Tools/Papyrus Compiler/PapyrusCompiler.exe'
$imports=Join-Path (Split-Path $repo -Parent) 'PapyrusSource'
$animationProject=Join-Path (Split-Path $repo -Parent) 'OSF Animation'
$missing=@()
foreach ($path in @($script:Config.FixtureSave,$compiler,(Join-Path $imports 'Starfield_Papyrus_Flags.flg'),(Join-Path $animationProject 'tools/BuildProduction.ps1'),(Join-Path $repo 'pack/OSFDefeatBasic.esm'),
    (Join-Path $script:ModRoot "$animationMod/SFSE/Plugins/OSF Animation.dll"),
    (Join-Path $script:ModRoot 'Auto Load'),
    (Join-Path $script:Config.Game 'Starfield.exe'),
    (Join-Path $script:Config.MO2 "profiles/$($script:Config.SourceProfile)/modlist.txt"))) {
    if (-not $path -or -not (Test-Path -LiteralPath $path)) { $missing+=,[string]$path }
}
if ($Preflight) {
    @{harness=$Harness;fixture=$script:Config.FixtureSave;animationMod=$animationMod;missing=$missing;runtimeCases=$expected} | ConvertTo-Json -Depth 5
    exit $(if ($missing.Count) {2} else {0})
}
if ($ResultPath -and (Test-Path -LiteralPath $ResultPath)) { throw 'ResultPath must be new.' }
$lock=$null; $outcome='blocked'; $message='Runtime preparation did not complete.'; $code=2; $started=$false
$source=Get-DefeatSourceIdentity $repo
if ($SourceHash -and $source.sha256 -cne $SourceHash) { throw '[blocked] Source fingerprint differs from caller.' }
$SourceHash=$source.sha256

function Own-Directory([string]$Root,[string]$Path) {
    $path=Assert-ChildPath $Root $Path; $marker=Join-Path $path '.osf-defeat-testing.json'
    if ((Test-Path -LiteralPath $path) -and -not (Test-Path -LiteralPath $marker)) { throw "[blocked] Unowned test directory: $path" }
    if (Test-Path -LiteralPath $marker) {
        if ((Read-JsonFile $marker).project -cne $repo) { throw '[blocked] Test directory belongs to another checkout.' }
    } else { Write-JsonFile $marker @{project=$repo} }
    return $path
}
function Reset-Fixture {
    if (@(Get-GameProcess).Count) { throw '[blocked] Game is running during fixture reset.' }
    $manifest=Read-JsonFile (Join-Path $script:FixtureRoot 'fixture.json')
    foreach ($file in $manifest.files) {
        $path=Assert-ChildPath $script:FixtureRoot (Join-Path $script:FixtureRoot "saves/$($file.name)")
        if ((Get-FileHash -LiteralPath $path).Hash -cne $file.sha256) { throw '[blocked] Fixture hash changed.' }
    }
    if ((Read-JsonFile (Join-Path $script:SaveRoot '.osf-test-harness.json')).harnessRoot -ne $script:HarnessRoot) { throw '[blocked] Save ownership mismatch.' }
    Copy-Tree (Join-Path $script:FixtureRoot 'saves') $script:SaveRoot
}
# Use the harness's existing launch-token preparation hook. All state/profile/mod
# paths above are Defeat-specific; no Animation payload or source is edited.
function Prepare-AnimationMailbox([string]$LaunchToken) {
    $script:DefeatLaunchToken=$LaunchToken
    Write-JsonFile (Join-Path $payload 'SFSE/Plugins/OSFDefeatTest.json') @{stateDir=$script:DefeatMailbox;launchToken=$LaunchToken;savePath=$script:SaveRoot+'\'}
}

try {
    if ($missing.Count) { throw "[blocked] Missing local prerequisites: $($missing -join ', ')" }
    [IO.Directory]::CreateDirectory((Join-Path $Harness 'state')) | Out-Null
    $lock=[IO.File]::Open((Join-Path $Harness 'state/runner.lock'),'OpenOrCreate','ReadWrite','None')
    if (@(Get-GameProcess).Count) { throw '[blocked] Close Starfield before starting a fresh Defeat test.' }
    $moExecutable=Join-Path $script:Config.MO2 'ModOrganizer.exe'
    foreach ($mo in @(Get-Process -Name ModOrganizer -ErrorAction SilentlyContinue | Where-Object Path -eq $moExecutable)) {
        $null=$mo.CloseMainWindow()
        if (-not $mo.WaitForExit(10000)) { throw '[blocked] Idle MO2 could not close normally; dismiss any dialog and close it before retrying.' }
    }
    $null=Start-RunArtifacts 'DefeatSmoke'
    $script:Run.sourceHash=$SourceHash
    $harnessIdentity=Get-DefeatSourceIdentity $Harness
    $script:Run.harnessSource=@{revision=$harnessIdentity.revision;sha256=$harnessIdentity.sha256;dirty=$harnessIdentity.dirty}
    $animationIdentity=Get-DefeatSourceIdentity $animationProject
    & pwsh -NoProfile -File (Join-Path $animationProject 'tools/BuildProduction.ps1')
    if ($LASTEXITCODE) { throw '[failed] Private Animation dependency build failed.' }
    $animationDll=Join-Path $animationProject 'build/windows/x64/releasedbg/OSF Animation.dll'
    $script:Run.animationSource=@{revision=$animationIdentity.revision;sha256=$animationIdentity.sha256;dirty=$animationIdentity.dirty}
    $script:Run.animationDll=@{path=$animationDll;sha256=(Get-FileHash -LiteralPath $animationDll).Hash}
    $script:DefeatMailbox=Join-Path $script:Run.path 'defeat-mailbox'
    [IO.Directory]::CreateDirectory($script:DefeatMailbox) | Out-Null
    $script:FixtureRoot=Join-Path $script:Run.path 'fixture'
    $script:SaveRoot=Assert-ChildPath $script:GameDocuments (Join-Path $script:GameDocuments ('OSFDefeatTestingFixture/' + $script:Run.id))
    if (Test-Path -LiteralPath $script:SaveRoot) { throw '[blocked] Run save directory already exists.' }
    Write-JsonFile (Join-Path $script:SaveRoot '.osf-test-harness.json') @{harnessRoot=$Harness}
    $script:Run.saveDirectory=$script:SaveRoot
    # Every run pins the explicitly configured local save and all matching sidecars.
    $save=Get-Item -LiteralPath $script:Config.FixtureSave
    if ($save.Extension -ine '.sfs') { throw '[blocked] FixtureSave must be an explicit disposable .sfs.' }
    $files=@(Get-ChildItem -LiteralPath $save.DirectoryName -File | Where-Object { $_.BaseName -eq $save.BaseName -or $_.Name.StartsWith($save.Name + '.') })
    $seed=Join-Path $script:FixtureRoot 'saves'; [IO.Directory]::CreateDirectory($seed) | Out-Null
    $files | Copy-Item -Destination $seed
    Write-JsonFile (Join-Path $script:FixtureRoot 'fixture.json') @{sourceSave=$save.FullName;validated=$false;files=@($files | ForEach-Object {@{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}})}
    Push-Location $repo
    try { & xmake build -y osfdefeat-runtime-tests; if ($LASTEXITCODE) { throw '[failed] Runtime DLL build failed.' } } finally { Pop-Location }
    $profile=Own-Directory (Join-Path $script:Config.MO2 'profiles') $script:ProfileRoot
    $sourceProfile=Join-Path $script:Config.MO2 "profiles/$($script:Config.SourceProfile)"
    Get-ChildItem -LiteralPath $sourceProfile -File | Copy-Item -Destination $profile -Force
    $mods=@($outputName,$payloadName,$script:HarnessModName,$script:AutoLoadModName)
    foreach ($mod in $mods) {
        $path=Own-Directory $script:ModRoot (Join-Path $script:ModRoot $mod)
        foreach ($file in Get-ChildItem -LiteralPath $path -File -Recurse -Force) {
            if ($file.Name -ne '.osf-defeat-testing.json') { Remove-Item -LiteralPath (Assert-ChildPath $path $file.FullName) -Force }
        }
    }
    Copy-Tree (Join-Path $repo 'dist') $payload
    $papyrus=Join-Path $repo 'tests/runtime/papyrus'
    $packScripts=Join-Path $repo 'pack/Scripts/Source'
    & $compiler $papyrus "-i=$papyrus;$packScripts;$(Join-Path $repo 'dist/Scripts/Source');$imports" "-o=$(Join-Path $payload 'Scripts')" "-f=$(Join-Path $imports 'Starfield_Papyrus_Flags.flg')" -all
    if ($LASTEXITCODE) { throw '[failed] Acceptance Papyrus compilation failed.' }
    & $compiler $packScripts "-i=$packScripts;$(Join-Path $repo 'dist/Scripts/Source');$imports" "-o=$(Join-Path $payload 'Scripts')" "-f=$(Join-Path $imports 'Starfield_Papyrus_Flags.flg')" -all
    if ($LASTEXITCODE) { throw '[failed] Pack Papyrus compilation failed.' }
    Copy-Item -LiteralPath (Join-Path $repo 'pack/OSFDefeatBasic.esm') -Destination $payload
    Copy-Tree (Join-Path $repo 'pack/SFSE/OSFDefeat/Consequences/Player') (Join-Path $payload 'SFSE/OSFDefeat/AcceptancePack')
    $script:Run.packEsmSha256=(Get-FileHash (Join-Path $payload 'OSFDefeatBasic.esm')).Hash
    $script:Run.acceptanceScripts=@(Get-ChildItem (Join-Path $payload 'Scripts') -Filter 'OSFDefeat*.pex' | ForEach-Object { @{name=$_.Name;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    [IO.Directory]::CreateDirectory((Join-Path $payload 'SFSE/Plugins')) | Out-Null
    Copy-Item -LiteralPath (Join-Path $repo 'build/tests/runtime/OSF Defeat.dll') -Destination (Join-Path $payload 'SFSE/Plugins/OSF Defeat.dll') -Force
    Copy-Item -LiteralPath $animationDll -Destination (Join-Path $payload 'SFSE/Plugins/OSF Animation.dll') -Force
    $script:Run.defeatDllSha256=(Get-FileHash (Join-Path $payload 'SFSE/Plugins/OSF Defeat.dll')).Hash
    Copy-Tree (Join-Path $script:ModRoot 'Auto Load') (Join-Path $script:ModRoot $script:AutoLoadModName)
    $plugins=Join-Path $script:ModRoot "$($script:HarnessModName)/SFSE/Plugins"; [IO.Directory]::CreateDirectory($plugins) | Out-Null
    Copy-Item -LiteralPath (Join-Path $Harness 'build/bin/OSFTestHarness.dll') -Destination $plugins
    Write-JsonFile (Join-Path $payload 'SFSE/OSFDefeat/Settings/zz-runtime-tests.json') @{version=3;processingEnabled=$true;disableInScenes=$false;defaultAutoRecoveryDelaySeconds=8;watchdogSeconds=30;explainDecisions=$true}
    $lines=@(Get-Content (Join-Path $sourceProfile 'modlist.txt'))
    $lines=@($lines | Where-Object { $_.TrimStart('+','-') -notin $mods } | ForEach-Object {
        $name=$_.TrimStart('+','-')
        if ($name -eq $animationMod) { '+'+$name }
        elseif ($_.StartsWith('+')) {
            $root=Assert-ChildPath $script:ModRoot (Join-Path $script:ModRoot $name)
            $conflict=$name -match 'Testing|Test Harness|OSF RE|Auto Load' -or
                (Test-Path (Join-Path $root 'SFSE/OSFDefeat')) -or (Test-Path (Join-Path $root 'SFSE/Plugins/OSF Defeat.dll')) -or
                (Test-Path (Join-Path $root 'SFSE/Plugins/OSF Animation.dll'))
            if ($conflict) { '-'+$name } else { $_ }
        } else { $_ }
    })
    if (-not ($lines -contains ('+'+$animationMod))) { throw '[blocked] Animation mod is absent from source profile.' }
    foreach ($path in @('SFSE/OSFDefeat','SFSE/Plugins/OSF Defeat.dll','SFSE/Plugins/OSFDefeatTest.json')) {
        if (Test-Path -LiteralPath (Join-Path $script:Config.MO2 "overwrite/$path")) { throw "[blocked] Shared overwrite shadows Defeat test payload: $path" }
    }
    [IO.File]::WriteAllLines((Join-Path $profile 'modlist.txt'),(@($mods | ForEach-Object {'+'+$_})+$lines))
    foreach ($list in @('plugins.txt','loadorder.txt')) {
        $path=Join-Path $profile $list
        $pluginLines=@(if (Test-Path -LiteralPath $path) {Get-Content -LiteralPath $path | Where-Object { $_.TrimStart('*') -ine 'OSFDefeatBasic.esm' }})
        $pluginLines+=if ($list -eq 'plugins.txt') {'*OSFDefeatBasic.esm'} else {'OSFDefeatBasic.esm'}
        [IO.File]::WriteAllLines($path,[string[]]$pluginLines)
    }
    Set-IniValue (Join-Path $profile 'settings.ini') 'General' 'LocalSaves' 'false'
    Set-IniValue (Join-Path $profile 'settings.ini') 'General' 'LocalSettings' 'true'
    Set-IniValue (Join-Path $profile 'settings.ini') 'custom_overwrites' ($script:Config.Executable.Replace(' ','%20')) $outputName
    foreach ($ini in @('Starfield.ini','StarfieldCustom.ini')) {
        Set-IniValue (Join-Path $profile $ini) 'General' 'sLocalSavePath' ([IO.Path]::GetRelativePath($script:GameDocuments,$script:SaveRoot)+'\')
    }
    Set-IniValue (Join-Path $profile 'StarfieldCustom.ini') 'Archive' 'bInvalidateOlderFiles' '1'
    Set-IniValue (Join-Path $profile 'StarfieldCustom.ini') 'Archive' 'sResourceDataDirsFinal' ''
    foreach ($key in @('bSaveOnPause','bSaveOnTravel','bSaveOnWait','bSaveOnRest')) { Set-IniValue (Join-Path $profile 'StarfieldPrefs.ini') 'GamePlay' $key '0' }
    $script:AnimationMode=$true # Enables only Prepare-AnimationMailbox in Start-TestGame.
    $null=Start-TestGame; $started=$true; $script:AnimationMode=$false
    $identity=Get-ProcessIdentity (Get-OwnedGame)
    $snapshotPath=Join-Path $script:DefeatMailbox 'snapshot.json'
    $snapshot=Wait-Observed 'Defeat test-build handshake' {Read-JsonFile $snapshotPath} {param($s) $s -and $s.state -in @('ready','failed')} 20
    if ($snapshot.state -eq 'failed') { throw "[blocked] Runtime preflight: $($snapshot.error)" }
    Test-DefeatRuntimeSnapshot $snapshot $identity $script:DefeatLaunchToken
    $null=Save-Capture 'defeat-before'
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
    Write-JsonFile (Join-Path $script:Run.path 'defeat-result.json') $snapshot
    $null=Save-Capture 'defeat-after'
    if ($snapshot.state -ne 'passed') { throw "[failed] Runtime assertion: $($snapshot.error)" }
    $actual=@($snapshot.cases.name)
    if ($actual.Count -ne $expected.Count -or @($actual | Sort-Object -Unique).Count -ne $actual.Count -or @(Compare-Object $expected $actual).Count -or
        @($snapshot.cases | Where-Object { $_.passed -isnot [bool] -or -not $_.passed }).Count) { throw '[failed] Incomplete runtime case evidence.' }
    if ((Get-DefeatSourceIdentity $repo).sha256 -cne $SourceHash) { throw '[blocked] Source changed during runtime tests.' }
    if ((Get-DefeatSourceIdentity $Harness).sha256 -cne $harnessIdentity.sha256) { throw '[blocked] Harness source changed during runtime tests.' }
    if ((Get-FileHash -LiteralPath $animationDll).Hash -cne $script:Run.animationDll.sha256) { throw '[blocked] Animation dependency changed during runtime tests.' }
    if ((Get-DefeatSourceIdentity $animationProject).sha256 -cne $animationIdentity.sha256) { throw '[blocked] Animation source changed during runtime tests.' }
    $outcome='passed'; $message="$($actual.Count) runtime assertions passed."; $code=0
} catch {
    $message=$_.Exception.Message; $outcome=if ($message.StartsWith('[failed]')) {'failed'} else {'blocked'}; $code=if ($outcome -eq 'failed') {1} else {2}
    try { if ($started -and (Get-OwnedGame)) { $null=Save-Capture 'defeat-failure' } } catch { Write-Warning "Failure capture unavailable: $_" }
    Write-Warning $message
} finally {
    $script:AnimationMode=$false
    if ($started -or (Test-Path -LiteralPath (Join-Path $script:StateRoot 'owner.json'))) {
        try { Stop-TestGame } catch { $outcome='blocked';$code=2;$message+=" Cleanup: $($_.Exception.Message)" }
    }
    if ($script:Run) { Finish-RunArtifacts $outcome $message }
    if ($lock) { $lock.Dispose() }
    $receipt=@{schemaVersion=1;outcome=$outcome;message=$message;sourceHash=$SourceHash;expectedCases=$expected;artifactDirectory=$(if ($script:Run) {$script:Run.path} else {$null})}
    if ($ResultPath) { Write-JsonFile ([IO.Path]::GetFullPath($ResultPath)) $receipt }
}
exit $code
