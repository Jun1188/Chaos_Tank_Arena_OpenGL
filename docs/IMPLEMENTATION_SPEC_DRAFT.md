# Chaos Tank Arena OpenGL — 구체적 구현 명세 초안

> **기준 문서**: `컴그제안서_Chaos_Tank_Arena_Proposal.docx` (SLAM & ZOOK: Chaos Tank)  
> **프레임워크 분담 원칙**:  
> - **변준영 (`namespace game`)**: `openGL_3dAssign_28/framework/`(`namespace graphics`)를 **변준영 책임 범위(창·입력·루프·로컬 테스트/프리뷰·씬/물리/AI/트릭샷/HUD)** 에서 재활용  
> - **박재성 (`namespace render`)**: **별도 렌더링 프레임워크** 사용 (28 `framework/`에 비의존)  
> - **핵심 설계 원칙**: 2개 네임스페이스 역할 분담 · `shared` 순수 데이터/콜백 경계로 두 프레임워크 디커플링 · **필수/선택 구현 분리** · **ImGui 미사용** · **근접전투는 선택 구현으로 분리**

---

## 1. 아키텍처 개요 및 프레임워크 경계

### 1.1 프레임워크 독립 구조 (2개 네임스페이스 + 공용 경계)
1. **`namespace game` (변준영 담당 — 28 `framework/` 재활용)**
   - `openGL_3dAssign_28/framework/`의 `Application`(GLFW 창·입력 디스패치·메인 루프), `Transform3D`, `Dynamics` 서브스텝 패턴을 재활용한다.
   - `TimeManager`, `ArenaScene`, `GameObject`, `TankEntity`(`PlayerTank`, `EnemyTank`), `Prop`(드럼통·점프 램프·바리케이드), `PhysicsWorld`(OBB·SAT·MTV), `BehaviorTree`/`Blackboard`, `TrickshotFSM`(슬로우·줌인), `GameHUD`(OpenGL 2D Ortho)를 구현한다.
   - 단독 개발·디버깅 시에는 28 `framework/`의 `graphics::Scene` + `Renderer`를 **로컬 프리뷰 어댑터(`Game28PreviewBackend`)** 로 연결해 `render` 완성 전에도 물리·AI·트릭샷을 독립 검증한다.
2. **`namespace render` (박재성 담당 — 별도 프레임워크 사용)**
   - 28 `framework/`를 `#include`하지 않고 박재성 측 별도 OpenGL 프레임워크 위에서 3D 모델·재질·쉐이더·동적 광원·파티클·카메라 연출을 구현한다.
   - 공용 경계(`src/shared/RenderBridge.h`)의 `IRenderBackend` 인터페이스만 구현해 `game`이 전달하는 순수 데이터(`RenderFrameSnapshot`, `FXEvent`, `CameraState`)를 받아 렌더링한다.
3. **`src/shared/RenderBridge.h` (공용 경계 헤더 — 순수 C++/GLM 타입만 사용)**
   - 두 프레임워크가 서로의 내부 헤더를 참조하지 않도록 **GLM 행렬/벡터 + Enum + Struct + 콜백 인터페이스**만 정의한다.

### 1.2 필수 구현(Core) vs 선택 구현(Optional) 분리 기준
| 구분 | `namespace game` (변준영 · 28 프레임워크 재활용) | `namespace render` (박재성 · 별도 프레임워크) |
| :--- | :--- | :--- |
| **필수 구현 (Core)** | 고정 물리 타임스텝(`TimeManager`), 탱크 계층(`Body→Turret→Barrel`) 및 주행·포탑 독립 회전·주포 사격, `OBB·SAT·MTV` 및 코믹 반발/토크·속도 클램핑, 상호작용 소품(폭발 드럼통·점프 램프·바리케이드), 적 AI `BehaviorTree`(1.물리복구 → 2.드럼통 → 3.원거리 → 4.순찰), 트릭샷 기본(체공 감지 → `TimeScale` 슬로우 + FOV 줌인 요청), 2D Ortho `GameHUD`, 28 기반 로컬 프리뷰 어댑터 | 별도 프레임워크 기반 3D 렌더링 파이프라인, 탱크·아레나 3D 모델/재질 로드, 기본 방향광 + 폭발/포격 동적 점광원, 빌보드/인스턴싱 파티클 풀(포구 화염·폭발 파편·연기, 깊이 쓰기 OFF), 3인칭 추적 카메라 및 FOV 줌인·셰이크 보간, `IRenderBackend` 구현 |
| **선택 구현 (Optional)** | **근접 전투**(포신 대회전 스윙, 부스트 육탄 돌진 — 우선 제외), 트릭샷 확장(QTE 타이밍 입력, 고속 스핀 + 초고속 바주카 직격), 보스 탱크 | 후처리 쉐이더(트릭샷 비네팅·색수차), 시네마틱 앵글·리플레이 카메라 |

