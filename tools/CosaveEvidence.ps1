# OSF persistence v1 framing, independently checked against retained on-disk bytes.
function Read-DefeatCosave([string]$Path,[hashtable]$Expected) {
    if (-not ('OSFDefeatTestCrc32' -as [type])) {
        Add-Type -TypeDefinition @'
public static class OSFDefeatTestCrc32 {
    public static uint Compute(byte[] bytes, int offset, int length) {
        uint crc=0xffffffff;
        for(int i=offset;i<offset+length;i++) {
            crc^=bytes[i];
            for(int bit=0;bit<8;bit++) crc=(crc>>1)^((crc&1)!=0?0xedb88320u:0u);
        }
        return ~crc;
    }
}
'@
    }
    $bytes=[IO.File]::ReadAllBytes($Path)
    if ($bytes.Length -lt 24 -or [Text.Encoding]::ASCII.GetString($bytes,0,4) -cne 'OSFP' -or [BitConverter]::ToUInt32($bytes,4) -ne 1) { throw '[failed] Invalid OSF sidecar header.' }
    [long]$offset=[BitConverter]::ToUInt32($bytes,8)
    $clients=[BitConverter]::ToUInt32($bytes,16)
    if ($offset -lt 24 -or $offset -gt $bytes.Length -or $clients -gt 4096) { throw '[failed] Invalid sidecar bounds.' }
    $found=@{}; $seen=@{}
    for ($i=0;$i -lt $clients;$i++) {
        if ($offset+20 -gt $bytes.Length) { throw '[failed] Truncated client header.' }
        $client=[Text.Encoding]::ASCII.GetString($bytes,$offset,4)
        $count=[BitConverter]::ToUInt32($bytes,$offset+4)
        $length=[BitConverter]::ToUInt32($bytes,$offset+8)
        $crc=[BitConverter]::ToUInt32($bytes,$offset+12); $offset+=20
        if ($seen.ContainsKey($client) -or $offset+$length -gt $bytes.Length -or $count -gt 65536) { throw '[failed] Invalid client framing.' }
        $seen[$client]=$true
        if ([OSFDefeatTestCrc32]::Compute($bytes,$offset,$length) -ne $crc) { throw '[failed] Sidecar checksum mismatch.' }
        $end=$offset+$length
        for ($j=0;$j -lt $count;$j++) {
            if ($offset+12 -gt $end) { throw '[failed] Truncated record header.' }
            $type=[Text.Encoding]::ASCII.GetString($bytes,$offset,4)
            $version=[BitConverter]::ToUInt32($bytes,$offset+4)
            $size=[BitConverter]::ToUInt32($bytes,$offset+8); $offset+=12
            if ($offset+$size -gt $end) { throw '[failed] Truncated record payload.' }
            if ($client -ceq 'OSFD') {
                if ($found.ContainsKey($type) -or $size -lt 4) { throw '[failed] Duplicate/empty Defeat record.' }
                $found[$type]=@{version=$version;count=[BitConverter]::ToUInt32($bytes,$offset);bytes=$size}
            }
            $offset+=$size
        }
        if ($offset -ne $end) { throw '[failed] Sidecar client has trailing bytes.' }
    }
    if ($offset -ne $bytes.Length) { throw '[failed] Sidecar has trailing bytes.' }
    if ($Expected.Count -eq 0 -and $seen.ContainsKey('OSFD')) { throw '[failed] Clean save still contains OSFD.' }
    if ($found.Count -ne $Expected.Count) { throw '[failed] Defeat sidecar record inventory differs.' }
    foreach ($type in $Expected.Keys) {
        $version=if ($type -in @('DFT0','CAP0')) {2} else {1}
        if (-not $found.ContainsKey($type) -or $found[$type].count -ne $Expected[$type] -or $found[$type].version -ne $version) {
            $actual=if ($found.ContainsKey($type)) { "count=$($found[$type].count), version=$($found[$type].version)" } else { 'missing' }
            throw "[failed] Unexpected $type record in real save: $actual; expected count=$($Expected[$type]), version=$version."
        }
    }
    return @{path=$Path;sha256=(Get-FileHash -LiteralPath $Path).Hash;clientCount=$clients;defeatRecords=$found}
}
