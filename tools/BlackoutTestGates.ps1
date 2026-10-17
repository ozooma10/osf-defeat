# Test-only source transformation. The ordinary builder never imports this file.
# Suspend at the existing generation guards so save/load can interrupt each
# transaction boundary. No theft, time, recovery, or load callback is replaced.
function New-BlackoutTestSource([string]$Source, [string]$Destination) {
    $text=[IO.File]::ReadAllText($Source).Replace("`r`n","`n")
    foreach ($phase in @(2,3)) {
        $anchor="    Phase = $phase`n    If !IsCurrent(run)"
        if (($text.Split([string[]]@($anchor),[StringSplitOptions]::None)).Count -ne 2) {
            throw "Blackout phase $phase guard changed; review test instrumentation."
        }
        $gate="    Phase = $phase`n    While IsCurrent(run) && OSFDefeatAcceptanceProbe.HoldBlackout($phase)`n        Utility.Wait(0.1)`n    EndWhile`n    If !IsCurrent(run)"
        $text=$text.Replace($anchor,$gate)
    }
    [IO.Directory]::CreateDirectory((Split-Path $Destination -Parent)) | Out-Null
    [IO.File]::WriteAllText($Destination,$text)
}