---

## 2. 디렉토리 및 파일 계층 구조 (초안)

```text
Chaos_Tank_Arena_OpenGL/
├── docs/
│   ├── IMPLEMENTATION_SPEC_DRAFT.md       # 본 구현 명세 초안
│   └── ARCHITECTURE_RELATIONS_DRAFT.xlsx  # 호출-리턴/상속/소유/콜백 관계표
├── assets/
│   ├── models/                            # 탱크(차체·포탑·포신), 드럼통, 램프, 바리케이드
│   └── shaders/                           # 다중 광원, 파티클, 2D HUD 쉐이더
└── src/
    ├── main.cpp                           # 조립 루트: game + IRenderBackend 바인딩
    ├── shared/                            # [공용 경계] GLM/STL만 의존 (프레임워크 중립)
    │   └── RenderBridge.h                 # RenderItem, CameraState, FXEvent, IRenderBackend
    ├── game/                              # [변준영] namespace game (28 framework 재활용)
    │   ├── framework/                     # openGL_3dAssign_28/framework/ 이관 (변준영 전용)
    │   ├── Game28PreviewBackend.h/.cpp    # 단독 테스트용 28 Scene/Renderer 어댑터
    │   ├── TimeManager.h/.cpp             # TimeScale, 고정 물리 accumulator(1/120s)
    │   ├── GameObject.h/.cpp              # 기본 Transform + 계층 노드
    │   ├── TankEntity.h/.cpp              # 차체-포탑-포신 계층, PlayerTank / EnemyTank
    │   ├── Prop.h/.cpp                    # ExplosiveDrum, JumpRamp, Barricade
    │   ├── PhysicsWorld.h/.cpp            # OBB, SAT, MTV, 코믹 반발/토크, 속도 클램핑
    │   ├── BehaviorTree.h/.cpp            # BTNode, Selector/Sequence, 필수 4순위 + Blackboard
    │   ├── TrickshotFSM.h/.cpp            # 체공 감지 → 슬로우·FOV 줌인 상태머신
    │   ├── ArenaScene.h/.cpp              # 웨이브·스코어·엔티티 소유 및 RenderSnapshot 생성
    │   ├── GameHUD.h/.cpp                 # OpenGL 2D Ortho 오버레이 (ImGui 미사용)
    │   └── optional/                      # [선택] MeleeActions.h, TrickshotQTE.h
    └── render/                            # [박재성] namespace render (별도 프레임워크 사용)
        ├── ExternalRenderBackend.h/.cpp   # IRenderBackend 구현체 (박재성 프레임워크 어댑터)
        ├── CameraController.h/.cpp        # 3인칭 추적, FOV 줌 보간, 셰이크
        ├── DynamicLightPool.h/.cpp        # 수명 기반 폭발·포격 점광원
        ├── ParticleSystem.h/.cpp          # 빌보드/인스턴싱 오브젝트 풀 (깊이 쓰기 OFF)
        ├── ModelRegistry.h/.cpp           # 별도 프레임워크 메시/재질 로드 및 캐시
        └── optional/                      # [선택] PostProcessVignette.h
```

---

## 3. 메인 프레임 루프 및 호출 순서

`main.cpp`에서 변준영 측 `graphics::Application`(28 프레임워크) 콜백을 등록하고, 렌더 백엔드(`Game28PreviewBackend` 또는 박재성 `ExternalRenderBackend`)를 `IRenderBackend`로 주입한다.

1. **초기화 (`setInitHandler`)**
   - `renderBackend->init(fbWidth, fbHeight)` → 별도 렌더 프레임워크(또는 28 프리뷰) 메시·쉐이더·파티클 풀 초기화
   - `game::GameHUD::init()` → 2D Ortho HUD 버퍼 초기화
   - `game::ArenaScene::init(physics)` → 바닥, 플레이어 탱크, 적 탱크 1~2대, 소품 3~6개 생성
   - 경계 콜백 바인딩 (`game` 이벤트 → `renderBackend->pushFXEvent / setCameraZoom / addCameraShake`)
2. **입력 (`setModifiedKeyHandler`, `setMouseHandler`, `setCursorHandler`, `setFocusHandler`)**
   - `game::ArenaScene::onKey / onMouse / onCursor / onFocus`로 입력 버퍼링 (포커스 상실 시 키 상태 초기화)
