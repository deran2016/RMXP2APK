<div align="center">
  <img src="docs/images/logo.png" alt="RMXP2APK 로고" width="200">

# RMXP2APK

**Lunetile XP 프로젝트를 Android APK로 빌드하는 실험적인 도구**

[![Status: Experimental](https://img.shields.io/badge/status-experimental-orange)](#지원-범위와-제한사항)
[![Platform: Windows](https://img.shields.io/badge/builder-Windows-0078D4)](#사전-준비)
[![Android: 8.0+](https://img.shields.io/badge/Android-8.0%2B%20%7C%20ARM64-3DDC84)](#지원-범위와-제한사항)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue)](LICENSE)

[빠른 시작](#빠른-시작) · [명령줄 사용](#명령줄-사용) · [지원 범위](#지원-범위와-제한사항) · [개발 및 검증](#개발-및-검증)
</div>

## 소개

RMXP2APK는 Windows에서 Lunetile XP 프로젝트를 Android ARM64용 개발 서명 APK로 빌드합니다. GUI와 명령줄을 제공하며, 게임 원본을 수정하지 않고 새 경로에 결과를 만듭니다.

> **실험적 프로젝트:** 모든 RPG Maker XP 게임이나 사용자 스크립트의 동작을 보장하지 않습니다. 생성된 APK는 Play 스토어용 정식 서명이나 AAB가 아닙니다.

### 주요 기능

- `Game.ini`와 프로젝트 데이터 및 정적 리소스 경로를 검사하고 지원하지 않는 기능을 보고합니다.
- UTF-8, CP949, CP932 인코딩과 명시적으로 지정한 단일 RTP를 지원합니다.
- 검토된 기본 XP 저장 스크립트를 스테이징 복사본에서 변환하고 원본을 보존합니다.
- 640×480 게임 화면과 가로 화면 터치 조작을 제공합니다. 방향 버튼을 누른 채 다른 방향으로 밀 수 있으며 방향과 결정·취소를 함께 사용할 수 있습니다.

## 사전 준비

Windows 환경에서 다음 도구가 필요합니다.

| 구성 요소 | 요구 사항 |
| --- | --- |
| Python | 3.12 및 tkinter |
| Java | JDK 21 |
| Android | SDK 36, Build Tools 36.1.0, NDK 28.2.13676358 |
| Gradle | 저장소에 포함된 8.13 래퍼 |
| 데이터 검사기 | Windows용 Ruby 4.0.7 x86 검사기 `build/runtime-native/bin/Release/rgssxp-ruby.exe` 또는 별도 지정 경로 |
| Android Ruby | Ruby 4.0.7 ARM64 정적 빌드와 생성된 인코딩 헤더 |

Android Ruby는 `build/android-ruby-arm64-pic/`에 준비하거나 `LUNETILE_ANDROID_RUBY_ROOT`로 위치를 지정합니다. 해당 디렉터리에는 `source/`, `build/libruby-static.a` 및 생성된 인코딩 헤더가 필요합니다. 새 PC로 소스를 옮길 때 데이터 검사기와 Android Ruby 빌드는 별도로 준비해야 합니다.

Android Ruby를 직접 빌드하려면 MSYS2 bash, Ruby 3.1 이상과 ERB, `make`, 공백이 없는 NDK 경로가 필요합니다. 스크립트는 Ruby 4.0.7 소스 아카이브의 SHA-256을 확인하고 기존 출력 폴더를 덮어쓰지 않습니다.

```bash
bash android/runtime/build-ruby-arm64.sh /path/to/ruby-4.0.7.tar.gz /path/to/android-ndk /path/to/new-android-ruby-dir
```

환경에 맞게 경로를 바꿔 설정합니다.

```powershell
$env:JAVA_HOME = 'C:\Path\To\JDK21'
$env:ANDROID_SDK_ROOT = "$env:LOCALAPPDATA\Android\Sdk"
$env:LUNETILE_ANDROID_RUBY_ROOT = 'C:\Path\To\new-android-ruby-dir'
```

## 빠른 시작

저장소 루트에서 GUI를 실행합니다.

```powershell
pythonw android/builder/gui.py
```

1. `Game.ini`가 있는 XP 프로젝트와 프로젝트 밖의 새 `.apk` 출력 경로를 선택합니다.
2. 원본에 맞는 Game.ini·데이터 인코딩을 선택합니다. 필요하면 검사기 Ruby와 단일 RTP 폴더를 지정합니다.
3. **APK 빌드 시작**을 누르고 로그에서 진행 상황과 오류를 확인합니다.

Game.ini에 RTP가 지정되어 있다면 해당 RTP의 `Graphics`와 `Audio`를 포함하는 폴더가 필요합니다. 게임과 RTP 자산을 포함하려면 해당 사용·배포 권한이 있어야 합니다.

Visual Studio C++ x64 빌드 도구가 있다면 다음 명령으로 `build/Android-APK-Builder.exe` 런처를 만들 수 있습니다. 기존 런처는 덮어쓰지 않습니다. 이 런처는 저장소의 소스와 빌드 도구를 사용하며 독립 배포본이 아닙니다.

```powershell
powershell -NoProfile -File android/builder/build_launcher.ps1
```

## 명령줄 사용

```powershell
python -X utf8 android/builder/build_android.py "C:\MyGame" "C:\Output\MyGame-debug.apk" --ini-encoding UTF-8 --data-encoding UTF-8 --ruby "build/runtime-native/bin/Release/rgssxp-ruby.exe" --rtp-dir "C:\RPGXP-RTP"
```

인코딩은 원본에 맞게 `UTF-8`, `CP949`, `CP932` 중 선택합니다. RTP가 없는 프로젝트에서는 `--rtp-dir`을 생략합니다. 원본과 기존 APK는 덮어쓰지 않습니다. 연결된 기기에는 `adb install "C:\Output\MyGame-debug.apk"`로 설치할 수 있습니다.

### 앱 ID와 저장 데이터

기본 앱 ID는 프로젝트의 절대 경로를 기반으로 생성되므로 프로젝트를 옮기거나 이름을 바꾸면 달라집니다. 기존 설치와 저장 데이터를 유지하려면 처음 출력된 ID를 기록하고 이후 GUI의 앱 ID 또는 `--application-id org.example.game`에 지정하세요. 업데이트에는 동일한 디버그 서명 키도 필요합니다. 서로 다른 게임에 같은 ID를 지정하면 Android가 같은 앱으로 취급합니다.

## 지원 범위와 제한사항

- **대상:** Android 8 이상, ARM64, 640×480 및 검토된 기본 XP 스크립트.
- **지원하지 않음:** 더 큰 해상도, 암호화된 RGSSAD 전용 프로젝트, 다중 RTP, 검토되지 않은 저장 스크립트, Windows DLL·Win32 API, MP3·WMA 및 Lunetile의 Windows 전용 확장 기능.
- **리소스와 오디오:** 검사기는 동적으로 구성된 모든 리소스 경로를 확인하지 못합니다. WAV·OGG·MIDI 재생은 Android 기기에 따라 달라질 수 있습니다.

별도의 XP 검증 프로젝트를 Android 에뮬레이터에서 실행해 타이틀, 이동, 이벤트, 전투, 저장과 재실행 후 불러오기를 확인했습니다. 이 결과는 해당 시나리오에 한정되며 모든 게임이나 실기기 전체 동작을 검증한 것은 아닙니다.

## 개발 및 검증

저장소 루트에서 Python 테스트를 실행합니다.

```powershell
python -X utf8 -B -m unittest discover -s android/builder -p 'test_*.py' -v
python -X utf8 -B -m unittest discover -s android/core -p 'test_*.py' -v
```

- [`android/builder/`](android/builder/) — 빌더 GUI, CLI 및 테스트
- [`android/core/`](android/core/) — 데이터 코어와 로더
- [`android/runtime/`](android/runtime/) — Android 런타임과 네이티브 빌드
- [`android/host/`](android/host/) · [`android/audio/`](android/audio/) · [`android/input/`](android/input/) — 화면, 오디오와 입력
- [`scripts/`](scripts/) — 프로젝트 검사 및 데이터 마이그레이션 도구

모듈별 README에서 빌드와 에뮬레이터 검증 방법을 확인하세요. 기여 지침은 [AGENTS.md](AGENTS.md)에 있습니다.

## 라이선스

이 프로젝트는 [MIT 라이선스](LICENSE)를 따릅니다. 게임 및 RTP 자산에는 별도의 권리가 적용될 수 있습니다.
