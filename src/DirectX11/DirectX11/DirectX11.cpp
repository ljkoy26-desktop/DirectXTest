#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wincodec.h>
#include <vector>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#define PLAYER_SPEED  240.0f  // 초당 이동 픽셀 (프레임 속도에 무관하게 일정한 속도로 이동)
#define FULLSCREEN_MODE 0  // 1 = 풀스크린, 0 = 창 모드
#define DIAGONAL_RELEASE_SEC 0.1f  // 대각선 키를 뗄 때 두 키가 떨어지는 시간 차를 허용하는 시간

// 로드카오스 스프라이트 (creature_17611.png 부터 순서대로)
// 방향 순서: 0=서, 1=남서, 2=남, 3=남동, 4=동, 5=북동, 6=북, 7=북서
// (예: 대기 동작 서=17611~17616, 남서=17617~17622, 남=17623~17628 ... 북서=17653~17658)
#define SPRITE_DIR_NAME      L"로드카오스"
#define SPRITE_FIRST_NUMBER  17611
#define SPRITE_DIR_COUNT     8
#define SPRITE_IDLE_FRAMES   6                                                     // 대기 동작 방향당 프레임 수
#define SPRITE_WALK_FRAMES   10                                                    // 걷기 동작 방향당 프레임 수
#define SPRITE_ATTACK_FRAMES 8                                                     // 공격 동작 방향당 프레임 수
#define SPRITE_IDLE_START    0                                                     // 대기 동작 0 ~ 47
#define SPRITE_WALK_START    (SPRITE_DIR_COUNT * SPRITE_IDLE_FRAMES)               // 걷기 동작 48 ~ 127
#define SPRITE_ATTACK_START  (SPRITE_WALK_START + SPRITE_DIR_COUNT * SPRITE_WALK_FRAMES)    // 공격 동작 128 ~ 191
#define SPRITE_FRAME_COUNT   (SPRITE_ATTACK_START + SPRITE_DIR_COUNT * SPRITE_ATTACK_FRAMES)
#define SPRITE_ANIM_FPS      12.0f
#define SPRITE_ATTACK_FPS    16.0f  // 공격 1회 = 8프레임 / 16 = 0.5초
#define SPRITE_HALF_WIDTH    40   // 화면 밖으로 나가지 않게 하는 여유 폭
#define SPRITE_HEIGHT        113  // 화면 밖으로 나가지 않게 하는 여유 높이

struct SpriteFrame
{
    ID3D11ShaderResourceView* lpSRV;
    int                       nWidth;
    int                       nHeight;
};

struct SpriteVertex
{
    float fX, fY;  // 화면 좌표를 -1 ~ 1 로 바꾼 위치
    float fU, fV;  // 텍스처 좌표
};

// 스프라이트용 셰이더: 텍스처를 그대로 찍고 알파 블렌딩으로 투명 처리
static const char g_szSpriteShader[] =
    "Texture2D    g_Texture : register(t0);\n"
    "SamplerState g_Sampler : register(s0);\n"
    "struct VSInput  { float2 pos : POSITION;    float2 uv : TEXCOORD0; };\n"
    "struct PSInput  { float4 pos : SV_POSITION; float2 uv : TEXCOORD0; };\n"
    "PSInput VSMain(VSInput input)\n"
    "{\n"
    "    PSInput output;\n"
    "    output.pos = float4(input.pos, 0.0f, 1.0f);\n"
    "    output.uv  = input.uv;\n"
    "    return output;\n"
    "}\n"
    "float4 PSMain(PSInput input) : SV_TARGET\n"
    "{\n"
    "    return g_Texture.Sample(g_Sampler, input.uv);\n"
    "}\n";