3. **갱신 (`setUpdateHandler(app, rawDt)`)**
   - `float scaledDt = timeMgr.beginFrame(rawDt);`
   - `trickshot.update(rawDt)` → `TimeScale` 및 목표 FOV 산출 → 콜백 발행
   - `arena.updateInputsAndAI(scaledDt)` → 플레이어 조향/포탑 조준, 적 `BehaviorTree::tick(scaledDt, bb)`
   - **고정 물리 서브스텝 루프** (`while (timeMgr.consumeFixedStep(fixedDt))`, `fixedDt = 1/120.f`):
     - `physics.step(fixedDt)` → OBB 갱신 → SAT·MTV 보정 → 코믹 반발/토크 → 속도 클램핑 → 충돌/폭발/체공 콜백 발행
   - `RenderFrameSnapshot snap = arena.buildRenderSnapshot();` → 모든 엔티티(`Body/Turret/Barrel`, `Prop`)의 월드 행렬·메시 키·상태를 순수 DTO로 패킹
   - `renderBackend->update(rawDt, scaledDt, snap)` → 카메라 추적/FOV 보간, 동적 점광원 감쇠, 파티클 갱신
4. **렌더링 (`setRenderHandler(app)`)**
   - `renderBackend->render(snap)` → 3D 아레나·탱크·소품·동적 광원 + 파티클(`glDepthMask(GL_FALSE)`) 렌더
   - `hud.draw(fbWidth, fbHeight, arena.getHudSnapshot())` → OpenGL 2D Ortho HUD 오버레이 렌더

---

## 4. 핵심 모듈별 구체 명세

### 4.1 공용 경계 (`src/shared/RenderBridge.h`)
- **목적**: 변준영(28 프레임워크)과 박재성(별도 프레임워크)이 서로의 헤더를 건드리지 않고 병렬 개발 후 즉시 통합 가능하게 함.
- **주요 구조체/인터페이스**:
  - `RenderItem { uint32_t id; MeshKey mesh; glm::mat4 worldMatrix; glm::vec4 tint; }`
  - `CameraTarget { glm::vec3 tankPos; glm::vec3 forward; float speed; }`
  - `FXEvent { FXType type; glm::vec3 pos; glm::vec3 dir; float power; }` (`MuzzleFlash`, `Explosion`, `SmokeTrail`)
  - `IRenderBackend`: `init()`, `update(rawDt, scaledDt, snap)`, `render(snap)`, `pushFXEvent(ev)`, `setZoomTarget(fovDeg)`, `addShake(amp, dur)`, `shutdown()`

### 4.2 `namespace game` (변준영 — 28 `framework/` 재활용)

| 클래스 | 주요 멤버(소유) | 핵심 메서드 시그니처 | 안전성·설계 규칙 |
| :--- | :--- | :--- | :--- |
| `TimeManager` | `timeScale_`, `accumulator_`, `fixedDt_(1/120.f)` | `float beginFrame(float rawDt)`<br>`bool consumeFixedStep(float& outDt)`<br>`void setTargetTimeScale(float scale, float blendSpeed)` | 슬로우 해제 시 튐 방지를 위한 `timeScale_` 보간, 프레임당 최대 서브스텝 4회 제한 |
| `GameObject` | `uint32_t id_`, `Transform3D`, `bool alive_` | `virtual void collectRenderItems(std::vector<RenderItem>&) const` | 28 `Transform3D` 재사용, 렌더 백엔드 비종속 |
| `TankEntity` | `bodyTx_`, `turretTx_`, `barrelTx_`, `hp_`, `RigidBodyState`, `OBBCollider` | `void setDriveInput(float throttle, float steer)`<br>`void aimTurret(glm::vec3 worldTarget, float dt)`<br>`bool fireMainCannon(ProjectileSpawn& out)` | `Body → Turret → Barrel` 계층 월드 행렬을 계산해 `RenderItem` 3개로 출력 |
| `PlayerTank` | `std::unique_ptr<TrickshotFSM>` | `void applyPlayerInput(const InputState&, float dt)` | 공중 체공 이벤트 수신 시 `TrickshotFSM` 진입 |
| `EnemyTank` | `std::unique_ptr<BehaviorTree>`, `Blackboard` | `void tickAI(float dt)` | 물리 피격/공중 시 `Blackboard.isAirborne = true`로 1순위 복구 강제 진입 |
| `Prop` | `PropType(Drum/Ramp/Barricade)`, `OBBCollider` | `void onHit(const HitEvent&)` | 드럼통 파괴 시 반경 폭발 + 연쇄 폭발 쿨타임/최대 속도 클램핑 |
| `PhysicsWorld` | `std::vector<BodyRef>`, 콜백(`onCollision`, `onExplosion`, `onAirborne`) | `void step(float fixedDt)`<br>`bool testOBB_SAT(const OBB&, const OBB&, MTV& out)`<br>`void applyComicImpulse(Body&, glm::vec3 impulse, glm::vec3 hitPoint)` | `fixedDt = 1/120s` 서브스텝 + 최대 선속도/각속도 클램핑 |
| `BehaviorTree` | `std::unique_ptr<BTNode> root_` | `BTStatus tick(float dt, Blackboard& bb)` | 필수 4순위(물리복구→드럼통→원거리→순찰). 선택(근접) 노드는 `optional/` 분리 |
| `TrickshotFSM` | `TrickshotState(Idle, AirborneSlow, Recovering)` | `void onAirborneStart()` / `onLanded()` / `update(float rawDt)` | [필수] 체공 시 슬로우(0.25~0.35) + FOV 줌인 콜백. [선택] QTE/바주카 훅 분리 |
| `GameHUD` | `GLuint vao_, vbo_, shader_` | `bool init()` / `void draw(int fbW, int fbH, const HudSnapshot&)` | ImGui 없이 OpenGL 2D Ortho 쿼드로 HP·부스트·트릭샷 게이지·웨이브·스코어 출력 |
| `Game28PreviewBackend` | `graphics::Scene&`, `graphics::Application&` | `IRenderBackend` 구현 (28 `Scene`/`Renderer` 기반) | 변준영 단독 개발·물리/BT 검증용 프리뷰 어댑터 (통합 시 `render`로 교체) |

