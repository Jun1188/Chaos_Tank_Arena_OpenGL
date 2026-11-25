# 공용 프레임워크 배치

[ComputerGraphics.zip 다운로드](https://drive.google.com/file/d/1FNkOfKsmY0-IKXOrCCD4LBZ7MYmNH08K/view). 설정 기준 배포본은 62,337,429바이트이며 SHA-256은 `1ff45f2d83b06b77cbec31865f1721066461d9225c2d99074e67eb143d8efe3d`이다. 배포본이 바뀌면 이 값과 경로를 같은 PR에서 갱신한다.

1. 저장소 루트에서 `framework/local/`을 만들고 ZIP을 그 안에 둔다.
2. 새 설치 시 루트에서 다음 PowerShell 명령으로 푼다. ZIP 안에 이미 최상위 `ComputerGraphics` 폴더가 있다. 아래 명령은 배포자의 Git 이력·IDE 캐시·개인 설정을 건너뛴다.

```powershell
tar.exe -xf framework/local/ComputerGraphics.zip -C framework/local --exclude "ComputerGraphics/.git" --exclude "ComputerGraphics/.vs" --exclude "*.user"
```

3. 다음 배치를 확인한 뒤 저장소 루트의 `ChaosTankArena.slnx`를 연다.

```text
framework/local/ComputerGraphics/
├── ComputerGraphics.slnx         배포자의 실습 솔루션
└── ComputerGraphics/
    ├── include/                  프레임워크·vendor 헤더
    ├── src/                      프레임워크 구현 및 수업 예제
    ├── lib/x64/{Debug,Release}/   구성별 import/static 라이브러리
    ├── glew32.dll
    ├── assimp-vc145-mt.dll
    ├── assimp-vc145-mtd.dll
    ├── data/
    └── shaders/
```

팀 프로젝트는 위 폴더를 프로젝트 파일 기준 상대경로로 참조한다. 빌드 시 선택한 구성에 맞는 Assimp DLL과 GLEW DLL을 실행 파일 옆에 복사한다. 배포 예제에 나오는 `data/...`와 `shaders/...`는 배포자의 프로젝트 디렉터리를 기준으로 한 경로다. 팀 솔루션의 디버거에서는 저장소 루트 기준 경로를 사용한다.
