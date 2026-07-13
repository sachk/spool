param(
    [Parameter(Mandatory)] [string] $StageDirectory
)

. (Join-Path $PSScriptRoot 'common.ps1')
Import-MsvcEnvironment

$stage = [IO.Path]::GetFullPath($StageDirectory)
if (-not (Test-Path -LiteralPath $stage -PathType Container)) {
    throw "The staged Windows payload was not found: $stage"
}

$dumpbin = (Get-Command dumpbin.exe -ErrorAction Stop).Source
$available = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
Get-ChildItem -LiteralPath $stage -Recurse -File | ForEach-Object {
    [void] $available.Add($_.Name)
}

$missing = [Collections.Generic.List[string]]::new()
$binaries = Get-ChildItem -LiteralPath $stage -Recurse -File |
    Where-Object Extension -In @('.dll', '.exe')

foreach ($binary in $binaries) {
    $imports = & $dumpbin /nologo /dependents $binary.FullName |
        ForEach-Object {
            if ($_ -match '^\s+([^\s]+\.dll)\s*$') { $Matches[1] }
        }
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin could not inspect $($binary.FullName)"
    }

    foreach ($import in $imports) {
        if ($available.Contains($import)) { continue }
        if ($import -match '^(api|ext)-ms-win-') { continue }

        $isCompilerRuntime = $import -match '^(concrt|msvcp|vcruntime)\d*(_\d+)?\.dll$'
        $systemDll = Join-Path $env:SystemRoot "System32\$import"
        if (-not $isCompilerRuntime -and (Test-Path -LiteralPath $systemDll)) { continue }

        $relative = $binary.FullName.Substring($stage.TrimEnd('\').Length + 1)
        $missing.Add("$relative -> $import")
    }
}

if ($missing.Count -gt 0) {
    throw "The Windows payload has missing runtime dependencies:`n$($missing -join "`n")"
}

Write-Host "Runtime dependency closure passed for $($binaries.Count) Windows binaries."
