# Chaos_Tank_Arena
Chaos_Tank_Arena Based on OpenGL, For Computer Graphics 26-2 Lecture    
26-2 컴퓨터 그래픽스 과목 term 프로젝트 협업 위한 repository

## 기술 스택:  
openGL  
visual studio, c++
glfw  
glew (or glad)  
assimp  
개인 개발한 프레임워크

## 제출과 역할

제출 마감은 **2026년 12월 6일**이다. 11월 28일까지의 역할별 목표를 바탕으로 11월 29일 저녁에 진행 상황을 논의한다. FigJam은 역할과 초안의 기준이며 세부 구현은 두 사람이 조정한다.

| 담당 | 작업 위치 | 기획 초안의 담당 범위 |
| --- | --- | --- |
| 변준영 | `src/game/`, `namespace game` | 시간 관리, 아레나·탱크 계층, 물리, 트릭샷, HUD. 기존 실습의 필요한 기능을 팀 프레임워크에 통합한다. |
| 박재성 | `src/render/`, `namespace render` | 텍스처·재질, 모델 등록, 카메라 애니메이션, 조명, 파티클, 단순 적 타겟. |
| 공동 | `src/shared/`, `src/main.cpp` | 프레임 상태와 효과 이벤트의 전달 규격, 앱 진입점. `src/shared/RenderBridge.h`는 개발 단계에서 합의할 헤더 위치다. |

두 사람 모두 같은 Drive 프레임워크를 받는다. 기존 명세·마일스톤에 적힌 별도 프레임워크, 세부 클래스, 날짜별 구현 목록은 이전 초안이다.

[FigJam 기획서](https://www.figma.com/board/rqLyZIJ20aW1PyBxnYrJet/) · [제안서](docs/컴그제안서_Chaos_Tank_Arena_Proposal.docx) · [구현 초안](docs/IMPLEMENTATION_SPEC_DRAFT.md) · [마일스톤 초안](docs/MILESTONE_PLAN.md)

## 다른 PC에서 시작하기

1. 저장소를 clone한다. 초기 설정 PR 병합 전에는 `git switch codex/junyoung-setup`으로 설정 브랜치를 연다. 병합 후에는 최신 `main`에서 시작한다.
2. 각자 [ComputerGraphics.zip](https://drive.google.com/file/d/1FNkOfKsmY0-IKXOrCCD4LBZ7MYmNH08K/view)을 받아 [프레임워크 배치 안내](framework/README.md)에 따라 압축을 푼다.
3. C++ 데스크톱 개발 구성, MSVC v145, Windows 10 이상 SDK가 있는 Visual Studio에서 저장소 루트의 `ChaosTankArena.slnx`를 연다. `Debug | x64` 또는 `Release | x64`를 선택한다.
4. 사용자 확인 시 F5를 누른다. 기대 결과는 제목이 `Chaos Tank Arena`인 빈 창이 열리고 닫기 버튼으로 종료되는 것이다. 실행 파일과 필요한 DLL은 `build/x64/<구성>/`에 모인다. 디버거의 작업 디렉터리는 저장소 루트다.

## 협업 규칙

- 각자 별도 clone 또는 worktree에서 작업한다. 변준영은 `game/<작업명>`, 박재성은 `render/<작업명>` 브랜치를 최신 `main`에서 만든다. 담당 기능 단위로 커밋하고 PR로 통합한다.
- 담당 폴더에 C++ 파일을 만들고 같은 폴더의 `Sources.props`에 등록한다. 상대 모듈의 내부 헤더는 포함하지 않는다. 공유 데이터에는 STL·GLM 값 타입을 사용하고 프레임워크 객체와 OpenGL 핸들은 담당 모듈이 소유한다.
- 공용 헤더·진입점·솔루션·프로젝트 설정·공용 셰이더를 바꿀 때는 상대 담당자가 PR에서 검토한다. 커밋 제목과 본문은 한글로 작성한다.
- 팀에서 편집할 소형 리소스와 셰이더는 `assets/`, 팀 코드·설정은 Git으로 공유한다. 별도 배포 대용량 리소스는 `assets/local/`에 둔다. ZIP에 들어 있는 데이터·셰이더는 원래 배포 경로를 유지한다. 새 대용량 자료는 Drive 위치와 버전을 먼저 공유한다.
- 개인 IDE 설정, 에이전트 지침, 배포 ZIP, 빌드 결과는 Git에서 제외한다. 새로 작성하거나 수정하는 팀 텍스트 파일은 UTF-8 BOM으로 저장한다.
