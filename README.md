# 세 점을 지나는 원 — MFC Dialog 과제

## 프로젝트 소개

별도의 Drawing Area에 점 세 개를 입력하면 세 점을 지나는 외접원(circumcircle)을 직접 계산하고 그리는 Visual Studio C++ / MFC Dialog 프로그램입니다. 기존 점을 드래그하는 동안 좌표와 외접원이 즉시 갱신됩니다. 랜덤 이동은 별도 작업 스레드가 0.5초 간격으로 정확히 10회 수행합니다.

작은 점과 외접원의 픽셀 좌표를 원 방정식으로 직접 계산합니다. 외접원의 중심과 일부 테두리가 Drawing Area 밖에 있어도 허용하며, 보이는 픽셀만 그립니다. 외접원 전체가 화면 안에 들어오도록 반지름이나 위치를 제한하지 않습니다.

## 실행 환경

- Windows x64
- Visual Studio 2026 / MSVC v145 또는 Visual Studio 2022 / MSVC v143
- C++17, Unicode, MFC Dialog Based Application
- Visual Studio Installer의 **C++를 사용한 데스크톱 개발**, 해당 도구 집합의 **C++ MFC(x86 및 x64)**, Windows SDK
- MFC와 C/C++ 런타임은 정적으로 링크합니다.
- UI 자동 검증에는 Python 3.10 이상이 필요합니다. 프로그램 실행 자체에는 Python이 필요하지 않습니다.

Visual Studio 2026에서는 v145, 그 외에는 v143을 선택하도록 프로젝트를 구성했습니다. Visual Studio 2022의 호환 설정은 제공하지만, 해당 버전에서 직접 빌드한 결과로 간주하지 않습니다.

## 실행 방법

1. 필요한 C++/MFC 구성 요소를 설치합니다.
2. 현재 폴더의 `Circumcircle.sln`을 Visual Studio에서 엽니다.
3. 솔루션 구성을 `Debug`, 플랫폼을 `x64`로 선택합니다.
4. **빌드 → 솔루션 빌드**를 실행한 뒤 `F5` 또는 `Ctrl+F5`로 실행합니다.
5. 배포용 빌드는 구성을 `Release`로 변경하고 다시 빌드합니다.

실행 파일은 다음 경로에 생성됩니다.

```text
bin/x64/Debug/Circumcircle.exe
bin/x64/Release/Circumcircle.exe
```

Visual Studio Developer PowerShell에서 프로젝트 루트를 기준으로 직접 빌드할 수도 있습니다.

```powershell
msbuild .\Circumcircle.sln /m /p:Configuration=Debug /p:Platform=x64
msbuild .\Circumcircle.sln /m /p:Configuration=Release /p:Platform=x64
```

일반 PowerShell에서는 Visual Studio 경로를 자동으로 찾는 스크립트를 사용합니다.

```powershell
.\build.ps1 -Configuration All -Test -UiTest
```

`-Test`는 두 구성의 수학·픽셀 검증을, `-UiTest`는 Python UI 검증을 실행합니다. 빌드만 필요하면 두 옵션을 생략합니다. `.vsconfig`에는 필요한 C++/MFC 구성 요소를 기록했습니다.

`MSB8041`이 발생하면 선택한 MSVC 도구 집합에 맞는 MFC 구성 요소가 설치되었는지 Visual Studio Installer에서 확인합니다. C++ 컴파일러만 설치되어 있는 경우 MFC 프로그램을 빌드할 수 없습니다.

## 주요 기능과 사용법

| 기능 | 동작 |
| --- | --- |
| 3 Point 입력 | Drawing Area의 빈 곳을 클릭하여 첫째·둘째·셋째 점을 만듭니다. 셋째 점이 입력되는 즉시 외접원을 표시합니다. |
| 최대 점 개수 | 점은 최대 3개입니다. 이후 빈 곳을 클릭해도 추가되지 않습니다. |
| 기존 점 선택 | 점의 원 내부를 누르면 새 점 생성 대신 해당 점의 드래그가 시작됩니다. 겹친 점은 마우스에 가장 가까운 점을 선택합니다. |
| Drag | 마우스를 이동할 때마다 해당 점, 좌표 표시, 외접원을 갱신합니다. 버튼을 놓거나 마우스 캡처를 잃으면 종료합니다. |
| 초기화 | 점 3개, 외접원, 좌표, 드래그 상태, 랜덤 진행 횟수를 모두 초기화합니다. 반지름과 두께 설정은 유지합니다. |
| Random Movement | 유효한 외접원이 있을 때 실행할 수 있습니다. 첫 이동은 약 0.5초 후, 마지막인 10회째는 약 5초 후입니다. |
| Worker Thread | 좌표 생성과 대기는 작업 스레드가 담당하고, 화면과 컨트롤 변경은 UI 스레드가 담당합니다. |
| 진행 표시 | 랜덤 이동 진행 상태를 `0 / 10`부터 `10 / 10`까지 표시합니다. 실행 중에는 랜덤 버튼을 비활성화합니다. |
| 잘못된 세 점 | 같은 점 또는 거의 일직선인 점은 그대로 유지하면서 외접원만 숨기고 상태를 안내합니다. 점을 드래그하면 다시 계산합니다. |

