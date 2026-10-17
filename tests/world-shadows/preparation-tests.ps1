#requires -Version 7.2
param(
    [Parameter(Mandatory)][string]$BuildReceipt,
    [string]$Harness = (Join-Path (Split-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) -Parent) 'OSF Test Harness')
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path (Split-Path $PSScriptRoot -Parent) -Parent
$root = Join-Path $repo ('build/shadows-world-preparation-tests/' + [guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory((Join-Path $root 'scripts')) | Out-Null
$mo2 = Join-Path $root 'mo2'
$profile = Join-Path $mo2 'profiles/Default'
[IO.Directory]::CreateDirectory($profile) | Out-Null
[IO.Directory]::CreateDirectory((Join-Path $mo2 'mods')) | Out-Null
# Use the real common helpers against a private fake workspace. Process queries are
# replaced only in this copied test helper; no local game/profile is touched.
$common = Get-Content -LiteralPath (Join-Path $Harness 'scripts/Common.ps1') -Raw
$common += @'

$script:GameDocuments = Join-Path $script:HarnessRoot 'documents'
function Get-GameProcess { @() }
function Get-Process { param($Name, $ErrorAction) @() }
'@
Set-Content -LiteralPath (Join-Path $root 'scripts/Common.ps1') -Value $common
$quotedMo2 = $mo2.Replace("'", "''")
Set-Content -LiteralPath (Join-Path $root 'config.psd1') -Value "@{MO2='$quotedMo2';SourceProfile='Default';TestProfile='unused';OutputDirectory='artifacts';Executable='Starfield (SFSE)'}"
Set-Content -LiteralPath (Join-Path $profile 'modlist.txt') -Value @('+Regular Mod','+Auto Load','+OSF Test Harness','-Disabled Mod','-OSF Animation','+ConflictingDefeat')
Set-Content -LiteralPath (Join-Path $profile 'plugins.txt') -Value @('*Starfield.esm','*Regular.esm','*OSFDefeatBasic.esm')
Set-Content -LiteralPath (Join-Path $profile 'loadorder.txt') -Value @('Starfield.esm','Regular.esm')
Set-Content -LiteralPath (Join-Path $profile 'settings.ini') -Value @('[General]','LocalSettings=false')
Set-Content -LiteralPath (Join-Path $profile 'StarfieldCustom.ini') -Value ''
$documents = Join-Path $root 'documents'
[IO.Directory]::CreateDirectory($documents) | Out-Null
Set-Content -LiteralPath (Join-Path $documents 'StarfieldCustom.ini') -Value @('[General]','bAlwaysActive=1')
Set-Content -LiteralPath (Join-Path $documents 'StarfieldPrefs.ini') -Value @('[Display]','iSize H=720')
$seed = Join-Path $root 'seed.sfs'
[IO.File]::WriteAllBytes($seed, [byte[]](1,2,3,4))
[IO.File]::WriteAllBytes((Join-Path $root 'seed.osf'), [byte[]](5,6,7))
$native = Join-Path $root 'production.dll'
[IO.File]::WriteAllBytes($native, [byte[]](8,9,10))
foreach ($mod in @('OSF Animation','ConflictingDefeat')) {
    $pluginsDir=Join-Path $mo2 "mods/$mod/SFSE/Plugins"
    [IO.Directory]::CreateDirectory($pluginsDir) | Out-Null
    $dll=if ($mod -eq 'OSF Animation') {'OSF Animation.dll'} else {'OSF Defeat.dll'}
    [IO.File]::WriteAllBytes((Join-Path $pluginsDir $dll),[byte[]](1))
}
$before = (Get-FileHash -LiteralPath (Join-Path $profile 'modlist.txt')).Hash
$checks = 0
function Check([bool]$Ok, [string]$Name) {
    if (-not $Ok) { throw "FAIL: $Name" }
    $script:checks++
    Write-Host "PASS: $Name"
}
$prepare = Join-Path $repo 'tools/prepare-shadows-world.ps1'
$arguments = @('-NoProfile','-File',$prepare,'-Harness',$root,'-DefeatDll',$native,'-FixtureSave',$seed,'-Kind','authored','-BuildReceipt',[IO.Path]::GetFullPath($BuildReceipt))
$planText = & pwsh @arguments -Plan
Check ($LASTEXITCODE -eq 0) 'plan succeeds'
$plan = $planText | ConvertFrom-Json
Check ($plan.status -eq 'planned' -and -not $plan.runtimeVerified) 'plan cannot claim preparation or runtime success'
Check (-not (Test-Path -LiteralPath $plan.profile) -and -not (Test-Path -LiteralPath $plan.saves)) 'plan creates nothing'
& pwsh @arguments
Check ($LASTEXITCODE -eq 0) 'preparation succeeds in private fixture'
$manifestPath = @(Get-ChildItem (Join-Path $root 'documents') -Filter fixture.json -Recurse)
Check ($manifestPath.Count -eq 1) 'one fixture manifest'
$manifest = Get-Content -LiteralPath $manifestPath[0].FullName -Raw | ConvertFrom-Json
Check (-not $manifest.runtimeVerified) 'preparation does not certify the game'
Check ((Get-FileHash -LiteralPath (Join-Path $profile 'modlist.txt')).Hash -ceq $before) 'source profile is unchanged'
Check ((Get-FileHash -LiteralPath (Join-Path $manifest.saves 'seed.sfs')).Hash -ceq (Get-FileHash -LiteralPath $seed).Hash) 'save copy matches source'
Check ((Get-FileHash -LiteralPath (Join-Path $manifest.saves 'seed.osf')).Hash -ceq (Get-FileHash -LiteralPath (Join-Path $root 'seed.osf')).Hash) 'sidecar copy matches source'
$mods = @(Get-Content -LiteralPath (Join-Path $manifest.profile 'modlist.txt'))
Check ($mods.Count -eq 8 -and $mods[0].EndsWith(' - Output') -and $mods[1].EndsWith(' - Payload')) 'output and payload occupy separate modlist lines'
Check ($mods -contains '+Regular Mod' -and $mods -contains '-Auto Load' -and $mods -contains '-OSF Test Harness') 'normal mod retained and automatic runners disabled'
$plugins = @(Get-Content -LiteralPath (Join-Path $manifest.profile 'plugins.txt'))
Check ($plugins -contains '*Regular.esm' -and $plugins -contains '*OSFDefeatShadows.esm') 'existing plugins and probe enabled'
Check ($mods -contains '+OSF Animation' -and $mods -contains '-ConflictingDefeat') 'animation enabled and conflicting native payload disabled'
Check ($plugins -notcontains '*OSFDefeatBasic.esm') 'Basic plugin excluded from Shadows profile'
Check ((Get-FileHash -LiteralPath (Join-Path $manifest.payload 'SFSE/Plugins/OSF Defeat.dll')).Hash -ceq (Get-FileHash -LiteralPath $native).Hash) 'production DLL copy verified'
$ini = Get-Content -LiteralPath (Join-Path $manifest.profile 'StarfieldCustom.ini') -Raw
Check ($ini.Contains('sLocalSavePath=OSFDefeatShadowsWorld\') -and $ini.Contains('bEnableLogging=1')) 'isolated save path and logging configured'
Check ($ini.Contains('bAlwaysActive=1') -and $manifest.settingsSource -eq $documents) 'effective global settings replace stale profile copies'
Check ((Get-Content -LiteralPath (Join-Path $manifest.profile 'StarfieldPrefs.ini') -Raw).Contains('iSize H=720')) 'display preferences retained'
Set-Content -LiteralPath (Join-Path $profile 'settings.ini') -Value @('[General]','LocalSettings=true')
$localPlan = & pwsh @arguments -Plan | ConvertFrom-Json
Check ($LASTEXITCODE -eq 0 -and $localPlan.settingsSource -eq $profile) 'local-settings profiles use their own INIs'
$badReceipt = Get-Content -LiteralPath $BuildReceipt -Raw | ConvertFrom-Json -AsHashtable
$badReceipt.files[0].sha256 = 'invalid'
$badPath = Join-Path $root 'bad-build.json'
$badReceipt | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $badPath
& pwsh -NoProfile -File $prepare -Harness $root -DefeatDll $native -FixtureSave $seed -Kind authored -BuildReceipt $badPath -Plan *> (Join-Path $root 'expected-rejection.log')
Check ($LASTEXITCODE -ne 0) 'changed build payload rejected'
Write-Host "$checks preparation checks passed. Evidence: $root"
