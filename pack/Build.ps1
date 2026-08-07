param(
    [string]$Target,
    [switch]$NoCompile,
    [switch]$NoPlugin,
    [switch]$NoDeploy
)

# OSF Defeat - Basic Consequences: builds OSFDefeatBasic.esm from the Spriggit
# source tree, compiles the pack's Papyrus, and deploys as its own MO2 mod so
# users can toggle the default content independently of the framework.

$ErrorActionPreference = "Stop"
$Root = $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($Target)) {
    $Target = "C:\Modding\Starfield\MO2\mods\OSF Defeat - Basic Consequences"
}

$Compiler = "C:\Program Files (x86)\Steam\steamapps\common\Starfield\Tools\Papyrus Compiler\PapyrusCompiler.exe"
$Flags    = "C:\Modding\Starfield\PapyrusSource\Starfield_Papyrus_Flags.flg"
# The pack calls OSFDefeat.* natives: import the framework's script source from the repo.
$Imports  = @(
    (Join-Path $Root "Scripts\Source"),
    (Join-Path (Split-Path -Parent $Root) "dist\Scripts\Source"),
    "C:\Modding\Starfield\PapyrusSource"
) -join ";"
$SourceDir    = Join-Path $Root "Scripts\Source"
$ScriptOut    = Join-Path $Root "Scripts"
$PluginSource = Join-Path $Root "Plugin"
$PluginEsm    = Join-Path $Root "OSFDefeatBasic.esm"
# Starfield is a separated-master game: Spriggit needs the Data folder to resolve masters.
$DataFolder   = "C:\Program Files (x86)\Steam\steamapps\common\Starfield\Data"

function Find-Spriggit {
    $cmd = Get-Command spriggit -ErrorAction SilentlyContinue
    if ($cmd) { return $cmd.Source }
    foreach ($p in @(
        "$env:USERPROFILE\.dotnet\tools\spriggit.exe",
        "$env:LOCALAPPDATA\Spriggit\Spriggit.CLI.exe",
        "C:\Modding\Starfield\Tools\Spriggit\Spriggit.CLI.exe"
    )) { if (Test-Path -LiteralPath $p) { return $p } }
    return $null
}

if (-not $NoCompile) {
    if (-not (Test-Path -LiteralPath $Compiler)) { throw "Papyrus compiler not found: $Compiler" }
    Write-Host "Compiling pack scripts..."
    & $Compiler $SourceDir "-i=$Imports" "-o=$ScriptOut" "-f=$Flags" -all 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw "Papyrus compile failed" }
}

if (-not $NoPlugin) {
    $Spriggit = Find-Spriggit
    if (-not $Spriggit) { throw "Spriggit not found; cannot build OSFDefeatBasic.esm" }
    Write-Host "Building OSFDefeatBasic.esm with Spriggit ($Spriggit)..."
    & $Spriggit deserialize -i $PluginSource -o $PluginEsm -d "$DataFolder" 2>&1 | ForEach-Object { Write-Host $_ }
    if ($LASTEXITCODE -ne 0) { throw "Spriggit deserialize failed" }
}

if (-not $NoDeploy) {
    $Target = [System.IO.Path]::GetFullPath($Target)
    $TargetRoot = [System.IO.Path]::GetPathRoot($Target)
    $RepoRoot = [System.IO.Path]::GetFullPath((Split-Path -Parent $Root))
    $TargetLeaf = Split-Path -Leaf $Target
    $BroadLeaves = @("Data", "mods", "Starfield", "Modding", "Scripts", "SFSE")
    if ($Target -eq $TargetRoot -or $Target -eq $RepoRoot -or
        $Target.StartsWith($RepoRoot + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -or
        $RepoRoot.StartsWith($Target + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase) -or
        $BroadLeaves -contains $TargetLeaf) {
        throw "Refusing unsafe deployment target: $Target"
    }
    Write-Host "Deploying owned payload paths to $Target"
    New-Item -ItemType Directory -Force -Path $Target | Out-Null
    foreach ($item in @("OSFDefeatBasic.esm", "meta.ini", "Scripts", "SFSE")) {
        $s = Join-Path $Root $item
        if (-not (Test-Path -LiteralPath $s)) { Write-Host "  (skip missing $item)"; continue }
        $d = [System.IO.Path]::GetFullPath((Join-Path $Target $item))
        if (-not $d.StartsWith($Target + [System.IO.Path]::DirectorySeparatorChar, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Resolved deployment item escaped target: $d"
        }
        # Replace each pack-owned path atomically at the path boundary. Copy-merge left stale PEX/
        # JSON files behind after renames, which made deployed behavior differ from the source tree.
        if (Test-Path -LiteralPath $d) {
            Remove-Item -LiteralPath $d -Recurse -Force
        }
        Copy-Item -LiteralPath $s -Destination $d -Recurse -Force
    }
}

Write-Host "Build complete."