좌표는 Drawing Area 왼쪽 위가 `(0, 0)`인 **로컬 픽셀 좌표**입니다. 입력창이나 버튼을 클릭해도 점이 생기지 않습니다. 드래그 시 점 중심은 Drawing Area 안으로 제한하지만, 점 원의 가장자리와 외접원은 영역 경계에서 잘릴 수 있습니다.

입력값은 다음 범위를 허용합니다. 유효한 숫자는 입력 즉시 적용합니다.

| 입력 | 기본값 | 허용 범위 |
| --- | --- | --- |
| 클릭 지점 원 반지름 | 6 px | 정수 1~64 |
| 정원 가장자리 두께 | 2 px | 정수 1~32 |

빈 값, 0, 음수, 문자, 소수, 범위를 넘는 숫자는 적용하지 않고 마지막 유효값을 유지합니다. 편집 중에는 안내 문구를 표시하며, 포커스를 잃거나 Enter를 누르면 입력창도 유효값으로 복원합니다.

랜덤 이동 중 초기화, 점 드래그 시작, 유효한 점 반지름 변경, 창 닫기는 실행 중인 작업을 취소합니다. 테두리 두께는 랜덤 이동 중에도 변경할 수 있습니다.

## 원 그리기 알고리즘

원 방정식으로 각 픽셀의 포함 여부를 판정하고, 8비트 회색조 바이트 배열인 `m_image`에 `fm[j * nPitch + i]` 방식으로 기록합니다. 작은 원은 경계 픽셀을 포함하도록 `<= r²` 조건을 사용합니다.

### 클릭 지점의 작은 원

`drawCircle()`에서 원의 경계 사각형과 Drawing Area의 교집합을 순회합니다. `isInCircle()`에서 각 픽셀 중심 `(x, y)`가 다음 조건을 만족하면 검은색을 기록합니다.

```text
dx = x - centerX
dy = y - centerY
dx² + dy² <= radius²
```

정수 좌표의 뺄셈과 제곱은 64비트 정수로 계산하여 오버플로를 방지합니다. 클리핑을 반복문 전에 수행하므로 배열 밖에 기록하지 않습니다.

### 세 점을 지나는 외접원의 테두리

`drawCircleOutline()`는 각 화면 픽셀 중심과 계산한 외접원 중심 사이의 거리 `d`를 구하고, 안쪽 반지름과 바깥쪽 반지름 사이인 픽셀만 기록합니다. 일반적인 경우 다음과 같은 두께 `t`의 띠 영역입니다.

```text
radius - t/2 <= d <= radius + t/2
```

아주 작은 원에 큰 두께를 적용할 때는 내부가 전부 채워지지 않도록 안쪽 반지름을 최소 `min(radius, 1 px)`로 유지하고, 남는 두께를 바깥쪽으로 확장합니다. 수학적으로 계산한 외접원 자체의 중심과 반지름은 변경하지 않습니다. 픽셀 중심을 기준으로 판정하므로 테두리의 화면 표현은 정수 픽셀 격자에 따릅니다.

반복 범위는 항상 Drawing Area의 크기로 제한됩니다. 거대한 원의 전체 둘레나 경계 사각형을 순회하지 않으므로, 화면 밖 중심이나 매우 큰 반지름 때문에 반복 횟수가 증가하지 않습니다. 중심이 먼 원은 거리 차를 유리화한 식으로 계산하여 큰 실수끼리의 뺄셈으로 생기는 정밀도 손실을 줄입니다.

### OnPaint와 화면 갱신

