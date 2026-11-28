# Chaos Tank Arena · 최소 플레이 루프 구현 명세 (2026-11-28 갱신본)

**첫 체크아웃에서 탱크를 직접 움직이고 조준해 쏜 뒤, 명중과 빗나감을 확인하고 같은 배치를 재시작한다.** 지상 사격(CP01)과 공중 트릭샷·코믹 반동 초안(CP02)까지 `game/cp01`·`game/cp02` 브랜치에 구현되었으며, 본 문서는 **CP01~CP02 실제 구현 결과를 반영해 중복 요소를 걷어내고 변경된 조작·렌더 규약과 추가 구현 과제(포탄 반동·체공 시 밋밋함 해소·유저 친화적 조준 UX)를 CP03~CP07에 재배치한 최신 계획**이다.

[엑셀 작업표](GAMEPLAY_CHECKPOINTS.xlsx)에서 작업 ID를 선택한 뒤 이 문서에서 같은 ID를 찾는다. 각 절은 목표, 선행 작업, 슈도 코드, 관계·근거, 사용자 검사 순이다. 초기 기준 커밋은 `dec36c2`, CP01 완료 커밋은 `game/cp01@9912e47`, CP02 완료 커밋은 `game/cp02@9592d37`이다.

---

## 세션 인계 핵심 요약 (2026-11-28 기준: 중복 · 변경 · 추가 구현)

### 1. 선행 구현 완료되어 CP03~CP07에서 중복 구현하지 않을 요소
- **모델·셰이더·한글 경로 선행 대응 (구 CP06·CP07 일부)**: `assets/tanks/sand-tank.glb`(`tmpBoxing::SandTankModel`, `makeSandTankSpec()`, 궤도 로드휠 `wheelSpinRad`)와 `assets/tanks/tank.vs`·`tank.fs`(실행 시 선두 3바이트 UTF-8 BOM 제거 포함), 한글 Windows 경로 Assimp 로드용 `src/utf8.manifest`가 이미 동작 중이다.
- **기초 코믹 반동·피격 물리 및 충돌 방지 (구 CP04·CP05 일부)**:
  - 적 `TankEntity::applyHitImpulse` 및 동적 바리케이드(`PropBody`)의 포탄 피격 회전·바운스·지면 안착이 이미 `ArenaScene`에 연결되어 있다.
  - 회전된 차체 3D `OBB` 8-꼭짓점 기준 지면 지지 높이(`minBodyYAboveSurface`), 지상 사격 소폭 체공·착지 보간(`recoilHopActive_`), 전·후방 2-구체 vs 회전 `OBB` 밀어내기(`resolveSolidObstacles`), 경사로 측·후면 벽 진입 차단(`resolveRampSideAndBackWalls`)이 구현되어 바닥 파고듦·물체 끼임을 1차 해소했다.
  - `EntityId` 기반 `enemies_` 관리, 피격 처리, 대상 소실 시 포커스·슬로우 즉시 해제(`QA-12`)가 동작 중이다.

### 2. 초기 기획 대비 변경 확정된 규칙 (CP03~CP07 공통 준수)
- **지상·체공 공통 수동 줌인 및 차징 줌인**:
  - 기존 ‘램프 체공 중 조건(`canEnter`) 충족 시 자동 1회 진입’ 대신, **지상·체공 언제든 우클릭(`RMB`) 홀드 시 즉시 줌인(`FOV 45° → 34°`, `0.32x` 슬로우)** 되며 그 상태에서 **좌클릭(`LMB`) 시 줌·슬로우 즉시 해제 + 포탄 발사**(우클릭 해제 시 발사 없이 복귀)된다.
  - **좌클릭 `1.2초` 이상 홀드** 시에도 차징 줌인·슬로우(`0.32x`)가 활성화되며 버튼을 뗄 때 해제·발사된다(짧은 좌클릭은 즉시 단발 발사). `TrickshotController::canEnter`는 `QA-09` 경계 단위 검사용으로 유지한다.
