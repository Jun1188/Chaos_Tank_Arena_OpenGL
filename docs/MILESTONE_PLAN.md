# Chaos Tank Arena OpenGL — 마일스톤 및 변준영 3일 단위 세부 구현 계획

> **기준일**: 2026-11-20 (금)  
> **최종 제출 마감**: 2026-12-06 (일) 23:59 (잔여 17일 / 약 2.5주)  
> **범위 기준**: 필수 구현(Core) 우선 완주 · **근접전투/QTE/비네팅 등은 선택 구현으로 분리** · **ImGui 미사용** · **변준영만 28 `framework/` 재활용 / 박재성 별도 프레임워크**

---

## 1. 팀 전체 1주일 단위 마일스톤 (`11/20 ~ 12/06`)

| 주차 | 기간 | 핵심 목표 | 변준영 (`namespace game` · 28 재활용) | 박재성 (`namespace render` · 별도 프레임워크) | 주간 완료 기준 (DoD) |
| :--- | :--- | :--- | :--- | :--- | :--- |
| **1주차** | **11/20 ~ 11/26** | 기반 구축 & 핵심 전투·코믹 물리 완성 | 28 `framework/` 이관, `RenderBridge.h` 합의, `TankEntity`(차체-포탑-포신) 주행·사격, `PhysicsWorld`(OBB·SAT·MTV·클램핑), `Prop`(드럼통·램프·바리케이드), `Game28PreviewBackend` 단독 검증 | 별도 프레임워크 기반 `IRenderBackend` 골격, 탱크·소품 3D 모델 로드, 기본 방향광 + 폭발 점광원, 빌보드/인스턴싱 파티클 기본 구현 | 양쪽 환경 빌드 성공 + 피격/폭발 시 탱크·드럼통이 과장된 물리 반동으로 튕기고 구르는 코어 전투 재현 |
| **2주차** | **11/27 ~ 12/03** | AI(BT) · 트릭샷(슬로우·줌인) · HUD & 렌더 통합 | `EnemyBT`(필수 4순위: 물리복구→드럼통→원거리→순찰), `TimeManager`(TimeScale), `TrickshotFSM`(체공 슬로우·FOV 줌인 요청), 2D Ortho `GameHUD`, **`11/29` 기능 범위 최종 확정** | `CameraController`(3인칭 추적·FOV 줌 보간·셰이크), 파티클 풀·동적 점광원 고도화, `ExternalRenderBackend` 본 통합 | 점프 램프/폭발 체공 시 슬로우·FOV 줌인 및 복귀 동작 + 적 AI 4순위 + 두 프레임워크 통합 실행 성공 |
| **3주차 (마감)** | **12/04 ~ 12/06** | 최종 QA · Release 패키징 · 산출물 제출 | 통합 회귀 버그 수정, 별도 실행 폴더 Release 검증, 최종 보고서·발표 자료·시연 영상 제작 및 `12/06 23:59` 제출 | 쉐이더·리소스 경로 Release 검증, 시각 연출 폴리싱, 최종 보고서·발표 자료·영상 공동 제작 및 각자 제출 | 별도 폴더에서 Release 실행파일 연속 플레이·재시작·종료 무오류 + 보고서(4쪽+)·발표자료·유튜브 링크 제출 |

---

## 2. 변준영 (`namespace game`) 3일 단위 세부 구현 목표

### Sprint 1 (`11/20 ~ 11/22`): 28 프레임워크 이관 & 탱크 계층 주행·사격
- **목표**: `openGL_3dAssign_28/framework/`를 `src/game/framework/`로 이관하고, 박재성과 `src/shared/RenderBridge.h` 계약을 확정한다.
- **세부 작업**:
  1. `src/shared/RenderBridge.h` 작성 (`RenderItem`, `CameraTarget`, `FXEvent`, `IRenderBackend`) 및 박재성과 합의
  2. `src/game/Game28PreviewBackend` 구현 (28 `Scene`/`Renderer`로 단독 프리뷰)
  3. 28의 `Console`을 `ArenaScene` + `GameObject` + `TankEntity`(`Body → Turret → Barrel`)로 분해
  4. `PlayerTank` WASD 주행 + 마우스 포탑 독립 회전 + 좌클릭 주포 발사 프로토타입
- **완료 기준**: `Game28PreviewBackend`에서 탱크가 주행하고 포탑/포신이 독립 회전하며 포탄이 발사됨.

### Sprint 2 (`11/23 ~ 11/25`): 고정 타임스텝 & 코믹 물리 (`OBB·SAT·MTV`) · 소품
- **목표**: TABS 스타일 과장된 강체 물리 반응과 상호작용 소품 3종을 안정적으로 구현한다.
- **세부 작업**:
  1. `TimeManager` 구현 (`rawDt` → `timeScale` 보간 + `fixedDt = 1/120s` accumulator, 프레임당 서브스텝 상한 4)
  2. `PhysicsWorld` 구현: 회전 `OBB` 갱신, `SAT·MTV` 침투 보정, 코믹 반발계수·회전 토크 적용, 최대 선속도·각속도 클램핑
  3. `Prop` 3종 구현: `ExplosiveDrum`(반경 폭발 + 연쇄 폭발 쿨타임), `JumpRamp`(체공 트리거), `Barricade`(파괴 가능)
  4. `onCollision`, `onExplosion`, `onAirborneChanged` 콜백 발행 테스트
