$ErrorActionPreference = 'Stop'
$root = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..\..'))
$build = Join-Path $root 'build'
$output = Join-Path $build 'Android-APK-Builder.exe'
if (Test-Path -LiteralPath $output) { throw "Output already exists: $output" }
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (-not $vs) { throw 'Visual Studio C++ x64 build tools were not found.' }
$vcvars = Join-Path $vs 'VC\Auxiliary\Build\vcvarsall.bat'
New-Item -ItemType Directory -Path $build -Force | Out-Null
$temp = Join-Path $build ('.android-gui-launcher-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $temp -ErrorAction Stop | Out-Null
$exe = Join-Path $temp 'launcher.exe'
$obj = Join-Path $temp 'launcher.obj'
$created = $false
try {
    $source = Join-Path $PSScriptRoot 'launcher.c'
    $command = 'call "{0}" x64 >nul && cl /nologo /W4 /O2 /MT /utf-8 "{1}" /Fo:"{2}" /Fe:"{3}" /link /SUBSYSTEM:WINDOWS /INCREMENTAL:NO user32.lib' -f $vcvars, $source, $obj, $exe
    & cmd.exe /d /s /c $command
    if ($LASTEXITCODE -ne 0) { throw 'Launcher compilation failed.' }
    $reader = [IO.File]::OpenRead($exe)
    try {
        $writer = [IO.File]::Open($output, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write)
        $created = $true
        try { $reader.CopyTo($writer) } finally { $writer.Dispose() }
    } finally { $reader.Dispose() }
} catch {
    if ($created) { Remove-Item -LiteralPath $output -Force }
    throw
} finally {
    if (Test-Path -LiteralPath $exe) { Remove-Item -LiteralPath $exe -Force }
    if (Test-Path -LiteralPath $obj) { Remove-Item -LiteralPath $obj -Force }
    Remove-Item -LiteralPath $temp -ErrorAction SilentlyContinue
}
Write-Host "GUI launcher: $output"
