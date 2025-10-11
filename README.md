# 🧵 Cloth Simulator — C++ & OpenGL

**실시간 Cloth Simulation**을 직접 구현하며 **그래픽스 파이프라인과 물리 시뮬레이션의 결합**을 학습한 프로젝트입니다.  
Rigidbody 같은 엔진 제공 컴포넌트를 쓰는 대신, **질점–스프링 모델 + Verlet 통합**을 처음부터 구현하면서  
“내가 만든 물리 모델을 눈앞에서 렌더링까지 연결한다”는 목표로 시작했습니다.  

---

## ✅ 목표
- C++로 **수치 시뮬레이션 × 실시간 렌더링** 구조 학습  
- OpenGL 3.3 Core 기반 **파이프라인 설계**(버텍스/인덱스/노멀/UV)  
- **패턴 텍스처링 → 타일링 → Export** 전체 흐름 정리

---

## 🔧 개발 환경
- **Language**: C++  
- **Render**: OpenGL 3.3 Core, GLFW, GLAD  
- **Math**: GLM  
- **UI**: ImGui  
- **IDE**: Visual Studio 2022

---

## 🎯 개발 의도
- **C++ / OpenGL 기반 저수준 구현 경험** 확보 → VAO/VBO, 셰이더, 버퍼 업데이트 흐름까지 직접 다룸  
- 단순한 포인트 렌더링을 넘어, **메시/노멀 계산/텍스처 매핑/Export**까지 “완결된 파이프라인” 구성  
- AI 기반 패턴 제너레이터(ImGui + Stable Diffusion)를 붙여 **디자인–시뮬레이션–Export**로 이어지는 흐름

---

## 🗂️ 현재 기능
- **질점–스프링 시뮬레이션 + Verlet 통합**  
- **구조/전단/굽힘 제약** 및 **중력 워밍업**  
- **마우스 피킹(최근접 입자)**, 드래그 시 prev 동기화로 진동 억제  
- **우클릭 임펄스 바람(`applyRadialImpulse`)** / **핀 토글**  
- **메시 렌더링 + 노멀 계산**, **텍스처 타일링(GL_REPEAT)**  
- **OBJ/MTL/PNG Export**(타일 스케일이 MTL의 `map_Kd -s`와 UV에 반영)  
- **ImGui 패턴 생성 UI**: Prompt / Negative 2칸 → `gen_pattern.py` 호출, `textures/generated.png` 자동 리로드

---

## 🎮 조작법
- **W / A / S / D**: 카메라 이동  
- **마우스 좌클릭 드래그**: 집힌 입자 이동(고정점 제외)  
- **마우스 우클릭**: 핀(고정) 토글 / 임펄스 바람  
- **Q / E**: 천(모델) 좌/우 회전  
- **R**: 천 및 회전 각도 초기화  
- **1 ~ 9**: 텍스처 타일 배율(1×1 ~ 9×9)  
- **K / L**: 코너 히트 반경 조절  
- **O**: **OBJ/MTL/텍스처 Export**  
- **ESC**: 종료

---

## 📷 실행 화면

| 패턴 반복 횟수 변경 기능 | 임펄스 기능 |
|:--:|:--:|
| ![Image](https://github.com/user-attachments/assets/6fbfcb58-7918-4726-a340-0fac24ff8f2d) | ![Image](https://github.com/user-attachments/assets/7c43d6cf-8c39-48b6-9f57-d325d11bb11f) |

| 고정핀 기능 | OBJ/MTL 출력 기능 |
|:--:|:--:|
| ![Image](https://github.com/user-attachments/assets/9bc45af1-66f8-4239-b0c5-940acbad0db3) | ![Image](https://github.com/user-attachments/assets/0a66d1f6-6fbb-4561-986e-87d7a257d49d) |

---

## 🖼️ 텍스처 & 패턴 워크플로우
- 텍스처는 `textures/`에 배치 (`textures/generated.png`)  
- 셰이더 `uTexScale`로 타일 반복(1~9)  
- **ImGui - Pattern Prompt Builder**
  - **Prompt / Negative** 입력 → **Run Pattern Generation**
  - 파이썬 스크립트 `gen_pattern.py` 실행 → 결과는 `textures/generated.png`로 저장 → 즉시 리로드  
  - **G**: 마지막 프롬프트 재생성 / **Shift+G**: 파일만 리로드
- Python은 우선 `venv\Scripts\python.exe`를 사용(없으면 시스템 Python)

---

## 📤 내보내기(Export)
- **O 키** → `out/cloth.obj`, `out/cloth.mtl`, `out/<texture>.png` 생성  
- **타일 스케일**은 OBJ `vt` 및 MTL `map_Kd -s`에 반영 → 외부 툴에서도 동일 타일링 재현  
- 내보내기 시 **풀스크린 플래시 + 짧은 프리뷰 고정** 연출

---

## 🚧 Roadmap
- **시뮬 속도 안정화**: dt clamp + 서브스텝  
- **바닥(Plane) 충돌 + S키 정착(Settle)**  
- **핀 편집 UX**: 박스 선택/다중 토글, 핀 리스트 HUD  
- **패턴 히스토리/퀵슬롯(1–5)**, 썸네일 미리보기  
- (옵션) **OBJ 베이크 텍스처**: 타일 이미지를 큰 PNG로 합성 저장