`CDrawingCanvas::OnPaint()`는 `InitImage()`로 흰색 배경을 초기화하고 Point 1~3 → 외접원 순서로 8비트 `m_image` 배열에 완성된 프레임을 구성합니다. `nPitch`는 행 하나의 바이트 길이이며, 4바이트 정렬 패딩을 포함합니다. 메모리는 `nPitch * nHeight`만큼 확보합니다. `nBpp = 8`과 256단계 회색조 팔레트를 사용하며, `SetDIBitsToDevice()`는 완성된 비트맵을 화면에 한 번 복사합니다. 이 API는 원 좌표나 모양을 생성하는 데 사용하지 않습니다.

배경 지우기와 직접 화면에 부분 그림을 그리는 처리를 분리하여 깜빡임을 줄입니다. 점 드래그 시 `WM_MOUSEMOVE`에서 재계산한 후 `RedrawWindow(..., RDW_UPDATENOW)`로 바로 다시 그립니다. 창이 가려졌다가 드러나더라도 저장한 상태에서 전체 프레임을 복원합니다.

## 세 점을 지나는 원 계산

`calculateCircumcircle()`은 첫째 점을 원점으로 평행 이동하고, 좌표 차의 최댓값으로 정규화합니다. 정규화한 둘째·셋째 점을 각각 `a=(ax, ay)`, `b=(bx, by)`라고 할 때 다음 식을 직접 계산합니다.

```text
A = ax² + ay²
B = bx² + by²
cross = ax*by - ay*bx
D = 2*cross

ux = (A*by - B*ay) / D
uy = (ax*B - bx*A) / D
```

`(ux, uy)`에 정규화 배율을 다시 곱한 뒤 첫째 점 좌표를 더하면 원래 좌표계의 외접원 중심입니다. 반지름은 평행 이동한 중심과 원점 사이의 거리로 구합니다. 모든 중심·반지름 계산은 `double`입니다.

거의 일직선인 입력은 `abs(cross) <= 1e-9 * max(A, B)`로 판정하며, 같은 점·0 반지름·유한하지 않은 계산 결과도 실패로 처리합니다. 실패 시 외접원을 그리지 않고 점과 UI는 유지합니다. 이 검사는 원이 화면 안에 들어오는지를 판단하는 제한이 아닙니다.

## Thread 구조

```text
UI: 랜덤 이동 시작, 좌표 생성 범위와 실행 세대 번호 확정
  ↓
std::thread → threadProcess: 세 점 랜덤 생성, 유효한 외접원 여부 검사
  ↓
condition_variable::wait_until: 0.5초 간격 대기 / 취소 시 즉시 기상
  ↓
mutex로 보호한 실행별 프레임 배열에 좌표 기록
  ↓
PostMessage(WM_APP + 1, 실행 세대 번호, 프레임 번호)
  ↓
UI 메시지 처리: 유효한 실행·순서인지 확인 후 좌표 복사
  ↓
점 갱신 → 외접원 재계산 → 좌표·진행 UI 갱신 → 다시 그리기
```

`steady_clock` 기준 시작 시각에 `500 * (i+1)` ms를 더한 절대 시각까지 대기합니다. 따라서 반복 처리 시간이 각 단계의 대기 시간에 계속 누적되지 않습니다. 실제 화면에 나타나는 시각에는 운영체제 스케줄링과 UI 메시지 처리에 따른 작은 오차가 있을 수 있습니다.

랜덤 좌표는 `m_nRadius`부터 `nWidth-1-m_nRadius`, `nHeight-1-m_nRadius` 사이에서 생성하여 작은 점 원이 화면 내부에 유지되도록 합니다. 이 범위를 `CRect`로 `threadProcess()`에 전달합니다. 세 점이 외접원을 만들 수 없으면 최대 128회 다시 생성하고, 모두 실패하면 동일 범위 안의 유효한 직각삼각형을 사용합니다. 외접원 전체의 화면 내 포함 여부로 좌표를 제한하지 않습니다.

작업 스레드는 MFC 객체, 컨트롤, CDC를 조작하지 않습니다. UI 전달 메시지에 소유권을 가진 힙 포인터를 넣지 않고, 실행별 공유 데이터의 고정된 프레임 배열을 `mutex`로 보호합니다. 취소 시 실행 세대 번호를 변경하여 이미 큐에 들어간 이전 메시지를 무시합니다. 취소 플래그 설정과 `notify_all()`로 대기를 깨운 다음 스레드를 `join()`하고 공유 상태를 해제합니다. 따라서 초기화나 창 닫기가 500ms 대기 종료를 기다릴 필요가 없습니다.

## 주요 파일