- **`B` 폭발 도약 & `Shift` 가속 돌진**: `B`는 줌인 없이 랜덤 코믹 폭발 도약 + 약한 슬로우(`0.55x`, 체공 중 `B` 재입력 토글)를 수행하고, `Shift`는 `2.15x` 가속 돌진을 수행한다.
- **소문자 `r` vs 대문자 `R` 분리**:
  - **소문자 `r`**: 제자리에서 **현재 조준점 방향**으로 차체 정자세 및 `WASD` 전방 기준축(`driveBasisYaw_`)을 원위치 정렬(`resetHullToAim`)한다(`W` 자체는 실시간 마우스 조준점을 자동 추종하지 않음).
  - **대문자 `R` (`Shift+R` / `CapsLock+R`, 보조 `F5`)**: 장면 전체 리셋(`ArenaScene::reset`)을 수행한다. 이후 모든 재시작 검사(`QA-01`, `QA-23`)는 대문자 `R` 기준으로 수행한다.
- **카메라-타겟 깊이 검사 기반 2-Pass 알파블렌딩 (`alpha = 0.14`)**: 줌인 중 카메라~타겟 뷰 깊이(`nearestDepth < targetDepth - 0.35f`) 사이에 끼어 시야를 가리는 차체·포탑·궤도·사물만 반투명 처리하고, 플레이어 포신(`BarrelOnly`)·줌 타겟·포탄·바닥은 불투명으로 유지한다(`CP03` 렌더 브리지 계약에 포함 필수).
- **조준점–`WASD` 연동 공전 쿼터뷰 카메라**: 월드 조준점(`worldAimPoint_`) 및 커서 인접 적(`hoveredEnemyId_`, 반경 `2.2m`)을 월드 고정 추적해, 적을 조준한 채 `WA`·`WD` 등으로 이동하면 카메라가 적 방향으로 부드럽게 공전 보간한다.

### 3. CP03 이후 반드시 해결할 신규 추가 구현 과제 (플레이 감각·UX 보강)
1. **포탄 반동 타격감·입체감 보강 (`CP04-FEEL` · `CP06-FX`)**:
   - 현재 차체 전체의 후방 소폭 홉만으로는 포격 임팩트가 단조롭다.
   - **포신 주퇴복좌(Barrel Recoil Kickback)**: 발사 직후 포신이 로컬 `+Z`(뒤쪽)로 즉각 후퇴했다가 감쇠 스프링/로그 보간으로 복귀하는 부위 애니메이션을 `TankEntity::evaluatePose`에 추가한다.
   - **카메라 반동 펀치 & 비대칭 토크**: 발사 순간 짧은 FOV/주시점 임팩트 펀치(과도한 멀미 없이 타격감만 전달)와 포탑 조준 각도(측면 사격 시 롤링 강조, 전방 사격 시 피칭 강조)에 따른 차체 반동 비대칭성을 강화한다.
2. **체공 시의 밋밋함 해소 (`CP04-FEEL` · `CP06-FX`)**:
   - 현재 체공 구간이 단순 포물선 상승·하강 위주라 공중 액션의 쾌감이 부족하다.
   - **체공 아크 리듬 & 공중 기동 연출**: 정점 부근 짧은 체류감(Hang-time) 대비 하강 가속감을 살리고, 공중에서 포탄 발사(역추진)나 `Shift` 에어 부스트 입력 시 진행 방향으로 차체 뱅크/기울기 변화와 명확한 속도 벡터 전환을 부여한다.
   - **체공 시각·물리 피드백**: 고고도 낙하 착지 시 착지 충격파(`Landing Slam` — 인접 소품/적 미세 흔들림 및 `FXEvent`)와 공중 포탄 궤적 라인/이펙트 요청을 추가한다.
3. **유저 친화적 조준 UX 개선 (`CP04-AIM` · `CP06-FX`)**:
   - 현재 3D 마우스 레이 단순 교차만으로는 이동·체공·고저차 상황에서 적을 직관적으로 맞추기 어렵다.
   - **소프트 에임 어시스트(자력 조준 & 탄도 보정)**: 커서 레이가 적 실루엣/근접 반경 내에 들어오면 조준점을 적 유효 부위 중심으로 부드럽게 흡착(자력 보정, 수동 미세 오프셋 허용)하고, 차체 기울기·고저차에 따른 포신 `pitch` 제한으로 탄이 엉뚱한 바닥에 박히지 않도록 발사 보정각을 지원한다.
   - **예상 탄착 인디케이터 & 명중 가능 피드백**: 포구 전방 실제 사선(`traceSegment`)이 닿는 월드 지점에 **3D 탄착 링/점**을 표시하고, 사선이 적에게 직격 가능한 상태(차폐물 없음 + 포신 각도 도달 완료)일 때 **조준선 색상을 즉시 강조(예: 시안/청록 → 적색/황색)**하여 발사 타이밍을 직관적으로 인지시킨다.

