#include <windows.h>
#include <ddraw.h>
#include <mmsystem.h>
#include <wincodec.h>
#include <vector>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#pragma comment(lib, "ddraw.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ole32.lib")

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#define SCREEN_BPP    32
#define PLAYER_SPEED  240.0f  // 초당 이동 픽셀 (프레임 속도에 무관하게 일정한 속도로 이동)
#define FULLSCREEN_MODE 0  // 1 = 풀스크린 배타 모드, 0 = 창 모드
#define FRAME_INTERVAL_MS 16  // Blt는 수직동기화를 기다리지 않으므로 약 60FPS로 직접 제한
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
#define SPRITE_COLOR_KEY_R   255 // 투명 처리용 컬러키 (마젠타)
#define SPRITE_COLOR_KEY_G   0
#define SPRITE_COLOR_KEY_B   255

struct SpriteFrame
{
    LPDIRECTDRAWSURFACE7 lpSurface;
    int                  nWidth;
    int                  nHeight;
    std::vector<DWORD>   vecPixels;  // PNG에서 읽은 32bit BGRA 원본 (서피스 복구 시 재사용)
};

LPDIRECTDRAW7        g_lpDD7         = NULL;
LPDIRECTDRAWSURFACE7 g_lpDDSPrimary  = NULL;
LPDIRECTDRAWSURFACE7 g_lpDDSBack     = NULL;
LPDIRECTINPUT8       g_lpDI8         = NULL;
LPDIRECTINPUTDEVICE8 g_lpDIDKeyboard = NULL;
HWND                 g_hWnd          = NULL;
BOOL                 g_bActive       = FALSE;
SpriteFrame          g_aSpriteFrames[SPRITE_FRAME_COUNT];
float                g_fPlayerX      = 0.0f;  // 캐릭터 발 위치 (가로 중앙)
float                g_fPlayerY      = 0.0f;  // 캐릭터 발 위치 (아래쪽)
int                  g_nPlayerDir    = 2;     // 처음에는 남쪽(정면)을 바라봄
bool                 g_bPlayerWalk   = false;
bool                 g_bPlayerAttack = false;
float                g_fAnimTime     = 0.0f;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirectDraw(HWND hWnd);
bool InitDirectInput(HWND hWnd);
bool LoadSprites();
bool UploadSpritePixels(SpriteFrame& frame);
bool RestoreSurfaces();
void UpdateInput(float fDeltaSec);
void DrawSprite(int nFrameIndex, int nFootX, int nFootY);
void RenderFrame();
void ReleaseSprites();
void ReleaseDirectDraw();
void ReleaseDirectInput();

int main()
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    HRESULT hrCom = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    if (!InitWindow(hInstance, SW_SHOWDEFAULT))
    {
        return 1;
    }

    if (!InitDirectDraw(g_hWnd))
    {
        ReleaseDirectDraw();
        return 1;
    }

    if (!LoadSprites())
    {
        MessageBox(g_hWnd, TEXT("로드카오스 스프라이트를 불러오지 못했습니다."), TEXT("DirectX7 Sample"), MB_OK | MB_ICONERROR);
        ReleaseSprites();
        ReleaseDirectDraw();
        return 1;
    }

    if (!InitDirectInput(g_hWnd))
    {
        ReleaseDirectInput();
        ReleaseSprites();
        ReleaseDirectDraw();
        return 1;
    }

    timeBeginPeriod(1);

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
            continue;
        }

        LARGE_INTEGER liNowTick = { 0 };
        QueryPerformanceCounter(&liNowTick);

        float fDeltaSec = static_cast<float>(liNowTick.QuadPart - liLastTick.QuadPart) / static_cast<float>(liFreq.QuadPart);

        if (fDeltaSec < FRAME_INTERVAL_MS / 1000.0f)
        {
            Sleep(1);
            continue;
        }

        liLastTick = liNowTick;

        int nFps = (fDeltaSec > 0.0f) ? static_cast<int>(1.0f / fDeltaSec) : 0;
        TCHAR szTitle[64];
        wsprintf(szTitle, TEXT("DirectX7 Sample - FPS:%d DeltaMS:%d"), nFps, static_cast<int>(fDeltaSec * 1000.0f));
        SetWindowText(g_hWnd, szTitle);

        UpdateInput(fDeltaSec);
        RenderFrame();
    }

    timeEndPeriod(1);

    ReleaseDirectInput();
    ReleaseSprites();
    ReleaseDirectDraw();

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
    wc.hbrBackground = NULL;
    wc.lpszClassName = TEXT("DirectX7SampleWndClass");

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
        TEXT("DirectX7 Sample"),
        dwStyle | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rcWindow.right - rcWindow.left, rcWindow.bottom - rcWindow.top,
        NULL, NULL, hInstance, NULL);

    if (g_hWnd == NULL)
    {
        return false;
    }

    ShowWindow(g_hWnd, nCmdShow);
    UpdateWindow(g_hWnd);
    return true;
}

