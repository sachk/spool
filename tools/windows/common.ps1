$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Import-MsvcEnvironment {
    if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
        $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
        if (-not (Test-Path -LiteralPath $vswhere)) {
            throw 'vswhere.exe was not found. Install Visual Studio 2022 with Desktop development with C++.'
        }

        $installPath = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
        if (-not $installPath) {
            throw 'No Visual Studio installation with the x64 C++ toolchain was found.'
        }

        $vcvars = Join-Path $installPath 'VC\Auxiliary\Build\vcvars64.bat'
        $environment = & $env:ComSpec /d /s /c "`"$vcvars`" >nul && set"
        $importedPath = $null
        foreach ($line in $environment) {
            if ($line -match '^([^=]+)=(.*)$') {
                # Codex and some terminal hosts expose both PATH and Path. vcvars
                # updates PATH; prefer that spelling when both are present, but
                # GitHub runners expose only the conventional mixed-case Path.
                if ($Matches[1] -ieq 'PATH') {
                    if ($Matches[1] -ceq 'PATH' -or $null -eq $importedPath) {
                        $importedPath = $Matches[2]
                    }
                    continue
                }
                Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2]
            }
        }
        if ($null -ne $importedPath) {
            Set-Item -Path Env:PATH -Value $importedPath
        }
    }

    # GitHub's Windows image also exposes MinGW. Ninja otherwise discovers its
    # c++.exe before cl.exe and silently mixes the GNU ABI with MSVC Qt/QCoro.
    $compiler = (Get-Command cl.exe -ErrorAction Stop).Source
    $env:CC = $compiler
    $env:CXX = $compiler
    foreach ($name in @('CC_LD', 'CXX_LD', 'WINDRES')) {
        Remove-Item -Path "Env:$name" -ErrorAction SilentlyContinue
    }
}

function Get-RepositoryRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

# tools\manifests\toolchain.json is the single place the Qt and FFmpeg
# versions are set; nothing here should repeat one.
function Get-ToolchainManifest {
    $manifest = Join-Path (Get-RepositoryRoot) 'tools\manifests\toolchain.json'
    return Get-Content -LiteralPath $manifest -Raw | ConvertFrom-Json
}

function Get-DefaultQtRoot {
    $qt = (Get-ToolchainManifest).qt
    return "C:\Qt\$($qt.version)\$($qt.windowsKit)"
}

# Windows caps a command line at 32767 characters. Refusing ~2200 FFmpeg
# components by name -- which is the only way to refuse them, because Meson has
# no wildcard and its auto_features cannot be scoped to a subproject -- is well
# past that, and meson.exe fails to start at all with "The filename or extension
# is too long". Meson reads the same settings from a native file.
function Write-FfmpegNativeFile {
    param(
        [Parameter(Mandatory)] [string] $ComponentOptions,
        [Parameter(Mandatory)] [string] $Destination
    )

    $generator = Join-Path (Get-RepositoryRoot) 'tools\ffmpeg-capabilities.py'
    & python $generator meson --platform windows --component-options $ComponentOptions --native-file $Destination
    if ($LASTEXITCODE -ne 0) {
        throw 'Generating the manifest-controlled Windows FFmpeg feature set failed.'
    }
    if (-not (Test-Path -LiteralPath $Destination)) {
        throw "The FFmpeg feature set was not written to $Destination."
    }
    return $Destination
}

function Initialize-WindowsBuildEnvironment {
    Import-MsvcEnvironment

    $cmakeBin = Join-Path $env:ProgramFiles 'CMake\bin'
    if (Test-Path -LiteralPath $cmakeBin) {
        $env:PATH = "$cmakeBin;$env:PATH"
    }

    $qtRoot = if ($env:JELLYFIN_QT_ROOT) { $env:JELLYFIN_QT_ROOT } else { Get-DefaultQtRoot }
    $qcoroRoot = if ($env:JELLYFIN_QCORO_ROOT) { $env:JELLYFIN_QCORO_ROOT } else { 'C:\Qt\qcoro-0.13-msvc2022' }
    $mpvRoot = if ($env:JELLYFIN_MPV_ROOT) { $env:JELLYFIN_MPV_ROOT } else { Join-Path (Get-RepositoryRoot) 'build\windows-deps\mpv' }

    foreach ($path in @($qtRoot, $qcoroRoot)) {
        if (-not (Test-Path -LiteralPath $path)) {
            throw "Required Windows dependency prefix was not found: $path"
        }
    }

    $env:JELLYFIN_QT_ROOT = $qtRoot
    $env:JELLYFIN_QCORO_ROOT = $qcoroRoot
    $env:JELLYFIN_MPV_ROOT = $mpvRoot
    $env:JELLYFIN_WINDOWS_PREFIX_PATH = "$qtRoot;$qcoroRoot"
    $env:PATH = "$qtRoot\bin;$env:PATH"
}

function Initialize-WindowsMpvBuildEnvironment {
    Import-MsvcEnvironment

    $toolDirectories = @(
        (Join-Path $env:ProgramFiles 'LLVM\bin'),
        (Join-Path $env:ProgramFiles 'Meson'),
        (Join-Path $env:ProgramFiles 'NASM'),
        (Join-Path $env:ProgramFiles 'Git\usr\bin')
    )
    foreach ($directory in $toolDirectories) {
        if (Test-Path -LiteralPath $directory) {
            $env:PATH = "$directory;$env:PATH"
        }
    }

    $requiredTools = @('clang.exe', 'clang++.exe', 'lld-link.exe', 'llvm-rc.exe', 'meson.exe', 'ninja.exe', 'nasm.exe')
    foreach ($tool in $requiredTools) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            throw "Required Windows mpv build tool was not found: $tool"
        }
    }

    $env:CC = 'clang'
    $env:CXX = 'clang++'
    $env:CC_LD = 'lld-link'
    $env:CXX_LD = 'lld-link'
    $env:WINDRES = 'llvm-rc'
}

# The FFmpeg feature set is not returned here: it is thousands of options, far
# past what Windows will accept on a command line, so it travels as a Meson
# native file instead. See Write-FfmpegNativeFile.
function Get-MpvFeatureArguments {
    param(
        [Parameter(Mandatory)] [string] $Platform,
        [switch] $IncludeSubprojects
    )

    $manifestPath = Join-Path (Get-RepositoryRoot) 'tools\manifests\mpv-native.json'
    $manifest = Get-Content -LiteralPath $manifestPath -Raw | ConvertFrom-Json
    $platformArguments = $manifest.platforms.$Platform
    if ($null -eq $platformArguments) {
        throw "The mpv feature manifest has no platform named '$Platform'."
    }

    $arguments = @($manifest.common) + @($platformArguments)
    if ($IncludeSubprojects -and $manifest.subprojects.$Platform) {
        $arguments += @($manifest.subprojects.$Platform)
    }
    return @($arguments | ForEach-Object { "-D$_" })
}