---

## 소유권과 팀 경계

| 소유자 | 데이터와 책임 | 전달값 |
|---|---|---|
| 변준영 · `game::` | ArenaScene이 플레이어 1대, 적·소품·탄환의 타입별 컬렉션을 소유한다. 탱크마다 강체 1개와 부위별 피격 OBB·포신 주퇴 상태를 둔다. 적은 안정적인 ID로 조회한다. | 모델 키, 부위 월드 행렬(`wheelSpinRad` 포함), 깊이 차폐 알파 힌트, 카메라 요청, 조준·탄착 HUD 상태, 일회성 FX |
| 박재성 · `render::` | 재질·모델 등록, 카메라 연출, 조명과 파티클. 같은 키의 GPU 모델은 여러 게임 객체가 공유한다. | 실제 표시한 view·projection·viewport·windowSize (`PresentedView`) |
| 공동 · 진입점과 공용 헤더 | 단일 GLFW 루프에서 입력 → 고정 갱신 → 실시간 카메라/줌 갱신 → 표시 순서를 연결한다. 공용 계약은 STL·GLM 값으로 표현한다. | 스냅샷은 여러 번 읽을 수 있고, FX는 프레임에 한 번 인계한다. |

## 체크아웃 로드맵

| 단계 | 목표일 | 상태 | 체크아웃에서 가능한 동작 |
|---|---|---|---|
| [CP01](#cp01) | 11/26~27 | **구현 완료** (`9912e47`) | 지상 사격 · `sand-tank.glb` 임시 프리뷰 · `F2` 단위 검사 |
| [CP02](#cp02) | 11/28 | **구현 완료** (`9592d37`) | 지상/공중 우클릭·홀드 줌, 깊이 알파블렌딩, `B` 폭발 체공, `r`/`R` 분리, 공전 카메라, 초기 코믹 반동·피격 |
| [CP03](#cp03) | 11/29 | **다음 착수** | `RenderBridge.h` 확정 및 팀 렌더 백엔드 연결 |
| [CP04](#cp04) | 11/30~12/1 | 예정 | 코믹 물리 통합(`SAT`·드럼통) + **포탄 반동 입체화 · 체공 밋밋함 해소 · 유저 친화적 조준 UX** |
| [CP05](#cp05) | 12/2 | 예정 | 다중 적 관리(`spawnEnemy`)와 4순위 행동 트리(`tickBrain`) |
| [CP06](#cp06) | 12/3~4 | 예정 | Meshy 모델 정합 · OpenGL 2D Ortho `GameHUD`(동적 조준·탄착 UI) · 접점/착지 FX |
| [CP07](#cp07) | 12/5 | 예정 | 타 PC `Release` 리소스·셰이더 검증 및 연속 플레이(`Shift+R`) 제출 후보 확정 |

---

<a id="cp01"></a>

## CP01 · 지상 사격 (11/26~27) — [완료: `game/cp01@9912e47`]

- `CP01-RESET`, `CP01-TICK`, `CP01-POSE`, `CP01-FIRE`, `CP01-HIT` 및 `F2` 단위 검사(`QA-02`, `QA-03`, `QA-06`) 구현 완료.
- `src/utf8.manifest` 적용으로 한글 경로 Assimp 로드 문제를 해결하고 `tmpBoxing::PreviewRenderer` 및 `SandTankModel`(`sand-tank.glb`)을 연결했다.
- **변경 참고**: 이후 CP02에서 장면 재시작 키가 대문자 `R`(`Shift+R` / `CapsLock+R` / `F5`)로 분리되었다.

---

<a id="cp02"></a>

## CP02 · 공중 트릭샷 및 코믹 기동 초안 (11/28) — [완료: `game/cp02@9592d37`]

- `CP02-FOCUS`(`selectTarget`), `CP02-AIR`(`canEnter` 단위 경계 함수 유지), `CP02-SHOT`(`TrickshotController::update`) 구현 완료.
- **확정된 조작·연출 확장 사항**:
  - 지상·체공 공통 `RMB` 홀드 즉시 줌인(`0.32x`) → `LMB` 즉시 해제·발사, `LMB 1.2s` 홀드 차징 줌인·해제 발사.
  - 줌인 중 카메라-타겟 뷰 깊이 검사 기반 2-Pass 차폐물 반투명(`alpha = 0.14`, 플레이어 포신·타겟·포탄·바닥 불투명).
  - `B` 폭발 도약(`0.55x` 약슬로우 토글), `Shift` 가속 돌진(`2.15x`), 소문자 `r` 현재 조준점 기준 차체 자세·`W` 전방 기준축 원위치 정렬, 대문자 `R` 장면 리셋.
  - 월드 조준점(`worldAimPoint_`)·커서 인접 적(`hoveredEnemyId_`) 기반 `WASD` 이동 연동 공전 쿼터뷰 카메라, 차체 8-꼭짓점 지면 지지(`minBodyYAboveSurface`) + 지상 소폭 체공 반동(`recoilHopActive_`) + 2-구체 회전 OBB 충돌 처리.

---

<a id="cp03"></a>

## CP03 · 팀 렌더 연결 (11/29)

**목표**  `tmpBoxing::PreviewRenderer` 단독 프리뷰와 박재성의 `render::` 백엔드가 같은 공용 스냅샷(`src/shared/RenderBridge.h`)을 읽도록 연결한다. 11월 29일 저녁 팀 협의에서 공용 헤더 구조체와 카메라·알파블렌딩·이벤트 반환 규약을 확정한다.

**선행 작업**  `game/cp02` 완료본 및 `render::` 백엔드 초안. 예정 태그 `game-cp03`.

### 슈도 코드

<a id="cp03-frame"></a>

#### CP03-FRAME · ArenaScene::buildFrame & RenderBridge.h

공동 담당 · 선행 CP02  
입력은 이전·현재 틱 자세, `renderAlpha`, 누적 `FXEvent`, 줌·조준 상태. 결과는 `RenderFrameSnapshot`, `FXEvent` 목록, `PresentedView`.

```text
src/shared/RenderBridge.h (순수 STL · GLM 값 타입만 사용)
    ModelKey / PartRenderItem { entityId, modelKey, worldMatrix, tint, wheelSpinRad, keepOpaqueInZoom, worldCenter, boundingRadius }
    ZoomOcclusionState { zoomAlphaFade, zoomFocusPoint }
    CameraRequest { activeOrMain, position, lookAt, fovDeg, shakeImpulse }
    AimHudSnapshot { reticleWorldPoint, muzzleTraceHitPoint, canHitEnemy, chargeProgress, selectedTargetId, timeScale }
    FXEvent { type(MuzzleFlash, CannonRecoilDust, HitSpark, Explosion, LandingSlam), origin, normalOrDir, scale }
    PresentedView { view, projection, viewport, windowSize, valid }

ON Application::Update 끝
    snap ← ArenaScene::buildRenderSnapshot(renderAlpha)
    fxEvents ← ArenaScene::consumeFrameFXEvents()   // 프레임당 1회 인계 후 내부 큐 비움
    renderBackend.update(clampedRawDt, snap, fxEvents)
ON Application::Render
    renderBackend.draw(snap, viewMatrix, projectionMatrix)
    PresentedView 갱신 (DPI 창 크기 windowSize와 프레임버퍼 viewport 분리 유지)
```

<a id="cp03-glsl"></a>

#### CP03-GLSL · ExternalRenderBackend::draw

재성·공동 담당 · 선행 CP03-FRAME  
입력은 `RenderFrameSnapshot`(`PartRenderItem`, `ZoomOcclusionState`), `view`, `projection`. 결과는 텍스처/단색 3D 장면 + 줌인 차폐 반투명 패스.

```text
1패스 (불투명, glDepthMask(GL_TRUE))
    바닥·지형·포탄·줌 타겟 및 카메라-타겟 깊이 바깥 객체, 플레이어 포신(Barrel) 렌더
2패스 (줌 차폐 반투명, zoomAlphaFade > 0.01일 때 glDepthMask(GL_FALSE) + GL_BLEND)
    카메라-타겟 뷰 깊이 사이에 위치한 차체·포탑·바리케이드 등을 alpha ≈ 0.14로 렌더
렌더 종료 시 깊이 쓰기·블렌딩·바인딩 상태 복원
```

### 사용자 검사

| 검사 ID · 작업 | 초기 상태와 조작 | 관찰할 결과 |
|---|---|---|
| QA-13 · CP03-FRAME (통합) | 프리뷰와 팀 렌더 백엔드 전환 → 창 크기 변경·DPI 배율·우클릭 줌 전환 뒤 적 피킹 및 발사 | 부위 피벗·궤도 회전·선택 레이가 일치하고, 같은 `FXEvent`가 중복 재생되지 않음 |
| QA-14 · CP03-GLSL (통합) | 텍스처 모델·단색 소품·줌인 차폐 상황 동시 렌더 | 재질 누수 없이 그려지며 줌인 시 포신·타겟은 불투명, 앞쪽 가림 파츠만 반투명 처리됨 |

---

<a id="cp04"></a>

## CP04 · 코믹 물리 심화 · 반동/체공 감각 · 유저 친화적 조준 (11/30~12/1)

**목표**  ① CP02의 1차 반동·충돌 로직을 일반 접점 충격(`applyImpulseAt`)·`SAT` 접촉 해결·폭발 드럼통으로 통합하고, ② **포탄 반동의 입체감**, ③ **체공 구간의 액션 리듬(밋밋함 해소)**, ④ **유저 친화적 조준 보조·탄착 표시**를 완성한다.

**선행 작업**  CP03 스냅샷 계약 및 CP02 반동·충돌 기반. 예정 태그 `game-cp04`.

### 슈도 코드

<a id="cp04-impulse"></a>

#### CP04-IMPULSE · Physics::applyImpulseAt (기존 `applyHitImpulse`·`PropBody` 통합)

준영 담당 · 선행 CP01-HIT, CP03-FRAME  
입력은 월드 접점 `p`, 충격 `J`, 질량중심 `c`, 관성 텐서. 결과는 중앙/모서리 피격이 구분되는 선속도·각속도.

```text
r ← p − c
선속도 += 역질량 × J (최대 선속도 클램핑)
각속도 += 월드 역관성 × cross(r, J) (최대 각속도 클램핑)
중앙 정면 피격(r × J ≈ 0)은 직선 밀림 위주, 모서리·포탑 상단 피격은 강한 회전·구름 발생
```

<a id="cp04-contact"></a>

#### CP04-CONTACT · Physics::resolveContacts & ExplosiveDrum

준영 담당 · 선행 CP04-IMPULSE  
입력은 탱크·드럼통·바리케이드 회전 `OBB`, 바닥·램프. 결과는 관통 없는 구름·접지와 연쇄 폭발 1회 보장.

```text
차체·소품 쌍 → 회전 OBB SAT 검사로 최소 침투 법선·깊이·접점 산출 (기존 2-구체 근사를 정식 OBB 접촉으로 보강)
바닥·램프 → 기존 8-꼭짓점 지지 높이(minBodyYAboveSurface) + 접점 상대속도 기반 저속 바운스 억제·마찰 감쇠 결합
ExplosiveDrum 피격 또는 강한 충돌 시 → 반경 폭발 충격파 발행(exploded 플래그로 동일 원인 중복 폭발 차단)
```

<a id="cp04-feel"></a>

#### CP04-FEEL · 포탄 반동 입체화 및 체공 액션 강화 [신규 추가]

준영 담당 · 선행 CP02, CP04-IMPULSE  
입력은 발사 이벤트, 조준 방향, 체공 상태(`altitude`, `velocity`, `Shift`·사격 입력). 결과는 포신 주퇴복좌, 카메라 임팩트, 다이나믹 체공 기동.

```text
ON 포탄 발사 (지상·체공 공통)
    barrelRecoilOffset_ ← 최대 후퇴량(예: 0.32m), 매 틱 비선형 스프링/로그 감쇠로 0 복귀 → evaluatePose의 barrel 로컬 Z에 반영
    카메라 임팩트 ← 짧은 FOV 펀치(+1.8° 후 즉시 복귀) + 조준 반대 방향 미세 반동 킥 전달
    차체 각속도 ← 포탑 상대 조준각(측면 사격 시 차체 롤링 증폭, 전방 사격 시 노즈업 피칭 증폭) 반영
ON 체공 구간 (밋밋함 해소)
    정점 부근(|vy| < 2.2m/s) 짧은 체류감과 하강 구간 가속 대비를 강화해 점프 아크의 리듬감 확보
    공중에서 Shift(에어 부스트) 또는 사격 역추진 시 진행 방향으로 즉각적인 궤도 꺾임 + 차체 뱅크 기울기 부여
    낙하 속도 vy < -7.5m/s 착지 시 → LandingSlam 이벤트 발행(주변 소품 미세 흔들림 + 착지 먼지/충격파 FX)
```

<a id="cp04-aim"></a>

#### CP04-AIM · 유저 친화적 조준 보조 및 예상 탄착 피드백 [신규 추가]

준영 담당 · 선행 CP02-FOCUS, CP03-FRAME  
입력은 화면 커서 레이, 적 부위 OBB, 현재 포구 자세(`muzzleOrigin`, `muzzleForward`). 결과는 소프트 에임 보정 목표점과 탄착 피드백 데이터.

```text
1. 소프트 에임 어시스트 (자력 보정)
    커서 레이 주변 원뿔/반경(예: 1.8m~2.4m) 내에 살아 있는 적이 있으면
    커서 이탈 전까지 적 유효 중심(차폐되지 않은 노출 부위 우선)으로 조준 목표점을 부드럽게 자력 보정한다
    단, 줌인 정밀 조준 중에는 사용자의 미세 마우스 오프셋을 그대로 존중한다
2. 실제 포구 사선 추적 (Predicted Impact)
    실제 발사될 pose.muzzleOrigin → pose.muzzleForward × 사거리 선분으로 traceSegment 수행
    탄착 월드 좌표(impactPoint)와 적 직격 가능 여부(canHitEnemy: 사선상 첫 접촉이 살아 있는 적일 때 true) 산출
    고저차·차체 기울기로 포신 pitch 한계에 걸려 사선이 어긋날 경우, HUD에 실제 탄착점과 커서 조준점의 차이를 시각적으로 분리 표시한다
```

### 사용자 검사

| 검사 ID · 작업 | 초기 상태와 조작 | 관찰할 결과 |
|---|---|---|
| QA-15 · CP04-IMPULSE (단위) | 정지한 동일 질량·관성 차체 두 개 → 같은 `J`를 질량중심과 중심에서 벗어난 접점에 각각 적용 | 선속도 변화 동일. 중앙은 각속도 변화 0, 모서리는 `r × J` 방향 회전 |
| QA-16 · CP04-CONTACT (플레이) | 차체 모서리가 먼저 닿도록 기울여 낙하 후 평지에 방치 | 바닥 관통 없이 모서리 접촉 후 반대쪽이 내려와 정지. 무한 바운스·끼임 없음 |
| QA-17 · CP04-CONTACT (단위) | 회전 상자 쌍과 폭발 드럼통 → 분리·겹침 검사 및 피격 시 폭발 처리 | 분리 시 무접촉, 겹침 시 유한 법선·깊이, 드럼통은 동일 원인에 1회만 폭발 |
| QA-24 · CP04-FEEL (플레이) | 지상 측면/전방 사격 후 `B` 도약 → 공중 `Shift` 기동·공중 사격 → 고고도 착지 | 발사마다 포신 주퇴복좌와 차체 롤/피치 반동이 구분되고, 공중 기동 궤도 변화와 고고도 착지 충격이 명확히 체감됨 |
| QA-25 · CP04-AIM (플레이) | 이동(`WA`) 및 체공 중 적 근처에 커서를 두었을 때와 바리케이드 뒤에 가렸을 때 비교 | 적 인접 시 부드럽게 조준이 보정되고 직격 가능할 때만 탄착 마커·조준선 색상이 강조되며, 차폐 시 바리케이드 위 탄착점이 표시됨 |

---

<a id="cp05"></a>

## CP05 · 적 관리와 행동 (12/2)

**목표**  같은 모델을 공유하는 적 2대 이상이 독립된 `EntityId`와 상태머신/행동 트리(`tickBrain`)로 탐색·추적·사격·드럼통 유폭을 수행하고, 공중·피격 중에는 물리 복구를 최우선으로 삼는다.

**선행 작업**  CP04 코믹 물리·드럼통 및 기존 `ArenaScene` `EntityId` 구조. 예정 태그 `game-cp05`.

### 슈도 코드

<a id="cp05-entity"></a>

#### CP05-ENTITY · ArenaScene::spawnEnemy

준영 담당 · 선행 CP04-CONTACT  
- 기존 단일 표적 벡터(`enemies_`)를 `spawnEnemy(spec, pos, yaw, patrolWaypoints)` 일반 생성·사망 정리 구조로 확장한다.
- 사망 틱 종료 시 제거(또는 파괴 잔해 상태 전환)하며 `selectedTargetId_`·`hoveredEnemyId_` 유효성을 즉시 갱신한다.

<a id="cp05-bt"></a>

#### CP05-BT · EnemyEntity::tickBrain

준영 담당 · 선행 CP05-ENTITY  

```text
1순위 (물리 복구): IF airborne OR recoilHop/피격 기울기 초과 → 주행·사격 정지, 착지·자세 안정 대기
2순위 (위험물 활용): ELSE IF 플레이어 인근 폭발 드럼통에 사선 확보 & 쿨다운 완료 → 드럼통 조준·사격
3순위 (원거리 교전): ELSE IF 플레이어 시야 확보 & 사거리 내 → 포탑 추적 조준 및 쿨다운 완료 시 일반 사격
4순위 (추적 / 순찰): ELSE IF 마지막 감지 위치 유효 → 장애물 회피 접근, 아니면 지정 순찰 지점 왕복
```

### 사용자 검사

| 검사 ID · 작업 | 초기 상태와 조작 | 관찰할 결과 |
|---|---|---|
| QA-18 · CP05-ENTITY (플레이) | 같은 모델의 적 A·B 스폰, B 우클릭 지정 → B 격파 후 A 피격 | B 제거 즉시 포커스가 해제되고 A의 HP만 독립 감소. 모델 중복 로드 없음 |
| QA-19 · CP05-BT (단위) | 동일 적에 피격 체공 / 시야·사거리 내 / 마지막 위치 / 미감지 조건 순차 입력 | 각각 복구(발사 0회) · 사격(쿨다운 준수) · 추적 · 순찰 명령 생성 |

---

<a id="cp06"></a>

## CP06 · 모델·HUD·효과 통합 (12/3~4)

**목표**  박재성의 `ModelRegistry`·파티클·조명과 변준영의 OpenGL 2D Ortho `GameHUD`(유저 친화적 조준·탄착 UI 포함) 및 물리/전투 이벤트를 완전 통합한다.

**선행 작업**  CP05 적 행동, CP03 렌더 브리지, CP04 조준·반동 데이터. 예정 태그 `game-cp06`.

### 슈도 코드

<a id="cp06-model"></a>

#### CP06-MODEL · ModelRegistry / TankEntity::evaluatePose

준영·재성 담당 · 선행 CP05-ENTITY, CP03-GLSL  
- 선행 검증된 `makeSandTankSpec()` 피벗·OBB 규격을 기준으로 최종 적·플레이어·소품(드럼통·바리케이드·램프) 모델 키와 로컬 보정 행렬을 일치시킨다.
- `CP04-FEEL`의 포신 주퇴복좌(`barrelRecoilOffset_`)가 적용된 `barrelWorld` / `barrelDraw` 행렬이 최종 모델에도 동일하게 반영되는지 검증한다.

<a id="cp06-fx"></a>

#### CP06-FX · GameHUD::draw & ExternalRenderBackend FX

준영·재성 담당 · 선행 CP06-MODEL, CP04-FEEL, CP04-AIM  

```text
3D 장면(불투명 → 줌 차폐 반투명) → 3D 탄착 링/조준선 보조 → 파티클(포구 화염·피격 스파크·폭발·착지 충격파) → 2D Ortho GameHUD 순서로 렌더
GameHUD (ImGui 미사용, OpenGL 2D 직교 투영)
    1. 커서 조준원 + 실제 포구 예상 탄착 마커 (canHitEnemy == true일 때 강조 색상 및 락온 표시)
    2. 좌클릭 1.2s 차징 줌 진행 링 / 우클릭 줌 상태 아이콘
    3. 플레이어 HP · Shift 부스트 상태 · 잔여 적 수 / 웨이브 클리어 배너
```

### 사용자 검사

| 검사 ID · 작업 | 초기 상태와 조작 | 관찰할 결과 |
|---|---|---|
| QA-20 · CP06-MODEL (통합) | 최종 모델과 디버그 OBB 동시 표시 → 포탑 회전·포신 고각·발사 주퇴복좌 확인 | 힌지·포구·주퇴 방향 정렬 일치. OBB 중앙은 명중, 실루엣 바깥 탄은 빗나감 |
| QA-21 · CP06-FX (플레이) | 텍스처 적용 적 조준·명중·빗나감·고고도 착지 재현 | 적 직격 가능 시 HUD 조준원 색상 전환, 명중에만 피격 FX 1회, 착지 시 충격 FX 표시, HUD 깊이/블렌딩 간섭 없음 |

---

<a id="cp07"></a>

## CP07 · 제출 후보 (12/5~12/6)

**목표**  다른 팀원 PC 및 별도 클린 폴더에서 `Release | x64` 리소스 배치와 연속 플레이·재시작(`Shift+R`)·종료를 최종 검증하고 시연 영상·보고서와 함께 제출한다.

**선행 작업**  CP06 통합본 및 동일 리소스 배포본. 예정 태그 `game-cp07`.

### 슈도 코드

<a id="cp07-asset"></a>

#### CP07-ASSET · Release 리소스 및 인코딩 배치

공동 담당 · 선행 CP06-FX  
- 모든 팀 텍스트·셰이더 원본은 UTF-8 BOM + CRLF로 저장소에서 관리하고, 셰이더 로더(또는 배포 스크립트)에서 선두 3바이트 UTF-8 BOM을 안전히 건너뛰어 컴파일한다(`SandTankModel.cpp` 검증 방식 통일).
- `src/utf8.manifest`가 포함된 `Release | x64` 실행 파일·필수 DLL(`glew32.dll`, `assimp-vc145-mt.dll`)·`assets/` 상대 경로를 클린 폴더와 팀원 PC 양쪽에서 대조한다.

<a id="cp07-replay"></a>

#### CP07-REPLAY · 연속 플레이 · `r`/`R` 경계 · 자원 해제

공동 담당 · 선행 CP07-ASSET  
- 1회차 플레이(`RMB` 줌·사격·드럼통 폭발·`B` 도약·`r` 차체 원위치·웨이브 진행) → **대문자 `R`(`Shift+R` 또는 `F5`)** 장면 리셋 → 2회차 플레이에서 이전 탄환·포커스 ID·줌 알파블렌딩·슬로우 배율·FX·드럼통 상태 잔류가 `0`인지 확인 후 정상 종료(`GL` 자원 해제)한다.

### 사용자 검사

| 검사 ID · 작업 | 초기 상태와 조작 | 관찰할 결과 |
|---|---|---|
| QA-22 · CP07-ASSET (통합) | 한글 사용자 경로 및 다른 팀원 PC의 클린 폴더에서 `Release` 실행 | 셰이더/Assimp 경로·컴파일·링크 오류 없이 모델·알파블렌딩·HUD·효과 정상 표시 |
| QA-23 · CP07-REPLAY (통합) | `Release` 실행 → 교전·줌·체공·`r` 정렬 → `Shift+R`(`R`) 리셋 → 재교전 → `ESC` 종료 | 2회차에 1회차 ID·탄환·FX·배율·반투명 상태 잔류 없음, 종료 시 크래시/오류 없음 |