bool InitDirectDraw(HWND hWnd)
{
    HRESULT hr = DirectDrawCreateEx(NULL, reinterpret_cast<void**>(&g_lpDD7), IID_IDirectDraw7, NULL);

    if (FAILED(hr))
    {
        return false;
    }

#if FULLSCREEN_MODE
    hr = g_lpDD7->SetCooperativeLevel(hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN | DDSCL_ALLOWREBOOT);

    if (FAILED(hr))
    {
        return false;
    }

    hr = g_lpDD7->SetDisplayMode(SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, 0, 0);

    if (FAILED(hr))
    {
        return false;
    }

    DDSURFACEDESC2 ddsd = { 0 };
    ddsd.dwSize         = sizeof(ddsd);
    ddsd.dwFlags        = DDSD_CAPS | DDSD_BACKBUFFERCOUNT;
    ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE | DDSCAPS_COMPLEX | DDSCAPS_FLIP;
    ddsd.dwBackBufferCount = 1;

    hr = g_lpDD7->CreateSurface(&ddsd, &g_lpDDSPrimary, NULL);

    if (FAILED(hr))
    {
        return false;
    }

    DDSCAPS2 ddscaps = { 0 };
    ddscaps.dwCaps = DDSCAPS_BACKBUFFER;

    hr = g_lpDDSPrimary->GetAttachedSurface(&ddscaps, &g_lpDDSBack);

    if (FAILED(hr))
    {
        return false;
    }
#else
    hr = g_lpDD7->SetCooperativeLevel(hWnd, DDSCL_NORMAL);

    if (FAILED(hr))
    {
        return false;
    }

    DDSURFACEDESC2 ddsdPrimary = { 0 };
    ddsdPrimary.dwSize         = sizeof(ddsdPrimary);
    ddsdPrimary.dwFlags        = DDSD_CAPS;
    ddsdPrimary.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;

    hr = g_lpDD7->CreateSurface(&ddsdPrimary, &g_lpDDSPrimary, NULL);

    if (FAILED(hr))
    {
        return false;
    }

    DDSURFACEDESC2 ddsdBack = { 0 };
    ddsdBack.dwSize         = sizeof(ddsdBack);
    ddsdBack.dwFlags        = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
    ddsdBack.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
    ddsdBack.dwWidth        = SCREEN_WIDTH;
    ddsdBack.dwHeight       = SCREEN_HEIGHT;

    hr = g_lpDD7->CreateSurface(&ddsdBack, &g_lpDDSBack, NULL);

    if (FAILED(hr))
    {
        return false;
    }

    LPDIRECTDRAWCLIPPER lpClipper = NULL;
    hr = g_lpDD7->CreateClipper(0, &lpClipper, NULL);

    if (FAILED(hr))
    {
        return false;
    }

    lpClipper->SetHWnd(0, hWnd);
    g_lpDDSPrimary->SetClipper(lpClipper);
    lpClipper->Release();
#endif

    g_bActive = TRUE;
    return true;
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
bool FindSpriteDirectory(WCHAR* pszOutDir, DWORD dwOutCount)
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

    UNREFERENCED_PARAMETER(dwOutCount);
    return false;
}