- **완료 기준**: 포탄·드럼통 폭발 시 탱크와 소품이 관통/발산 없이 공중으로 튕기고 구른 뒤 안정화됨.

### Sprint 3 (`11/26 ~ 11/28`): 적 AI `BehaviorTree` (필수 4순위) & 웨이브 루프
- **목표**: 적 탱크 1~2대가 필수 4순위 행동 트리로 자율 교전하고 웨이브 승패가 판정되도록 한다.
- **세부 작업**:
  1. `BTNode`, `BTSelector`, `BTSequence`, `Blackboard` 구현 (`unique_ptr` 단일 소유)
  2. 필수 4순위 노드 구현:
     - `1순위`: `SeqPhysicsRecovery` (공중/피격 시 속도 클램핑 & 착지 대기)
     - `2순위`: `SeqArenaHazard` (플레이어 인근 폭발 드럼통 감지 → 조준·사격)
     - `3순위`: `SeqRangedCombat` (시야 확보 → 포탑 추적 · 주포 사격 / 바리케이드 회피 추격)
     - `4순위`: `ActArenaPatrol` (웨이포인트 순찰 & 포탑 탐색)
  3. `ArenaScene`에 적 탱크 1~2대 스폰, 피격/파괴, 웨이브 클리어 및 재시작 상태 연결
- **완료 기준**: 적 탱크가 평소 순찰/원거리 교전/드럼통 사격을 수행하고, 폭발로 날아가면 착지 복구를 최우선 수행함.

### Sprint 4 (`11/29 ~ 12/01`): 트릭샷 기본(슬로우·줌인) & 2D Ortho `GameHUD` (**11/29 범위 확정**)
- **목표**: 핵심 하이라이트인 공중 트릭샷 기본 요소와 ImGui 없는 2D HUD를 완성한다.
- **세부 작업**:
  1. **`11/29` 팀 중간 점검**: 필수 구현 진행률 확인 후 선택 구현(근접전투·QTE·비네팅) 착수 여부 최종 확정
  2. `TrickshotFSM` 구현: 점프 램프/폭발 체공 감지 → `TimeManager` 슬로우(0.3x) + `IRenderBackend::setZoomTarget` 요청 → 착지 시 튐 없이 부드럽게 복귀
  3. `GameHUD` 구현: OpenGL 2D 직교 투영(Ortho)으로 HP·부스트·트릭샷 게이지·웨이브·스코어·잔여 적 표시 (ImGui 미사용)
  4. 타이틀/일시정지(Esc)/결과 오버레이 및 자원 해제 종료 흐름 연결
- **완료 기준**: 점프 램프 진입 시 슬로우 모션 + FOV 줌인이 자연스럽게 동작하고, HUD 수치가 실시간 갱신됨.

### Sprint 5 (`12/02 ~ 12/04`): 박재성 별도 프레임워크 본 통합 & 안정화 (선택 구현 판단)
- **목표**: `main.cpp`의 렌더 백엔드를 `Game28PreviewBackend`에서 박재성 `ExternalRenderBackend`로 전환해 최종 통합한다.
- **세부 작업**:
  1. `RenderFrameSnapshot`·`FXEvent`(`onExplosion`, `onCannonFired`, `onSlowMoChanged`) 실연동 및 좌표계/행렬 일치 검증
  2. 통합 회귀 테스트: 슬로우 해제 시 카메라 튐, 연쇄 폭발 발산, 파티클 알파 정렬, 프레임률(60fps) 점검
  3. *(일정 여유 시)* `src/game/optional/`에 분리해 둔 선택 구현(근접전투 또는 QTE) 중 1개만 선별 장착
- **완료 기준**: 박재성 3D 모델·파티클·동적 점광원 + 변준영 물리·BT·트릭샷·HUD가 단일 실행파일로 무오류 통합 동작.

### Sprint 6 (`12/05 ~ 12/06`): Release 패키징 · 산출물 제작 · 최종 제출
- **목표**: 과제 제출 기준(코드·Release 실행파일·보고서 4쪽 이상·발표자료·시연 영상)을 완비해 제출한다.
- **세부 작업**:
  1. 별도 클린 폴더에 Release `.exe` + DLL + `assets/` 복사 후 연속 플레이·재시작·종료 테스트
  2. 3~5분 하이라이트 시연 영상 녹화 및 유튜브 업로드
  3. 최종 보고서(4쪽 이상, AI 활용 사례 3~5개 및 직접 설계/검증 내용 포함) 및 발표 자료 작성
  4. **`12/06 23:59` 전** 팀원 각자 압축본 제출
- **완료 기준**: 외부 PC/별도 폴더에서 Release 실행파일 즉시 실행 확인 + 모든 제출물 업로드 완료.
