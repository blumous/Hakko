# Hakko

**Hakko** 는 데이터 허브를 중심에 둔 렌더링 프레임워크입니다.

애플리케이션 (서비스 어댑터) 은 허브에 리소스 / 데이터박스 / 전송체(RTB) 를 쌓기만 하고,
렌더러는 그 허브를 매 프레임 읽어 그립니다.
렌더러는 교체 가능한 백엔드이고 (렌더 어댑터, 현재 D3D12), 허브 API 는 백엔드와 무관합니다.

현재 목표는 이 위에 엔진 / 뷰어 같은 애플리케이션을 서비스 어댑터 계층으로 올리는 것입니다.
(스터디 프로젝트, v0.1 은 렌더링 최소 세트)

```
Service Adapter ──(Resource / DataBox / RTB)──▶ DataHub ──(read per frame)──▶ Renderer (D3D12)
```

### 방향성

현재 데이터허브에 붙는 첫 번째 레이어로 렌더러가 구현된 상태입니다.
허브는 레이어마다 데이터 규약 (리소스 종류, 데이터박스 타입, 전송체) 을 늘려 갈 수 있게 되어 있고, 레이어 사이의 요청도 전송체(\*TransferBox) 로 오갑니다.
물리 / 오디오 / 애니메이션 같은 레이어를 같은 방식으로 허브에 붙여,
각 레이어가 허브를 통해서만 서로를 보는 자율 멀티레이어 프레임워크가 목표입니다.
v0.1 은 그중 렌더링 레이어의 최소 세트이고, 다음 단계는 MMD 뷰어 (물리 / 오디오 / 애니메이션 레이어) 입니다.

현재 버전: **v0.1.0-preview** (공개 레포에는 마이너 버전 단위로 올라갑니다)

## 지원 범위 (v0.1)

| 지원 | 명시적 미지원 (요청하면 리포트 후 무시, 크래시 없음) |
|---|---|
| 메쉬 로드 (assimp, 서브메쉬 / 스킨 가중치), 텍스처 로드 (WIC, DDS 큐브맵) | 애니메이션 / 본 계층 (바인드 포즈까지만) |
| drawProp / texProp, 불투명 / 반투명 드로우, 스카이박스 / 배경판 | 2D 스크린 드로우 (타입만), 커스텀 파이프라인 (액션 노드) |
| 레이어 합성 (8), 전역 + 로컬 조명 8칸 (PBR-lite), 환경맵 / 조명 행렬 바인딩 | 그림자 / SSAO (노드 자리만), 컴퓨트 자원 / 액션 |
| 리사이즈 / 최소화, vsync, `RendererDesc` 한계 / 튜닝 값 | 스텐실, MSAA, 로디드 텍스처에 RT / DS 출력, `DrawProp` 의 `fillType` / `depthBias` / `DepthUsage` Equal·Always (기본값으로 동작) |

## 요구 사항

- Windows 10 이상, D3D12 + 셰이더 모델 6.0 이상 GPU
- Visual Studio 2022 또는 2026 (C++ 데스크톱 워크로드)
- CMake 3.24 이상 (VS 2026 프리셋은 CMake 4.2 이상)
- vcpkg (환경변수 `VCPKG_ROOT` 가 vcpkg 폴더를 가리키게, 프리셋 `$env{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake` 툴체인)

의존성은 `vcpkg.json` 이 전부이며 첫 구성 때 자동으로 설치됩니다:
directx-headers, directxtex, directx-dxc, assimp

> 비ASCII 경로 (한글 등) 는 기본 프리셋 (Visual Studio 제너레이터 + 정적 트리플릿) 에서는 문제없이 빌드됩니다. 
> Ninja 같은 다른 제너레이터로 바꿀 때는 응답 파일 인코딩 문제로 의존성 링크가 실패할 수 있으니 영문 경로를 사용하세요.

## 빌드

```bash
cmake --preset vs2022-x64
```

```bash
cmake --build --preset release
```

VS 2026 은 `vs2026-x64` / `release-2026` 프리셋을 쓰면 됩니다.
결과물은 `build/<프리셋>/Samples/Basic/<구성>/HakkoSample.exe` 이고,
디폴트 셰이더와 `dxil.dll`, 샘플 리소스는 빌드 후 단계가 실행 파일 옆에 복사합니다.
VS Code 는 CMake Tools 가 프리셋을 그대로 읽습니다.

## 디렉터리

```
Hakko/            라이브러리 (정적). 엔진에 넘기는 단위
  Data/           데이터 허브: 리소스 / 데이터박스 / 전송체(RTB)
  Render/         렌더러 인터페이스 + D3D12 구현 (private/D3D12/shader 에 디폴트 셰이더)
  Core/           디바이스 / 큐 (싱글턴)
  Service/        서비스 어댑터 (허브를 쓰는 쪽의 틀)
Samples/Basic/    창 호스트 + 샘플 서비스 + 리소스
```

각 모듈의 `public/` 만 include 경로에 잡히고 `private/` 는 라이브러리 내부용입니다.
`DataHub_Interface.h` 는 선택, 편의기능 헤더 (파일 로드 + 등록을 한 번에) 이며,
본체 API 는 `DataHub.h` 하나로 전부 보입니다.

## 문서

- [docs/GUIDE.md](docs/GUIDE.md): 퀵스타트 (허브 API), 예약 리소스, 렌더러 옵션, 엔진에 붙이기 (수명주기), 셰이더 계약
- [docs/BUILD.md](docs/BUILD.md): 새 PC 세팅, 엔진 (.vcxproj) 연동 메모
- [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md): 서드파티 라이브러리 / 샘플 에셋 라이선스

## 라이선스

MIT. 포함된 서드파티 라이브러리와 샘플 에셋의 라이선스는 `THIRD_PARTY_NOTICES.md` 에 작성되어 있습니다.
