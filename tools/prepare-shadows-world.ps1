#requires -Version 7.2
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$FixtureSave,
    [Parameter(Mandatory)][ValidateSet('authored','generated')][string]$Kind,
    [Parameter(Mandatory)][string]$BuildReceipt,
    [string]$DefeatDll = (Join-Path (Split-Path $PSScriptRoot -Parent) 'build/windows/x64/releasedbg/OSF Defeat.dll'),
    [string]$Harness = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'OSF Test Harness'),
    [switch]$Plan
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
# Reuse the harness's path guards, copy routine, INI writer and local configuration.
# This prepares an interactive profile; it does not launch or claim a game process.
. (Join-Path $Harness 'scripts/Common.ps1')
$build = Read-JsonFile ([IO.Path]::GetFullPath($BuildReceipt))
if (-not $build -or $build.outcome -cne 'passed' -or $build.runtimeVerified -ne $false) { throw 'Use a successful Shadows world-probe build.json.' }
if (-not $build.worldProbe) { throw 'Build with tools/build-shadows.ps1 -WorldProbe first.' }
$expectedFiles = @('OSFDefeatShadows.esm', 'Scripts/OSFDefeatShadowsAftermath.pex', 'Scripts/OSFDefeatShadowsHunt.pex', 'Scripts/OSFDefeatShadowsWorldProbe.pex', 'SFSE/OSFDefeat/Consequences/Player/minimal-shadows.json')
$apiPex = Join-Path $repo 'dist/Scripts/OSFDefeat.pex'
foreach ($nativeFile in @($DefeatDll,$apiPex)) { if (-not (Test-Path -LiteralPath $nativeFile -PathType Leaf)) { throw "Build the production DLL/API first: $nativeFile" } }
$nativeInventory = @(@{source=[IO.Path]::GetFullPath($DefeatDll);path='SFSE/Plugins/OSF Defeat.dll';sha256=(Get-FileHash -LiteralPath $DefeatDll).Hash}, @{source=$apiPex;path='Scripts/OSFDefeat.pex';sha256=(Get-FileHash -LiteralPath $apiPex).Hash})
$nativeInventory += @(Get-ChildItem (Join-Path $repo 'dist/SFSE/OSFDefeat/Settings') -File -Recurse | ForEach-Object {
    @{source=$_.FullName;path=[IO.Path]::GetRelativePath((Join-Path $repo 'dist'),$_.FullName);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash}
})
$animationMod = if ($script:Config.ContainsKey('DefeatAnimationMod')) { $script:Config.DefeatAnimationMod } else { 'OSF Animation' }
if (-not (Test-Path (Join-Path $script:ModRoot "$animationMod/SFSE/Plugins/OSF Animation.dll"))) { throw 'Production OSF Animation dependency is missing.' }
$actualFiles = @($build.files | ForEach-Object { $_.path.Replace('\','/') })
if (@(Compare-Object $expectedFiles $actualFiles).Count) { throw 'Unexpected probe payload inventory.' }
foreach ($file in $build.files) {
    $path = Assert-ChildPath $build.payload (Join-Path $build.payload $file.path)
    if ((Get-FileHash -LiteralPath $path).Hash -cne $file.sha256) { throw "Build payload changed: $path" }
}
foreach ($file in $build.source) {
    $path = Assert-ChildPath $repo (Join-Path $repo $file.path)
    if ((Get-FileHash -LiteralPath $path).Hash -cne $file.sha256) { throw 'Probe source changed; rebuild before preparing.' }
}
$save = Get-Item -LiteralPath $FixtureSave
if ($save.Extension -ine '.sfs') { throw 'FixtureSave must be an explicit disposable .sfs.' }
$seedFiles = @(Get-ChildItem -LiteralPath $save.DirectoryName -File | Where-Object { $_.BaseName -eq $save.BaseName -or $_.Name.StartsWith($save.Name + '.') })
$seedInventory = @($seedFiles | ForEach-Object { @{name=$_.Name;source=$_.FullName;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
$sourceProfile = Assert-ChildPath (Join-Path $script:Config.MO2 'profiles') (Join-Path $script:Config.MO2 "profiles/$($script:Config.SourceProfile)")
if (-not (Test-Path -LiteralPath (Join-Path $sourceProfile 'modlist.txt'))) { throw 'Source profile is missing.' }
$settingsPath = Join-Path $sourceProfile 'settings.ini'
$usesLocalSettings = (Test-Path -LiteralPath $settingsPath) -and
    ((Get-Content -LiteralPath $settingsPath -Raw) -match '(?im)^\s*LocalSettings\s*=\s*(true|1)\s*$')
$iniSource = if ($usesLocalSettings) { $sourceProfile } else { $script:GameDocuments }
foreach ($relative in @('OSFDefeatShadows.esm','Scripts/OSFDefeatShadowsWorldProbe.pex','SFSE/Plugins/OSFDefeatTest.json','SFSE/Plugins/OSFTestHarness.dll','SFSE/Plugins/OSF Defeat.dll','SFSE/OSFDefeat')) {
    if (Test-Path -LiteralPath (Join-Path $script:Config.MO2 "overwrite/$relative")) { throw "Shared overwrite conflicts with the probe: $relative" }
}
$id = $Kind + '-' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N').Substring(0,8)
$profileName = "OSF Defeat Shadows World $id"
$payloadName = "$profileName - Payload"
$outputName = "$profileName - Output"
$profile = Assert-ChildPath (Join-Path $script:Config.MO2 'profiles') (Join-Path $script:Config.MO2 "profiles/$profileName")
$payload = Assert-ChildPath $script:ModRoot (Join-Path $script:ModRoot $payloadName)
$output = Assert-ChildPath $script:ModRoot (Join-Path $script:ModRoot $outputName)
$saves = Assert-ChildPath $script:GameDocuments (Join-Path $script:GameDocuments "OSFDefeatShadowsWorld/$id")
$manifest = [ordered]@{
    schemaVersion=1;status='prepared';runtimeVerified=$false;kind=$Kind;profileName=$profileName
    profile=$profile;payload=$payload;output=$output;saves=$saves;sourceProfile=$sourceProfile;settingsSource=$iniSource
    buildReceipt=[IO.Path]::GetFullPath($BuildReceipt);fixture=$seedInventory;native=$nativeInventory
}
if ($Plan) { $manifest.status = 'planned'; $manifest | ConvertTo-Json -Depth 6; return }
if (@(Get-GameProcess).Count -or @(Get-Process -Name ModOrganizer -ErrorAction SilentlyContinue).Count) {
    throw 'Close Starfield and MO2 before preparing the private profile.'
}
[IO.Directory]::CreateDirectory((Join-Path $Harness 'state')) | Out-Null
$lock = [IO.File]::Open((Join-Path $Harness 'state/runner.lock'), 'OpenOrCreate', 'ReadWrite', 'None')
try {
    foreach ($directory in @($profile,$payload,$output,$saves)) {
        if (Test-Path -LiteralPath $directory) { throw "Refusing existing destination: $directory" }
        [IO.Directory]::CreateDirectory($directory) | Out-Null
        Write-JsonFile (Join-Path $directory '.osf-defeat-shadows-world.json') @{project=$repo;run=$id}
    }
    Get-ChildItem -LiteralPath $sourceProfile -File | Where-Object { $_.Name -notlike '.osf-*' } | Copy-Item -Destination $profile
    # A profile with LocalSettings=false uses the Documents INIs. Its cached profile
    # copies may be empty or stale; seed the effective settings before enabling isolation.
    if (-not $usesLocalSettings) {
        foreach ($ini in @('Starfield.ini','StarfieldCustom.ini','StarfieldPrefs.ini')) {
            $sourceIni = Join-Path $iniSource $ini
            if (Test-Path -LiteralPath $sourceIni -PathType Leaf) {
                Copy-Item -LiteralPath $sourceIni -Destination (Join-Path $profile $ini) -Force
            }
        }
    }
    # Copy only the verified payload inventory, never arbitrary build output.
    foreach ($file in $build.files) {
        $destination = Assert-ChildPath $payload (Join-Path $payload $file.path)
        [IO.Directory]::CreateDirectory((Split-Path $destination -Parent)) | Out-Null
        Copy-Item -LiteralPath (Join-Path $build.payload $file.path) -Destination $destination
        if ((Get-FileHash -LiteralPath $destination).Hash -cne $file.sha256) { throw 'Probe copy hash mismatch.' }
    }
    foreach ($file in $nativeInventory) {
        $destination = Assert-ChildPath $payload (Join-Path $payload $file.path)
        [IO.Directory]::CreateDirectory((Split-Path $destination -Parent)) | Out-Null
        Copy-Item -LiteralPath $file.source -Destination $destination
        if ((Get-FileHash -LiteralPath $destination).Hash -cne $file.sha256 -or (Get-FileHash -LiteralPath $file.source).Hash -cne $file.sha256) { throw 'Native payload changed during copy.' }
    }
    foreach ($file in $seedInventory) {
        $destination = Assert-ChildPath $saves (Join-Path $saves $file.name)
        Copy-Item -LiteralPath $file.source -Destination $destination
        if ((Get-FileHash -LiteralPath $destination).Hash -cne $file.sha256 -or (Get-FileHash -LiteralPath $file.source).Hash -cne $file.sha256) { throw 'Fixture changed during copy.' }
    }
    $lines = @(Get-Content -LiteralPath (Join-Path $sourceProfile 'modlist.txt') | ForEach-Object {
        $name = $_.TrimStart('+','-')
        if ($name -eq $animationMod) { '+' + $name }
        elseif ($_.StartsWith('+')) {
            $root = Assert-ChildPath $script:ModRoot (Join-Path $script:ModRoot $name)
            if ($name -match 'Testing|Test Harness|OSF RE|Auto Load|OSF Defeat Spike|OSF Defeat Shadows World' -or (Test-Path (Join-Path $root 'SFSE/OSFDefeat')) -or (Test-Path (Join-Path $root 'SFSE/Plugins/OSF Defeat.dll'))) { '-' + $name } else { $_ }
        } else { $_ }
    })
    if ($lines -notcontains ('+' + $animationMod)) { throw 'Animation dependency is absent from the source profile.' }
    [IO.File]::WriteAllLines((Join-Path $profile 'modlist.txt'), [string[]](@(('+'+$outputName), ('+'+$payloadName)) + $lines))
    foreach ($list in @('plugins.txt','loadorder.txt')) {
        $path = Join-Path $profile $list
        $pluginLines = @(if (Test-Path -LiteralPath $path) { Get-Content -LiteralPath $path | Where-Object { $_.TrimStart('*') -notin @('OSFDefeatShadows.esm','OSFDefeatBasic.esm','OSFDefeatPersistenceSpike.esm') } })
        $pluginLines += if ($list -eq 'plugins.txt') { '*OSFDefeatShadows.esm' } else { 'OSFDefeatShadows.esm' }
        [IO.File]::WriteAllLines($path, [string[]]$pluginLines)
    }
    Set-IniValue (Join-Path $profile 'settings.ini') 'General' 'LocalSaves' 'false'
    Set-IniValue (Join-Path $profile 'settings.ini') 'General' 'LocalSettings' 'true'
    Set-IniValue (Join-Path $profile 'settings.ini') 'custom_overwrites' ($script:Config.Executable.Replace(' ','%20')) $outputName
    foreach ($ini in @('Starfield.ini','StarfieldCustom.ini')) {
        Set-IniValue (Join-Path $profile $ini) 'General' 'sLocalSavePath' ([IO.Path]::GetRelativePath($script:GameDocuments,$saves) + '\')
    }
    Set-IniValue (Join-Path $profile 'StarfieldCustom.ini') 'Archive' 'bInvalidateOlderFiles' '1'
    Set-IniValue (Join-Path $profile 'StarfieldCustom.ini') 'Archive' 'sResourceDataDirsFinal' ''
    Set-IniValue (Join-Path $profile 'StarfieldCustom.ini') 'Papyrus' 'bEnableLogging' '1'
    Set-IniValue (Join-Path $profile 'StarfieldCustom.ini') 'Papyrus' 'bEnableTrace' '1'
    foreach ($key in @('bSaveOnPause','bSaveOnTravel','bSaveOnWait','bSaveOnRest')) {
        Set-IniValue (Join-Path $profile 'StarfieldPrefs.ini') 'GamePlay' $key '0'
    }
    Write-JsonFile (Join-Path $saves 'fixture.json') $manifest
    Write-Host "Prepared profile: $profileName"
    Write-Host "Private saves: $saves"
    Write-Host 'Launch this profile through MO2 + SFSE, then load the copied seed. No runtime result has been recorded.'
} finally {
    $lock.Dispose()
}
