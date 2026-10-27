# Android RGSS 데이터 코어 검증

이 디렉터리는 Windows Lunetile에서 플랫폼 비의존 데이터 타입과 RPG 스키마를 가져온 실험용 구성입니다. `lunetile/`의 파일은 이 저장소의 `src/runtime`에서 원본 바이트 그대로 복사했으며, 해당 MIT 라이선스를 함께 보관합니다. 그래픽·입력·오디오·게임 실행기와 APK 빌드 기능은 포함하지 않습니다.

Ruby 4.0.7 초기화 시 `ruby_setup` 이후 `ruby_options` / `ruby_exec_node`를 실행해야 Marshal 등 기본 메서드가 공개됩니다. 그다음 `lunetile_android_init_data`를 `rb_protect`로 호출합니다. 앱에 연결할 때는 `lunetile_android_core`와 `lunetile_android_encodings`의 오브젝트를 Ruby 정적 라이브러리보다 먼저 링크해야 합니다.

## 확인한 환경과 산출물

- NDK 28.2.13676358, Android API 26, `arm64-v8a`, 클린 Ruby 4.0.7 PIC 정적 빌드.
- Android Emulator `emulator-5562`, Android 16/API 36, `ro.product.cpu.abilist=x86_64,arm64-v8a`. 에뮬레이터 실행 결과이며 실제 ARM 기기 검증은 아닙니다.
- 데이터·스크립트 검증 `lunetile_core_smoke` SHA-256: `f59497740f6da5041d1036dd18b234b1151da435e851e3f5d306f2044e54d27e`.
- Ruby `libruby-static.a` SHA-256: `a60e649149434ce9ace4f6b17b57d6c271eee5e931e58073337a392f4f27af0c`.
- `Actors.rxdata`: `a099769d59f40522e92ad0e0df44de2ee23307d2a2c1dd5a38d85bd93d640d1d`.
- `Map001.rxdata`: `85fc10528ca27c847784f1d458c2a1abead97a32620e2b7da8dcf733e1732752`.
- `System.rxdata`: `b927a69e05ec6fe3e76231a9d7fca33b606827078e0c7fd5d915db7710da179e`.

위 세 데이터는 `build/RGSSXP-validation-playable-20261018/Data/`의 UTF-8 검증 프로젝트에서 읽기만 했습니다. Table·Color·Rect·IntegerMap의 Marshal 왕복, Game_Map의 IntegerMap 복원, 잘못된 Table 입력 거부, 해당 XP 데이터 세 파일의 역직렬화, CP949 한글 한 글자와 CP932 일본어 한 글자의 UTF-8 양방향 변환이 통과했습니다. CP949/CP932 프로젝트 전체를 검증한 것은 아닙니다.

합성 `Scripts.rxdata`로 UTF-8·CP949·CP932 스크립트의 ID와 순서 및 원본 압축 바이트 보존, 인코딩 선언을 포함한 텍스트 변환, 손상 압축·압축 뒤 추가 바이트·손상 문자·잘못된 엔트리의 실행 전 거부도 확인했습니다. 선택한 게임 인코딩을 기준으로 평가 직전 UTF-8로 바꾸며 원본 스크립트 엔트리는 수정하지 않습니다. 게임의 Ruby 코드는 실행 권한을 가진 코드이며 이 로더는 신뢰하지 않는 스크립트를 격리하는 보안 샌드박스가 아닙니다. 검증 프로젝트의 원본 `Scripts.rxdata`는 변경하지 않았고 SHA-256은 `d3afc04ff450a25e35ef243654d69814ab4d10347fb393f7a1a7574790fb2fc0`입니다.

## 재현 명령

다음 명령은 이 PC의 경로를 사용합니다. 다른 환경에서는 SDK, NDK, Ruby 경로를 바꿔야 합니다. Ruby에서 생성한 `encdb.h`와 `transdb.h`가 필요합니다. 이번 검증은 Android Ruby 빌드에서 생성한 두 헤더를 사용했습니다. 같은 Ruby 4.0.7의 Windows Lunetile 생성본과 헤더 해시가 일치합니다.