IDXGISwapChain*          g_lpSwapChain        = NULL;
ID3D11Device*            g_lpD3DDevice        = NULL;
ID3D11DeviceContext*     g_lpD3DContext       = NULL;
ID3D11RenderTargetView*  g_lpRenderTargetView = NULL;
ID3D11VertexShader*      g_lpVertexShader     = NULL;
ID3D11PixelShader*       g_lpPixelShader      = NULL;
ID3D11InputLayout*       g_lpInputLayout      = NULL;
ID3D11Buffer*            g_lpVertexBuffer     = NULL;
ID3D11SamplerState*      g_lpSamplerState     = NULL;
ID3D11BlendState*        g_lpBlendState       = NULL;
ID3D11RasterizerState*   g_lpRasterizerState  = NULL;
LPDIRECTINPUT8           g_lpDI8              = NULL;
LPDIRECTINPUTDEVICE8     g_lpDIDKeyboard      = NULL;
HWND                     g_hWnd               = NULL;
BOOL                     g_bActive            = FALSE;
SpriteFrame              g_aSpriteFrames[SPRITE_FRAME_COUNT];
float                    g_fPlayerX           = 0.0f;  // 캐릭터 발 위치 (가로 중앙)
float                    g_fPlayerY           = 0.0f;  // 캐릭터 발 위치 (아래쪽)
int                      g_nPlayerDir         = 2;     // 처음에는 남쪽(정면)을 바라봄
bool                     g_bPlayerWalk        = false;
bool                     g_bPlayerAttack      = false;
float                    g_fAnimTime          = 0.0f;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirect3D(HWND hWnd);
bool InitSpritePipeline();
bool InitDirectInput(HWND hWnd);
bool LoadSprites();
bool CreateRenderTarget();
void ReleaseRenderTarget();
void ResizeSwapChain(UINT uWidth, UINT uHeight);
void UpdateInput(float fDeltaSec);
void DrawSprite(int nFrameIndex, int nFootX, int nFootY);
void RenderFrame();
void ReleaseSprites();
void ReleaseDirect3D();
void ReleaseDirectInput();

int main()
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    HRESULT hrCom = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (!InitWindow(hInstance, SW_SHOWDEFAULT))
    {
        return 1;
    }

    if (!InitDirect3D(g_hWnd) || !InitSpritePipeline())
    {
        ReleaseDirect3D();
        return 1;
    }

    if (!LoadSprites())
    {
        MessageBox(g_hWnd, TEXT("로드카오스 스프라이트를 불러오지 못했습니다."), TEXT("DirectX11 Sample"), MB_OK | MB_ICONERROR);
        ReleaseSprites();
        ReleaseDirect3D();
        return 1;
    }

    if (!InitDirectInput(g_hWnd))
    {
        ReleaseDirectInput();
        ReleaseSprites();
        ReleaseDirect3D();
        return 1;
    }

    MSG msg = { 0 };

    LARGE_INTEGER liFreq = { 0 };
    QueryPerformanceFrequency(&liFreq);

    LARGE_INTEGER liLastTick = { 0 };
    QueryPerformanceCounter(&liLastTick);

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
            continue;
        }

        if (!g_bActive)
        {
            WaitMessage();
            QueryPerformanceCounter(&liLastTick);  // 비활성 동안 흐른 시간은 무시
            continue;
        }

        // Present(1)이 수직동기화를 기다리므로 프레임 제한은 따로 하지 않고 경과 시간만 계산
        LARGE_INTEGER liNowTick = { 0 };
        QueryPerformanceCounter(&liNowTick);

        float fDeltaSec = static_cast<float>(liNowTick.QuadPart - liLastTick.QuadPart) / static_cast<float>(liFreq.QuadPart);
        liLastTick = liNowTick;

        if (fDeltaSec > 0.1f)
        {
            fDeltaSec = 0.1f;  // 창을 끌거나 할 때 멈췄던 시간만큼 순간이동하지 않도록
        }

        UpdateInput(fDeltaSec);
        RenderFrame();
    }

    ReleaseDirectInput();
    ReleaseSprites();
    ReleaseDirect3D();

    if (SUCCEEDED(hrCom))
    {
        CoUninitialize();
    }

    return static_cast<int>(msg.wParam);
}