// WIC로 PNG를 32bit BGRA 픽셀 배열로 읽는다
bool LoadPngPixels(IWICImagingFactory* pFactory, const WCHAR* pszPath, SpriteFrame& frame)
{
    IWICBitmapDecoder*     pDecoder   = NULL;
    IWICBitmapFrameDecode* pFrame     = NULL;
    IWICFormatConverter*   pConverter = NULL;
    bool                   bResult    = false;

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

    UINT uWidth  = 0;
    UINT uHeight = 0;

    if (SUCCEEDED(hr))
    {
        hr = pConverter->GetSize(&uWidth, &uHeight);
    }

    if (SUCCEEDED(hr))
    {
        frame.nWidth  = static_cast<int>(uWidth);
        frame.nHeight = static_cast<int>(uHeight);
        frame.vecPixels.resize(uWidth * uHeight);

        hr = pConverter->CopyPixels(NULL, uWidth * 4, uWidth * uHeight * 4, reinterpret_cast<BYTE*>(frame.vecPixels.data()));
        bResult = SUCCEEDED(hr);
    }

    if (pConverter != NULL) pConverter->Release();
    if (pFrame     != NULL) pFrame->Release();
    if (pDecoder   != NULL) pDecoder->Release();
    return bResult;
}

// 8bit 색상 값을 서피스 픽셀 포맷(16/24/32bit)에 맞게 변환
DWORD PackColor(const DDPIXELFORMAT& ddpf, BYTE r, BYTE g, BYTE b)
{
    DWORD adwMask[3]  = { ddpf.dwRBitMask, ddpf.dwGBitMask, ddpf.dwBBitMask };
    BYTE  abValue[3]  = { r, g, b };
    DWORD dwResult    = 0;

    for (int i = 0; i < 3; ++i)
    {
        DWORD dwMask = adwMask[i];

        if (dwMask == 0)
        {
            continue;
        }

        int nShift = 0;
        while (((dwMask >> nShift) & 1) == 0) ++nShift;

        int nBits = 0;
        while (((dwMask >> (nShift + nBits)) & 1) != 0) ++nBits;

        DWORD dwValue = (nBits >= 8) ? (static_cast<DWORD>(abValue[i]) << (nBits - 8)) : (static_cast<DWORD>(abValue[i]) >> (8 - nBits));
        dwResult |= (dwValue << nShift) & dwMask;
    }

    return dwResult;
}

// 저장해 둔 BGRA 픽셀을 서피스에 기록. 반투명(알파 < 128) 픽셀은 컬러키로 바꿔 투명 처리
bool UploadSpritePixels(SpriteFrame& frame)
{
    if (frame.lpSurface == NULL)
    {
        return false;
    }

    DDSURFACEDESC2 ddsd = { 0 };
    ddsd.dwSize = sizeof(ddsd);

    HRESULT hr = frame.lpSurface->Lock(NULL, &ddsd, DDLOCK_WAIT | DDLOCK_WRITEONLY, NULL);

    if (FAILED(hr))
    {
        return false;
    }

    const DDPIXELFORMAT& ddpf = ddsd.ddpfPixelFormat;
    DWORD dwBytesPerPixel = ddpf.dwRGBBitCount / 8;
    DWORD dwColorKey      = PackColor(ddpf, SPRITE_COLOR_KEY_R, SPRITE_COLOR_KEY_G, SPRITE_COLOR_KEY_B);

    for (int y = 0; y < frame.nHeight; ++y)
    {
        BYTE* pRow = static_cast<BYTE*>(ddsd.lpSurface) + y * ddsd.lPitch;

        for (int x = 0; x < frame.nWidth; ++x)
        {
            DWORD dwSrc = frame.vecPixels[y * frame.nWidth + x];
            BYTE  a     = static_cast<BYTE>(dwSrc >> 24);
            BYTE  r     = static_cast<BYTE>(dwSrc >> 16);
            BYTE  g     = static_cast<BYTE>(dwSrc >> 8);
            BYTE  b     = static_cast<BYTE>(dwSrc);

            DWORD dwPixel = dwColorKey;

            if (a >= 128)
            {
                dwPixel = PackColor(ddpf, r, g, b);

                // 실제 그림 색이 우연히 컬러키와 같으면 살짝 바꿔서 구멍이 나지 않게 한다
                if (dwPixel == dwColorKey)
                {
                    dwPixel = PackColor(ddpf, r, 8, b);
                }
            }

            BYTE* pDst = pRow + x * dwBytesPerPixel;

            if (dwBytesPerPixel == 4)
            {
                *reinterpret_cast<DWORD*>(pDst) = dwPixel;
            }
            else if (dwBytesPerPixel == 2)
            {
                *reinterpret_cast<WORD*>(pDst) = static_cast<WORD>(dwPixel);
            }
            else if (dwBytesPerPixel == 3)
            {
                pDst[0] = static_cast<BYTE>(dwPixel);
                pDst[1] = static_cast<BYTE>(dwPixel >> 8);
                pDst[2] = static_cast<BYTE>(dwPixel >> 16);
            }
        }
    }

    frame.lpSurface->Unlock(NULL);
    return true;
}

