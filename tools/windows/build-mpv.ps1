param(
    [switch] $Clean,
    [switch] $SkipWrapUpdate
)

. (Join-Path $PSScriptRoot 'common.ps1')
Initialize-WindowsMpvBuildEnvironment

$root = Get-RepositoryRoot
$source = Join-Path $root 'mpv'
$dependencyRoot = Join-Path $root 'build\windows-deps'
$buildSource = Join-Path $dependencyRoot 'mpv-source'
$buildDirectory = Join-Path $dependencyRoot 'mpv-build'
$prefix = Join-Path $dependencyRoot 'mpv'
$packageCache = Join-Path $dependencyRoot 'meson-package-cache'

foreach ($path in @($buildSource, $buildDirectory, $prefix, $packageCache)) {
    $resolvedParent = [IO.Path]::GetFullPath((Split-Path $path -Parent))
    if (-not $resolvedParent.StartsWith([IO.Path]::GetFullPath($dependencyRoot), [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify a path outside the Windows dependency root: $path"
    }
}

if ($Clean) {
    foreach ($path in @($buildSource, $buildDirectory, $prefix)) {
        if (Test-Path -LiteralPath $path) {
            Remove-Item -LiteralPath $path -Recurse -Force
        }
    }
}

# Work in a disposable mirror so Meson wraps never dirty the mpv submodule.
# Use -Clean after changing the fork; ordinary retries retain downloaded wraps.
if (-not (Test-Path -LiteralPath $buildSource)) {
    New-Item -ItemType Directory -Force $buildSource | Out-Null
    Get-ChildItem -LiteralPath $source -Force |
        Where-Object { $_.Name -notin @('.git', 'build') } |
        Copy-Item -Destination $buildSource -Recurse -Force
}
New-Item -ItemType Directory -Force $packageCache | Out-Null

$env:MESON_PACKAGE_CACHE_DIR = $packageCache
$subprojects = Join-Path $buildSource 'subprojects'
New-Item -ItemType Directory -Force $subprojects | Out-Null

Push-Location $buildSource
try {
    if (-not $SkipWrapUpdate) {
        meson wrap update-db
        if ($LASTEXITCODE -ne 0) { throw 'Failed to update the Meson WrapDB catalog.' }
    }

    foreach ($wrap in @('curl', 'expat', 'freetype2', 'fribidi', 'harfbuzz', 'libpng', 'luajit', 'zlib', 'xxhash')) {
        if (-not (Test-Path (Join-Path $subprojects "$wrap.wrap"))) {
            meson wrap install $wrap
            if ($LASTEXITCODE -ne 0) { throw "Failed to install the Meson wrap: $wrap" }
        }
    }

    meson subprojects download curl
    if ($LASTEXITCODE -ne 0) { throw 'Failed to download the pinned curl source.' }
    $curlRoot = Get-ChildItem -LiteralPath $subprojects -Directory -Filter 'curl-*' | Select-Object -First 1
    if (-not $curlRoot) { throw 'The downloaded curl subproject was not found.' }
    $curlProject = Join-Path $curlRoot.FullName 'meson.build'
    $curlLibrary = Join-Path $curlRoot.FullName 'lib\meson.build'
    $curlProjectText = (Get-Content -LiteralPath $curlProject -Raw).Replace(
        "if get_option('default_library') == 'static'",
        'if true')
    $curlLibraryText = (Get-Content -LiteralPath $curlLibrary -Raw).Replace(
        'curl_lib = library(',
        'curl_lib = static_library(').Replace(
        "  version: '4.8.0',`n",
        '')
    [IO.File]::WriteAllText($curlProject, $curlProjectText, [Text.UTF8Encoding]::new($false))
    [IO.File]::WriteAllText($curlLibrary, $curlLibraryText, [Text.UTF8Encoding]::new($false))

    # MSVC cannot run FFmpeg's own configure, so Windows takes FFmpeg as a
    # meson subproject from the GStreamer port. Its pin lives in
    # toolchain.json beside the tarball every other platform builds.
    $windowsFfmpeg = (Get-ToolchainManifest).ffmpeg.windows
    @"
[wrap-git]
url = $($windowsFfmpeg.wrapUrl)
revision = $($windowsFfmpeg.wrapRevision)
depth = 1

[provide]
dependency_names = libavcodec, libavdevice, libavfilter, libavformat, libavutil, libswresample, libswscale
"@ | Set-Content -LiteralPath (Join-Path $subprojects 'ffmpeg.wrap') -Encoding ascii

    @'
[wrap-git]
url = https://github.com/libass/libass
revision = f9fd3d20dff1cd84b7c74c8ae7f79711ad7736fa
depth = 1
'@ | Set-Content -LiteralPath (Join-Path $subprojects 'libass.wrap') -Encoding ascii

    @'
[wrap-git]
url = https://code.videolan.org/videolan/libplacebo.git
revision = a7a18af88ff0a17c04840dcb3246047bb6b46df3
depth = 1
clone-recursive = true
'@ | Set-Content -LiteralPath (Join-Path $subprojects 'libplacebo.wrap') -Encoding ascii

    # This pinned libplacebo revision uses project_source_root(), which points
    # at mpv when libplacebo is nested as a Meson subproject. Correct the path
    # in the disposable mirror so its bundled Python generators are found.
    meson subprojects download libplacebo
    if ($LASTEXITCODE -ne 0) { throw 'Failed to download the pinned libplacebo source.' }
    $libplaceboRoot = Join-Path $subprojects 'libplacebo'
    & git -C $libplaceboRoot submodule update --init --recursive
    if ($LASTEXITCODE -ne 0) { throw 'Failed to initialize libplacebo submodules.' }
    $libplaceboMeson = Join-Path $libplaceboRoot 'meson.build'
    $libplaceboText = Get-Content -LiteralPath $libplaceboMeson -Raw
    $pythonExecutable = if (Get-Command py.exe -ErrorAction SilentlyContinue) {
        (& py.exe -3 -c 'import sys; print(sys.executable)').Trim()
    } elseif (Get-Command python.exe -ErrorAction SilentlyContinue) {
        (Get-Command python.exe).Source
    } else {
        throw 'A system Python interpreter is required by libplacebo build-time generators.'
    }
    $pythonMesonPath = $pythonExecutable.Replace('\', '/')
    $libplaceboText = $libplaceboText.Replace(
        "thirdparty = meson.project_source_root()/'3rdparty'",
        "thirdparty = meson.current_source_dir()/'3rdparty'")
    $libplaceboText = $libplaceboText.Replace(
        "python = import('python').find_installation()",
        "python = import('python').find_installation('$pythonMesonPath')")
    [IO.File]::WriteAllText($libplaceboMeson, $libplaceboText, [Text.UTF8Encoding]::new($false))

    $setupArguments = @(
        'setup',
        $buildDirectory,
        $buildSource,
        '--prefix', $prefix,
        '--libdir', 'lib',
        '--buildtype', 'release',
        '--default-library', 'shared',
        '--wrap-mode', 'forcefallback'
    ) + @(Get-MpvFeatureArguments -Platform windows -IncludeSubprojects)

    if (Test-Path (Join-Path $buildDirectory 'build.ninja')) {
        $setupArguments = @('setup', '--reconfigure') + $setupArguments[1..($setupArguments.Count - 1)]
    } elseif (Test-Path -LiteralPath $buildDirectory) {
        Remove-Item -LiteralPath $buildDirectory -Recurse -Force
    }
    & meson @setupArguments
    if ($LASTEXITCODE -ne 0) { throw 'Configuring the Windows libmpv build failed.' }

    # Meson 1.9 records a fallback's per-subproject core option during the
    # initial setup but may still instantiate that first fallback as shared.
    # Reapply these after the fallbacks exist so libmpv has no extra project
    # DLLs and installation does not expect import libraries for their tools.
    meson configure $buildDirectory '-Dcurl:default_library=static' '-Dluajit:default_library=static'
    if ($LASTEXITCODE -ne 0) { throw 'Configuring static Windows libmpv dependencies failed.' }

    # Ninja's default process fan-out can exhaust Windows process creation
    # resources while compiling the large static FFmpeg closure.
    meson compile -C $buildDirectory --jobs 4
    if ($LASTEXITCODE -ne 0) { throw 'Building Windows libmpv failed.' }
    if (Test-Path -LiteralPath $prefix) {
        Remove-Item -LiteralPath $prefix -Recurse -Force
    }
    meson install -C $buildDirectory
    if ($LASTEXITCODE -ne 0) { throw 'Installing Windows libmpv failed.' }
} finally {
    Pop-Location
}

$mpvDll = Get-ChildItem (Join-Path $prefix 'bin') -Filter '*mpv*.dll' -File | Select-Object -First 1
$mpvLibrary = Get-ChildItem (Join-Path $prefix 'lib') -Filter 'mpv.lib' -File | Select-Object -First 1
if (-not $mpvDll -or -not $mpvLibrary) {
    throw "The source build did not produce the expected libmpv DLL and MSVC import library below $prefix."
}

Write-Host "Built Windows libmpv from $source"
Write-Host "DLL: $($mpvDll.FullName) ($([math]::Round($mpvDll.Length / 1MB, 2)) MiB)"
Write-Host "Import library: $($mpvLibrary.FullName)"
