# Chaos Tank Arena 개발 로그 — 2026-11-26

- **작성일**: 2026-11-26
- **담당**: 변준영 (`src/game/`, `namespace game`)
- **기준 커밋 / 작업 브랜치**: `main@dec36c2` → `game/cp01`

---

## 1. 코드 및 협업 컨벤션

### 1.1 모듈 경계와 파일 관리

| 구분 | 경로 및 네임스페이스 | 소유 책임 및 경계 규칙 |
| --- | --- | --- |
| 게임 로직 | `src/game/`, `namespace game` | 시간 관리(`TimeManager`), 탱크 계층(`TankEntity`), 물리·충돌(`Physics`), 아레나 세션(`ArenaScene`), 향후 트릭샷·AI·HUD |
| 렌더링 | `src/render/`, `namespace render` | 재질·모델 레지스트리, 카메라 연출, 조명, 파티클, 외부 렌더 백엔드 |
| 임시 박싱·루프 | `src/tmpBoxing/`, `namespace tmpBoxing` | 본 구현 전 전체 루프 구동에 필요한 임시 박싱 렌더러(`PreviewRenderer`), 경로 정규화(`AssetPath`) 및 미구현 담당 영역 대체 스크립트. 박싱용 `.obj`는 `assets/boxing/`에 배치한다. |
| 공동 경계 | `src/shared/`, `src/main.cpp` | 단일 GLFW 루프 조립(`입력 → 고정 틱 갱신 → 렌더`) 및 모듈 간 순수 값 타입(`STL`·`GLM`) 전달 규격 |

- 상대 모듈(`src/game/` ↔ `src/render/`)의 내부 헤더는 직접 `#include`하지 않는다.
- OpenGL 핸들(`VAO`, `VBO`, 프로그램 등)과 로컬 프레임워크 객체(`Shader`, `Model`, `Mesh`, `Camera`)는 각 담당 모듈이 소유하고 GL 컨텍스트가 유효한 동안 해제한다.
- 담당 폴더에 새 `.cpp`/`.h` 파일을 추가하면 같은 폴더의 `Sources.props`(`src/game/Sources.props`, `src/render/Sources.props`, `src/tmpBoxing/Sources.props`)에 `$(MSBuildThisFileDirectory)` 기준으로 등록한다.

### 1.2 컴파일 환경 및 헤더 규칙

- **컴파일러 설정**: Visual Studio `v145`, `stdcpplatest` (C++23), `/W3`, Conformance Mode(`true`), `x64` (`Debug` / `Release`).
- **헤더 가드 및 포함 경로**:
  - 모든 헤더 파일 최상단에 `#pragma once`를 사용한다.
  - 프로젝트 내부 헤더는 `src/` 루트 기준 큰따옴표 경로(`"game/..."`, `"render/..."`, `"shared/..."`)를 사용하고, 로컬 프레임워크·외부 라이브러리·표준 헤더는 꺾쇠(`<...>`)를 사용한다.
  - `src/main.cpp`에서 프레임워크 입력 콜백을 사용할 때는 `#include <Application.inl>` 이전에 `CG_APPLICATION_CUSTOM_CALLBACK_KEY`, `CG_APPLICATION_CUSTOM_CALLBACK_MOUSE_BUTTON`, `CG_APPLICATION_CUSTOM_CALLBACK_CURSOR_POS` 매크로를 정의한다.

### 1.3 명명 규칙 (`namespace game`)

- **타입 (`class`, `struct`, `enum class`, `using`)**: `PascalCase` (예: `ArenaScene`, `TankEntity`, `TickCommand`, `PartKind`)
- **함수·메서드·지역 변수·매개변수**: `camelCase` (예: `beginFrame`, `consumeStep`, `evaluatePose`, `traceSegment`)
- **클래스 비공개 멤버 변수**: 후행 밑줄 `camelCase_` (예: `accumulator_`, `cooldownRemaining_`, `projectiles_`)
- **상수**: `kPascalCase` + `constexpr` / `inline constexpr` (예: `kFixedDt`, `kInvalidEntityId`)
- **로컬 배포 프레임워크 호출 시**: `framework/local/` 내부 클래스(`Application`, `Camera`, `Shader`, `Model`, `Mesh`)는 배포본 고유의 `PascalCase` 메서드 및 `m_` 멤버 명명을 그대로 호출하며 배포본 소스는 수정하지 않는다.

