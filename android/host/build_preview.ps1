param(
    [Parameter(Mandatory = $true)][string]$Sdk,
    [Parameter(Mandatory = $true)][string]$Jdk,
    [Parameter(Mandatory = $true)][string]$OutputDir,
    [string]$AudioFixtures
)
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '../..')).Path
$Output = [System.IO.Path]::GetFullPath($OutputDir)
if (Test-Path -LiteralPath $Output) { throw "Output already exists: $Output" }
$AndroidJar = Join-Path $Sdk 'platforms/android-36/android.jar'
$Tools = Join-Path $Sdk 'build-tools/36.1.0'
$env:JAVA_HOME = $Jdk
New-Item -ItemType Directory -Path (Join-Path $Output 'classes'), (Join-Path $Output 'dex') | Out-Null
function Check-Exit { if ($LASTEXITCODE -ne 0) { throw "Build tool failed: $LASTEXITCODE" } }
$Sources = Get-ChildItem -LiteralPath (Join-Path $PSScriptRoot 'src') -Recurse -Filter '*.java' | ForEach-Object { $_.FullName }
& (Join-Path $Jdk 'bin/javac.exe') -encoding UTF-8 -cp $AndroidJar -d (Join-Path $Output 'classes') $Sources
Check-Exit
$Classes = Get-ChildItem -LiteralPath (Join-Path $Output 'classes') -Recurse -Filter '*.class' | ForEach-Object { $_.FullName }
& (Join-Path $Tools 'd8.bat') --min-api 23 --lib $AndroidJar --output (Join-Path $Output 'dex') $Classes
Check-Exit
$AaptArgs = @('package', '-f', '-M', (Join-Path $PSScriptRoot 'AndroidManifest.xml'), '-I', $AndroidJar, '-F', (Join-Path $Output 'unsigned.apk'))
if ($AudioFixtures) { $AaptArgs += @('-A', $AudioFixtures) }
& (Join-Path $Tools 'aapt.exe') $AaptArgs
Check-Exit
Copy-Item -LiteralPath (Join-Path $Output 'dex/classes.dex') -Destination (Join-Path $Output 'classes.dex')
Push-Location $Output
try { & (Join-Path $Tools 'aapt.exe') add unsigned.apk classes.dex; Check-Exit } finally { Pop-Location }
& (Join-Path $Jdk 'bin/keytool.exe') -genkeypair -keystore (Join-Path $Output 'preview.keystore') -storepass android -keypass android -alias preview -keyalg RSA -keysize 2048 -validity 365 -dname 'CN=Host Preview' | Out-Null
Check-Exit
& (Join-Path $Tools 'apksigner.bat') sign --ks (Join-Path $Output 'preview.keystore') --ks-key-alias preview --ks-pass pass:android --key-pass pass:android --out (Join-Path $Output 'host-preview-no-game.apk') (Join-Path $Output 'unsigned.apk')
Check-Exit
& (Join-Path $Tools 'apksigner.bat') verify (Join-Path $Output 'host-preview-no-game.apk')
Check-Exit
Write-Output "UI preview only (no game runtime): $(Join-Path $Output 'host-preview-no-game.apk')"
