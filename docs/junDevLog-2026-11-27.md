# Chaos Tank Arena 개발 로그 — 2026-11-27

- **작성일**: 2026-11-27
- **담당**: 변준영 (`src/game/`, `namespace game`)
- **기준 커밋 / 작업 브랜치**: `game/cp01@9912e47` → `game/cp02`

---

## 1. 브랜치 및 기준선 정리

- `game/cp01`에 CP01 지상 사격 루프, `assets/boxing/cube.obj`, `assets/tanks/sand-tank.glb` 프리뷰 연결(`tmpBoxing::SandTankModel`, `makeSandTankSpec()`, `wheelSpinRad`)을 커밋(`9912e47`)했다.
- `game/cp01@9912e47`에서 `game/cp02` 브랜치를 분기해 CP02 공중 트릭샷 작업을 분리했다.

---

## 2. CP02 공중 트릭샷 구현 및 조작 개편 (`game/cp02`)

### 2.1 핵심 구현 항목

| 파일 | 주요 변경 내용 |
| --- | --- |
| `src/game/TrickshotController.h`, `src/game/TrickshotController.cpp` | `CP02-FOCUS` 우클릭 표시 자세 기반 최근접 대상 지정(`selectTarget`, 반경 `R` 미적용), `CP02-AIR` 높이 `H(1.6m)`·수평 반경 `R(12.5m)`·생존·체공당 1회 조건(`canEnter`), `CP02-SHOT` 실제 시간(`rawDt`) 메인↔서브 카메라 선형 보간(`CameraRequest`)·`0.25x` 슬로우·제한 yaw/pitch 직접 조준·클릭 아이콘 1회·단발 잠금·착지/대상 소실 즉시 복귀 구현 |
| `src/game/TankEntity.h`, `src/game/TankEntity.cpp` | `RampSurface` 경사 주행·이탈 이륙·체공 중 중력·약한 4방향 공중 궤도 보정·착지 처리 추가. `WASD` 4방향 차체 이동(`W:-Z`, `S:+Z`, `A:-X`, `D:+X`) 및 이동 방향으로 차체 자동 정렬 적용(`sand-tank.glb` 바퀴 회전 `wheelSpinRad` 유지) |
| `src/game/TimeManager.h`, `src/game/TimeManager.cpp` | `R` 키 홀드(`turretAimActive`)로 상체(포탑·포신) 마우스 회전 제어, 재시작 키 `F5`로 이동, 우클릭(`selectTargetRequested`), 실시간 마우스 델타(`consumeRawMouseDelta`), `F3` QA-12 대상 소실 테스트 트리거 추가 |
| `src/game/ArenaScene.h`, `src/game/ArenaScene.cpp` | 오른쪽 레인 점프 램프·적 좌측 전방 불투명 바리케이드(QA-08 차폐 검사용) 배치, 표시 자세 콜라이더 캐시(`displayedColliders_`), 선택 대상 청록 강조·반경 `R` 바닥 원(F1 디버그), `F2` 단위 검사에 `QA-09` 경계 검사 추가 |
| `src/game/Sources.props` | `TrickshotController.h`, `TrickshotController.cpp` MSBuild 항목 등록 |
| `src/tmpBoxing/PreviewRenderer.cpp` | 트릭샷 `AimReady` 상태에서 2D NDC 조준선·좌클릭 아이콘 오버레이 출력(깊이 테스트 상태 복원 포함) |
| `src/main.cpp` | `rawDt` 기반 `updateRealTime`, 동적 `timeScale`을 `TimeManager::beginFrame`에 전달, `CameraRequest` 메인↔서브 보간 적용, 조작 안내 및 창 제목 상태 표시 갱신 |

### 2.2 현재 조작 체계 (`game/cp02`)

- **몸체(차체) 이동**: `W`/`A`/`S`/`D` 4방향 이동(이동 방향으로 차체 정렬, 체공 중 약한 궤도 보정)
- **상체(포탑·포신) 회전**: `R` 키 홀드 + 마우스 조준 (방향키 `←`/`→`/`↑`/`↓`로 단독 부위 회전 확인 유지)
- **사격 / 대상 지정**: 좌클릭(`LMB`) 발사, 우클릭(`RMB`) 적 포커스 지정·해제
- **카메라 / 검사 / 재시작**: 휠 클릭(`MMB`) 홀드 카메라 회전, 숫자패드 `8/2/4/6/9/3` 카메라 이동, `F1` 디버그 표시 토글, `F2` 단위 검사(`QA-02·03·06·09`), `F3` QA-12용 선택 대상 소실 트리거, `F5` 재시작, `ESC` 종료

---

## 3. 검증 상태 및 다음 단계

- **완료된 확인**: 신규·수정 파일 UTF-8 BOM + CRLF 저장, `git diff --check` 공백·개행 점검 및 `game/cp02` 커밋 완료.
- **사용자 확인 대기**: Visual Studio `Debug | x64` 빌드 및 실행 후 조작(`WASD` 4방향, `R` 홀드 상체 회전, `F5` 재시작)과 `QA-08` ~ `QA-12` 시나리오 검증.
- **다음 단계**: CP02 시연·검증 완료 후 11/29 팀 협의에 맞춰 `CP03`(공용 `RenderBridge.h` 및 `ExternalRenderBackend` 연결) 진행.