bool InitWindow(HINSTANCE hInstance, int nCmdShow)
{
    WNDCLASSEX wc = { 0 };
    wc.cbSize        = sizeof(WNDCLASSEX);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInstance;
    wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = TEXT("DirectX11SampleWndClass");

    if (!RegisterClassEx(&wc))
    {
        return false;
    }

#if FULLSCREEN_MODE
    DWORD dwStyle = WS_POPUP;
    RECT rcWindow = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
#else
    DWORD dwStyle = WS_OVERLAPPEDWINDOW;
    RECT rcWindow = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
    AdjustWindowRect(&rcWindow, dwStyle, FALSE);
#endif

    g_hWnd = CreateWindowEx(
        0,
        wc.lpszClassName,
        TEXT("DirectX11 Sample"),
        dwStyle,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top,
        NULL, NULL, hInstance, NULL);

    if (g_hWnd == NULL)
    {
        return false;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    g_bActive = TRUE;
    return true;
}

bool InitDirect3D(HWND hWnd)
{
    DXGI_SWAP_CHAIN_DESC scd = { 0 };
    scd.BufferCount                        = 1;
    scd.BufferDesc.Width                   = SCREEN_WIDTH;
    scd.BufferDesc.Height                  = SCREEN_HEIGHT;
    scd.BufferDesc.Format                  = DXGI_FORMAT_R8G8B8A8_UNORM;
    scd.BufferDesc.RefreshRate.Numerator   = 60;
    scd.BufferDesc.RefreshRate.Denominator = 1;
    scd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    scd.OutputWindow                       = hWnd;
    scd.SampleDesc.Count                   = 1;
    scd.SampleDesc.Quality                 = 0;
    scd.Windowed                           = !FULLSCREEN_MODE;
    scd.SwapEffect                         = DXGI_SWAP_EFFECT_DISCARD;

    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;

    HRESULT hr = D3D11CreateDeviceAndSwapChain(
        NULL,
        D3D_DRIVER_TYPE_HARDWARE,
        NULL,
        0,
        &featureLevel,
        1,
        D3D11_SDK_VERSION,
        &scd,
        &g_lpSwapChain,
        &g_lpD3DDevice,
        NULL,
        &g_lpD3DContext);

    if (FAILED(hr))
    {
        return false;
    }

    if (!CreateRenderTarget())
    {
        return false;
    }

    return true;
}

// 스프라이트를 그리는 데 필요한 셰이더, 정점 버퍼, 샘플러, 알파 블렌딩 상태를 만든다
bool InitSpritePipeline()
{
    ID3DBlob* lpVSBlob    = NULL;
    ID3DBlob* lpPSBlob    = NULL;
    ID3DBlob* lpErrorBlob = NULL;

    HRESULT hr = D3DCompile(g_szSpriteShader, sizeof(g_szSpriteShader) - 1, "SpriteShader", NULL, NULL, "VSMain", "vs_4_0", 0, 0, &lpVSBlob, &lpErrorBlob);

    if (FAILED(hr))
    {
        if (lpErrorBlob != NULL)
        {
            OutputDebugStringA(static_cast<const char*>(lpErrorBlob->GetBufferPointer()));
            lpErrorBlob->Release();
        }

        return false;
    }

    hr = D3DCompile(g_szSpriteShader, sizeof(g_szSpriteShader) - 1, "SpriteShader", NULL, NULL, "PSMain", "ps_4_0", 0, 0, &lpPSBlob, &lpErrorBlob);

    if (FAILED(hr))
    {
        if (lpErrorBlob != NULL)
        {
            OutputDebugStringA(static_cast<const char*>(lpErrorBlob->GetBufferPointer()));
            lpErrorBlob->Release();
        }

        lpVSBlob->Release();
        return false;
    }

    hr = g_lpD3DDevice->CreateVertexShader(lpVSBlob->GetBufferPointer(), lpVSBlob->GetBufferSize(), NULL, &g_lpVertexShader);

    if (SUCCEEDED(hr))
    {
        hr = g_lpD3DDevice->CreatePixelShader(lpPSBlob->GetBufferPointer(), lpPSBlob->GetBufferSize(), NULL, &g_lpPixelShader);
    }

    if (SUCCEEDED(hr))
    {
        D3D11_INPUT_ELEMENT_DESC aLayout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        hr = g_lpD3DDevice->CreateInputLayout(aLayout, 2, lpVSBlob->GetBufferPointer(), lpVSBlob->GetBufferSize(), &g_lpInputLayout);
    }

    lpVSBlob->Release();
    lpPSBlob->Release();

    if (FAILED(hr))
    {
        return false;
    }

    // 사각형 1개(정점 4개)를 매번 위치만 바꿔 그리므로 CPU에서 쓰기 가능한 동적 버퍼로 만든다
    D3D11_BUFFER_DESC bd = { 0 };
    bd.ByteWidth      = sizeof(SpriteVertex) * 4;
    bd.Usage          = D3D11_USAGE_DYNAMIC;
    bd.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = g_lpD3DDevice->CreateBuffer(&bd, NULL, &g_lpVertexBuffer);

    if (FAILED(hr))
    {
        return false;
    }

    // 도트 그림이 뿌옇게 번지지 않도록 POINT 필터 사용
    D3D11_SAMPLER_DESC sd = {};
    sd.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
    sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.ComparisonFunc = D3D11_COMPARISON_NEVER;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;

    hr = g_lpD3DDevice->CreateSamplerState(&sd, &g_lpSamplerState);

    if (FAILED(hr))
    {
        return false;
    }

    // PNG 알파값으로 반투명 블렌딩 (DirectX7의 컬러키와 달리 테두리가 부드럽게 나온다)
    D3D11_BLEND_DESC blend = {};
    blend.RenderTarget[0].BlendEnable           = TRUE;
    blend.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
    blend.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    blend.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    blend.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    blend.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    hr = g_lpD3DDevice->CreateBlendState(&blend, &g_lpBlendState);

    if (FAILED(hr))
    {
        return false;
    }

    D3D11_RASTERIZER_DESC rd = {};
    rd.FillMode        = D3D11_FILL_SOLID;
    rd.CullMode        = D3D11_CULL_NONE;
    rd.DepthClipEnable = TRUE;

    hr = g_lpD3DDevice->CreateRasterizerState(&rd, &g_lpRasterizerState);
    return SUCCEEDED(hr);
}

bool InitDirectInput(HWND hWnd)
{
    HRESULT hr = DirectInput8Create(
        GetModuleHandle(NULL),
        DIRECTINPUT_VERSION,
        IID_IDirectInput8,
        reinterpret_cast<void**>(&g_lpDI8),
        NULL);

    if (FAILED(hr))
    {
        return false;
    }

    hr = g_lpDI8->CreateDevice(GUID_SysKeyboard, &g_lpDIDKeyboard, NULL);

    if (FAILED(hr))
    {
        return false;
    }

    hr = g_lpDIDKeyboard->SetDataFormat(&c_dfDIKeyboard);

    if (FAILED(hr))
    {
        return false;
    }

    hr = g_lpDIDKeyboard->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_FOREGROUND);

    if (FAILED(hr))
    {
        return false;
    }

    g_lpDIDKeyboard->Acquire();

    g_fPlayerX = static_cast<float>(SCREEN_WIDTH) / 2.0f;
    g_fPlayerY = static_cast<float>(SCREEN_HEIGHT + SPRITE_HEIGHT) / 2.0f;
    return true;
}