| 파일 | 역할 |
| --- | --- |
| `Circumcircle.sln` | Visual Studio 솔루션 |
| `Circumcircle/Circumcircle.vcxproj` | x64 Debug/Release, C++17, 정적 MFC 설정 |
| `Circumcircle/CircumcircleApp.cpp` | MFC 애플리케이션 및 모달 Dialog 진입점 |
| `Circumcircle/CircumcircleDlg.h/.cpp` | 입력 검증, 좌표·상태 UI, 초기화, 작업 스레드와 메시지 처리 |
| `Circumcircle/DrawingCanvas.h/.cpp` | 별도 CStatic 그리기 영역, 마우스 입력·드래그, OnPaint |
| `Circumcircle/CircleGeometry.h/.cpp` | 외접원 수학 계산, 작은 원과 테두리 픽셀 rasterization |
| `Circumcircle/Circumcircle.rc`, `resource.h` | Dialog 및 컨트롤 리소스 |
| `Circumcircle/app.manifest` | Windows 애플리케이션 매니페스트 |
| `build.ps1`, `.vsconfig` | 빌드·검증 스크립트 및 Visual Studio 구성 요소 목록 |
| `tests/GeometryTests.cpp`, `GeometryTests.vcxproj` | 외접원 계산·픽셀 rasterization·경계 조건 검증 |
| `tests/UiSmokeTest.py` | 실제 MFC 실행 파일의 Win32 메시지 기반 UI 통합 검증 |
| `tests/CapturePreview.py` | 실제 창 화면을 PNG로 캡처하는 검증 도구 |

## 금지 API

프로그램의 원과 점을 그리는 코드에서 `Ellipse`, `CDC::Ellipse`, GDI+, `gdiplus`, `Polygon`, `DrawPolygon`, `FillPolygon`, `PolyPolygon` 및 원을 자동 생성하는 유사 API를 사용하지 않습니다. OpenCV 등의 외부 이미지 라이브러리나 circle fitting 라이브러리도 사용하지 않습니다. 테두리 두께는 Pen Width에 의존하지 않고 픽셀 판정 조건으로 직접 처리합니다.

프로젝트 루트에서 아래 명령으로 구현 소스의 금지 API 이름을 검색할 수 있습니다. README는 금지 API를 설명하기 위해 이름을 포함하므로 검색 범위를 소스 파일로 지정합니다. 검색 결과가 없으면 `rg`의 종료 코드는 1입니다.

```powershell
rg -n -i "Ellipse|GDI\+|gdiplus|FillPolygon|DrawPolygon|PolyPolygon|\bPolygon\b|opencv" Circumcircle -g "*.cpp" -g "*.h" -g "*.rc" -g "*.vcxproj"
```

## 검증 방법

### 수학 및 픽셀 검증

PowerShell에서 프로젝트 루트를 기준으로 실행합니다.

```powershell
.\build.ps1 -Configuration All -Test
```

이 검증은 MFC에 의존하지 않는 실제 `CircleGeometry.cpp`를 사용합니다. 알려진 외접원, 모든 점을 통과하는지 여부, 일직선과 거의 일직선, 큰 좌표, 점 원의 내부·경계, 비정수 중심과 두께, 원 밖 배열 접근 방지, 큰 원과 화면 밖 중심, 작은 원의 내부 보존, 잘못된 버퍼·반지름을 확인합니다.

### 실제 MFC UI 통합 검증

실행 파일을 빌드한 후 실행합니다. Python 표준 라이브러리만 사용합니다.

```powershell
python .\tests\UiSmokeTest.py --configuration Debug --offscreen
python .\tests\UiSmokeTest.py --configuration Release --offscreen
```

테스트는 실제 프로그램을 별도 프로세스로 숨겨 실행하고, `--offscreen` 옵션으로 창을 화면 밖에 이동한 뒤 활성화하지 않고 표시합니다. 따라서 사용자 화면을 가리지 않으면서 실제 `WM_PAINT` 경로도 실행합니다. Win32 메시지로 클릭·드래그·버튼·입력 변경을 전달하고 좌표와 외접원 수치, 10회의 이동 시각, UI 메시지 응답, 실행 중 초기화와 종료를 검증합니다. 결과는 `artifacts/ui-smoke-debug.json`, `artifacts/ui-smoke-release.json`에 기록합니다.

`tests/CapturePreview.py`는 같은 방식으로 실행한 실제 창의 화면을 PNG로 캡처합니다. 화면 레이아웃과 원 표시는 캡처 결과로 검토하며, 정지 이미지로 드래그 깜빡임을 측정했다고 간주하지 않습니다.