### 1.4 좌표·시간·시뮬레이션 규약

- **좌표계**: 오른손 좌표계 기준 `+Y` 상방, `XZ` 평면을 지면(바닥 상단 `y = 0.0f`), 엔티티 로컬 전방을 `-Z`(`(0, 0, -1)`)로 통일한다.
- **고정 물리 틱과 표시 보간**:
  - 시뮬레이션은 `kFixedDt = 1.0f / 120.0f` 고정 틱으로만 갱신한다.
  - 0틱 프레임에서 발생한 일회성 입력(좌클릭 발사, `R` 재시작 등)은 다음 유효 틱이 소비할 때까지 보존하고, 소비 직후 해제한다.
  - 화면 표시용 프레임(`buildFrame(alpha)`)은 잔여 누적 시간 비율 `alpha`로 직전·현재 틱 자세를 보간(`glm::mix`, `glm::slerp`)한다.
- **강체 계층과 그리기 스케일 분리**:
  - `bodyWorld → turretWorld(로컬 yaw) → barrelWorld(힌지 로컬 pitch) → muzzle` 체인에는 순수 강체 변환(이동·회전)만 유지한다.
  - 단위 큐브(`[-0.5, 0.5]^3`) 등의 메시 크기 보정(`halfExtents * 2.0f`)은 부위별 그리기 행렬(`bodyDraw`, `turretDraw`, `barrelDraw`)에만 곱해 자식 피벗과 피격 `OBB`가 왜곡되지 않도록 한다.
- **안정 엔티티 ID**: 컨테이너 원소 주소나 인덱스를 포커스·탄환 소유자로 저장하지 않고 `EntityId`(`std::uint32_t`, 무효값 `0`)로 조회한다.

### 1.5 인코딩 및 Git 규칙

- 새로 작성하거나 수정하는 팀 공유 텍스트 파일은 **UTF-8 BOM (`EF BB BF`) + CRLF**로 저장한다.
- 브랜치는 최신 `main`에서 `game/<작업명>` 또는 `render/<작업명>`으로 분기하고, 커밋 제목과 본문은 한글로 작성한다.
- 대용량 배포본(`framework/local/`, `assets/local/`), 빌드 산출물(`build/`), 개인 환경 파일은 Git 추적에서 제외한다.

---

## 2. 작업 내역 요약

### 2.1 기준선 확인 (`main@dec36c2`)

- 로컬 프레임워크(`framework/local/ComputerGraphics/`) 헤더·셰이더(`projectionView.vs`, `uniformColor.fs`)·기본 메시(`cube.obj`) 및 `ChaosTankArena.vcxproj` 링크 구성을 점검했다.
- `main@dec36c2`에서 `game/cp01` 브랜치를 생성했다.

### 2.2 CP01 지상 사격 최소 플레이 루프 구현 (`game/cp01`)

