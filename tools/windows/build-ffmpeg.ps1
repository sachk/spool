param([switch] $Clean)

. (Join-Path $PSScriptRoot 'common.ps1')
Import-MsvcEnvironment
$root = Get-RepositoryRoot
$deps = Join-Path $root 'build\windows-deps'
$pin = (Get-ToolchainManifest).ffmpeg
$source = Join-Path $deps "ffmpeg-$($pin.version)"
$archive = Join-Path $deps "ffmpeg-$($pin.version).tar.xz"
$headers = Join-Path $deps 'nv-codec-headers'
$prefix = Join-Path $deps 'ffmpeg'
$build = Join-Path $deps 'ffmpeg-build'
$bash = 'C:\msys64\usr\bin\bash.exe'
if (-not (Test-Path $bash)) {
    throw 'Install MSYS2 with make, diffutils and pkgconf (C:\msys64) to build upstream FFmpeg.'
}
New-Item -ItemType Directory -Force $deps | Out-Null
if (-not (Test-Path $archive)) { Invoke-WebRequest $pin.url -OutFile $archive }
if ((Get-FileHash $archive -Algorithm SHA256).Hash -ine $pin.sha256) {
    throw "FFmpeg source checksum mismatch: $archive"
}
if (-not (Test-Path $source)) {
    & "$env:SystemRoot\System32\tar.exe" -xf $archive -C $deps
    if ($LASTEXITCODE -ne 0) { throw 'Extracting FFmpeg failed.' }
}
if (-not (Test-Path $headers)) {
    git clone https://github.com/FFmpeg/nv-codec-headers.git $headers
    if ($LASTEXITCODE -ne 0) { throw 'Fetching NVIDIA decode headers failed.' }
}
git -C $headers checkout --detach e844e5b26f46bb77479f063029595293aa8f812d
if ($LASTEXITCODE -ne 0) { throw 'Checking out NVIDIA decode headers failed.' }
if ($Clean) {
    foreach ($path in @($build, $prefix)) {
        if (Test-Path $path) { Remove-Item $path -Recurse -Force }
    }
}
New-Item -ItemType Directory -Force $build | Out-Null
$flags = Join-Path $build 'configure-flags.txt'
& python (Join-Path $root 'tools\ffmpeg-capabilities.py') configure --platform windows |
    Set-Content $flags -Encoding utf8NoBOM
if ($LASTEXITCODE -ne 0) { throw 'Generating FFmpeg capabilities failed.' }
$env:SPOOL_MSVC_BIN = Split-Path (Get-Command cl.exe).Source
$env:SPOOL_NASM_BIN = Split-Path (Get-Command nasm.exe).Source
& $bash (Join-Path $PSScriptRoot 'build-ffmpeg.sh').Replace('\', '/') `
    $source $headers $prefix $build
if ($LASTEXITCODE -ne 0) { throw 'Building upstream Windows FFmpeg failed.' }
& python (Join-Path $root 'tools\ffmpeg-capabilities.py') audit-components --platform windows `
    (Join-Path $build 'config_components.h')
if ($LASTEXITCODE -ne 0) { throw 'Windows FFmpeg capabilities failed verification.' }
& python (Join-Path $PSScriptRoot 'check-ffmpeg.py') $prefix
if ($LASTEXITCODE -ne 0) { throw 'Windows FFmpeg runtime failed verification.' }
# Ensure subsequent Meson builds use the existing clang/MSVC-compatible toolchain.
Initialize-WindowsMpvBuildEnvironment
$env:PKG_CONFIG = 'C:\msys64\usr\bin\pkgconf.exe'