// 실행 파일 위치에서 위로 올라가며 스프라이트 폴더를 찾는다 (VS 디버깅/직접 실행 모두 대응)
bool FindSpriteDirectory(WCHAR* pszOutDir)
{
    WCHAR szBase[MAX_PATH] = { 0 };
    GetModuleFileNameW(NULL, szBase, MAX_PATH);

    for (int nDepth = 0; nDepth < 8; ++nDepth)
    {
        WCHAR* pSlash = wcsrchr(szBase, L'\\');

        if (pSlash == NULL)
        {
            break;
        }

        *pSlash = L'\0';

        WCHAR szTest[MAX_PATH] = { 0 };
        wsprintfW(szTest, L"%s\\%s\\creature_%05d.png", szBase, SPRITE_DIR_NAME, SPRITE_FIRST_NUMBER);

        if (GetFileAttributesW(szTest) != INVALID_FILE_ATTRIBUTES)
        {
            wsprintfW(pszOutDir, L"%s\\%s", szBase, SPRITE_DIR_NAME);
            return true;
        }
    }

    return false;
}

// WIC로 PNG를 읽어 텍스처(B8G8R8A8)와 셰이더 리소스 뷰를 만든다
bool LoadSpriteTexture(IWICImagingFactory* pFactory, const WCHAR* pszPath, SpriteFrame& frame)
{
    IWICBitmapDecoder*     pDecoder   = NULL;
    IWICBitmapFrameDecode* pFrame     = NULL;
    IWICFormatConverter*   pConverter = NULL;
    std::vector<BYTE>      vecPixels;
    UINT                   uWidth     = 0;
    UINT                   uHeight    = 0;

    HRESULT hr = pFactory->CreateDecoderFromFilename(pszPath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &pDecoder);

    if (SUCCEEDED(hr))
    {
        hr = pDecoder->GetFrame(0, &pFrame);
    }

    if (SUCCEEDED(hr))
    {
        hr = pFactory->CreateFormatConverter(&pConverter);
    }

    if (SUCCEEDED(hr))
    {
        hr = pConverter->Initialize(pFrame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom);
    }

    if (SUCCEEDED(hr))
    {
        hr = pConverter->GetSize(&uWidth, &uHeight);
    }

    if (SUCCEEDED(hr))
    {
        vecPixels.resize(uWidth * uHeight * 4);
        hr = pConverter->CopyPixels(NULL, uWidth * 4, static_cast<UINT>(vecPixels.size()), vecPixels.data());
    }

    if (pConverter != NULL) pConverter->Release();
    if (pFrame     != NULL) pFrame->Release();
    if (pDecoder   != NULL) pDecoder->Release();

    if (FAILED(hr))
    {
        return false;
    }

    D3D11_TEXTURE2D_DESC td = { 0 };
    td.Width            = uWidth;
    td.Height           = uHeight;
    td.MipLevels        = 1;
    td.ArraySize        = 1;
    td.Format           = DXGI_FORMAT_B8G8R8A8_UNORM;
    td.SampleDesc.Count = 1;
    td.Usage            = D3D11_USAGE_IMMUTABLE;
    td.BindFlags        = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA srd = { 0 };
    srd.pSysMem     = vecPixels.data();
    srd.SysMemPitch = uWidth * 4;

    ID3D11Texture2D* lpTexture = NULL;
    hr = g_lpD3DDevice->CreateTexture2D(&td, &srd, &lpTexture);

    if (FAILED(hr))
    {
        return false;
    }

    hr = g_lpD3DDevice->CreateShaderResourceView(lpTexture, NULL, &frame.lpSRV);
    lpTexture->Release();  // 뷰가 텍스처 참조를 들고 있으므로 여기서 해제해도 된다

    if (FAILED(hr))
    {
        return false;
    }

    frame.nWidth  = static_cast<int>(uWidth);
    frame.nHeight = static_cast<int>(uHeight);
    return true;
}