### 4.3 `namespace render` (박재성 — 별도 프레임워크 사용)

| 클래스 | 주요 멤버(소유) | 핵심 메서드 시그니처 | 안전성·설계 규칙 |
| :--- | :--- | :--- | :--- |
| `ExternalRenderBackend` | 박재성 프레임워크 렌더러·쉐이더·모델·파티클·광원·카메라 | `IRenderBackend` 구현 (`init`, `update`, `render`, `pushFXEvent`, `setZoomTarget`, `addShake`) | 28 `framework/` 비의존, `RenderBridge.h`만 포함해 독립 컴파일 가능 |
| `CameraController` | `currentFov_`, `targetFov_`, `shakeAmp_` | `void update(float rawDt, const CameraTarget&)` | 슬로우 해제 시 `exp(-k * rawDt)` 감쇠 보간으로 카메라 튐 방지 |
| `DynamicLightPool` | 수명 기반 점광원 풀 | `void spawnFlash(glm::vec3 pos, glm::vec3 color, float intensity, float ttl)` | 기본 방향광 + 폭발/포격 동적 점광원 1~2개 이상 실시간 반영 |
| `ParticleSystem` | 빌보드/인스턴싱 오브젝트 풀 | `void emitBurst(const FXEvent&)` / `update(dt)` / `draw(view, proj)` | 오브젝트 풀링 + `glDepthMask(GL_FALSE)` + 알파 블렌딩 |

---

## 5. 단계별 병렬 개발 및 통합 로드맵

| 단계 | 변준영 (`namespace game` · 28 재활용) | 박재성 (`namespace render` · 별도 프레임워크) | 통합 검증 기준 (DoD) |
| :--- | :--- | :--- | :--- |
| **1단계 (경계 합의)** | `RenderBridge.h` 확정 + 28 `framework/` 이관 + `Game28PreviewBackend`로 탱크 주행·포탑 회전 실행 | 별도 프레임워크에서 `IRenderBackend` 골격 + 탱크/아레나 메시 로드 실행 | 양쪽 환경에서 동일한 `RenderFrameSnapshot` 포맷으로 빌드 성공 |
| **2단계 (물리 + FX)** | `PhysicsWorld`(OBB·SAT·MTV·클램핑) + 소품(드럼통·램프·바리케이드) 구현 | `ParticleSystem`(빌보드/인스턴싱 풀) + `DynamicLightPool` + 다중 광원 쉐이더 구현 | `onExplosion`/`onCannonFired` 이벤트 연동 시 파편·점광원·코믹 반발 재현 |
| **3단계 (BT + 트릭샷)** | `EnemyBT`(필수 4순위) + `TimeManager` + `TrickshotFSM`(슬로우·줌인) 구현 | `CameraController`(3인칭 추적·FOV 줌 보간·셰이크) 고도화 | 체공 시 슬로우·줌인 + 적 AI 4순위 동작 확인 |
| **4단계 (통합 & 선택)** | 2D Ortho `GameHUD` 통합 + `main.cpp` 백엔드를 `ExternalRenderBackend`로 전환 | Release 리소스 경로/쉐이더 검증 + 여유 시 선택 구현(비네팅 등) 연결 | 별도 실행 폴더에서 연속 플레이·재시작·종료 및 녹화 검증 완료 |
