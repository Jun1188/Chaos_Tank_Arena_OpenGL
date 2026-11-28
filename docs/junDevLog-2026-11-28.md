# Chaos Tank Arena 개발 로그 — 2026-11-28

- **작성일**: 2026-11-28
- **담당**: 변준영 (`src/game/`, `namespace game`)
- **기준 커밋 / 작업 브랜치**: `game/cp02@e324733` → `game/cp02`

---

## 1. `e324733` 이후 구현·개선 내역 (`game/cp02`)

### 1.1 파일별 주요 변경 내용

| 파일 | 주요 변경 내용 |
| --- | --- |
| `src/game/TimeManager.h`, `src/game/TimeManager.cpp` | `Shift` 가속 돌진 홀드(`boostActive`), 좌·우클릭 홀드 및 해제 이벤트(`isLmbHeld`, `isRmbHeld`, `lmbReleased`, `rmbReleased`), 실제 마우스 이동 감지(`cursorMoved`), `GLFW_LOCK_KEY_MODS` 기반 소문자 `r`(`alignHullRequested`: 차체 자세 원위치)과 대문자 `R`(`restartRequested`: 장면 리셋, 보조 `F5`) 구분 처리 추가 |
| `src/game/TrickshotController.h`, `src/game/TrickshotController.cpp` | `ZoomTriggerSource`(`None`, `RightMouse`, `LeftHold`) 도입: 지상·체공 공통 우클릭 홀드 즉시 줌인(`FOV 45° → 34°`, `0.32x` 슬로우) 및 좌클릭 시 즉시 줌·슬로우 해제 후 발사, 좌클릭 `1.2초` 이상 홀드 시 줌인·슬로우 유지 및 버튼 해제 시 복귀·발사, `B` 체공 시 무줌인 약한 슬로우(`0.55x`) 토글, 5차 다항식(`smootherstep`) 줌 전환 및 둔감한 위치 추적(`camTrackPos_`)·깊이 검사용 `zoomAlphaFade`·`zoomFocusPoint` 산출 |
| `src/game/TankEntity.h`, `src/game/TankEntity.cpp` | 소문자 `r` 입력 시에만 현재 조준점을 향하도록 차체 정자세 및 `WASD` 전방 기준축(`driveBasisYaw_`) 원위치 정렬(`resetHullToAim`), 회전 차체 3D `OBB` 8-꼭짓점 기준 정확한 지면 지지 높이(`minBodyYAboveSurface`) 산출·서브틱 클램핑으로 바닥 파고듦 해결, 지상 사격 시 소폭 허공+후방 체공 후 밑면 착지 보간(`recoilHopActive_`, `log1p` 정자세 안착), 전·후방 2-구체 vs 회전 `OBB` 및 경사로 측·후면 벽 충돌 보강(`resolveSolidObstacles`, `resolveRampSideAndBackWalls`), 공중 연사 반동 감쇠(`0.55^n`)·고도 중력·피격 충격(`applyHitImpulse`) 구현 |
| `src/game/Physics.h` | 사물 충돌·피격 구분을 위한 `PartKind::Prop` 열거값 추가 |
| `src/game/ArenaScene.h`, `src/game/ArenaScene.cpp` | 바리케이드 동적 강체(`PropBody`)의 피격 코믹 물리 및 탱크·경사로 상호 밀어내기 연결, 월드 고정 조준점(`worldAimPoint_`) 및 커서 근접 적(`hoveredEnemyId_`, 반경 `2.2m`) 자동 추적으로 `WASD` 이동 시 시선 각도가 자연스럽게 변하는 `AimCameraContext` 구성, 플레이어↔적↔바리케이드 전 구간 장애물 충돌 전달 |
| `assets/tanks/tank.fs` | 부위별 반투명 렌더링을 위한 `uniform float objectAlpha;` 추가 |
| `src/tmpBoxing/SandTankModel.h`, `src/tmpBoxing/SandTankModel.cpp` | `TankRenderFilter`(`All`, `BarrelOnly`, `NonBarrelOnly`) 및 `alpha` 인자 지원으로 줌인 시 플레이어 포신 불투명 유지·차체/포탑/궤도 선택 반투명 드로우 분리 |
| `src/tmpBoxing/PreviewRenderer.h`, `src/tmpBoxing/PreviewRenderer.cpp` | 프레임워크 `transparentColor.fs` 연결 및 카메라-타겟 뷰 공간 깊이 비교(`nearestDepth < targetDepth - 0.35f`) 기반 2-Pass 렌더링(1패스 불투명 `glDepthMask(GL_TRUE)`, 2패스 시야 차폐 파츠·사물 `alpha = 0.14` + `glDepthMask(GL_FALSE)` + `GL_BLEND`) 구현 및 GL 상태 복원 |
| `src/main.cpp` | 플레이어 실시간 위치(`78%`)에서 월드 조준점·적을 향하는 시선 벡터를 추적해 `WA` 등 이동 사격 시 적 방향으로 부드럽게 공전 보간(`2.15/s → 2.55/s` 2단 감쇠)하고, 지상 반동 소폭 체공(`< 0.85m`)의 수직 흔들림은 걸러내면서 `B` 고고도 체공 시 거리·높이·`lookAt`을 확장하는 쿼터뷰 메인 카메라 구현 |