bool LoadSprites()
{
    WCHAR szDir[MAX_PATH] = { 0 };

    if (!FindSpriteDirectory(szDir))
    {
        return false;
    }

    IWICImagingFactory* pFactory = NULL;
    HRESULT hr = CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&pFactory));

    if (FAILED(hr))
    {
        return false;
    }

    bool bResult = true;

    for (int i = 0; i < SPRITE_FRAME_COUNT; ++i)
    {
        WCHAR szPath[MAX_PATH] = { 0 };
        wsprintfW(szPath, L"%s\\creature_%05d.png", szDir, SPRITE_FIRST_NUMBER + i);

        if (!LoadSpriteTexture(pFactory, szPath, g_aSpriteFrames[i]))
        {
            bResult = false;
            break;
        }
    }

    pFactory->Release();
    return bResult;
}

bool CreateRenderTarget()
{
    if (g_lpSwapChain == NULL || g_lpD3DDevice == NULL)
    {
        return false;
    }

    ID3D11Texture2D* lpBackBuffer = NULL;
    HRESULT hr = g_lpSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&lpBackBuffer));

    if (FAILED(hr))
    {
        return false;
    }

    hr = g_lpD3DDevice->CreateRenderTargetView(lpBackBuffer, NULL, &g_lpRenderTargetView);
    lpBackBuffer->Release();

    if (FAILED(hr))
    {
        return false;
    }

    return true;
}

void ReleaseRenderTarget()
{
    if (g_lpRenderTargetView == NULL)
    {
        return;
    }

    g_lpRenderTargetView->Release();
    g_lpRenderTargetView = NULL;
}

void ResizeSwapChain(UINT uWidth, UINT uHeight)
{
    if (g_lpSwapChain == NULL)
    {
        return;
    }

    if (uWidth == 0 || uHeight == 0)
    {
        return;
    }

    ReleaseRenderTarget();
    g_lpD3DContext->OMSetRenderTargets(0, NULL, NULL);

    HRESULT hr = g_lpSwapChain->ResizeBuffers(0, uWidth, uHeight, DXGI_FORMAT_UNKNOWN, 0);

    if (FAILED(hr))
    {
        return;
    }

    CreateRenderTarget();
}

