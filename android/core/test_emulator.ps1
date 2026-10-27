param(
    [Parameter(Mandatory=$true)][string]$Adb,
    [Parameter(Mandatory=$true)][string]$Device,
    [Parameter(Mandatory=$true)][string]$Core,
    [Parameter(Mandatory=$true)][string]$Data,
    [Parameter(Mandatory=$true)][string]$FixtureRoot
)
$ErrorActionPreference = 'Stop'
$remote = '/data/local/tmp/lunetile-core-' + [Guid]::NewGuid().ToString('N')
& $Adb -s $Device shell mkdir $remote
if ($LASTEXITCODE -ne 0) { throw 'Could not create unique test directory.' }
try {
    & $Adb -s $Device push $Core "$remote/core"
    if ($LASTEXITCODE -ne 0) { throw 'Failed to push test executable.' }
    & $Adb -s $Device shell mkdir "$remote/data"
    foreach ($file in @('Actors.rxdata','Map001.rxdata','System.rxdata')) {
        & $Adb -s $Device push (Join-Path $Data $file) "$remote/data/$file"
        if ($LASTEXITCODE -ne 0) { throw "Failed to push $file" }
    }
    foreach ($kind in @('utf8','cp949','cp932')) {
        & $Adb -s $Device push (Join-Path $FixtureRoot "android-script-fixtures-$kind") "$remote/$kind"
        if ($LASTEXITCODE -ne 0) { throw "Failed to push $kind" }
    }
    & $Adb -s $Device shell ln -s /system/build.prop "$remote/utf8/link"
    if ($LASTEXITCODE -ne 0) { throw 'Could not create test symlink.' }
    & $Adb -s $Device shell chmod 700 "$remote/core"
    foreach ($test in @(@('data','utf8','UTF-8'),@('-','cp949','CP949'),@('-','cp932','CP932'))) {
        $dataPath = if ($test[0] -eq '-') { '-' } else { "$remote/data" }
        Write-Host "Testing $($test[2])"
        & $Adb -s $Device shell "$remote/core" $dataPath "$remote/$($test[1])" $test[2]
        if ($LASTEXITCODE -ne 0) { throw "Script test failed: $($test[2])" }
    }
} finally {
    & $Adb -s $Device shell rm -f "$remote/core" | Out-Null
    foreach ($file in @('Actors.rxdata','Map001.rxdata','System.rxdata')) { & $Adb -s $Device shell rm -f "$remote/data/$file" | Out-Null }
    & $Adb -s $Device shell rmdir "$remote/data" | Out-Null
    foreach ($kind in @('utf8','cp949','cp932')) {
        foreach ($file in @('good.rxdata','bad_zlib.rxdata','trailing_zlib.rxdata','invalid_text.rxdata','bad_entry.rxdata','probe.png')) { & $Adb -s $Device shell rm -f "$remote/$kind/$file" | Out-Null }
        if ($kind -eq 'utf8') { & $Adb -s $Device shell rm -f "$remote/$kind/link" | Out-Null }
        & $Adb -s $Device shell rmdir "$remote/$kind" | Out-Null
    }
    & $Adb -s $Device shell rmdir $remote | Out-Null
}
