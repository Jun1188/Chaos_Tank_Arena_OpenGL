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
| `src/game/TankEntity.h`, `src/game/TankEntity.cpp` | 소문자 `r` 입력 시에만 현재 조준점을 향하도록 차체 정자세 및 `WASD` 전방 기준축(`driveBasisYaw_`)을 원위치 정렬(`resetHullToAim`, 이후 `W` 입력 시 해당 기준 전방으로 주행하며 `W` 자체는 실시간 조준점을 자동 추종하지 않음), `Shift` 가속 돌진(`2.15x`), 지상 미세 구름 반동과 `log1p` 복귀, 공중 연사 반동 감쇠(`0.55^n`), 고도(`4.5m` 초과) 비례 중력·하강 가속(`1.35x`) 및 밑면 착지 보간, 피격 충격 반응(`applyHitImpulse`) 구현 |
| `src/game/Physics.h` | 사물 충돌·피격 구분을 위한 `PartKind::Prop` 열거값 추가 |
| `src/game/ArenaScene.h`, `src/game/ArenaScene.cpp` | 바리케이드를 동적 강체(`PropBody`)로 전환해 포탄 피격 시 적 탱크와 함께 TABS식 코믹 회전·바운스·지면 안착 동작 연결, 카메라 자기회전 피드백 없이 조준 대상·방향을 전달하는 `AimCameraContext`(`cameraAimContext`) 추가, `PreviewFrame`에 줌 알파블렌딩 페이드·타겟 좌표·객체별 바운딩 중심/반경 및 불투명 유지 플래그 전달 |
| `assets/tanks/tank.fs` | 부위별 반투명 렌더링을 위한 `uniform float objectAlpha;` 추가 |
| `src/tmpBoxing/SandTankModel.h`, `src/tmpBoxing/SandTankModel.cpp` | `TankRenderFilter`(`All`, `BarrelOnly`, `NonBarrelOnly`) 및 `alpha` 인자 지원으로 줌인 시 플레이어 포신 불투명 유지·차체/포탑/궤도 선택 반투명 드로우 분리 |
| `src/tmpBoxing/PreviewRenderer.h`, `src/tmpBoxing/PreviewRenderer.cpp` | 프레임워크 `transparentColor.fs` 연결 및 카메라-타겟 뷰 공간 깊이 비교(`nearestDepth < targetDepth - 0.35f`) 기반 2-Pass 렌더링(1패스 불투명 `glDepthMask(GL_TRUE)`, 2패스 시야 차폐 파츠·사물 `alpha = 0.14` + `glDepthMask(GL_FALSE)` + `GL_BLEND`) 구현 및 GL 상태 복원 |
| `src/main.cpp` | 차체(`WASD`) 회전과 분리해 키 조작 반응성을 둔감화(`1.8m` 소프트 데드존·지상 미세 반동 필터·2단 감쇠 보간)하고, 현재 조준 대상·방향을 대략적으로(`±12.6°` 완충각 + 2단 각도 보간) 가리키면서 `WASD`·`B` 체공 고도·반동 변위에 맞춰 거리·높이·시야 리드(`lookAt`)를 부드럽게 보간하는 쿼터뷰 메인 카메라 구현 |

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

## 3. 이후 작업 내역 정리 (`11/28 마무리 ~ 12/06 제출`)

| 단계 | 일정 기준 | 대상 경로 / 범위 | 세부 작업 내역 |
| --- | --- | --- | --- |
| **1단계: CP02 실기 검증·마무리** | `11/28` | `src/game/`, `src/tmpBoxing/`, `docs/` | 사용자 VS `Debug \| x64` 플레이 테스트 피드백 반영(홀드 시간 `1.2s`, 깊이 임계값 `0.35m`, 반투명도 `0.14`, 반동·중력 계수 미세 튜닝), `F2` 단위 검사 확인 후 `game/cp02` 커밋·푸시 |
| **2단계: 팀 협의 및 공용 경계 확정** | `11/29 저녁` | `src/shared/RenderBridge.h`, `src/main.cpp` | 박재성(`namespace render`)과 11/28 구현 결과 공유 및 세부 경계 조율(적 타겟/AI 분담, 카메라 줌·슬로우 연출 제어권, 깊이 알파블렌딩 정보 전달 방식, 모델·소품 식별자). `STL`·`GLM` 순수 값 타입 기반 `RenderBridge.h`(프레임 스냅샷·카메라/줌 상태·`FXEvent`) 합의 및 PR 상호 검토 |
| **3단계: 렌더 브리지 연결 및 게임 루프 완성** | `11/30 ~ 12/03` | `src/game/`, `src/shared/`, `src/main.cpp` | ① `ArenaScene` 스냅샷을 `RenderBridge.h` 규격으로 출력해 `render` 모듈 백엔드와 연결(`tmpBoxing` 분리·대체)<br>② 상호작용 소품 확장(폭발 드럼통 반경 충격파·연쇄 폭발 클램핑) 및 팀 합의 범위에 맞춘 적 교전·웨이브 클리어·재시작 루프 완성<br>③ ImGui를 사용하지 않는 OpenGL 2D 직교 투영 기반 `GameHUD`(체력·부스트·슬로우/줌 상태·잔여 적·결과 표시) 구현 |
| **4단계: 통합 QA·패키징·제출** | `12/04 ~ 12/06` | 전체 저장소 / `Release \| x64` | ① 통합 회귀 점검(슬로우 해제 시 카메라 튐, 다중 폭발 시 속도 클램핑, 한글/상대 경로 리소스 로드, 재시작 `R` 및 종료 시 GL 자원 해제)<br>② 별도 클린 폴더에서 `Release \| x64` 실행파일·DLL·`assets/` 단독 구동 검증<br>③ 시연 영상·발표 자료·최종 보고서 작성 및 **12/06** 최종 제출 |
