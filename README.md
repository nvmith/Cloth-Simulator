# 🧵 Cloth Simulator (C++ & OpenGL)

**질점–스프링(Particle–Spring) 기반 천 시뮬레이터**  
Verlet 통합으로 실시간 거동을 구현하고, 메시/텍스처/내보내기까지 다룹니다.

---

## ✅ 목표
- C++로 **수치 시뮬레이션 + 실시간 렌더링** 구조 학습  
- OpenGL 3.3 Core로 **파이프라인 구성**  
- 물리 -> 메시 -> 텍스처 -> **OBJ/MTL 익스포트**까지 확장

---

## 🔧 개발 환경
- 언어: C++
- 렌더링: OpenGL 3.3 Core / GLFW / GLAD
- 수학: GLM
- UI: ImGui
- IDE: Visual Studio 2022

---

## 🗂️ 현재 구현된 기능
- Verlet 통합 기반 **질점–스프링** 시뮬레이션  
- 제약(구조/전단/굽힘) 해결 및 중력 워밍업 적용  
- **마우스 피킹(최근접 입자)** 및 드래그 이동 (prev 동기화로 진동 억제)
- **임펄스 바람(우클릭, applyRadialImpulse)** — 커서 지점 중심, prevPos 기반 속도 임펄스  
- **핀 토글(우클릭)**, 코너 히트 우선 로직, 핀 상태 색상(흰=free, 노랑=drag, 빨강=fixed)  
- **메시 렌더링 + 노멀 계산**  
- **텍스처 매핑**(GL_REPEAT), **타일 배율 실시간 제어**  
- **OBJ/MTL/PNG 내보내기** (텍스처 자동 복사, UV 타일 반영)  
- **ImGui 패턴 생성 UI**: Prompt / Negative Prompt **2칸 입력** -> Python 파이프라인 호출  
- **CLIP 77 토큰 트렁케이트** & 선택적 후처리(무이음/팔레트 양자화) 유지

---

## 🎮 조작법
- **W / A / S / D**: 카메라 이동  
- **마우스 좌클릭 드래그**: 집힌 입자 이동 (고정점 제외)  
- **마우스 우클릭**: 핀(고정) 토글 / 임펄스  
- **Q / E**: 천(모델) 좌/우 회전  
- **R**: 천 초기화 + 회전 각도 초기화  
- **1 ~ 9**: 텍스처 타일 배율 = **1×1 ~ 9×9**  
- **K / L**: 코너 히트 반경 조절  
- **O**: 현재 화면 상태 기준 **OBJ/MTL/텍스처** 내보내기  
- **ESC**: 종료  

> 참고: **과거 ‘입력모드(I/Ctrl+V/Enter)’는 제거**되었고, 패턴 생성은 ImGui 패널에서 수행합니다.

---

### 📷 실행 예시
<img width="642" height="383" alt="Image" src="https://github.com/user-attachments/assets/7e11309a-94ee-457f-8d6d-4d6e66d8b2f1" />

---

## 🖼️ 텍스처 & 타일링
- 텍스처는 `textures/` 폴더에 배치 (예: `textures/blue_camo_pattern.png`)  
- 셰이더의 `uTexScale`로 타일 반복, **1~9**로 실시간 변경  
- 내보내기 시 `uvScale`을 OBJ의 `vt`와 MTL의 `map_Kd -s`에 반영 ->  
  **외부 뷰어에서도 시뮬레이터와 동일한 타일링** 재현

---

## 🤖 패턴 생성 워크플로우 (ImGui)
- 실행하면 **ImGui 패널 “Pattern Prompt Builder”**에서  
  - **Prompt**: 원하는 프롬프트 직접 입력  
  - **Negative Prompt**: 배제할 요소 입력  
  - **Run Pattern Generation** 버튼 클릭 -> `gen_pattern.py` 호출, 결과는 `textures/generated.png`로 자동 저장 후 즉시 리로드
- **G**: 마지막 Prompt/Negative로 재생성  
- **Shift+G**: `textures/generated.png` **파일만 리로드**

> 실행 시 Python은 가상환경의 `venv\Scripts\python.exe`를 우선 사용하며, 없을 경우 시스템 `python`을 사용합니다.  
> 내부적으로 **`--style-off`** 를 전달해 자동 꼬리 스타일을 비활성화합니다.

---

## 🐍 gen_pattern.py (요약)
- 모델: 기본 `sd-legacy/stable-diffusion-v1-5` (교체 가능)  
- **CLIP 77 토큰 안전 트렁케이트** 적용 (Prompt/Negative 모두)  
- 옵션:
  - `--seamless-psd` : 고급 무이음(PSD) 보정  
  - `--seamless`     : 간이 무이음  
  - `--flat-post --flat-colors N [--flat-dither]` : **팔레트 양자화(알베도화)**  
  - `--rgb` : 알파 제거 후 RGB 저장  
  - `--steps / --guidance / --seed / --size` : 샘플링/해상도 제어  
- 출력: 기본 `textures/generated.png`  

---

## 📤 내보내기
- **O 키**로 `out/cloth.obj`, `out/cloth.mtl`, `out/<texture>.png` 생성  
- MTL에는 `map_Kd -s <uvScale> <uvScale> 1 <texture.png>` 기록  
- 텍스처 파일을 **out/**로 자동 복사해 뷰어 호환성 보장  
- 내보내기 시 **풀스크린 플래시 + 짧은 프리뷰 고정** 연출

---

## 🧪 앞으로 할 일
- **시뮬 속도 안정화**: dt clamp + 서브스텝으로 프레임 변동 시 과속 방지  
- **바닥(Plane) 충돌 + S 키 정착(Settle)**  
- **핀 편집 UX**: 박스 선택/다중 토글, 핀 리스트 HUD  
- **패턴 히스토리/퀵 슬롯(1–5)**, 미리보기 썸네일  
- (옵션) **OBJ 베이크 텍스처**: 타일 이미지를 큰 PNG로 합성 저장

---

## 📦 버전 관리 메모
- 일반적으로 **`venv/`는 .gitignore에 포함** (용량/호환성 이슈)  
- 대신 **`requirements.txt`** 또는 **`environment.yml`** 로 의존성 고정  
- 이 저장소는 `thirdparty/`에 외부 라이브러리 헤더/백엔드 포함

---

## 📝 변경 요약 (현재 빌드와 차이점)
- ✅ **우클릭 임펄스 바람 추가** (`applyRadialImpulse`) — 커서 지점 중심  
- ✅ **ImGui 패턴 생성 UI 도입** (Prompt/Negative 2칸)  
- ✅ **입력모드 제거** (I/Ctrl+V/Enter 방식 삭제)  
- ✅ **자동 스타일 꼬리(`--style`) 비활성화** 기본화  
- ✅ CLIP 77 토큰 트렁케이트 및 후처리 파이프라인 유지