bool LoadSprites()
{
    WCHAR szDir[MAX_PATH] = { 0 };

    if (!FindSpriteDirectory(szDir, MAX_PATH))
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

    for (int i = 0; i < SPRITE_FRAME_COUNT && bResult; ++i)
    {
        SpriteFrame& frame = g_aSpriteFrames[i];

        WCHAR szPath[MAX_PATH] = { 0 };
        wsprintfW(szPath, L"%s\\creature_%05d.png", szDir, SPRITE_FIRST_NUMBER + i);

        if (!LoadPngPixels(pFactory, szPath, frame))
        {
            bResult = false;
            break;
        }

        DDSURFACEDESC2 ddsd = { 0 };
        ddsd.dwSize         = sizeof(ddsd);
        ddsd.dwFlags        = DDSD_CAPS | DDSD_WIDTH | DDSD_HEIGHT;
        ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
        ddsd.dwWidth        = frame.nWidth;
        ddsd.dwHeight       = frame.nHeight;

        hr = g_lpDD7->CreateSurface(&ddsd, &frame.lpSurface, NULL);

        if (FAILED(hr))
        {
            bResult = false;
            break;
        }

        DDPIXELFORMAT ddpf = { 0 };
        ddpf.dwSize = sizeof(ddpf);
        frame.lpSurface->GetPixelFormat(&ddpf);

        DDCOLORKEY ddck = { 0 };
        ddck.dwColorSpaceLowValue  = PackColor(ddpf, SPRITE_COLOR_KEY_R, SPRITE_COLOR_KEY_G, SPRITE_COLOR_KEY_B);
        ddck.dwColorSpaceHighValue = ddck.dwColorSpaceLowValue;
        frame.lpSurface->SetColorKey(DDCKEY_SRCBLT, &ddck);

        bResult = UploadSpritePixels(frame);
    }

    pFactory->Release();
    return bResult;
}

