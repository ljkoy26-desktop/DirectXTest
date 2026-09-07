#include <windows.h>
#include <ddraw.h>

#pragma comment(lib, "ddraw.lib")
#pragma comment(lib, "dxguid.lib")

#define SCREEN_WIDTH  640
#define SCREEN_HEIGHT 480
#define SCREEN_BPP    32
#define FULLSCREEN_MODE 0  // 1 = 풀스크린 배타 모드, 0 = 창 모드

LPDIRECTDRAW7        g_lpDD7        = NULL;
LPDIRECTDRAWSURFACE7 g_lpDDSPrimary = NULL;
LPDIRECTDRAWSURFACE7 g_lpDDSBack    = NULL;
HWND                 g_hWnd         = NULL;
BOOL                 g_bActive      = FALSE;

LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
bool InitWindow(HINSTANCE hInstance, int nCmdShow);
bool InitDirectDraw(HWND hWnd);
bool RestoreSurfaces();
void RenderFrame();
void ReleaseDirectDraw();

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

    MSG msg = { 0 };

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

        RenderFrame();
    }

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