### 1.2 현재 조작 체계 (`game/cp02` 최신)

- **차체 이동 / 가속 돌진**: 차체 전방 기준축(`driveBasisYaw_`) 기준 `W`/`A`/`S`/`D` 4방향 이동(`W` 자체는 실시간 조준점을 자동 추종하지 않음), `Shift` 유지 시 가속 돌진(`2.15x`)
- **차체 자세 원위치 (`r`) / 장면 리셋 (`R`)**: 소문자 `r` 입력 시 현재 조준점을 향하도록 차체 정자세와 `WASD` 전방 기준축(`W` 방향)을 원위치 정렬, 대문자 `R`(`Shift + R` 또는 `CapsLock + R`, 보조 `F5`) 입력 시 아레나 장면 전체 리셋
- **상체(포탑·포신) 회전**: 마우스 이동(기본) / 방향키 `←`/`→`/`↑`/`↓`(단독 부위 회전)
- **우클릭 줌인 → 좌클릭 발사**: 지상·체공 공통으로 우클릭(`RMB`) 유지 시 적 피킹 타겟(없으면 조준점) 기준 줌인 + 슬로우(`0.32x`) 유지 → 그 상태에서 좌클릭(`LMB`) 시 줌·슬로우 즉시 해제 및 포탄 발사(우클릭 해제 시 발사 없이 복귀)
- **좌클릭 단발 / 홀드 줌인 발사**: 짧은 좌클릭은 즉시 발사, `1.2초` 이상 좌클릭 홀드 시 줌인 + 슬로우(`0.32x`) 진입 후 버튼을 떼면 줌·슬로우 해제와 함께 발사
- **폭발 체공 (`B`)**: 랜덤 TABS식 과장 폭발 도약 + 무줌인 약한 슬로우(`0.55x`), 체공 중 `B` 재입력으로 약한 슬로우 토글
- **검사 / 종료**: `F1` 디버그 표시 토글, `F2` 단위 검사(`QA-02·03·06·09`), `F3` QA-12 선택 대상 소실 트리거, `ESC` 종료

### 1.3 모듈·네임스페이스 경계 및 파일 규칙 점검

- `src/game/*`: `namespace game`(내부 `game::Physics` 및 익명 네임스페이스 포함)만 사용하며 OpenGL·프레임워크·`render`·`tmpBoxing` 헤더 참조 없음.
- `src/tmpBoxing/*`: `namespace tmpBoxing`만 사용하며 `game::PreviewFrame`/`game::TankPose` 읽기 전용 임시 프리뷰 어댑터 역할만 수행.
- `src/render/*`: 미수정(박재성 담당 영역 보존).
- `src/main.cpp`: 진입점 조립용 익명 네임스페이스(`RuntimeState`)만 사용.
- 신규·수정 팀 공유 텍스트 파일 전체 **UTF-8 BOM (`EF BB BF`) + CRLF** 저장 및 `git diff --check` 공백·개행 이상 없음 확인.

---

## 2. 검증 상태

