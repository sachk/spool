$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

function Import-MsvcEnvironment {
    if (Get-Command cl.exe -ErrorAction SilentlyContinue) {
        return
    }

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
    foreach ($line in $environment) {
        if ($line -match '^([^=]+)=(.*)$') {
            # Codex and some terminal hosts expose both PATH and Path. vcvars
            # updates PATH; importing the stale mixed-case duplicate afterward
            # would silently discard the compiler directories.
            if ($Matches[1] -ceq 'Path') { continue }
            Set-Item -Path "Env:$($Matches[1])" -Value $Matches[2]
        }
    }
}

function Get-RepositoryRoot {
    return (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

function Initialize-WindowsBuildEnvironment {
    Import-MsvcEnvironment

    $cmakeBin = Join-Path $env:ProgramFiles 'CMake\bin'
    if (Test-Path -LiteralPath $cmakeBin) {
        $env:PATH = "$cmakeBin;$env:PATH"
    }

    $qtRoot = if ($env:JELLYFIN_QT_ROOT) { $env:JELLYFIN_QT_ROOT } else { 'C:\Qt\6.11.1\msvc2022_64' }
    $qcoroRoot = if ($env:JELLYFIN_QCORO_ROOT) { $env:JELLYFIN_QCORO_ROOT } else { 'C:\Qt\qcoro-0.12-msvc2022' }
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
