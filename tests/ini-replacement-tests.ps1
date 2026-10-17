. (Join-Path $root 'tools/RuntimeSession.ps1')
if (-not ('DefeatTestFileLock' -as [type])) {
    Add-Type -TypeDefinition @'
public static class DefeatTestFileLock {
    public static System.Threading.Tasks.Task ReleaseAfter(System.IDisposable handle, int milliseconds) {
        return System.Threading.Tasks.Task.Run(() => { System.Threading.Thread.Sleep(milliseconds); handle.Dispose(); });
    }
}
'@
}
$atomicRoot=Join-Path $root ('build/ini-contract-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($atomicRoot) | Out-Null
$original=Join-Path $atomicRoot 'profile.ini'
$replacement=Join-Path $atomicRoot 'profile.tmp'
[IO.File]::WriteAllText($original,'original profile')
[IO.File]::WriteAllText($replacement,'new save directory')
$held=[IO.File]::Open($original,'Open','Read','Read')
try {
    Reject { Move-DefeatPrivateFile $replacement $original 200 }
    if ([IO.File]::ReadAllText($original) -cne 'original profile' -or [IO.File]::ReadAllText($replacement) -cne 'new save directory') { throw 'Denied replacement damaged the profile or pending update.' }; $checks++
} finally { $held.Dispose() }
$held=[IO.File]::Open($original,'Open','Read','Read')
$release=[DefeatTestFileLock]::ReleaseAfter($held,350)
try {
    Move-DefeatPrivateFile $replacement $original 3000
    if ([IO.File]::ReadAllText($original) -cne 'new save directory' -or (Test-Path -LiteralPath $replacement)) { throw 'Replacement did not recover after the file lock cleared.' }; $checks++
} finally { $release.GetAwaiter().GetResult(); $held.Dispose() }