void UpdateInput(float fDeltaSec)
{
    if (g_lpDIDKeyboard == NULL)
    {
        return;
    }

    BYTE abKeyState[256] = { 0 };
    HRESULT hr = g_lpDIDKeyboard->GetDeviceState(sizeof(abKeyState), abKeyState);

    if (FAILED(hr))
    {
        g_lpDIDKeyboard->Acquire();
        return;
    }

    if (abKeyState[DIK_ESCAPE] & 0x80)
    {
        PostMessage(g_hWnd, WM_CLOSE, 0, 0);
        return;
    }

    // 입력 방향 -> 스프라이트 방향 (0=서, 1=남서, 2=남, 3=남동, 4=동, 5=북동, 6=북, 7=북서)
    static const int s_anDirTable[3][3] =
    {
        //  x=-1  x=0  x=+1
        {   7,    6,   5 },  // y=-1 (위)
        {   0,   -1,   4 },  // y= 0
        {   1,    2,   3 },  // y=+1 (아래)
    };

    int nDirX = 0;
    int nDirY = 0;

    if (abKeyState[DIK_LEFT]  & 0x80) nDirX -= 1;
    if (abKeyState[DIK_RIGHT] & 0x80) nDirX += 1;
    if (abKeyState[DIK_UP]    & 0x80) nDirY -= 1;
    if (abKeyState[DIK_DOWN]  & 0x80) nDirY += 1;

    // 대각선 키 두 개는 정확히 동시에 떼어지지 않는다.
    // 대각선 입력 직후 잠깐(DIAGONAL_RELEASE_SEC) 한쪽 키만 남은 경우는 키를 떼는 중으로 보고 대각선 방향을 유지한다.
    static float s_fSinceDiagonal = DIAGONAL_RELEASE_SEC;
    static int   s_nLastDiagX     = 0;
    static int   s_nLastDiagY     = 0;

    if (nDirX != 0 && nDirY != 0)
    {
        s_fSinceDiagonal = 0.0f;
        s_nLastDiagX     = nDirX;
        s_nLastDiagY     = nDirY;
    }
    else
    {
        s_fSinceDiagonal += fDeltaSec;
    }

    int nFaceDir = s_anDirTable[nDirY + 1][nDirX + 1];  // -1 = 방향키 없음

    bool bReleasingDiagonal = (s_fSinceDiagonal < DIAGONAL_RELEASE_SEC) &&
                              ((nDirX == 0 && nDirY == s_nLastDiagY) || (nDirY == 0 && nDirX == s_nLastDiagX));

    if (bReleasingDiagonal)
    {
        nFaceDir = s_anDirTable[s_nLastDiagY + 1][s_nLastDiagX + 1];
    }

    // 공격 중에는 동작이 끝날 때까지 이동/방향 전환 없이 공격 동작만 재생
    if (g_bPlayerAttack)
    {
        g_fAnimTime += fDeltaSec;

        if (g_fAnimTime * SPRITE_ATTACK_FPS >= SPRITE_ATTACK_FRAMES)
        {
            g_bPlayerAttack = false;
            g_bPlayerWalk   = false;
            g_fAnimTime     = 0.0f;
        }

        return;
    }

    // A 키를 누르면 공격 시작 (누르고 있으면 연속 공격).
    // 방향키를 같이 누르고 있으면 그 방향으로 돌아서 공격한다.
    if (abKeyState[DIK_A] & 0x80)
    {
        if (nFaceDir >= 0)
        {
            g_nPlayerDir = nFaceDir;
        }

        g_bPlayerAttack = true;
        g_bPlayerWalk   = false;
        g_fAnimTime     = 0.0f;
        return;
    }

    // 대각선 이동 시 속도가 빨라지지 않도록 1/√2 배
    float fMove = PLAYER_SPEED * fDeltaSec;

    if (nDirX != 0 && nDirY != 0)
    {
        fMove *= 0.70710678f;
    }

    g_fPlayerX += nDirX * fMove;
    g_fPlayerY += nDirY * fMove;

    if (g_fPlayerX < SPRITE_HALF_WIDTH)
    {
        g_fPlayerX = static_cast<float>(SPRITE_HALF_WIDTH);
    }

    if (g_fPlayerX > SCREEN_WIDTH - SPRITE_HALF_WIDTH)
    {
        g_fPlayerX = static_cast<float>(SCREEN_WIDTH - SPRITE_HALF_WIDTH);
    }

    if (g_fPlayerY < SPRITE_HEIGHT)
    {
        g_fPlayerY = static_cast<float>(SPRITE_HEIGHT);
    }

    if (g_fPlayerY > SCREEN_HEIGHT)
    {
        g_fPlayerY = static_cast<float>(SCREEN_HEIGHT);
    }

    int  nNewDir  = nFaceDir;
    bool bNewWalk = (nNewDir >= 0);

    if (bNewWalk != g_bPlayerWalk || (bNewWalk && nNewDir != g_nPlayerDir))
    {
        g_fAnimTime = 0.0f;  // 동작이나 방향이 바뀌면 애니메이션을 처음부터
    }

    if (bNewWalk)
    {
        g_nPlayerDir = nNewDir;
    }

    g_bPlayerWalk = bNewWalk;
    g_fAnimTime  += fDeltaSec;
}

