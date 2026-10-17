#requires -Version 7.2
[CmdletBinding()]
param(
    [string]$Game = (Join-Path ${env:ProgramFiles(x86)} 'Steam/steamapps/common/Starfield'),
    [string]$Imports = (Join-Path (Split-Path (Split-Path $PSScriptRoot -Parent) -Parent) 'PapyrusSource'),
    [string]$Spriggit = '',
    [string]$DotNetDirectory = '',
    [string]$OutputDirectory = '',
    [switch]$WorldProbe
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repo = Split-Path $PSScriptRoot -Parent
$source = Join-Path $repo 'shadows'
$compiler = Join-Path $Game 'Tools/Papyrus Compiler/PapyrusCompiler.exe'
$flags = Join-Path $Imports 'Starfield_Papyrus_Flags.flg'
foreach ($path in @($compiler, $flags, (Join-Path $Game 'Data/Starfield.esm'))) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Missing prerequisite: $path" }
}
if (-not $Spriggit) {
    $command = Get-Command spriggit -ErrorAction SilentlyContinue
    $candidates = @(
        (Join-Path $repo 'build/tools/spriggit-0.40.1/Spriggit.CLI.exe'),
        (Join-Path (Split-Path $repo -Parent) 'Tools/Spriggit/Spriggit.CLI.exe'),
        (Join-Path $env:USERPROFILE '.dotnet/tools/spriggit.exe'),
        (Join-Path $env:LOCALAPPDATA 'Spriggit/Spriggit.CLI.exe')
    )
    if ($command) { $candidates = @($command.Source) + $candidates }
    $Spriggit = $candidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } | Select-Object -First 1
}
if (-not $Spriggit) { throw 'Spriggit is missing. Supply -Spriggit; see shadows/README.md.' }
if (-not $DotNetDirectory -and (Test-Path (Join-Path $repo 'build/tools/dotnet9/dotnet.exe'))) {
    $DotNetDirectory = Join-Path $repo 'build/tools/dotnet9'
}
$output = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else {
    Join-Path $repo ('build/shadows/' + [DateTime]::UtcNow.ToString('yyyyMMdd-HHmmss') + '-' + [guid]::NewGuid().ToString('N'))
}
if (Test-Path -LiteralPath $output) { throw 'OutputDirectory must be new; evidence is never overwritten.' }
$payload = Join-Path $output 'payload'
$scripts = Join-Path $payload 'Scripts'
[IO.Directory]::CreateDirectory($scripts) | Out-Null
$oldPath = $env:PATH
$oldDotNet = $env:DOTNET_ROOT
$receipt = [ordered]@{schemaVersion=1;outcome='building';runtimeVerified=$false;worldProbe=[bool]$WorldProbe;payload=$payload;source=@();files=@()}
function Invoke-BuildTool([string]$Name, [string]$Program, [string[]]$Arguments) {
    & $Program @Arguments *>&1 | Tee-Object -FilePath (Join-Path $output "$Name.log") | Out-Host
    if ($LASTEXITCODE) { throw "$Name failed (exit $LASTEXITCODE); see $output" }
}
try {
    if ($DotNetDirectory) {
        $env:DOTNET_ROOT = [IO.Path]::GetFullPath($DotNetDirectory)
        $env:PATH = $env:DOTNET_ROOT + [IO.Path]::PathSeparator + $oldPath
    }
    $inputs = @(Get-ChildItem (Join-Path $source 'Plugin'), (Join-Path $source 'Scripts'), (Join-Path $source 'SFSE') -File -Recurse)
    $worldSource = Join-Path $repo 'tests/world-shadows'
    if ($WorldProbe) { $inputs += @(Get-ChildItem (Join-Path $worldSource 'Plugin'), (Join-Path $worldSource 'Scripts') -File -Recurse) }
    $receipt.source = @($inputs | ForEach-Object { @{path=[IO.Path]::GetRelativePath($repo, $_.FullName);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    Invoke-BuildTool 'papyrus' $compiler @((Join-Path $source 'Scripts/Source'), "-i=$(Join-Path $source 'Scripts/Source');$(Join-Path $repo 'dist/Scripts/Source');$Imports", "-o=$scripts", "-f=$flags", '-all')
    $pluginInput = Join-Path $source 'Plugin'
    if ($WorldProbe) {
        Invoke-BuildTool 'world-papyrus' $compiler @((Join-Path $worldSource 'Scripts/Source'), "-i=$(Join-Path $worldSource 'Scripts/Source');$(Join-Path $source 'Scripts/Source');$(Join-Path $repo 'dist/Scripts/Source');$Imports", "-o=$scripts", "-f=$flags", '-all')
        $pluginInput = Join-Path $output 'plugin-input'
        Copy-Item -LiteralPath (Join-Path $source 'Plugin') -Destination $pluginInput -Recurse
        Get-ChildItem (Join-Path $worldSource 'Plugin') | ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $pluginInput -Recurse -Force }
    }
    $esm = Join-Path $payload 'OSFDefeatShadows.esm'
    Invoke-BuildTool 'plugin' $Spriggit @('deserialize', '-i', $pluginInput, '-o', $esm, '-d', (Join-Path $Game 'Data'))
    Invoke-BuildTool 'roundtrip' $Spriggit @('serialize', '-i', $esm, '-o', (Join-Path $output 'roundtrip'), '-g', 'Starfield', '-p', 'Spriggit.Yaml.Starfield', '-v', '0.40.1', '-d', (Join-Path $Game 'Data'))
    Copy-Item -LiteralPath (Join-Path $source 'SFSE') -Destination (Join-Path $payload 'SFSE') -Recurse
    $verifyArgs = @((Join-Path $source 'verify_plugin.py'), $esm)
    if ($WorldProbe) { $verifyArgs += '--world-probe' }
    Invoke-BuildTool 'verify' 'python' $verifyArgs
    foreach ($inputFile in $receipt.source) {
        if ((Get-FileHash -LiteralPath (Join-Path $repo $inputFile.path)).Hash -cne $inputFile.sha256) { throw 'Probe source changed during build.' }
    }
    $receipt.files = @(Get-ChildItem $payload -Recurse -File | ForEach-Object { @{path=[IO.Path]::GetRelativePath($payload, $_.FullName);sha256=(Get-FileHash -LiteralPath $_.FullName).Hash} })
    $receipt.outcome = 'passed'
} catch {
    $receipt.outcome = 'failed'
    $receipt['error'] = $_.Exception.Message
    throw
} finally {
    $env:PATH = $oldPath
    $env:DOTNET_ROOT = $oldDotNet
    $receipt | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $output 'build.json') -Encoding utf8
    Write-Host "Build receipt: $(Join-Path $output 'build.json')"
}