```powershell
cmake -S android/core -B build/android-core-clean -G Ninja `
  '-DCMAKE_MAKE_PROGRAM=C:/Users/Inyeob Kim/AppData/Local/Android/Sdk/cmake/3.22.1/bin/ninja.exe' `
  '-DCMAKE_TOOLCHAIN_FILE=C:/Users/Inyeob Kim/AppData/Local/Android/Sdk/ndk/28.2.13676358/build/cmake/android.toolchain.cmake' `
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-26 `
  '-DRUBY_SOURCE_DIR=C:/Path/To/build/android-ruby-arm64-pic/source' `
  '-DRUBY_BUILD_DIR=C:/Path/To/build/android-ruby-arm64-pic/build' `
  '-DRUBY_LIBRARY=C:/Path/To/build/android-ruby-arm64-pic/build/libruby-static.a' `
  '-DRUBY_ENCODINGS_HEADER_DIR=C:/Path/To/build/android-ruby-arm64-pic/build'
cmake --build build/android-core-clean --parallel 4
python -X utf8 -m unittest discover -s android/core -p 'test_*.py' -v
```

장치 시험은 기존 경로를 덮어쓰지 않도록 새 임시 디렉터리를 만들고 실패 시 중단했습니다. 실행 후 명시한 시험 파일과 디렉터리만 삭제했습니다. 아래 합성 데이터 출력 경로는 이미 존재하면 중단합니다.

```powershell
python -X utf8 android\core\make_script_fixtures.py build\android-fixture2\android-script-fixtures-utf8 UTF-8
python -X utf8 android\core\make_script_fixtures.py build\android-fixture2\android-script-fixtures-cp949 CP949
python -X utf8 android\core\make_script_fixtures.py build\android-fixture2\android-script-fixtures-cp932 CP932
powershell -NoProfile -ExecutionPolicy Bypass -File android\core\test_emulator.ps1 `
  -Adb 'C:\Android\Sdk\platform-tools\adb.exe' -Device emulator-5562 `
  -Core 'C:\Path\To\build\android-core-clean\lunetile_core_smoke' `
  -Data 'C:\Path\To\build\RGSSXP-validation-playable-20261018\Data' `
  -FixtureRoot 'C:\Path\To\build\android-fixture2'
```

결과: 세 인코딩 각각 `PASS: RGSS values and script loader`.

별도 Android 앱 전용 저장 경로와 재시작 시험은 [저장 검증](SAVE-VALIDATION.md)에 기록했습니다.

## 앱 자산 검증

`GameAssets.prepare(Context)`는 APK `assets/game/`을 앱 전용 `filesDir/GameAssets/<내용 SHA-256>/`에 준비합니다. 기존 버전이 동일하면 재사용하고 다른 버전은 별도 디렉터리에 보관합니다. 기존 버전이 변경되었다면 덮어쓰지 않고 오류를 반환합니다. `Game.ini`, `android-build.json`, `Data`, `Game.ini`에 지정된 `Data/` 내 스크립트의 존재와 경로를 검사합니다. 저장 슬롯은 별도 `filesDir/Saves/RGSSXP/`에 유지합니다.

`assets-diagnostic-no-game.apk`(SHA-256 `dd6c3c68ae5e85fd5e090d6589c799807618ecef7d36f2036b1252f8c3f5e4f0`)를 같은 에뮬레이터에서 시험했습니다. 실제 AssetManager 사용, 동일 버전 재사용, 업데이트 버전 분리, 기존 저장 보존, CP949·CP932 Game.ini, 경로 탈출·스크립트 누락·손상된 메타데이터 거부, 변경된 기존 자산 덮어쓰기 거부가 통과했습니다. 진단 패키지는 삭제했고 임계값은 `null`로 복원했습니다. Ruby 자원 조회의 확장자 탐색, 상위/절대 경로와 외부 심볼릭 링크 차단은 별도 ARM64 코어 시험에서 통과했습니다.