// 발 위치(가로 중앙, 아래쪽)를 기준으로 스프라이트를 그린다
void DrawSprite(int nFrameIndex, int nFootX, int nFootY)
{
    SpriteFrame& frame = g_aSpriteFrames[nFrameIndex];

    if (frame.lpSRV == NULL)
    {
        return;
    }

    // 픽셀 좌표 -> 화면 좌표(-1 ~ 1, 위쪽이 +)
    float fLeft   = static_cast<float>(nFootX - frame.nWidth / 2);
    float fTop    = static_cast<float>(nFootY - frame.nHeight);
    float fRight  = fLeft + frame.nWidth;
    float fBottom = fTop + frame.nHeight;

    float fX0 =  fLeft   / SCREEN_WIDTH  * 2.0f - 1.0f;
    float fX1 =  fRight  / SCREEN_WIDTH  * 2.0f - 1.0f;
    float fY0 = -fTop    / SCREEN_HEIGHT * 2.0f + 1.0f;
    float fY1 = -fBottom / SCREEN_HEIGHT * 2.0f + 1.0f;

    D3D11_MAPPED_SUBRESOURCE mapped = { 0 };
    HRESULT hr = g_lpD3DContext->Map(g_lpVertexBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

    if (FAILED(hr))
    {
        return;
    }

    // 삼각형 스트립 순서: 좌상, 우상, 좌하, 우하
    SpriteVertex* pVertices = static_cast<SpriteVertex*>(mapped.pData);
    pVertices[0] = { fX0, fY0, 0.0f, 0.0f };
    pVertices[1] = { fX1, fY0, 1.0f, 0.0f };
    pVertices[2] = { fX0, fY1, 0.0f, 1.0f };
    pVertices[3] = { fX1, fY1, 1.0f, 1.0f };
    g_lpD3DContext->Unmap(g_lpVertexBuffer, 0);

    g_lpD3DContext->PSSetShaderResources(0, 1, &frame.lpSRV);
    g_lpD3DContext->Draw(4, 0);
}

void RenderFrame()
{
    if (g_lpD3DContext == NULL || g_lpRenderTargetView == NULL)
    {
        return;
    }

    static float fColorIndex = 0.0f;
    fColorIndex += 1.0f;

    if (fColorIndex > 255.0f)
    {
        fColorIndex = 0.0f;
    }

    const float fClearColor[4] =
    {
        fColorIndex / 255.0f,
        0.0f,
        1.0f - (fColorIndex / 255.0f),
        1.0f
    };

    D3D11_VIEWPORT viewport = { 0 };
    viewport.Width    = static_cast<float>(SCREEN_WIDTH);
    viewport.Height   = static_cast<float>(SCREEN_HEIGHT);
    viewport.MinDepth = 0.0f;
    viewport.MaxDepth = 1.0f;

    g_lpD3DContext->RSSetViewports(1, &viewport);
    g_lpD3DContext->OMSetRenderTargets(1, &g_lpRenderTargetView, NULL);
    g_lpD3DContext->ClearRenderTargetView(g_lpRenderTargetView, fClearColor);

    // 스프라이트 그리기 상태 설정
    UINT uStride = sizeof(SpriteVertex);
    UINT uOffset = 0;
    const float fBlendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

    g_lpD3DContext->IASetInputLayout(g_lpInputLayout);
    g_lpD3DContext->IASetVertexBuffers(0, 1, &g_lpVertexBuffer, &uStride, &uOffset);
    g_lpD3DContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    g_lpD3DContext->VSSetShader(g_lpVertexShader, NULL, 0);
    g_lpD3DContext->PSSetShader(g_lpPixelShader, NULL, 0);
    g_lpD3DContext->PSSetSamplers(0, 1, &g_lpSamplerState);
    g_lpD3DContext->RSSetState(g_lpRasterizerState);
    g_lpD3DContext->OMSetBlendState(g_lpBlendState, fBlendFactor, 0xFFFFFFFF);

    // 내가 이동할 수 있는 로드카오스 캐릭터 (멈춰 있으면 대기, 움직이면 걷기, A 키는 공격)
    int nActionStart = g_bPlayerWalk ? SPRITE_WALK_START  : SPRITE_IDLE_START;
    int nActionCount = g_bPlayerWalk ? SPRITE_WALK_FRAMES : SPRITE_IDLE_FRAMES;
    int nAnimFrame   = static_cast<int>(g_fAnimTime * SPRITE_ANIM_FPS) % nActionCount;

    if (g_bPlayerAttack)
    {
        // 공격은 반복하지 않고 한 번만 재생 (마지막 프레임에서 멈춤)
        nActionStart = SPRITE_ATTACK_START;
        nActionCount = SPRITE_ATTACK_FRAMES;
        nAnimFrame   = static_cast<int>(g_fAnimTime * SPRITE_ATTACK_FPS);

        if (nAnimFrame >= SPRITE_ATTACK_FRAMES)
        {
            nAnimFrame = SPRITE_ATTACK_FRAMES - 1;
        }
    }

    int nFrameIndex = nActionStart + g_nPlayerDir * nActionCount + nAnimFrame;

    DrawSprite(nFrameIndex, static_cast<int>(g_fPlayerX), static_cast<int>(g_fPlayerY));

    g_lpSwapChain->Present(1, 0);
}

void ReleaseSprites()
{
    for (int i = 0; i < SPRITE_FRAME_COUNT; ++i)
    {
        if (g_aSpriteFrames[i].lpSRV != NULL)
        {
            g_aSpriteFrames[i].lpSRV->Release();
            g_aSpriteFrames[i].lpSRV = NULL;
        }
    }
}

template <typename T>
void SafeRelease(T*& lpObject)
{
    if (lpObject != NULL)
    {
        lpObject->Release();
        lpObject = NULL;
    }
}

void ReleaseDirect3D()
{
    ReleaseRenderTarget();

    SafeRelease(g_lpRasterizerState);
    SafeRelease(g_lpBlendState);
    SafeRelease(g_lpSamplerState);
    SafeRelease(g_lpVertexBuffer);
    SafeRelease(g_lpInputLayout);
    SafeRelease(g_lpPixelShader);
    SafeRelease(g_lpVertexShader);

    if (g_lpSwapChain != NULL)
    {
        g_lpSwapChain->Release();
        g_lpSwapChain = NULL;
    }

    if (g_lpD3DContext != NULL)
    {
        g_lpD3DContext->Release();
        g_lpD3DContext = NULL;
    }

    if (g_lpD3DDevice != NULL)
    {
        g_lpD3DDevice->Release();
        g_lpD3DDevice = NULL;
    }
}

void ReleaseDirectInput()
{
    if (g_lpDIDKeyboard != NULL)
    {
        g_lpDIDKeyboard->Unacquire();
        g_lpDIDKeyboard->Release();
        g_lpDIDKeyboard = NULL;
    }

    if (g_lpDI8 != NULL)
    {
        g_lpDI8->Release();
        g_lpDI8 = NULL;
    }
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    if (uMsg == WM_KEYDOWN && wParam == VK_ESCAPE)
    {
        PostMessage(hWnd, WM_CLOSE, 0, 0);
        return 0;
    }

    if (uMsg == WM_SIZE)
    {
        ResizeSwapChain(LOWORD(lParam), HIWORD(lParam));
        return 0;
    }

    if (uMsg == WM_ACTIVATEAPP)
    {
        g_bActive = static_cast<BOOL>(wParam);
        return 0;
    }

    if (uMsg == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hWnd, uMsg, wParam, lParam);
}
