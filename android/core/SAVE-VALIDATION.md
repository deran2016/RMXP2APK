# Android 저장 코어 검증

Android Activity의 `getFilesDir()`을 `lunetile_android_init_saves`에 전달하면 `Saves/RGSSXP/Save1.rxdata`부터 4개 슬롯을 그 앱 전용 저장소에 만듭니다. 40Hz에서는 SAVE v1, 120Hz에서는 SAVE v2 헤더와 Marshal 스트림을 사용하며 검증된 최근 저장 최대 세 개를 백업합니다. 쓰기 전에 직렬화 결과를 검사하고, 같은 디렉터리의 새 임시 파일을 flush/fsync한 뒤 rename으로 교체합니다. 쓰기 실패 시 임시 파일을 정리합니다.

120Hz 모드에서는 기존 40Hz 저장을 불러올 때 시간 관련 필드와 프레임 수를 변환합니다. 40Hz 모드는 120Hz 저장을 읽거나 해당 슬롯에 덮어쓰지 않습니다. 원본 RGSS 세이브 가져오기, 실제 Windows Lunetile 저장파일과의 교차 호환, 게임 저장 장면 연결, 갑작스러운 전원 차단 내구성은 검증하지 않았습니다. 이 구현이 호환 가능한 전체 게임 저장 기능이라는 뜻은 아닙니다.

## 진단 APK

Android Emulator `emulator-5562`(Android 16/API 36, x86_64/arm64 ABI)에서 다음 순서를 실행했습니다.

1. 앱 전용 디렉터리에 `{map: 1, value: 1977}`을 저장하고, 잘못된 저장을 시도한 후 원래 바이트가 유지되는지 확인.
2. `{map: 1, value: 1978}`을 저장한 뒤 직전 값의 백업 및 40Hz 모드의 120Hz 헤더 거부와 원본 바이트 보존을 확인.
3. 앱을 강제 종료하고 다른 프로세스에서 다시 실행해 저장값과 백업을 읽음.

```text
10-27 09:46:27.952 10571 10571 I LunetileSaveProbe: PASS write and rollback
10-27 09:46:32.569 10661 10661 I LunetileSaveProbe: PASS restart read and backup
Storage threshold original=null restored=null
```

진단 APK SHA-256: `5b9bb11be66adb70087ba29a94f4d01313e27d7b0e0389fdd100943858ac784a`. 실행 당시 네이티브 라이브러리 SHA-256: `01705570ec4944690ab097b0d6844be8edfff92b38c2e7deB194330423752701`. 둘 다 게임 APK가 아닙니다. 저장 공간 경고 임계값은 임시로 1로 낮춘 뒤 원래 `null`로 복원했고, 새로 설치한 `org.lunetile.savediagnostic20261027` 진단 패키지만 삭제했습니다.

## 빌드와 시험

먼저 [데이터 코어 빌드](README.md)의 CMake 구성과 빌드를 실행합니다. 아래 명령은 기존 출력 디렉터리가 있으면 중단합니다.

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File android\core\probe\build-probe.ps1 `
  -Library 'C:\Path\To\build\android-core-clean\liblunetile_save_probe.so' `
  -Sdk 'C:\Android\Sdk' `
  -Jdk 'C:\Program Files\Eclipse Adoptium\jdk-21.0.11.10-hotspot' `
  -OutputDir 'C:\Path\To\build\android-core-save-probe-4'
```

장치에 같은 패키지가 이미 있으면 설치하지 마세요. 시험 시 `adb install` 후 `am start -W -n org.lunetile.savediagnostic20261027/.ProbeActivity --es mode write`, `am force-stop org.lunetile.savediagnostic20261027`, 같은 명령의 `--es mode read`를 차례로 실행하고 `LunetileSaveProbe` 로그를 확인합니다. 장치 설정은 사전에 확인하고 복원하며 새로 설치한 진단 앱만 삭제합니다.

## 120Hz 추가 검증

검토된 기본 XP 템플릿의 120Hz APK를 60Hz Android 16 에뮬레이터에서 실행하여 타이틀, 맵, 저장 및 앱 재시작 후 불러오기를 확인했습니다. 별도의 Ruby 테스트는 40Hz 저장 읽기, 120Hz 저장, 40Hz 덮어쓰기 방지와 시간 필드의 단일 변환을 확인합니다. 실기기, 실제 화면 120FPS, 전투 및 전체 게임은 검증하지 않았습니다.
