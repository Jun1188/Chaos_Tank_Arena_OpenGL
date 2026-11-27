# Chaos Tank Arena 개발 로그 — 2026-11-27

- **작성일**: 2026-11-27
- **담당**: 변준영 (`src/game/`, `namespace game`)
- **기준 커밋 / 작업 브랜치**: `game/cp01@9912e47` → `game/cp02`

---

## 1. 브랜치 및 기준선 정리

- `game/cp01`에 CP01 지상 사격 루프, `assets/boxing/cube.obj`, `assets/tanks/sand-tank.glb` 프리뷰 연결(`tmpBoxing::SandTankModel`, `makeSandTankSpec()`, `wheelSpinRad`)을 커밋(`9912e47`)했다.
- `game/cp01@9912e47`에서 `game/cp02` 브랜치를 분기해 CP02 공중 트릭샷과 TABS식 코믹 물리·카메라·조작 개선을 진행했다.

---

## 2. CP02 구현 및 플레이 감각 개선 (`game/cp02`)

### 2.1 주요 구현·수정 항목

| 파일 | 주요 변경 내용 |
| --- | --- |
| `src/game/TrickshotController.h`, `src/game/TrickshotController.cpp` | `CP02-FOCUS` 우클릭 표시 자세 기반 최근접 대상 지정(`selectTarget`, DPI 창-뷰포트 좌표 정규화, 반경 `R` 미적용), `CP02-AIR` `canEnter` 경계 판정, `CP02-SHOT` 실제 시간(`rawDt`) + `smoothstep` 기반 `0.12x` 슬로우 및 1인칭 근접 백뷰(`FOV 45° → 26°`, 차체 자전과 분리된 월드 수평 안정 각도 + 위치·조준 방향 추종), `B` 즉시 진입/토글(`enterImmediate`, `cancelImmediate`) 구현 |
| `src/game/TankEntity.h`, `src/game/TankEntity.cpp` | `WASD` 4방향 차체 이동·정렬, `B` 키 랜덤·과장 TABS식 폭발 도약(`triggerExplosionJump`, `std::mt19937` 난수 기반 수평·수직 추진 및 3축 공중 회전), 발사 시 지상/공중 TABS 코믹 반동(`applyFireRecoil`), 하강 시 탱크 밑면(궤도)이 바닥을 향하도록 정자세 `slerp` 착지 보간, 장애물 최소 침투축 밀어내기(`resolveObstaclePenetrationXZ`)로 착지 후 `WASD` 끼임 해소 |
| `src/game/TimeManager.h`, `src/game/TimeManager.cpp` | 마우스 이동 기본 상체(포탑·포신) 조준, `R`(및 `F5`) 재시작, `B` 폭발 점프·슬로우 토글, `RMB` 눌림 시점 좌표 스냅샷(`selectCursorPos`), 실시간 마우스 델타(`consumeRawMouseDelta`), `F3` QA-12 대상 소실 테스트 트리거 추가 |
| `src/game/ArenaScene.h`, `src/game/ArenaScene.cpp` | 점프 램프·불투명 바리케이드 배치, 보간 위치(`renderAlpha`) 기준 실시간 카메라·포신 조준 동기화로 슬로우 중 버벅임 제거, `B` 입력 시 미지정 상태라도 최근접 생존 적 자동 포커스로 백뷰 전환 보장, 포탄 박스 빨간색(`0.96, 0.14, 0.12`) 표시 및 발사/폭발 이펙트, `F2` 단위 검사에 `QA-09` 추가 |
| `src/game/Sources.props` | `TrickshotController.h`, `TrickshotController.cpp` MSBuild 항목 등록 |
| `src/tmpBoxing/PreviewRenderer.cpp` | 트릭샷 `AimReady` 상태에서 2D NDC 조준선·좌클릭 아이콘 오버레이 출력(깊이 테스트 상태 복원 포함) |
| `src/main.cpp` | 데드존(`2.6m`)과 저반응 방향 감쇠를 적용한 둔감한 플레이어 추종 쿼터뷰 메인 카메라, `B` 근접 백뷰 전환 연결, `glfwGetWindowSize` 기반 DPI 픽킹 보정 및 창 제목 실시간 상태(`Focus`/`Trickshot`/`FOV`) 표시 |

### 2.2 현재 조작 체계 (`game/cp02`)

- **몸체(차체) 이동**: `W`/`A`/`S`/`D` 4방향 이동(이동 방향으로 차체 정렬, 체공 중 약한 4방향 궤도 보정)
- **상체(포탑·포신) 회전**: 마우스 이동(기본) / 방향키 `←`/`→`/`↑`/`↓`(단독 부위 회전 확인)
- **사격 / 반동**: 좌클릭(`LMB`) 발사 — 빨간색 포탄 발사 + 플레이어 탱크 TABS식 코믹 반동(지상 홉·뒤밀림, 공중 역추진·회전)
- **대상 지정 / 폭발 트릭샷**: 우클릭(`RMB`) 적 포커스 지정·해제, `B` 키 TABS식 랜덤 과장 폭발 점프 + `0.12x` 슬로우 + 포신 동기화 1인칭 근접 백뷰 진입/해제 토글
- **검사 / 재시작**: `F1` 디버그 표시 토글, `F2` 단위 검사(`QA-02·03·06·09`), `F3` QA-12 선택 대상 소실 트리거, `R`(또는 `F5`) 재시작, `ESC` 종료

---

## 3. 검증 상태 및 다음 단계

- **완료된 확인**: 전 신규·수정 파일 UTF-8 BOM + CRLF 저장, `git diff --check` 공백·개행 점검 및 `game/cp02` 커밋 완료.
- **사용자 확인 대기**: Visual Studio `Debug | x64` 빌드 및 실행 후 ① 둔감한 쿼터뷰 메인 카메라, ② `B` 랜덤 과장 점프·`0.12x` 슬로우·차체 자전과 독립된 포신 동기화 백뷰, ③ 빨간색 포탄과 지상/공중 코믹 반동, ④ 하강·착지 시 밑면 정렬 보간 및 착지 직후 `WASD` 동작 검증.
- **다음 단계**: CP02 시연·검증 완료 후 11/29 팀 협의에 맞춰 `CP03`(공용 `RenderBridge.h` 및 `ExternalRenderBackend` 연결) 진행.
