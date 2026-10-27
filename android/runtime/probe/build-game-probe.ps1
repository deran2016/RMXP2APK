param(
    [Parameter(Mandatory=$true)][string]$GameAssets,
    [Parameter(Mandatory=$true)][string]$NativeLibrary,
    [Parameter(Mandatory=$true)][string]$Sdk,
    [Parameter(Mandatory=$true)][string]$Ndk,
    [Parameter(Mandatory=$true)][string]$Jdk,
    [Parameter(Mandatory=$true)][string]$OutputDir
)
$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '../../..')).Path
$Output = [IO.Path]::GetFullPath($OutputDir)
if (Test-Path -LiteralPath $Output) { throw "Output already exists: $Output" }
$Tools = Join-Path $Sdk 'build-tools/36.1.0'
$AndroidJar = Join-Path $Sdk 'platforms/android-36/android.jar'
$Cxx = Join-Path $Ndk 'toolchains/llvm/prebuilt/windows-x86_64/sysroot/usr/lib/aarch64-linux-android/libc++_shared.so'
foreach ($Path in @($GameAssets, $NativeLibrary, $Cxx, $AndroidJar)) { if (!(Test-Path -LiteralPath $Path)) { throw "Missing input: $Path" } }
New-Item -ItemType Directory -Path (Join-Path $Output 'classes'), (Join-Path $Output 'dex'), (Join-Path $Output 'assets'), (Join-Path $Output 'lib/arm64-v8a') | Out-Null
Copy-Item -LiteralPath $GameAssets -Destination (Join-Path $Output 'assets/game') -Recurse
Copy-Item -LiteralPath $NativeLibrary -Destination (Join-Path $Output 'lib/arm64-v8a/liblunetile_game.so')
Copy-Item -LiteralPath $Cxx -Destination (Join-Path $Output 'lib/arm64-v8a/libc++_shared.so')
$env:JAVA_HOME = $Jdk
$env:TMP = $Output
$env:TEMP = $Output
function Check-Exit { if ($LASTEXITCODE -ne 0) { throw "Build tool failed: $LASTEXITCODE" } }
$Sources = @('android/host/src/org/lunetile/host/GameHostView.java','android/host/src/org/lunetile/host/Viewport.java',
  'android/host/src/org/lunetile/host/TouchInputState.java','android/host/src/org/lunetile/host/GameAudio.java',
  'android/audio/java/org/lunetile/audio/AudioBridge.java','android/audio/java/org/lunetile/audio/GameSession.java',
  'android/core/java/org/lunetile/host/GameAssets.java','android/app/java/org/lunetile/host/NativeGameEngine.java',
  'android/app/java/org/lunetile/host/GameActivity.java','android/runtime/java/org/lunetile/runtime/BitmapCodec.java') | ForEach-Object { Join-Path $Root $_ }
& (Join-Path $Jdk 'bin/javac.exe') --release 8 -encoding UTF-8 -cp $AndroidJar -d (Join-Path $Output 'classes') $Sources
Check-Exit
$Classes = Get-ChildItem -LiteralPath (Join-Path $Output 'classes') -Recurse -Filter '*.class' | ForEach-Object FullName
& (Join-Path $Tools 'd8.bat') --min-api 26 --lib $AndroidJar --output (Join-Path $Output 'dex') $Classes
Check-Exit
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'GameProbeManifest.xml') -Destination (Join-Path $Output 'AndroidManifest.xml')
& (Join-Path $Tools 'aapt.exe') package -f -M (Join-Path $Output 'AndroidManifest.xml') -I $AndroidJar -A (Join-Path $Output 'assets') -F (Join-Path $Output 'unsigned.apk')
Check-Exit
Copy-Item (Join-Path $Output 'dex/classes.dex') (Join-Path $Output 'classes.dex')
Push-Location $Output
try { & (Join-Path $Tools 'aapt.exe') add unsigned.apk classes.dex lib/arm64-v8a/liblunetile_game.so lib/arm64-v8a/libc++_shared.so; Check-Exit } finally { Pop-Location }
& (Join-Path $Tools 'zipalign.exe') -f 4 (Join-Path $Output 'unsigned.apk') (Join-Path $Output 'aligned.apk')
Check-Exit
& (Join-Path $Jdk 'bin/keytool.exe') -genkeypair -keystore (Join-Path $Output 'probe.keystore') -storepass android -keypass android -alias probe -keyalg RSA -keysize 2048 -validity 365 -dname 'CN=Game Probe' | Out-Null
Check-Exit
& (Join-Path $Tools 'apksigner.bat') sign --ks (Join-Path $Output 'probe.keystore') --ks-key-alias probe --ks-pass pass:android --key-pass pass:android --out (Join-Path $Output 'game-probe.apk') (Join-Path $Output 'aligned.apk')
Check-Exit
& (Join-Path $Tools 'apksigner.bat') verify (Join-Path $Output 'game-probe.apk')
Check-Exit
Write-Output "Experimental diagnostic only: $(Join-Path $Output 'game-probe.apk')"
