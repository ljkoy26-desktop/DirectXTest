#include <windows.h>
#include <ddraw.h>
#include <mmsystem.h>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

#pragma comment(lib, "ddraw.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "dinput8.lib")
#pragma comment(lib, "winmm.lib")

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#define SCREEN_BPP    32
#define SQUARE_SIZE   32
#define SQUARE_SPEED  240.0f  // 초당 이동 픽셀 (프레임 속도에 무관하게 일정한 속도로 이동)
#define FULLSCREEN_MODE 0  // 1 = 풀스크린 배타 모드, 0 = 창 모드
#define FRAME_INTERVAL_MS 16  // Blt는 수직동기화를 기다리지 않으므로 약 60FPS로 직접 제한

LPDIRECTDRAW7        g_lpDD7         = NULL;
LPDIRECTDRAWSURFACE7 g_lpDDSPrimary  = NULL;
LPDIRECTDRAWSURFACE7 g_lpDDSBack     = NULL;
LPDIRECTINPUT8       g_lpDI8         = NULL;
LPDIRECTINPUTDEVICE8 g_lpDIDKeyboard = NULL;
HWND                 g_hWnd          = NULL;
BOOL                 g_bActive       = FALSE;
float                g_fSquareX      = 0.0f;
float                g_fSquareY      = 0.0f;
int                  g_nSquareX      = 0;
int                  g_nSquareY      = 0;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirectDraw(HWND hWnd);
bool InitDirectInput(HWND hWnd);
bool RestoreSurfaces();
void UpdateInput(float fDeltaSec);
void RenderFrame();
void ReleaseDirectDraw();
void ReleaseDirectInput();

int main()
{
    HINSTANCE hInstance = GetModuleHandle(NULL);

    if (!InitWindow(hInstance, SW_SHOWDEFAULT))
    {
        return 1;
    }

    if (!InitDirectDraw(g_hWnd))
    {
        ReleaseDirectDraw();
        return 1;
    }

    if (!InitDirectInput(g_hWnd))
    {
        ReleaseDirectInput();
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
    ReleaseDirectDraw();
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

    g_fSquareX = static_cast<float>(SCREEN_WIDTH - SQUARE_SIZE) / 2.0f;
    g_fSquareY = static_cast<float>(SCREEN_HEIGHT - SQUARE_SIZE) / 2.0f;
    g_nSquareX = static_cast<int>(g_fSquareX);
    g_nSquareY = static_cast<int>(g_fSquareY);
    return true;
}

bool RestoreSurfaces()
{
    if (g_lpDDSPrimary == NULL)
    {
        return false;
    }

    HRESULT hr = g_lpDDSPrimary->Restore();

    if (FAILED(hr))
    {
        return false;
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

    float fMove = SQUARE_SPEED * fDeltaSec;

    if (abKeyState[DIK_LEFT] & 0x80)
    {
        g_fSquareX -= fMove;
    }

    if (abKeyState[DIK_RIGHT] & 0x80)
    {
        g_fSquareX += fMove;
    }

    if (abKeyState[DIK_UP] & 0x80)
    {
        g_fSquareY -= fMove;
    }

    if (abKeyState[DIK_DOWN] & 0x80)
    {
        g_fSquareY += fMove;
    }

    if (g_fSquareX < 0.0f)
    {
        g_fSquareX = 0.0f;
    }

    if (g_fSquareX > SCREEN_WIDTH - SQUARE_SIZE)
    {
        g_fSquareX = static_cast<float>(SCREEN_WIDTH - SQUARE_SIZE);
    }

    if (g_fSquareY < 0.0f)
    {
        g_fSquareY = 0.0f;
    }

    if (g_fSquareY > SCREEN_HEIGHT - SQUARE_SIZE)
    {
        g_fSquareY = static_cast<float>(SCREEN_HEIGHT - SQUARE_SIZE);
    }

    g_nSquareX = static_cast<int>(g_fSquareX);
    g_nSquareY = static_cast<int>(g_fSquareY);
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

    RECT rcSquare = { g_nSquareX, g_nSquareY, g_nSquareX + SQUARE_SIZE, g_nSquareY + SQUARE_SIZE };

    DDBLTFX ddbltfxSquare = { 0 };
    ddbltfxSquare.dwSize      = sizeof(ddbltfxSquare);
    ddbltfxSquare.dwFillColor = RGB(255, 255, 0);

    g_lpDDSBack->Blt(&rcSquare, NULL, NULL, DDBLT_COLORFILL | DDBLT_WAIT, &ddbltfxSquare);

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