### 최종 확인 항목

- [x] x64 Debug 및 Release의 MFC 빌드 성공
- [x] 첫째·둘째·셋째 클릭과 즉시 외접원 표시, 넷째 빈 공간 클릭 무시
- [x] 편집창·버튼 클릭의 Drawing Area 입력 분리
- [x] 반지름·두께 입력 검증과 3개 로컬 좌표 표시
- [x] 드래그 중 매 마우스 이동에 좌표·외접원 갱신 및 캡처 종료
- [x] 초기화 후 새 첫째 점 입력 가능
- [x] 0.5초 간격으로 정확히 10회 랜덤 이동 및 UI 응답 유지
- [x] 랜덤 이동 중 초기화·재시작·창 닫기의 안전한 스레드 종료
- [x] 겹치거나 거의 일직선인 점의 안전한 처리
- [x] 화면 밖 원 중심·큰 반지름·클리핑 및 작은 원 내부 보존
- [x] 금지 API 소스 검색 결과 0건
- [x] 실제 실행 화면 캡처의 레이아웃·원 표시·영역 밖 원 클리핑 확인
- [x] 메모리 프레임 완성 후 한 번 복사하는 그리기 구조 및 드래그 중 즉시 재그리기 확인

### 빌드 검증 기록

Windows x64 / Visual Studio 2026 / MSVC v145에서 Debug와 Release를 직접 빌드하고 검증했습니다. 초기 `MSB8041`은 공식 Visual Studio MFC 구성 요소 설치 후 해결되었습니다. 최종 빌드는 두 구성 모두 경고 0개, 오류 0개입니다.

| 검증 | Debug x64 | Release x64 | 기록 |
| --- | --- | --- | --- |
| MFC 애플리케이션 빌드 | 성공, 경고 0 / 오류 0 | 성공, 경고 0 / 오류 0 | [Debug 로그](artifacts/build-Debug.log), [Release 로그](artifacts/build-Release.log) |
| MSVC 수학·픽셀 검증 | 1,897,831개 검사 통과 | 1,897,831개 검사 통과 | [Debug 결과](artifacts/geometry-Debug.txt), [Release 결과](artifacts/geometry-Release.txt) |
| 실제 MFC UI 통합 검증 (`--offscreen`) | 8 / 8 통과 | 8 / 8 통과 | [Debug 결과](artifacts/ui-smoke-debug.json), [Release 결과](artifacts/ui-smoke-release.json) |
| 랜덤 이동 횟수·총 시간 | 10회 / 5.0214초 | 10회 / 5.0128초 | 위 UI 검증 결과의 `random_ten_steps` |
| 랜덤 실행 중 최대 UI 메시지 응답 시간 | 3.230 ms | 4.693 ms | 각 구성에서 `WM_NULL` 응답 243회 측정 |
| 금지 API 소스 검색 | 일치 0건 | 동일 소스 | [검색 결과](artifacts/forbidden-api-scan.txt) |
| 실제 창 화면 검토 | 레이아웃·세 점·원 표시 확인 | 동일 UI 구현 | [Debug 화면](artifacts/preview-debug-printwindow.png), [캡처 기록](artifacts/preview-debug.json) |

실제 화면 그리기를 수행한 `--offscreen` 검증에서 첫 이동은 Debug 0.5261초, Release 0.5206초 후에 관측했습니다. 첫 대기를 포함한 이동 간격은 두 구성 합계 0.4935~0.5261초였으며, 10회 완료 후 0.7초 동안 추가 이동이 없음을 확인했습니다. UI 응답 수치는 해당 실행 환경에서 테스트가 관측한 값입니다. 자동 검사에는 드래그 중 좌표·외접원 갱신, 입력값 검증, 초기화 중 오래된 메시지 무시, 재시작 및 작업 중 정상 종료가 포함됩니다.

실제 창의 `PrintWindow` 캡처에서 컨트롤과 글자가 겹치지 않고 세 점과 외접원이 표시되며, 화면 아래로 나가는 원이 자연스럽게 잘리는 것을 확인했습니다. 더블 버퍼링과 드래그 중 즉시 재그리기를 검증했지만, 정지 캡처를 이용한 깜빡임 측정은 수행하지 않았습니다.

Visual Studio 2022 / v143은 호환 구성을 제공하지만 해당 환경에서 직접 빌드·실행하지 않았습니다.
