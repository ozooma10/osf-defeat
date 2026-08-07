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

$Names = @{}
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
        if ($Version.GetInt32() -notin @(1, 2)) {
            throw "Unsupported JSON version in $($File.FullName)"
        }

        if ($File.DirectoryName -like "*Consequences*Player*") {
            $NameProperty = [System.Text.Json.JsonElement]::new()
            if (-not $Root.TryGetProperty("name", [ref]$NameProperty)) {
                throw "Consequence is missing name: $($File.FullName)"
            }
            $Name = $NameProperty.GetString()
            if ([string]::IsNullOrWhiteSpace($Name)) {
                throw "Consequence has an empty name: $($File.FullName)"
            }
            if ($Names.ContainsKey($Name)) {
                throw "Duplicate bundled consequence name '$Name': $($Names[$Name]) and $($File.FullName)"
            }
            $Names[$Name] = $File.FullName

            $Property = [System.Text.Json.JsonElement]::new()
            if ($Root.TryGetProperty("weight", [ref]$Property) -and $Property.GetInt32() -lt 1) {
                throw "Consequence weight must be >= 1: $($File.FullName)"
            }
            foreach ($Key in @("recoverySeconds", "maxHoldSeconds", "cooldownSeconds")) {
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

Write-Host "Verification passed: $($JsonFiles.Count) JSON files, $($Names.Count) bundled consequences, Papyrus payloads present."
