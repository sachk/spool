. (Join-Path $PSScriptRoot 'common.ps1')
Initialize-WindowsBuildEnvironment
$root = Get-RepositoryRoot
$buildDir = Join-Path $root 'build\windows-release\app'
$stageDir = Join-Path $root 'build\windows-release\stage'
$exe = Join-Path $buildDir 'jellyfin-native.exe'

if (-not (Test-Path -LiteralPath $exe)) {
    throw "Release executable was not found: $exe"
}

if (Test-Path -LiteralPath $stageDir) {
    Remove-Item -LiteralPath $stageDir -Recurse -Force
}
New-Item -ItemType Directory -Path $stageDir | Out-Null
Copy-Item -LiteralPath $exe -Destination $stageDir

& (Join-Path $env:JELLYFIN_QT_ROOT 'bin\windeployqt.exe') `
    --release --no-translations --no-system-d3d-compiler --no-system-dxc-compiler `
    --no-compiler-runtime --no-opengl-sw `
    --skip-plugin-types qmltooling,generic `
    --include-plugins qwebp `
    --exclude-plugins qsqlibase,qsqlmimer,qsqloci,qsqlodbc,qsqlpsql `
    --qmldir (Join-Path $root 'qml') (Join-Path $stageDir 'jellyfin-native.exe')
if ($LASTEXITCODE -ne 0) { throw 'windeployqt failed.' }

$webpPlugin = Join-Path $stageDir 'imageformats\qwebp.dll'
if (-not (Test-Path -LiteralPath $webpPlugin)) {
    throw 'Qt WebP support was not deployed. Install qt.qt6.6111.addons.qtimageformats with MaintenanceTool.'
}

$mpvRuntimeDlls = @(Get-ChildItem (Join-Path $env:JELLYFIN_MPV_ROOT 'bin') -Filter '*.dll' -File)
$mpvDll = $mpvRuntimeDlls | Where-Object Name -Like '*mpv*.dll' | Select-Object -First 1
if (-not $mpvDll) { throw "libmpv DLL was not found below $env:JELLYFIN_MPV_ROOT\bin" }
foreach ($runtimeDll in $mpvRuntimeDlls) {
    Copy-Item -LiteralPath $runtimeDll.FullName -Destination $stageDir
}

$crtDirectory = Join-Path $env:VCToolsRedistDir 'x64\Microsoft.VC143.CRT'
$crtRuntimeDlls = @(Get-ChildItem -LiteralPath $crtDirectory -Filter '*.dll' -File -ErrorAction SilentlyContinue)
if ($crtRuntimeDlls.Count -eq 0) {
    throw "The app-local MSVC runtime was not found below $crtDirectory"
}
foreach ($runtimeDll in $crtRuntimeDlls) {
    Copy-Item -LiteralPath $runtimeDll.FullName -Destination $stageDir
}

$licenseDir = Join-Path $stageDir 'licenses'
New-Item -ItemType Directory -Path $licenseDir | Out-Null
Copy-Item -LiteralPath (Join-Path $root 'app\notices\OPEN_SOURCE_NOTICES.txt') `
    -Destination (Join-Path $licenseDir 'OPEN_SOURCE_NOTICES.txt')
Copy-Item -LiteralPath (Join-Path $root 'LICENSE') `
    -Destination (Join-Path $licenseDir 'GPL-2.0-or-later.txt')
Copy-Item -LiteralPath (Join-Path $root 'qml\fonts\Inter-LICENSE.txt') `
    -Destination (Join-Path $licenseDir 'Inter-OFL.txt')
Copy-Item -LiteralPath (Join-Path $root 'qml\fonts\MaterialIcons-LICENSE.txt') `
    -Destination (Join-Path $licenseDir 'MaterialIcons-Apache-2.0.txt')
Copy-Item -LiteralPath (Join-Path $root 'qml\fonts\SourceSerif4-LICENSE.md') `
    -Destination (Join-Path $licenseDir 'SourceSerif4-OFL.md')

& (Join-Path $PSScriptRoot 'test-runtime-closure.ps1') -StageDirectory $stageDir

Write-Host "Staged Windows release: $stageDir"
