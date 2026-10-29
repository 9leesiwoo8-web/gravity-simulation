# Gravity Simulation

2D N-body 중력 시뮬레이션. 태양계, 쌍성계, 은하 충돌, 소행성 벨트를 실시간으로 보여 주고,
천체끼리 충돌하면 병합하거나 파편으로 부서진다. 중력 계산은 입자가 많을 때 OpenCL로 GPU에서 한다.

## 특징

- 속도 Verlet 적분, Plummer softening 중력
- 충돌 시 질량·운동량·질량중심을 보존하며 병합 또는 파편화
- 입자 128개 이상이면 GPU(OpenCL)에서 여러 스텝을 연속으로 계산, 그보다 적거나 GPU를 쓸 수 없으면 CPU로 계산
- SFML 렌더링: 궤적, 충돌 이펙트, HUD

## 바로 실행하기 (빌드 없이)

[`release/moolli_release.zip`](release/moolli_release.zip)을 받아 압축을 풀고 `main.exe`를 실행한다.
64비트 Windows와 최신 그래픽 드라이버만 있으면 된다. 압축 안의 DLL을 빼면 실행되지 않는다.

## 필요한 것 (Windows, 직접 빌드할 때)

1. [MSYS2](https://www.msys2.org)를 `C:\msys64`에 설치한다.
2. MSYS2 **MINGW64** 터미널에서 패키지를 설치한다.

   ```
   pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-make mingw-w64-x86_64-sfml mingw-w64-x86_64-opencl-headers mingw-w64-x86_64-opencl-icd
   ```

   SFML은 **3.x**가 필요하다 (2.x에서는 컴파일되지 않는다).
3. 그래픽 드라이버를 최신으로 유지한다. OpenCL과 `double`을 지원하는 GPU가 있으면 GPU로 계산하고,
   없으면 자동으로 CPU로 계산한다.

## 빌드와 실행

MINGW64 터미널에서:

```
mingw32-make          # main.exe 빌드
mingw32-make run      # 빌드 후 실행
mingw32-make clean    # 빌드 결과 삭제
```

PowerShell에서 `main.exe`를 직접 실행하려면 `C:\msys64\mingw64\bin`이 PATH에 있어야 SFML DLL을 찾는다.
시작하면 터미널에 `[GpuGravity] GPU 사용: ...`이 찍히면 GPU로 계산하는 것이다.

## 조작

| 입력 | 동작 |
|---|---|
| F1 ~ F5 | 시나리오 선택 (태양계, 쌍성계, 은하 충돌, 소행성 벨트, 샌드박스) |
| 1 ~ 4 | 만들 천체 종류 (소행성, 행성, 거대 행성, 별) |
| 마우스 왼쪽 드래그 | 천체 발사 (드래그한 방향과 거리로 속도 결정) |
| 마우스 오른쪽 드래그 | 화면 이동 |
| 휠 | 확대·축소 |
| Space | 일시정지 |
| `+` / `-` | 시뮬레이션 속도 조절 |
| R | 현재 시나리오 다시 시작 |
| T | 궤적 켜기/끄기 |
| Esc | 발사 취소 |

## 구조

```
Main/       프로그램 시작점, 메인 루프
Config/     조절 값 (G, 시간 스텝, GPU 임계값, 창·카메라 등)
Math/       Vector2
Physics/    Particle, PhysicsWorld(충돌·CPU 계산), GpuGravity(OpenCL)
Renderer/   SFML 창, 입력, 그리기
Scenario/   시작 배치 5종
```

## 조절할 수 있는 값 (`Config/Config.h`)

- `GPU_MIN_PARTICLES`: 이 수 이상일 때만 GPU로 계산한다.
- `GPU_COLLISION_DELAY_STEPS`: GPU가 입자가 처음 겹친 뒤 이만큼 더 진행하고 충돌을 처리한다. 0이면 겹친 스텝에서 바로 처리한다.
- `DEFAULT_TIME_SCALE`: 프레임당 물리 스텝 수. 느리면 낮춘다.
- `Scenario/Scenario.cpp`의 `add_ring(...)`: 시나리오별 입자 수.
