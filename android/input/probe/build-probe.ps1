param(
    [Parameter(Mandatory=$true)][string]$RubyRoot,
    [Parameter(Mandatory=$true)][string]$Sdk,
    [Parameter(Mandatory=$true)][string]$Ndk,
    [Parameter(Mandatory=$true)][string]$Jdk,
    [Parameter(Mandatory=$true)][string]$OutputDir
)
$ErrorActionPreference = 'Stop'
$Output = [IO.Path]::GetFullPath($OutputDir)
if (Test-Path -LiteralPath $Output) { throw "Output already exists: $Output" }
$Build = Join-Path $RubyRoot 'build'
$Source = Join-Path $RubyRoot 'source'
$Library = Join-Path $Build 'libruby-static.a'
$Headers = Join-Path $Build '.ext/include/aarch64-linux-android-android'
$Toolchain = Join-Path $Ndk 'toolchains/llvm/prebuilt/windows-x86_64'
$AndroidJar = Join-Path $Sdk 'platforms/android-36/android.jar'
$Tools = Join-Path $Sdk 'build-tools/36.1.0'
foreach ($Path in @($Library, $Headers, $AndroidJar, (Join-Path $Toolchain 'bin/clang.exe'))) {
    if (!(Test-Path -LiteralPath $Path)) { throw "Required input is missing: $Path" }
}
New-Item -ItemType Directory -Path (Join-Path $Output 'classes'), (Join-Path $Output 'dex'), (Join-Path $Output 'lib/arm64-v8a') | Out-Null
$env:JAVA_HOME = $Jdk
$env:TMP = $Output
$env:TEMP = $Output
function Check-Exit { if ($LASTEXITCODE -ne 0) { throw "Build tool failed: $LASTEXITCODE" } }
& (Join-Path $Toolchain 'bin/clang.exe') '--target=aarch64-linux-android26' "--sysroot=$Toolchain/sysroot" -shared -fPIC "-I$Headers" "-I$Source/include" (Join-Path $PSScriptRoot 'input_probe.c') (Join-Path $PSScriptRoot '../input_jni.c') $Library -ldl -lm -lz -o (Join-Path $Output 'lib/arm64-v8a/libinput_ruby_probe.so')
Check-Exit
& (Join-Path $Jdk 'bin/javac.exe') --release 8 -encoding UTF-8 -cp $AndroidJar -d (Join-Path $Output 'classes') (Join-Path $PSScriptRoot 'InputProbeActivity.java') (Join-Path $PSScriptRoot '../../host/src/org/lunetile/host/TouchInputState.java')
Check-Exit
$Classes = Get-ChildItem -LiteralPath (Join-Path $Output 'classes') -Recurse -Filter '*.class' | ForEach-Object { $_.FullName }
& (Join-Path $Tools 'd8.bat') --min-api 26 --lib $AndroidJar --output (Join-Path $Output 'dex') $Classes
Check-Exit
& (Join-Path $Tools 'aapt.exe') package -f -M (Join-Path $PSScriptRoot 'AndroidManifest.xml') -I $AndroidJar -F (Join-Path $Output 'unsigned.apk')
Check-Exit
Copy-Item -LiteralPath (Join-Path $Output 'dex/classes.dex') -Destination (Join-Path $Output 'classes.dex')
Push-Location $Output
try { & (Join-Path $Tools 'aapt.exe') add unsigned.apk classes.dex lib/arm64-v8a/libinput_ruby_probe.so; Check-Exit } finally { Pop-Location }
& (Join-Path $Tools 'zipalign.exe') -f 4 (Join-Path $Output 'unsigned.apk') (Join-Path $Output 'aligned.apk')
Check-Exit
& (Join-Path $Jdk 'bin/keytool.exe') -genkeypair -keystore (Join-Path $Output 'probe.keystore') -storepass android -keypass android -alias probe -keyalg RSA -keysize 2048 -validity 365 -dname 'CN=Input Ruby Probe' | Out-Null
Check-Exit
& (Join-Path $Tools 'apksigner.bat') sign --ks (Join-Path $Output 'probe.keystore') --ks-key-alias probe --ks-pass pass:android --key-pass pass:android --out (Join-Path $Output 'input-ruby-probe-no-game.apk') (Join-Path $Output 'aligned.apk')
Check-Exit
& (Join-Path $Tools 'apksigner.bat') verify (Join-Path $Output 'input-ruby-probe-no-game.apk')
Check-Exit
Write-Output "Ruby input diagnostic only; no game runtime: $(Join-Path $Output 'input-ruby-probe-no-game.apk')"