| 파일 | 주요 구현 내용 |
| --- | --- |
| `src/game/TimeManager.h`, `src/game/TimeManager.cpp` | `1/120`초 고정 틱 누적·소비(`TimeManager`), 키 홀드 상태와 일회성 클릭·재시작 큐를 분리 관리하는 `InputBuffer` 구현 |
| `src/game/Physics.h`, `src/game/Physics.cpp` | 슬랩(Slab) 방식 선분-회전 `OBB` 교차 검사(`intersectSegmentOBB`) 및 발사자 제외 최소 `fraction` 단일 접촉 선택(`Physics::traceSegment`) 구현 |
| `src/game/TankEntity.h`, `src/game/TankEntity.cpp` | `WASD` 차체 주행·회전, 마우스 레이 목표점 기반 포탑 `yaw`·포신 `pitch` 계산(`aimAt`), 방향키 수동 부위 회전(`adjustManualAim`), 부위 `OBB`·그리기 행렬·포구 자세 산출(`evaluatePose`), 쿨다운 기반 탄환 생성(`tryFire`) 구현 |
| `src/game/ArenaScene.h`, `src/game/ArenaScene.cpp` | 바닥·플레이어(ID 1)·정지 표적(ID 2) 배치 및 `R` 초기화(`reset`), 탄환 이동·최초 피격 1회 피해·지형 접촉/수명 만료 제거, 보간 프리뷰 스냅샷(`buildFrame`), `F2` 단위 검사(`runCp01UnitChecks`: QA-02·03·06) 구현 |
| `src/tmpBoxing/AssetPath.h`, `src/tmpBoxing/AssetPath.cpp` | 저장소 루트·빌드 폴더 기준 상대 경로 탐색 및 `lexically_normal().make_preferred()` 구분자 정규화 헬퍼 구현 |
| `src/tmpBoxing/PreviewRenderer.h`, `src/tmpBoxing/PreviewRenderer.cpp` | `assets/boxing/cube.obj`(Assimp `Model`)와 배포본 `projectionView.vs` + `uniformColor.fs`를 사용한 단색 박스, `OBB` 와이어프레임, 축·힌지·포구 조준선 렌더링 및 종료 시 GL 자원 해제 구현 |
| `assets/boxing/cube.obj` | 임시 박싱 프리뷰용 단위 큐브(`[-0.5, +0.5]^3`) 메시 배치 (`.gitignore` `!/assets/**/*.obj` 규칙으로 Git 추적) |
| `src/utf8.manifest`, `ChaosTankArena.vcxproj` | Windows 프로세스 활성 코드페이지 UTF-8 매니페스트(`<activeCodePage>UTF-8</activeCodePage>`) 연결로 배포본 `Model.cpp`(Assimp) 수정 없이 한글 경로(`변준영\바탕 화면`) 모델 로드 실패 해결, `src/tmpBoxing/Sources.props` 임포트 추가 |
| `src/game/Sources.props`, `src/tmpBoxing/Sources.props` | 게임 로직(`src/game/`)과 임시 박싱·루프 코드(`src/tmpBoxing/`)의 MSBuild 소스 목록 분리 등록 |
| `src/main.cpp` | 콜백 매크로 활성화, 고정 틱 루프 및 재시작 시 입력·시간·카메라 초기화 연결, 좌클릭 시 `Application::CaptureMouse`가 강제하는 `GLFW_CURSOR_DISABLED`를 일반 조준 커서(`GLFW_CURSOR_NORMAL`)로 복원(휠 클릭 유지 시에만 자유 시점 회전 허용), 표시 카메라 행렬(`PresentedView`) 및 `tmpBoxing::PreviewRenderer` 연결 |

### 2.3 검증 상태 및 후속 예정 작업

- **완료된 확인**: 전 신규·수정 파일 UTF-8 BOM(`efbbbf`) 및 CRLF 개행 적용, `git diff --check` 공백·개행 이상 없음 확인.
- **사용자 확인 대기 (`QA-01` ~ `QA-07`)**:
  - Visual Studio `Debug | x64` 빌드 및 실행 확인
  - `F2` 단위 검사(`QA-02`, `QA-03`, `QA-06`) 콘솔 `PASS` 확인
  - 플레이 검사: `R` 재시작 복원(`QA-01`), 부위 계층 독립 회전·힌지 고정(`QA-04`), 좌클릭 유지 시 단발 발사·포구 정렬(`QA-05`), 빗나감 시 무피해·탄환 소멸(`QA-07`)
- **다음 단계**: 사용자 빌드·QA 통과 확인 후 `game/cp01` 커밋 기록 및 `CP02`(점프 램프, 우클릭 적 지정, 체공 트릭샷 슬로우·포커스 전환) 착수.
