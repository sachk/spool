param([switch] $Clean)

# libplacebo needs a GLSL compiler for both its Vulkan and its D3D11 backend,
# and SPIRV-Cross on top of that to reach HLSL for D3D11. Linux takes both from
# nixpkgs. Windows has neither, and Meson's WrapDB carries no glslang and no
# shaderc, so there is nothing to install the way curl or harfbuzz are
# installed -- they are built from source here, as FFmpeg already is.
#
# Vulkan headers are deliberately not among them: libplacebo carries its own
# under 3rdparty and uses those when they are present, so nothing here needs
# the Vulkan SDK.
. (Join-Path $PSScriptRoot 'common.ps1')
Import-MsvcEnvironment
$root = Get-RepositoryRoot
$deps = Join-Path $root 'build\windows-deps'
$pin = (Get-ToolchainManifest).shaderTools
$prefix = Join-Path $deps 'shader-tools'
New-Item -ItemType Directory -Force $deps | Out-Null

if ($Clean -and (Test-Path $prefix)) { Remove-Item -LiteralPath $prefix -Recurse -Force }

function Get-Pinned {
    param([string] $Name, [object] $Source)

    $archive = Join-Path $deps "$Name-$($pin.version).tar.gz"
    $extracted = Join-Path $deps "$Name-$($pin.version)"
    if (-not (Test-Path $archive)) { Invoke-WebRequest $Source.url -OutFile $archive }
    if ((Get-FileHash $archive -Algorithm SHA256).Hash -ine $Source.sha256) {
        throw "$Name source checksum mismatch: $archive"
    }
    if (-not (Test-Path $extracted)) {
        New-Item -ItemType Directory -Force $extracted | Out-Null
        # One directory deep in the tarball, named for the tag.
        & "$env:SystemRoot\System32\tar.exe" -xf $archive -C $extracted --strip-components=1
        if ($LASTEXITCODE -ne 0) { throw "Extracting $Name failed." }
    }
    return $extracted
}

# glslang, static: libplacebo finds it with find_library plus a check for
# glslang/build_info.h, and looks under <vulkan-sdk>/lib for the libraries, so
# the prefix is handed to it as though it were an SDK.
$glslangMarker = Join-Path $prefix 'include\glslang\build_info.h'
if (-not (Test-Path $glslangMarker)) {
    $source = Get-Pinned 'glslang' $pin.glslang
    $build = Join-Path $deps 'glslang-build'
    if (Test-Path $build) { Remove-Item -LiteralPath $build -Recurse -Force }
    cmake -S $source -B $build -GNinja `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_INSTALL_PREFIX="$prefix" `
        -DENABLE_OPT=OFF `
        -DENABLE_GLSLANG_BINARIES=OFF `
        -DGLSLANG_TESTS=OFF `
        -DBUILD_SHARED_LIBS=OFF
    if ($LASTEXITCODE -ne 0) { throw 'Configuring glslang failed.' }
    cmake --build $build --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Building glslang failed.' }
    cmake --install $build
    if ($LASTEXITCODE -ne 0) { throw 'Installing glslang failed.' }
}

# SPIRV-Cross, shared: libplacebo asks pkg-config for spirv-cross-c-shared,
# which only the shared build installs a .pc for.
$spirvMarker = Join-Path $prefix 'lib\pkgconfig\spirv-cross-c-shared.pc'
if (-not (Test-Path $spirvMarker)) {
    $source = Get-Pinned 'spirv-cross' $pin.spirvCross
    $build = Join-Path $deps 'spirv-cross-build'
    if (Test-Path $build) { Remove-Item -LiteralPath $build -Recurse -Force }
    cmake -S $source -B $build -GNinja `
        -DCMAKE_BUILD_TYPE=Release `
        -DCMAKE_INSTALL_PREFIX="$prefix" `
        -DSPIRV_CROSS_SHARED=ON `
        -DSPIRV_CROSS_STATIC=ON `
        -DSPIRV_CROSS_CLI=OFF `
        -DSPIRV_CROSS_ENABLE_TESTS=OFF
    if ($LASTEXITCODE -ne 0) { throw 'Configuring SPIRV-Cross failed.' }
    cmake --build $build --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Building SPIRV-Cross failed.' }
    cmake --install $build
    if ($LASTEXITCODE -ne 0) { throw 'Installing SPIRV-Cross failed.' }
}

if (-not (Test-Path $spirvMarker)) {
    throw "SPIRV-Cross did not install a pkg-config file at $spirvMarker"
}
if (-not (Test-Path $glslangMarker)) {
    throw "glslang did not install its headers at $glslangMarker"
}
Write-Host "shader tools: $prefix"
