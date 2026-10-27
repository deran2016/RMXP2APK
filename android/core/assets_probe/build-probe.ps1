param(
    [Parameter(Mandatory=$true)][string]$Sdk,
    [Parameter(Mandatory=$true)][string]$Jdk,
    [Parameter(Mandatory=$true)][string]$ScriptsFixture,
    [Parameter(Mandatory=$true)][string]$OutputDir
)
$ErrorActionPreference = 'Stop'
$Output = [IO.Path]::GetFullPath($OutputDir)
if (Test-Path -LiteralPath $Output) { throw "Output already exists: $Output" }
$AndroidJar = Join-Path $Sdk 'platforms/android-36/android.jar'
$Tools = Join-Path $Sdk 'build-tools/36.1.0'
foreach ($Path in @($ScriptsFixture, $AndroidJar, (Join-Path $Tools 'aapt.exe'))) {
    if (!(Test-Path -LiteralPath $Path)) { throw "Required input is missing: $Path" }
}
New-Item -ItemType Directory -Path (Join-Path $Output 'classes'), (Join-Path $Output 'dex'), (Join-Path $Output 'assets/game/Data'), (Join-Path $Output 'assets/game/Graphics') | Out-Null
$utf8 = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText((Join-Path $Output 'assets/game/Game.ini'), "[Game]`nScripts=Data/Scripts.rxdata`n", $utf8)
[IO.File]::WriteAllText((Join-Path $Output 'assets/game/android-build.json'), '{"data_encoding":"UTF-8","ini_encoding":"UTF-8"}', $utf8)
[IO.File]::WriteAllText((Join-Path $Output 'assets/game/Graphics/probe.png'), 'diagnostic image', $utf8)
Copy-Item -LiteralPath $ScriptsFixture -Destination (Join-Path $Output 'assets/game/Data/Scripts.rxdata')
$env:JAVA_HOME = $Jdk
$env:TMP = $Output
$env:TEMP = $Output
function Check-Exit { if ($LASTEXITCODE -ne 0) { throw "Build tool failed: $LASTEXITCODE" } }
& (Join-Path $Jdk 'bin/javac.exe') --release 8 -encoding UTF-8 -cp $AndroidJar -d (Join-Path $Output 'classes') (Join-Path $PSScriptRoot '../java/org/lunetile/host/GameAssets.java') (Join-Path $PSScriptRoot 'AssetsProbeActivity.java')
Check-Exit
$Classes = Get-ChildItem -LiteralPath (Join-Path $Output 'classes') -Recurse -Filter '*.class' | ForEach-Object { $_.FullName }
& (Join-Path $Tools 'd8.bat') --min-api 26 --lib $AndroidJar --output (Join-Path $Output 'dex') $Classes
Check-Exit
& (Join-Path $Tools 'aapt.exe') package -f -M (Join-Path $PSScriptRoot 'AndroidManifest.xml') -A (Join-Path $Output 'assets') -I $AndroidJar -F (Join-Path $Output 'unsigned.apk')
Check-Exit
Copy-Item -LiteralPath (Join-Path $Output 'dex/classes.dex') -Destination (Join-Path $Output 'classes.dex')
Push-Location $Output
try { & (Join-Path $Tools 'aapt.exe') add unsigned.apk classes.dex; Check-Exit } finally { Pop-Location }
& (Join-Path $Tools 'zipalign.exe') -f 4 (Join-Path $Output 'unsigned.apk') (Join-Path $Output 'aligned.apk')
Check-Exit
& (Join-Path $Jdk 'bin/keytool.exe') -genkeypair -keystore (Join-Path $Output 'probe.keystore') -storepass android -keypass android -alias probe -keyalg RSA -keysize 2048 -validity 365 -dname 'CN=Lunetile Assets Diagnostic' | Out-Null
Check-Exit
& (Join-Path $Tools 'apksigner.bat') sign --ks (Join-Path $Output 'probe.keystore') --ks-key-alias probe --ks-pass pass:android --key-pass pass:android --out (Join-Path $Output 'assets-diagnostic-no-game.apk') (Join-Path $Output 'aligned.apk')
Check-Exit
& (Join-Path $Tools 'apksigner.bat') verify (Join-Path $Output 'assets-diagnostic-no-game.apk')
Check-Exit
Write-Output "Assets diagnostic only; no game runtime: $(Join-Path $Output 'assets-diagnostic-no-game.apk')"