- **완료된 정적 점검**: 네임스페이스·헤더 의존성 경계 점검, UTF-8 BOM + CRLF 인코딩 검증, `git diff --check` 통과.
- **사용자 실행 확인 대기 (`Debug | x64`, `F5`)**:
  1. 지상/체공 상태에서 우클릭 홀드 줌인·슬로우 및 좌클릭 시 즉시 해제·발사 동작 확인
  2. 좌클릭 `1.2초` 미만 즉시 발사 vs `1.2초` 이상 홀드 시 줌인·슬로우 유지 후 해제 발사 확인
  3. 줌인 중 카메라-타겟 사이 차체·바리케이드만 반투명(`0.14`) 처리되고 포신·타겟·포탄·배경은 선명하게 유지되는지 확인
  4. 지상 사격 미세 구름 반동과 `log1p` 복귀, 공중 연사 반동 감쇠·고도 중력 하강·밑면 착지 보간, 적·바리케이드 피격 코믹 물리 및 `F2` 단위 검사 통과 확인
  5. 소문자 `r` 입력 시에만 현재 조준점을 향하도록 차체 자세 및 `W` 전방 기준축이 원위치 정렬되는지(`W` 단독 입력 시 실시간 조준점 미추종), 대문자 `R`(`Shift+R` / `CapsLock+R`) 입력 시 장면 전체가 리셋되는지 확인

---

## 3. 이후 작업 내역 정리 (`CP03 ~ CP07` · `11/29 ~ 12/06`)

| 단계 | 일정 기준 | 대상 경로 / 범위 | 세부 작업 내역 (선행 구현 중복 제거 및 신규 보강 반영) |
| --- | --- | --- | --- |
| **CP03: 팀 렌더 연결** | `11/29 저녁` | `src/shared/RenderBridge.h`, `src/main.cpp` | 박재성(`namespace render`)과 공용 스냅샷 계약 확정: 기존 부위 행렬·`CameraRequest`·`FXEvent`·`PresentedView` 외에 CP02에서 선행 구현된 **궤도 바퀴 회전(`wheelSpinRad`), 줌 깊이 차폐 알파블렌딩 정보(`zoomAlphaFade`, `zoomTargetPos`, 포신 불투명 분리)** 전달 방식 합의 및 PR 검토 |
| **CP04: 코믹 물리 정교화 & 감각·조준 개선** | `11/30 ~ 12/01` | `src/game/` | ① 기존 `TankEntity`·`PropBody` 반동/충돌을 `Physics::applyImpulseAt`·`resolveContacts`(SAT·모서리 착지·드럼통 폭발 1회 보장)로 통합<br>② **[추가] 포탄 반동 타격감 보강**: 포신 축 방향 주퇴복좌(Kickback), 발사 순간 카메라 임팩트/미세 셰이크, 조준각 비대칭 차체 피칭·롤링 강화<br>③ **[추가] 체공 시 밋밋함 해소**: 상승·정점·급하강 리듬 대비 강화, 공중 사격·`Shift` 대시 시 궤도 꺾임/뱅크 연출 및 착지 충격(`Landing Slam`) 이벤트 추가<br>④ **[추가] 유저 친화적 조준(Aiming UX)**: 적 실루엣 인접 소프트 에임 보정(고저각 탄도 보정), 3D 예상 탄착점 링/마커 및 명중 가능 시 조준선 색상 피드백 도입 |
| **CP05: 적 관리와 행동(BT)** | `12/02` | `src/game/` | 다중 적 스폰·사망 정리(`spawnEnemy`) 및 4단계 행동 트리(`tickBrain`: 공중/피격 복구 중 사격 정지 → 시야·사거리 사격/드럼통 유폭 → 마지막 감지 위치 추적 → 순찰) 구현 |
| **CP06: 모델·HUD·효과 통합** | `12/03 ~ 12/04` | `src/game/`, `src/render/` | Meshy 적·소품 모델 피벗 정합(`ModelRegistry`), OpenGL 2D Ortho `GameHUD`(유저 친화적 동적 조준원·탄착 피드백·HP·부스트·`1.2s` 차징 게이지·잔여 적), 명중 접점·발사·착지 `FXEvent` 연결 |
| **CP07: 제출 후보 검증·패키징** | `12/05 ~ 12/06` | 전체 저장소 / `Release \| x64` | GLSL UTF-8 BOM 런타임 제거·`utf8.manifest` 기반 타 PC `Release \| x64` 리소스 검증, 대문자 `R`(`Shift+R`/`F5`) 연속 재시작·종료 무잔류 확인 및 최종 산출물 제출 |
