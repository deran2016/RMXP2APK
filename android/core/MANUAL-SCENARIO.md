# Windows와 Android 수동 비교용 XP 프로젝트

`make_manual_scenario.py`는 `build/RGSSXP-validation-playable-20261018`을 읽어 `build/` 안의 새로운 테스트 복사본을 만듭니다. 이미 있는 대상 폴더는 사용하지 않습니다. 원본의 모든 파일을 복사 전후 해시로 비교하고 오디오 280개 파일도 사본과 비교합니다. 원본 세이브와 개인 파일은 복사하지 않습니다.

복사본에서는 기존 테스트 전용 동적 훅 `RGSSXP stock boot test` 한 개만 제거합니다. Map001 이벤트 1은 대화·선택·연출·전투·맵 이동 명령을 유지하며, 수동 저장을 먼저 할 수 있도록 위치를 (4,4)에서 (9,8), 트리거를 자동 실행에서 결정 버튼으로 바꾸고 기존 `001-Fighter01` 그래픽을 지정합니다. 그 외 맵 데이터는 역직렬화·원복 후 비교합니다. 이 복사본은 사용자가 만든 원본 프로젝트가 아닌 통제된 시험 데이터입니다.

```powershell
python -X utf8 android/core/make_manual_scenario.py build/NEW_FIXTURE --ruby 'E:\Develop\RGSSXP\build\runtime-native\bin\Release\rgssxp-ruby.exe'
```

이번 테스트 복사본의 `Data/Scripts.rxdata` SHA-256은 `2de0b5ebe5913ceb4eb200421dc08f1aec1f318471601fd049d482e5fd5af13d`, `Data/Map001.rxdata`는 `0b84687bfcef430e3134976d48da54c0c5f464f11460902bbd9ca0f8bea8ca0e`입니다. 원본의 두 파일은 각각 `d3afc04ff450a25e35ef243654d69814ab4d10347fb393f7a1a7574790fb2fc0`, `85fc10528ca27c847784f1d458c2a1abead97a32620e2b7da8dcf733e1732752`를 유지했습니다. 원본 Title BGM `064-Slow07.mid`, 전투 BGM `001-Battle01.mid`를 포함해 오디오 교체는 하지 않았습니다.

## 현재 Windows Lunetile 기준

별도 `build/android-manual-scenario-lunetile-1`에서 `Game.ini`의 라이브러리만 `Lunetile.dll`로 지정하고 `E:\Develop\RGSSXP\build\runtime-native\bin\Release\Lunetile.dll`을 복사했습니다. 실행 중 실제 로드된 DLL의 SHA-256은 `d93c2bd8d3ef313bdd6bdca2b8dc96faf8fd67563330af8634285f2dc0baf06f`입니다.

실제 Windows 게임 창에서 다음 키를 입력했습니다.

1. `New Game`에서 Enter → Left → Right → Escape. 메뉴에서 걸음 수 2, 0G를 확인합니다.
2. Down 네 번 → Enter(`Save`) → Enter(`File1`)로 저장합니다.
3. 게임 프로세스를 완전히 종료하고 다시 시작합니다. `Continue` → `File1`을 선택해 로드합니다.
4. 맵에서 Escape로 메뉴를 확인하면 걸음 수 2, 0G, 4명의 HP/SP가 저장 전과 같습니다. 메뉴를 닫고 Down으로 남쪽 이벤트를 향한 후 Enter를 누르면 대화/선택지가 표시됩니다. `Continue`를 선택하면 전투 화면에 진입합니다.

저장 데이터는 읽기 전용으로 검사했습니다: SAVE v1, map 1, 위치 (9,7), 방향 6, 걸음 2, 0G, save_count 1, switch 3/4/5 미설정. 파티 `[id, HP, SP, level, exp]`는 `[1,741,534,1,0]`, `[2,695,591,1,0]`, `[7,486,661,1,0]`, `[8,422,606,1,0]`입니다. 병렬 이벤트 변수 2는 40이었지만 경과 시간에 따라 달라지므로 플랫폼 비교는 `> 0`만 사용합니다. 저장 SHA-256은 `0b6e54f8c307c4e06c703c4d673d7eece90bf6ff45468c90456bf62b82e70727`입니다.

이 시험은 실제 기기 오디오 청취, 전투 승리·엔딩, Android와의 동등성이나 원본 게임 전체 호환을 증명하지 않습니다. 이전 별도 시험에서 `RGSS104E.dll` SHA-256 `3868eece...`를 사용한 결과는 현재 `Lunetile.dll` 결과와 구분합니다.

## Android 저장 비교

같은 입력 데이터로 만든 진단 APK `build/android-builder-reentry-diagnostic.apk`의 SHA-256은 `aec53eb8ca5217037b4664444e02d5e22fc33abc77371da49b361d7d415fda93`입니다. Android 16/API 36 x86_64 에뮬레이터의 ARM64 변환 실행 환경에서 원본 오디오를 사용해 `New Game → 왼쪽 → 오른쪽 → 메뉴 Save → File1 → 시스템 Back으로 종료 → 앱 재실행 → Continue → File1`을 수행했습니다. 맵과 메뉴 복귀를 확인했습니다. 이 APK는 게임 진단용이며 일반 빌더의 완성 또는 실제 기기 검증을 뜻하지 않습니다.

Android에서 추출한 `build/android-builder-reentry-save1.rxdata`의 SHA-256은 `4a5dd59d8a11f92f40caedd06dd29f183b167cbe2757eb6137251f49430620ca`입니다. Windows와 동일한 읽기 전용 Ruby 검사기로 두 SAVE v1 파일을 각각 읽은 결과는 다음과 같습니다.

| 저장 필드 | Windows Lunetile | Android 진단 |
|---|---|---|
| 맵·좌표·방향 | 1, (9,7), 6 | 1, (9,7), 6 |
| 걸음·골드·저장 횟수 | 2, 0, 1 | 2, 0, 1 |
| 파티 ID·HP·SP·레벨·경험치 | 위 네 명의 값 | 네 명 모두 일치 |
| 스위치 3·4·5 | 저장 내부값 `nil` | 저장 내부값 `nil` |
| 병렬 이벤트 변수 2 | 40 | 5 |

스위치의 공개 게임 값은 양쪽 모두 false입니다. 변수 2는 5프레임 간격으로 증가하므로 사용자 입력 시간에 따른 차이이며 양쪽 모두 0보다 큽니다. 바이트 해시와 플레이 시간은 일치할 필요가 없습니다. Windows에서 전투 화면 진입을 관찰했으나, 위 Android 재진입 시험은 전투 승리·엔딩까지 비교한 결과가 아닙니다. 원본 음원을 포함하고 사용했으나 실제 스피커의 소리를 청취하거나 파형을 비교하지는 않았습니다.
