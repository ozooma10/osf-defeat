param()

$ErrorActionPreference = "Stop"
$Repo = [System.IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot))
$JsonOptions = [System.Text.Json.JsonDocumentOptions]::new()
$JsonOptions.AllowTrailingCommas = $true
$JsonOptions.CommentHandling = [System.Text.Json.JsonCommentHandling]::Skip

function Read-JsonDocument([string]$Path) {
    $Text = [System.IO.File]::ReadAllText($Path)
    return [System.Text.Json.JsonDocument]::Parse($Text, $JsonOptions)
}

$JsonFiles = Get-ChildItem -LiteralPath (Join-Path $Repo "dist") -Filter "*.json" -File -Recurse
$PackJsonRoot = Join-Path $Repo "pack\SFSE\OSFDefeat"
if (Test-Path -LiteralPath $PackJsonRoot) {
    $JsonFiles += Get-ChildItem -LiteralPath $PackJsonRoot -Filter "*.json" -File -Recurse
}

$ConsequenceIds = @{}
foreach ($File in $JsonFiles) {
    $Document = Read-JsonDocument $File.FullName
    try {
        $Root = $Document.RootElement
        if ($File.FullName -notlike "*SFSE\OSFDefeat*") {
            continue
        }
        $Version = [System.Text.Json.JsonElement]::new()
        if (-not $Root.TryGetProperty("version", [ref]$Version)) {
            throw "JSON is missing version: $($File.FullName)"
        }
        if ($Version.GetInt32() -notin @(1, 2, 3)) {
            throw "Unsupported JSON version in $($File.FullName)"
        }

        if ($File.DirectoryName -like "*Consequences*Player*") {
            $IdentityProperty = [System.Text.Json.JsonElement]::new()
            $IdentityKey = if ($Version.GetInt32() -ge 3) { "id" } else { "name" }
            if (-not $Root.TryGetProperty($IdentityKey, [ref]$IdentityProperty)) {
                throw "Consequence is missing $IdentityKey`: $($File.FullName)"
            }
            $Identity = $IdentityProperty.GetString()
            if ([string]::IsNullOrWhiteSpace($Identity)) {
                throw "Consequence has an empty $IdentityKey`: $($File.FullName)"
            }
            if ($ConsequenceIds.ContainsKey($Identity)) {
                throw "Duplicate bundled consequence ID '$Identity': $($ConsequenceIds[$Identity]) and $($File.FullName)"
            }
            $ConsequenceIds[$Identity] = $File.FullName

            if ($Version.GetInt32() -ge 3) {
                $DisplayNameProperty = [System.Text.Json.JsonElement]::new()
                if (-not $Root.TryGetProperty("displayName", [ref]$DisplayNameProperty) -or
                    [string]::IsNullOrWhiteSpace($DisplayNameProperty.GetString())) {
                    throw "Consequence is missing displayName: $($File.FullName)"
                }
                foreach ($LegacyKey in @("name", "recoverySeconds", "holdRecovery", "maxHoldSeconds")) {
                    $LegacyProperty = [System.Text.Json.JsonElement]::new()
                    if ($Root.TryGetProperty($LegacyKey, [ref]$LegacyProperty)) {
                        throw "Schema v3 consequence uses legacy key $LegacyKey`: $($File.FullName)"
                    }
                }
            }

            $Property = [System.Text.Json.JsonElement]::new()
            if ($Root.TryGetProperty("weight", [ref]$Property) -and $Property.GetInt32() -lt 1) {
                throw "Consequence weight must be >= 1: $($File.FullName)"
            }
            foreach ($Key in @("autoRecoveryDelaySeconds", "recoverySeconds", "leaseTimeoutSeconds", "maxHoldSeconds", "cooldownSeconds")) {
                $Property = [System.Text.Json.JsonElement]::new()
                if ($Root.TryGetProperty($Key, [ref]$Property) -and
                    ([double]::IsNaN($Property.GetDouble()) -or [double]::IsInfinity($Property.GetDouble()))) {
                    throw "Consequence $Key must be finite: $($File.FullName)"
                }
            }
        }
    } finally {
        $Document.Dispose()
    }
}

$LegacySettingsRoot = Join-Path $Repo "dist\SFSE\OSFDefeat\Validation"
if (Test-Path -LiteralPath $LegacySettingsRoot) {
    $LegacySettingsFiles = Get-ChildItem -LiteralPath $LegacySettingsRoot -Filter "*.json" -File
    if ($LegacySettingsFiles) {
        throw "Shipped framework settings must use SFSE\OSFDefeat\Settings; legacy Validation is read-only compatibility."
    }
}

$SourceRoot = Join-Path $Repo "pack\Scripts\Source"
$BinaryRoot = Join-Path $Repo "pack\Scripts"
foreach ($Source in Get-ChildItem -LiteralPath $SourceRoot -Filter "*.psc" -File) {
    $Binary = Join-Path $BinaryRoot ($Source.BaseName + ".pex")
    if (-not (Test-Path -LiteralPath $Binary)) {
        throw "Missing compiled Papyrus payload for $($Source.Name)"
    }
}

$Xmake = [System.IO.File]::ReadAllText((Join-Path $Repo "xmake.lua"))
if ($Xmake.Contains("os.trycp")) {
    throw "xmake.lua still contains silent os.trycp deployment"
}

$PapyrusSource = [System.IO.File]::ReadAllText((Join-Path $Repo "dist\Scripts\Source\OSFDefeat.psc"))
foreach ($CanonicalNative in @(
    "RecoverActor", "ScheduleAutomaticRecovery", "HoldRecovery", "GetAutomaticRecoverySecondsRemaining",
    "SuspendActorAI", "ResumeActorAI", "AcquireDefeatLease", "ReleaseDefeatLease",
    "GetDefeatLeaseHolder", "GetLastPlayerConsequenceId", "GetLastPlayerRecentHostileAttackers"
)) {
    if (-not $PapyrusSource.Contains("Function $CanonicalNative")) {
        throw "Papyrus source is missing canonical native $CanonicalNative"
    }
}

foreach ($LegacyNative in @(
    "RescueActor", "PacifyActor", "ReleaseActor", "IsPacified", "ClaimDefeat", "ReleaseClaim",
    "GetClaimOwner", "SetRecoveryTimer", "GetRecoverySecondsRemaining", "GetLastPlayerParticipants"
)) {
    if (-not $PapyrusSource.Contains("Function $LegacyNative")) {
        throw "Papyrus source dropped compatibility native $LegacyNative"
    }
}

$PersistenceSource = [System.IO.File]::ReadAllText((Join-Path $Repo "src\Core\Persistence.cpp"))
foreach ($StableFourCC in @(
    "OSF_PERSISTENCE_FOURCC('D', 'F', 'T', '0')",
    "OSF_PERSISTENCE_FOURCC('C', 'P', 'T', '0')",
    "OSF_PERSISTENCE_FOURCC('C', 'A', 'P', '0')",
    "OSF_PERSISTENCE_FOURCC('P', 'C', 'F', '0')"
)) {
    if (-not $PersistenceSource.Contains($StableFourCC)) {
        throw "Persistence compatibility FourCC changed or disappeared: $StableFourCC"
    }
}

Write-Host "Verification passed: $($JsonFiles.Count) JSON files, $($ConsequenceIds.Count) bundled consequence IDs, Papyrus payloads present."
