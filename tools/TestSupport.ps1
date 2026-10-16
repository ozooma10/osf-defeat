#requires -Version 7.2
Set-StrictMode -Version Latest

function Get-DefeatSourceIdentity([string]$Root) {
    $revision = & git -C $Root rev-parse HEAD
    if ($LASTEXITCODE) { throw '[blocked] Cannot identify source revision.' }
    $status = @(& git -C $Root status --porcelain)
    $files = @(& git -C $Root -c core.quotepath=false ls-files --cached --others --exclude-standard)
    if ($LASTEXITCODE) { throw '[blocked] Cannot enumerate source files.' }
    $records = @($files | Sort-Object -Unique | ForEach-Object {
        $path = Join-Path $Root $_
        if (Test-Path -LiteralPath $path -PathType Leaf) {
            [ordered]@{path=$_;sha256=(Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash}
        }
    })
    $submodules = @(& git -C $Root submodule status --recursive)
    if ($LASTEXITCODE) { throw '[blocked] Cannot identify submodules.' }
    $bytes = [Text.Encoding]::UTF8.GetBytes(($records | ConvertTo-Json -Compress) + ($submodules -join "`n"))
    @{revision=[string]$revision;dirty=($status.Count -gt 0);sha256=[Convert]::ToHexString([Security.Cryptography.SHA256]::HashData($bytes));files=$records;submodules=$submodules}
}

function Test-DefeatCaseReport($Report, [string[]]$Expected) {
    if ($Report.schemaVersion -ne 1 -or $Report.passed -isnot [bool] -or -not $Report.passed -or @($Report.cases).Count -ne $Expected.Count -or -not $Expected.Count) {
        throw '[failed] Missing, empty, incomplete or failing native case report.'
    }
    $actual = @($Report.cases | ForEach-Object { $_.name })
    if (@($actual | Sort-Object -Unique).Count -ne $actual.Count -or @(Compare-Object $Expected $actual).Count) {
        throw '[failed] Native case inventory differs from the compiled source.'
    }
    foreach ($case in $Report.cases) {
        if ($case.passed -isnot [bool] -or -not $case.passed -or $case.checks -lt 1) { throw '[failed] Native case did not execute passing assertions.' }
    }
}

function Test-DefeatInventory([string]$Root, $Coverage, [string[]]$RuntimeCases) {
    $suites=@($Coverage.suite | Sort-Object -Unique)
    $sources=@(Get-ChildItem -LiteralPath (Join-Path $Root 'tests/native') -Filter '*_tests.cpp' -File | ForEach-Object { $_.BaseName -replace '_tests$','' } | Sort-Object)
    $build=Get-Content (Join-Path $Root 'tests/xmake.lua') -Raw
    $list=[regex]::Match($build,'local suites\s*=\s*\{([^}]+)\}').Groups[1].Value
    $targets=@([regex]::Matches($list,'"([a-z]+)"') | ForEach-Object {$_.Groups[1].Value} | Sort-Object)
    if (-not $suites.Count -or @(Compare-Object $suites $sources).Count -or @(Compare-Object $suites $targets).Count) {
        throw '[failed] Coverage, native source files and XMake test targets disagree.'
    }
    if (-not $RuntimeCases.Count -or @($RuntimeCases | Sort-Object -Unique).Count -ne $RuntimeCases.Count) {
        throw '[failed] Runtime inventory is empty or duplicated.'
    }
    $mapped=@($Coverage | ForEach-Object {$_.runtime} | Sort-Object -Unique)
    if (@(Compare-Object $RuntimeCases $mapped).Count) { throw '[failed] Runtime coverage map and case inventory disagree.' }
}

function Test-DefeatRuntimeSnapshot($Snapshot, $Identity, [string]$LaunchToken, [string]$RunId = '') {
    if ($Snapshot.schemaVersion -ne 1 -or $Snapshot.pid -ne $Identity.pid -or
        [string]$Snapshot.processStartFileTime -cne [string]$Identity.processStartFileTime -or
        $Snapshot.launchToken -cne $LaunchToken -or ($RunId -and $Snapshot.runId -cne $RunId)) {
        throw '[blocked] Runtime snapshot belongs to another process or run.'
    }
    $now = [DateTimeOffset]::UtcNow.ToUnixTimeMilliseconds()
    if ($Snapshot.updatedAtUtcMs -lt $now - 15000 -or $Snapshot.updatedAtUtcMs -gt $now + 1000) {
        throw '[blocked] Runtime snapshot is stale or future dated.'
    }
}