bool RestoreSurfaces()
{
    if (g_lpDD7 == NULL)
    {
        return false;
    }

    // 주 서피스, 백버퍼, 스프라이트 서피스를 한 번에 복구한 뒤 스프라이트 그림을 다시 올린다
    HRESULT hr = g_lpDD7->RestoreAllSurfaces();

    if (FAILED(hr))
    {
        return false;
    }

    for (int i = 0; i < SPRITE_FRAME_COUNT; ++i)
    {
        UploadSpritePixels(g_aSpriteFrames[i]);
    }

    return true;
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

// 발 위치(가로 중앙, 아래쪽)를 기준으로 스프라이트를 그린다. 화면 밖 부분은 직접 잘라낸다
void DrawSprite(int nFrameIndex, int nFootX, int nFootY)
{
    SpriteFrame& frame = g_aSpriteFrames[nFrameIndex];

    if (frame.lpSurface == NULL)
    {
        return;
    }

    RECT rcSrc = { 0, 0, frame.nWidth, frame.nHeight };
    RECT rcDst = { nFootX - frame.nWidth / 2, nFootY - frame.nHeight, 0, 0 };
    rcDst.right  = rcDst.left + frame.nWidth;
    rcDst.bottom = rcDst.top + frame.nHeight;

    if (rcDst.left < 0)              { rcSrc.left   -= rcDst.left;                     rcDst.left   = 0; }
    if (rcDst.top < 0)               { rcSrc.top    -= rcDst.top;                      rcDst.top    = 0; }
    if (rcDst.right > SCREEN_WIDTH)  { rcSrc.right  -= rcDst.right - SCREEN_WIDTH;     rcDst.right  = SCREEN_WIDTH; }
    if (rcDst.bottom > SCREEN_HEIGHT){ rcSrc.bottom -= rcDst.bottom - SCREEN_HEIGHT;   rcDst.bottom = SCREEN_HEIGHT; }

    if (rcDst.left >= rcDst.right || rcDst.top >= rcDst.bottom)
    {
        return;
    }

    g_lpDDSBack->Blt(&rcDst, frame.lpSurface, &rcSrc, DDBLT_KEYSRC | DDBLT_WAIT, NULL);
}

void RenderFrame()
{
    if (g_lpDDSBack == NULL)
    {
        return;
    }

    static DWORD dwColorIndex = 0;
    dwColorIndex = (dwColorIndex + 1) % 256;

    DDBLTFX ddbltfx = { 0 };
    ddbltfx.dwSize      = sizeof(ddbltfx);
    ddbltfx.dwFillColor = RGB(dwColorIndex, 0, 255 - dwColorIndex);
    
    HRESULT hr = g_lpDDSBack->Blt(NULL, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &ddbltfx);

    if (hr == DDERR_SURFACELOST)
    {
        RestoreSurfaces();
        return;
    }

    if (FAILED(hr))
    {
        return;
    }

    // 내가 이동할 수 있는 로드카오스 캐릭터 (멈춰 있으면 대기, 움직이면 걷기 동작)
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
    int nFrameIndex  = nActionStart + g_nPlayerDir * nActionCount + nAnimFrame;

    DrawSprite(nFrameIndex, static_cast<int>(g_fPlayerX), static_cast<int>(g_fPlayerY));

#if FULLSCREEN_MODE
    hr = g_lpDDSPrimary->Flip(NULL, DDFLIP_WAIT);
#else
    RECT rcClient = { 0 };
    GetClientRect(g_hWnd, &rcClient);

    POINT ptOrigin = { 0, 0 };
    ClientToScreen(g_hWnd, &ptOrigin);
    OffsetRect(&rcClient, ptOrigin.x, ptOrigin.y);

    hr = g_lpDDSPrimary->Blt(&rcClient, g_lpDDSBack, NULL, DDBLT_WAIT, NULL);
#endif

    if (hr == DDERR_SURFACELOST)
    {
        RestoreSurfaces();
    }
}

void ReleaseSprites()
{
    for (int i = 0; i < SPRITE_FRAME_COUNT; ++i)
    {
        if (g_aSpriteFrames[i].lpSurface != NULL)
        {
            g_aSpriteFrames[i].lpSurface->Release();
            g_aSpriteFrames[i].lpSurface = NULL;
        }
    }
}

void ReleaseDirectDraw()
{
    if (g_lpDDSBack != NULL)
    {
        g_lpDDSBack->Release();
        g_lpDDSBack = NULL;
    }

    if (g_lpDDSPrimary != NULL)
    {
        g_lpDDSPrimary->Release();
        g_lpDDSPrimary = NULL;
    }

    if (g_lpDD7 != NULL)
    {
        g_lpDD7->SetCooperativeLevel(g_hWnd, DDSCL_NORMAL);
        g_lpDD7->Release();
        g_lpDD7 = NULL;
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
