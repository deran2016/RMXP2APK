param(
    [Parameter(Mandatory=$true)][string]$Library,
    [Parameter(Mandatory=$true)][string]$Sdk,
    [Parameter(Mandatory=$true)][string]$Jdk,
    [Parameter(Mandatory=$true)][string]$OutputDir
)
$ErrorActionPreference = 'Stop'
$Output = [IO.Path]::GetFullPath($OutputDir)
if (Test-Path -LiteralPath $Output) { throw "Output already exists: $Output" }
$AndroidJar = Join-Path $Sdk 'platforms/android-36/android.jar'
$Tools = Join-Path $Sdk 'build-tools/36.1.0'
foreach ($Path in @($Library, $AndroidJar, (Join-Path $Tools 'aapt.exe'))) {
    if (!(Test-Path -LiteralPath $Path)) { throw "Required input is missing: $Path" }
}
New-Item -ItemType Directory -Path (Join-Path $Output 'classes'), (Join-Path $Output 'dex'), (Join-Path $Output 'lib/arm64-v8a') | Out-Null
Copy-Item -LiteralPath $Library -Destination (Join-Path $Output 'lib/arm64-v8a/liblunetile_save_probe.so')
$env:JAVA_HOME = $Jdk
$env:TMP = $Output
$env:TEMP = $Output
function Check-Exit { if ($LASTEXITCODE -ne 0) { throw "Build tool failed: $LASTEXITCODE" } }
& (Join-Path $Jdk 'bin/javac.exe') --release 8 -encoding UTF-8 -cp $AndroidJar -d (Join-Path $Output 'classes') (Join-Path $PSScriptRoot 'ProbeActivity.java')
Check-Exit
$Classes = Get-ChildItem -LiteralPath (Join-Path $Output 'classes') -Recurse -Filter '*.class' | ForEach-Object { $_.FullName }
& (Join-Path $Tools 'd8.bat') --min-api 26 --lib $AndroidJar --output (Join-Path $Output 'dex') $Classes
Check-Exit
& (Join-Path $Tools 'aapt.exe') package -f -M (Join-Path $PSScriptRoot 'AndroidManifest.xml') -I $AndroidJar -F (Join-Path $Output 'unsigned.apk')
Check-Exit
Copy-Item -LiteralPath (Join-Path $Output 'dex/classes.dex') -Destination (Join-Path $Output 'classes.dex')
Push-Location $Output
try { & (Join-Path $Tools 'aapt.exe') add unsigned.apk classes.dex lib/arm64-v8a/liblunetile_save_probe.so; Check-Exit } finally { Pop-Location }
& (Join-Path $Tools 'zipalign.exe') -f 4 (Join-Path $Output 'unsigned.apk') (Join-Path $Output 'aligned.apk')
Check-Exit
& (Join-Path $Jdk 'bin/keytool.exe') -genkeypair -keystore (Join-Path $Output 'probe.keystore') -storepass android -keypass android -alias probe -keyalg RSA -keysize 2048 -validity 365 -dname 'CN=Lunetile Save Diagnostic' | Out-Null
Check-Exit
& (Join-Path $Tools 'apksigner.bat') sign --ks (Join-Path $Output 'probe.keystore') --ks-key-alias probe --ks-pass pass:android --key-pass pass:android --out (Join-Path $Output 'save-diagnostic-no-game.apk') (Join-Path $Output 'aligned.apk')
Check-Exit
& (Join-Path $Tools 'apksigner.bat') verify (Join-Path $Output 'save-diagnostic-no-game.apk')
Check-Exit
Write-Output "Save diagnostic only; no game runtime: $(Join-Path $Output 'save-diagnostic-no-game.apk')"
