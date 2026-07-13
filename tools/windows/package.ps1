param(
    [string] $StageDirectory,
    [string] $OutputDirectory,
    [string] $MakeNsis
)

. (Join-Path $PSScriptRoot 'common.ps1')
$root = Get-RepositoryRoot
$stage = if ($StageDirectory) { [IO.Path]::GetFullPath($StageDirectory) } else {
    Join-Path $root 'build\windows-release\stage'
}
$output = if ($OutputDirectory) { [IO.Path]::GetFullPath($OutputDirectory) } else { Join-Path $root 'dist' }

foreach ($relative in @('jellyfin-native.exe', 'mpv-2.dll', 'imageformats\qwebp.dll')) {
    if (-not (Test-Path -LiteralPath (Join-Path $stage $relative))) {
        throw "The staged Windows payload is incomplete: $relative"
    }
}

$version = (Get-Content -LiteralPath (Join-Path $root 'VERSION') -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') {
    throw "VERSION is not a three-part semantic version: $version"
}

if (-not $MakeNsis) {
    $command = Get-Command makensis.exe -ErrorAction SilentlyContinue
    $candidates = @()
    if ($command) { $candidates += $command.Source }
    if ($env:NSIS_ROOT) { $candidates += Join-Path $env:NSIS_ROOT 'makensis.exe' }
    $candidates += Join-Path $root 'build\windows-tools\nsis\makensis.exe'
    $candidates += Join-Path ${env:ProgramFiles(x86)} 'NSIS\makensis.exe'
    $MakeNsis = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (-not $MakeNsis -or -not (Test-Path -LiteralPath $MakeNsis)) {
    throw 'makensis.exe was not found. Install NSIS 3 or set NSIS_ROOT.'
}

New-Item -ItemType Directory -Force $output | Out-Null
$portable = Join-Path $output "Jellyfin-Native-$version-Windows-x64-Portable.exe"
$installer = Join-Path $output "Jellyfin-Native-$version-Windows-x64-Setup.exe"
Remove-Item -LiteralPath $portable, $installer -Force -ErrorAction SilentlyContinue

function Invoke-NsisPackage {
    param([string] $Script, [string] $Artifact)

    $arguments = @(
        '/V2',
        '/NOCD',
        "/DVERSION=$version",
        "/DSTAGE_DIR=$stage",
        "/DSOURCE_ROOT=$root",
        "/DOUTPUT_FILE=$Artifact",
        (Join-Path $PSScriptRoot $Script)
    )
    & $MakeNsis @arguments
    if ($LASTEXITCODE -ne 0) {
        throw "NSIS failed to build $Script"
    }
    if (-not (Test-Path -LiteralPath $Artifact)) {
        throw "NSIS did not create the expected artifact: $Artifact"
    }
}

Invoke-NsisPackage -Script 'portable.nsi' -Artifact $portable
Invoke-NsisPackage -Script 'installer.nsi' -Artifact $installer

$sevenZip = Get-Command 7z.exe -ErrorAction SilentlyContinue
if ($sevenZip) {
    foreach ($artifact in @($portable, $installer)) {
        $listing = & $sevenZip.Source l -slt $artifact
        if ($LASTEXITCODE -ne 0) { throw "7-Zip could not inspect $artifact" }
        foreach ($required in @('jellyfin-native.exe', 'mpv-2.dll', 'qwebp.dll')) {
            if (-not ($listing | Select-String -SimpleMatch $required -Quiet)) {
                throw "$artifact does not contain $required"
            }
        }
    }
}

$checksumPath = Join-Path $output "Jellyfin-Native-$version-Windows-x64-SHA256SUMS.txt"
$checksumLines = @($portable, $installer) | ForEach-Object {
    $hash = (Get-FileHash -LiteralPath $_ -Algorithm SHA256).Hash.ToLowerInvariant()
    "$hash  $([IO.Path]::GetFileName($_))"
}
[IO.File]::WriteAllLines($checksumPath, $checksumLines, [Text.UTF8Encoding]::new($false))

Get-Item -LiteralPath $portable, $installer, $checksumPath |
    Select-Object Name, Length, @{ Name = 'MiB'; Expression = { [math]::Round($_.Length / 1MB, 2) } }
