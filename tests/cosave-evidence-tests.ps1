. (Join-Path (Split-Path $PSScriptRoot -Parent) 'tools/CosaveEvidence.ps1')
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('OSFDefeatCosave-'+[guid]::NewGuid().ToString('N')+'.osf')
function Inspect([byte[]]$Bytes,[hashtable]$Expected) {
    [IO.File]::WriteAllBytes($fixture,$Bytes)
    $null=Read-DefeatCosave $fixture $Expected
}
function U32([byte[]]$Bytes,[int]$Offset,[uint32]$Value) { [BitConverter]::GetBytes($Value).CopyTo($Bytes,$Offset) }
try {
    $empty=[byte[]]::new(24)
    [Text.Encoding]::ASCII.GetBytes('OSFP').CopyTo($empty,0)
    U32 $empty 4 1; U32 $empty 8 24
    Inspect $empty @{}; $checks++
    Reject { Inspect $empty @{DFT0=2} }
    $valid=[byte[]]::new(60); $empty.CopyTo($valid,0)
    U32 $valid 16 1
    [Text.Encoding]::ASCII.GetBytes('OSFD').CopyTo($valid,24)
    U32 $valid 28 1; U32 $valid 32 16
    [Text.Encoding]::ASCII.GetBytes('DFT0').CopyTo($valid,44)
    U32 $valid 48 2; U32 $valid 52 4; U32 $valid 56 2
    U32 $valid 36 ([OSFDefeatTestCrc32]::Compute($valid,44,16))
    Inspect $valid @{DFT0=2}; $checks++
    Reject { Inspect $valid @{} }
    Reject { Inspect $valid @{DFT0=1} }
    Reject { Inspect $valid @{CAP0=2} }
    for ($length=0;$length -lt $valid.Length;$length++) {
        $truncated=[byte[]]::new($length); [Array]::Copy($valid,$truncated,$length)
        Reject { Inspect $truncated @{DFT0=2} }
    }
    foreach ($offset in @(0,4,8,16,28,32,36,44,48,52,56)) {
        $bad=[byte[]]$valid.Clone(); $bad[$offset]=$bad[$offset] -bxor 255
        Reject { Inspect $bad @{DFT0=2} }
    }
    $bad=[byte[]]$valid.Clone(); U32 $bad 48 1; U32 $bad 36 ([OSFDefeatTestCrc32]::Compute($bad,44,16))
    Reject { Inspect $bad @{DFT0=2} } # Valid checksum, wrong record version.
    Reject { Inspect ([byte[]]($valid+0)) @{DFT0=2} }
    $duplicate=[byte[]]($valid+$valid[24..59]); U32 $duplicate 16 2
    Reject { Inspect $duplicate @{DFT0=2} }
    $duplicate=[byte[]]($valid+$valid[44..59]); U32 $duplicate 28 2; U32 $duplicate 32 32
    U32 $duplicate 36 ([OSFDefeatTestCrc32]::Compute($duplicate,44,32))
    Reject { Inspect $duplicate @{DFT0=2} }
    $bad=[byte[]]($valid+0); U32 $bad 32 17; U32 $bad 36 ([OSFDefeatTestCrc32]::Compute($bad,44,17))
    Reject { Inspect $bad @{DFT0=2} } # Valid checksum, unclaimed payload byte.
} finally {
    if (Test-Path -LiteralPath $fixture) { Remove-Item -LiteralPath $fixture }
}
